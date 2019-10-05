/*
 *  functions.cpp  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2001-2019 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "functions.h"
#include "functions_p.h"

#include "collectionmodel.h"
#include "collectionsearch.h"
#include "alarmcalendar.h"
#include "alarmtime.h"
#include "autoqpointer.h"
#include "alarmlistview.h"
#include "editdlg.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "messagewin.h"
#include "preferences.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "templatemenuaction.h"

#include "config-kalarm.h"

#include <kalarmcal_version.h>
#include <kalarmcal/identities.h>
#include <kalarmcal/kaevent.h>

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/Person>
#include <KCalendarCore/Duration>
using namespace KCalendarCore;
#include <KIdentityManagement/kidentitymanagement/identitymanager.h>
#include <KIdentityManagement/kidentitymanagement/identity.h>
#include <KHolidays/HolidayRegion>

#include <kconfiggroup.h>
#include <KSharedConfig>
#include <ktoggleaction.h>
#include <kactioncollection.h>
#include <KLocalizedString>
#include <kauth.h>
#include <kstandardguiitem.h>
#include <kstandardshortcut.h>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <kfileitem.h>

#include <QAction>
#include <QDir>
#include <QRegExp>
#include <QFileDialog>
#include <QDesktopWidget>
#include <QtDBus>
#include <QTimer>
#include <qglobal.h>
#include <QStandardPaths>
#include "kalarm_debug.h"

using namespace Akonadi;


namespace
{
bool            refreshAlarmsQueued = false;

struct UpdateStatusData
{
    KAlarm::UpdateResult status;   // status code and KOrganizer error message if any
    int                  warnErr;
    int                  warnKOrg;

    explicit UpdateStatusData(KAlarm::UpdateStatus s = KAlarm::UPDATE_OK) : status(s), warnErr(0), warnKOrg(0) {}
    // Set an error status and increment to number of errors to warn about
    void setError(KAlarm::UpdateStatus st, int errorCount = -1)
    {
        status.set(st);
        if (errorCount < 0)
            ++warnErr;
        else
            warnErr = errorCount;
    }
    // Update the error status with a KOrganizer related status
    void korgUpdate(const KAlarm::UpdateResult& result)
    {
        if (result.status != KAlarm::UPDATE_OK)
        {
            ++warnKOrg;
            if (result.status > status.status)
                status = result;
        }
    }
};

const QLatin1String KMAIL_DBUS_SERVICE("org.kde.kmail");
//const QLatin1String KMAIL_DBUS_IFACE("org.kde.kmail.kmail");
//const QLatin1String KMAIL_DBUS_WINDOW_PATH("/kmail/kmail_mainwindow_1");
const QLatin1String KORG_DBUS_SERVICE("org.kde.korganizer");
const QLatin1String KORG_DBUS_IFACE("org.kde.korganizer.Korganizer");
// D-Bus object path of KOrganizer's notification interface
#define       KORG_DBUS_PATH            "/Korganizer"
#define       KORG_DBUS_LOAD_PATH       "/korganizer_PimApplication"
//const QLatin1String KORG_DBUS_WINDOW_PATH("/korganizer/MainWindow_1");
const QLatin1String KORG_MIME_TYPE("application/x-vnd.akonadi.calendar.event");
const QLatin1String KORGANIZER_UID("korg-");

const QLatin1String ALARM_OPTS_FILE("alarmopts");
const char*         DONT_SHOW_ERRORS_GROUP = "DontShowErrors";

void editNewTemplate(EditAlarmDlg::Type, const KAEvent* preset, QWidget* parent);
void displayUpdateError(QWidget* parent, KAlarm::UpdateError, const UpdateStatusData&, bool showKOrgError = true);
KAlarm::UpdateResult sendToKOrganizer(const KAEvent&);
KAlarm::UpdateResult deleteFromKOrganizer(const QString& eventID);
KAlarm::UpdateResult runKOrganizer();
QString uidKOrganizer(const QString& eventID);
}


namespace KAlarm
{

Private* Private::mInstance = nullptr;

/******************************************************************************
* Display a main window with the specified event selected.
*/
MainWindow* displayMainWindowSelected(Akonadi::Item::Id eventId)
{
    MainWindow* win = MainWindow::firstWindow();
    if (!win)
    {
        if (theApp()->checkCalendar())    // ensure calendar is open
        {
            win = MainWindow::create();
            win->show();
        }
    }
    else
    {
        // There is already a main window, so make it the active window
        win->hide();        // in case it's on a different desktop
        win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
        win->show();
        win->raise();
        win->activateWindow();
    }
    if (win  &&  eventId >= 0)
        win->selectEvent(eventId);
    return win;
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
KToggleAction* createAlarmEnableAction(QObject* parent)
{
    KToggleAction* action = new KToggleAction(i18nc("@action", "Enable &Alarms"), parent);
    action->setChecked(theApp()->alarmsEnabled());
    QObject::connect(action, &QAction::toggled, theApp(), &KAlarmApp::setAlarmsEnabled);
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), &KAlarmApp::alarmEnabledToggled, action, &QAction::setChecked);
    return action;
}

/******************************************************************************
* Create a "Stop Play" action.
*/
QAction* createStopPlayAction(QObject* parent)
{
    QAction* action = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), i18nc("@action", "Stop Play"), parent);
    action->setEnabled(MessageWin::isAudioPlaying());
    QObject::connect(action, &QAction::triggered, theApp(), &KAlarmApp::stopAudio);
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), &KAlarmApp::audioPlaying, action, &QAction::setEnabled);
    return action;
}

/******************************************************************************
* Create a "Spread Windows" action.
*/
KToggleAction* createSpreadWindowsAction(QObject* parent)
{
    KToggleAction* action = new KToggleAction(i18nc("@action", "Spread Windows"), parent);
    QObject::connect(action, &QAction::triggered, theApp(), &KAlarmApp::spreadWindows);
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), &KAlarmApp::spreadWindowsToggled, action, &QAction::setChecked);
    return action;
}

/******************************************************************************
* Add a new active (non-archived) alarm.
* Save it in the calendar file and add it to every main window instance.
* Parameters: msgParent = parent widget for any calendar selection prompt or
*                         error message.
*             event - is updated with the actual event ID.
*/
UpdateResult addEvent(KAEvent& event, Collection* calendar, QWidget* msgParent, int options, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::addEvent:" << event.id();
    bool cancelled = false;
    UpdateStatusData status;
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status.status = UPDATE_FAILED;
    else
    {
        // Save the event details in the calendar file, and get the new event ID
        AlarmCalendar* cal = AlarmCalendar::resources();
        // Note that AlarmCalendar::addEvent() updates 'event'.
        if (!cal->addEvent(event, msgParent, (options & USE_EVENT_ID), calendar, (options & NO_RESOURCE_PROMPT), &cancelled))
        {
            status.status = UPDATE_FAILED;
        }
        else
        {
            if (!cal->save())
                status.status = SAVE_FAILED;
        }
        if (status.status == UPDATE_OK)
        {
            if ((options & ALLOW_KORG_UPDATE)  &&  event.copyToKOrganizer())
            {
                UpdateResult st = sendToKOrganizer(event);    // tell KOrganizer to show the event
                status.korgUpdate(st);
            }

        }
    }

    if (status.status != UPDATE_OK  &&  !cancelled  &&  msgParent)
        displayUpdateError(msgParent, ERR_ADD, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Add a list of new active (non-archived) alarms.
* Save them in the calendar file and add them to every main window instance.
* The events are updated with their actual event IDs.
*/
UpdateResult addEvents(QVector<KAEvent>& events, QWidget* msgParent, bool allowKOrgUpdate, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::addEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    Collection collection;
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status.status = UPDATE_FAILED;
    else
    {
        collection = CollectionControlModel::instance()->destination(CalEvent::ACTIVE, msgParent);
        if (!collection.isValid())
        {
            qCDebug(KALARM_LOG) << "KAlarm::addEvents: No calendar";
            status.status = UPDATE_FAILED;
        }
    }
    if (status.status == UPDATE_OK)
    {
        AlarmCalendar* cal = AlarmCalendar::resources();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            // Save the event details in the calendar file, and get the new event ID
            KAEvent& event = events[i];
            if (!cal->addEvent(event, msgParent, false, &collection))
            {
                status.setError(UPDATE_ERROR);
                continue;
            }
            if (allowKOrgUpdate  &&  event.copyToKOrganizer())
            {
                UpdateResult st = sendToKOrganizer(event);    // tell KOrganizer to show the event
                status.korgUpdate(st);
            }

        }
        if (status.warnErr == events.count())
            status.status = UPDATE_FAILED;
        else if (!cal->save())
            status.setError(SAVE_FAILED, events.count());  // everything failed
    }

    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_ADD, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Save the event in the archived calendar and adjust every main window instance.
* The event's ID is changed to an archived ID if necessary.
*/
bool addArchivedEvent(KAEvent& event, Collection* collection)
{
    qCDebug(KALARM_LOG) << "KAlarm::addArchivedEvent:" << event.id();
    bool archiving = (event.category() == CalEvent::ACTIVE);
    if (archiving  &&  !Preferences::archivedKeepDays())
        return false;   // expired alarms aren't being kept
    AlarmCalendar* cal = AlarmCalendar::resources();
    KAEvent newevent(event);
    newevent.setItemId(-1);    // invalidate the Akonadi item ID since it's a new item
    KAEvent* const newev = &newevent;
    if (archiving)
    {
        newev->setCategory(CalEvent::ARCHIVED);    // this changes the event ID
        newev->setCreatedDateTime(KADateTime::currentUtcDateTime());   // time stamp to control purging
    }
    // Note that archived resources are automatically saved after changes are made
    if (!cal->addEvent(newevent, nullptr, false, collection))
        return false;
    event = *newev;   // update event ID etc.

    return true;
}

/******************************************************************************
* Add a new template.
* Save it in the calendar file and add it to every template list view.
* 'event' is updated with the actual event ID.
* Parameters: promptParent = parent widget for any calendar selection prompt.
*/
UpdateResult addTemplate(KAEvent& event, Collection* collection, QWidget* msgParent)
{
    qCDebug(KALARM_LOG) << "KAlarm::addTemplate:" << event.id();
    UpdateStatusData status;

    // Add the template to the calendar file
    AlarmCalendar* cal = AlarmCalendar::resources();
    KAEvent newev(event);
    if (!cal->addEvent(newev, msgParent, false, collection))
        status.status = UPDATE_FAILED;
    else
    {
        event = newev;   // update event ID etc.
        if (!cal->save())
            status.status = SAVE_FAILED;
        else
        {
            return UpdateResult(UPDATE_OK);
        }
    }

    if (msgParent)
        displayUpdateError(msgParent, ERR_TEMPLATE, status);
    return status.status;
}

/******************************************************************************
* Modify an active (non-archived) alarm in the calendar file and in every main
* window instance.
* The new event must have a different event ID from the old one.
*/
UpdateResult modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::modifyEvent:" << oldEvent.id();

    UpdateStatusData status;
    if (!newEvent.isValid())
    {
        deleteEvent(oldEvent, true);
        status.status = UPDATE_FAILED;
    }
    else
    {
        EventId oldId(oldEvent);
        if (oldEvent.copyToKOrganizer())
        {
            // Tell KOrganizer to delete its old event.
            // But ignore errors, because the user could have manually
            // deleted it since KAlarm asked KOrganizer to set it up.
            deleteFromKOrganizer(oldId.eventId());
        }
        // Update the event in the calendar file, and get the new event ID
        AlarmCalendar* cal = AlarmCalendar::resources();
        if (!cal->modifyEvent(oldId, newEvent))
            status.status = UPDATE_FAILED;
        else
        {
            if (!cal->save())
                status.status = SAVE_FAILED;
            if (status.status == UPDATE_OK)
            {
                if (newEvent.copyToKOrganizer())
                {
                    UpdateResult st = sendToKOrganizer(newEvent);    // tell KOrganizer to show the new event
                    status.korgUpdate(st);
                }

                // Remove "Don't show error messages again" for the old alarm
                setDontShowErrors(oldId);
            }
        }
    }

    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_MODIFY, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Update an active (non-archived) alarm from the calendar file and from every
* main window instance.
* The new event will have the same event ID as the old one.
* The event is not updated in KOrganizer, since this function is called when an
* existing alarm is rescheduled (due to recurrence or deferral).
*/
UpdateResult updateEvent(KAEvent& event, QWidget* msgParent, bool archiveOnDelete)
{
    qCDebug(KALARM_LOG) << "KAlarm::updateEvent:" << event.id();

    if (!event.isValid())
        deleteEvent(event, archiveOnDelete);
    else
    {
        // Update the event in the calendar file.
        AlarmCalendar* cal = AlarmCalendar::resources();
        cal->updateEvent(event);
        if (!cal->save())
        {
            if (msgParent)
                displayUpdateError(msgParent, ERR_ADD, UpdateStatusData(SAVE_FAILED));
            return UpdateResult(SAVE_FAILED);
        }

    }
    return UpdateResult(UPDATE_OK);
}

/******************************************************************************
* Update a template in the calendar file and in every template list view.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
UpdateResult updateTemplate(KAEvent& event, QWidget* msgParent)
{
    AlarmCalendar* cal = AlarmCalendar::resources();
    const KAEvent* newEvent = cal->updateEvent(event);
    UpdateStatus status = UPDATE_OK;
    if (!newEvent)
        status = UPDATE_FAILED;
    else if (!cal->save())
        status = SAVE_FAILED;
    if (status != UPDATE_OK)
    {
        if (msgParent)
            displayUpdateError(msgParent, ERR_TEMPLATE, UpdateStatusData(SAVE_FAILED));
        return UpdateResult(status);
    }

    return UpdateResult(UPDATE_OK);
}

/******************************************************************************
* Delete alarms from the calendar file and from every main window instance.
* If the events are archived, the events' IDs are changed to archived IDs if necessary.
*/
UpdateResult deleteEvent(KAEvent& event, bool archive, QWidget* msgParent, bool showKOrgErr)
{
    QVector<KAEvent> events(1, event);
    return deleteEvents(events, archive, msgParent, showKOrgErr);
}

UpdateResult deleteEvents(QVector<KAEvent>& events, bool archive, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::deleteEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    AlarmCalendar* cal = AlarmCalendar::resources();
    bool deleteWakeFromSuspendAlarm = false;
    const QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        // Save the event details in the calendar file, and get the new event ID
        KAEvent* event = &events[i];
        const QString id = event->id();


        // Delete the event from the calendar file
        if (event->category() != CalEvent::ARCHIVED)
        {
            if (event->copyToKOrganizer())
            {
                // The event was shown in KOrganizer, so tell KOrganizer to
                // delete it. But ignore errors, because the user could have
                // manually deleted it from KOrganizer since it was set up.
                UpdateResult st = deleteFromKOrganizer(id);
                status.korgUpdate(st);
            }
            if (archive  &&  event->toBeArchived())
            {
                KAEvent ev(*event);
                addArchivedEvent(ev);     // this changes the event ID to an archived ID
            }
        }
        if (!cal->deleteEvent(*event, false))   // don't save calendar after deleting
            status.setError(UPDATE_ERROR);

        if (id == wakeFromSuspendId)
            deleteWakeFromSuspendAlarm = true;

        // Remove "Don't show error messages again" for this alarm
        setDontShowErrors(EventId(*event));
    }

    if (status.warnErr == events.count())
        status.status = UPDATE_FAILED;
    else if (!cal->save())      // save the calendars now
        status.setError(SAVE_FAILED, events.count());
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_DELETE, status, showKOrgErr);

    // Remove any wake-from-suspend scheduled for a deleted alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);

    return status.status;
}

/******************************************************************************
* Delete templates from the calendar file and from every template list view.
*/
UpdateResult deleteTemplates(const KAEvent::List& events, QWidget* msgParent)
{
    int count = events.count();
    qCDebug(KALARM_LOG) << "KAlarm::deleteTemplates:" << count;
    if (!count)
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    AlarmCalendar* cal = AlarmCalendar::resources();
    for (const KAEvent* event : events)
    {
        // Update the window lists
        // Delete the template from the calendar file
        AlarmCalendar* cal = AlarmCalendar::resources();
        if (!cal->deleteEvent(*event, false))   // don't save calendar after deleting
            status.setError(UPDATE_ERROR);
    }

    if (status.warnErr == count)
        status.status = UPDATE_FAILED;
    else if (!cal->save())      // save the calendars now
        status.setError(SAVE_FAILED, count);
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_TEMPLATE, status);
    return status.status;
}

/******************************************************************************
* Delete an alarm from the display calendar.
*/
void deleteDisplayEvent(const QString& eventID)
{
    qCDebug(KALARM_LOG) << "KAlarm::deleteDisplayEvent:" << eventID;
    AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
    if (cal)
        cal->deleteDisplayEvent(eventID, true);   // save calendar after deleting
}

/******************************************************************************
* Undelete archived alarms, and update every main window instance.
* The archive bit is set to ensure that they get re-archived if deleted again.
* 'ineligibleIDs' is filled in with the IDs of any ineligible events.
*/
UpdateResult reactivateEvent(KAEvent& event, Collection* calendar, QWidget* msgParent, bool showKOrgErr)
{
    QVector<EventId> ids;
    QVector<KAEvent> events(1, event);
    return reactivateEvents(events, ids, calendar, msgParent, showKOrgErr);
}

UpdateResult reactivateEvents(QVector<KAEvent>& events, QVector<EventId>& ineligibleIDs, Collection* col, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::reactivateEvents:" << events.count();
    ineligibleIDs.clear();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    Collection collection;
    if (col)
        collection = *col;
    if (!collection.isValid())
        collection = CollectionControlModel::instance()->destination(CalEvent::ACTIVE, msgParent);
    if (!collection.isValid())
    {
        qCDebug(KALARM_LOG) << "KAlarm::reactivateEvents: No calendar";
        status.setError(UPDATE_FAILED, events.count());
    }
    else
    {
        int count = 0;
        AlarmCalendar* cal = AlarmCalendar::resources();
        const KADateTime now = KADateTime::currentUtcDateTime();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            // Delete the event from the archived resource
            KAEvent* event = &events[i];
            if (event->category() != CalEvent::ARCHIVED
            ||  !event->occursAfter(now, true))
            {
                ineligibleIDs += EventId(*event);
                continue;
            }
            ++count;

            KAEvent newevent(*event);
            KAEvent* const newev = &newevent;
            newev->setCategory(CalEvent::ACTIVE);    // this changes the event ID
            if (newev->recurs()  ||  newev->repetition())
                newev->setNextOccurrence(now);   // skip any recurrences in the past
            newev->setArchive();    // ensure that it gets re-archived if it is deleted

            // Save the event details in the calendar file.
            // This converts the event ID.
            if (!cal->addEvent(newevent, msgParent, true, &collection))
            {
                status.setError(UPDATE_ERROR);
                continue;
            }
            if (newev->copyToKOrganizer())
            {
                UpdateResult st = sendToKOrganizer(*newev);    // tell KOrganizer to show the event
                status.korgUpdate(st);
            }


            if (cal->event(EventId(*event))  // no error if event doesn't exist in archived resource
            &&  !cal->deleteEvent(*event, false))   // don't save calendar after deleting
                status.setError(UPDATE_ERROR);
            events[i] = newevent;
        }

        if (status.warnErr == count)
            status.status = UPDATE_FAILED;
        // Save the calendars, even if all events failed, since more than one calendar was updated
        if (!cal->save()  &&  status.status != UPDATE_FAILED)
            status.setError(SAVE_FAILED, count);
    }
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_REACTIVATE, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Enable or disable alarms in the calendar file and in every main window instance.
* The new events will have the same event IDs as the old ones.
*/
UpdateResult enableEvents(QVector<KAEvent>& events, bool enable, QWidget* msgParent)
{
    qCDebug(KALARM_LOG) << "KAlarm::enableEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    AlarmCalendar* cal = AlarmCalendar::resources();
    bool deleteWakeFromSuspendAlarm = false;
    const QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        KAEvent* event = &events[i];
        if (event->category() == CalEvent::ACTIVE
        &&  enable != event->enabled())
        {
            event->setEnabled(enable);

            if (!enable  &&  event->id() == wakeFromSuspendId)
                deleteWakeFromSuspendAlarm = true;

            // Update the event in the calendar file
            const KAEvent* newev = cal->updateEvent(event);
            if (!newev)
                qCCritical(KALARM_LOG) << "KAlarm::enableEvents: Error updating event in calendar:" << event->id();
            else
            {
                cal->disabledChanged(newev);

                // If we're disabling a display alarm, close any message window
                if (!enable  &&  (event->actionTypes() & KAEvent::ACT_DISPLAY))
                {
                    MessageWin* win = MessageWin::findEvent(EventId(*event));
                    delete win;
                }
            }
        }
    }

    if (!cal->save())
        status.setError(SAVE_FAILED, events.count());
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_ADD, status);

    // Remove any wake-from-suspend scheduled for a disabled alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);

    return status.status;
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge all archived events from the default archived alarm resource whose end
* time is longer ago than 'purgeDays'. All events are deleted if 'purgeDays' is
* zero.
*/
void purgeArchive(int purgeDays)
{
    if (purgeDays < 0)
        return;
    qCDebug(KALARM_LOG) << "KAlarm::purgeArchive:" << purgeDays;
    const QDate cutoff = KADateTime::currentLocalDate().addDays(-purgeDays);
    Collection collection = CollectionControlModel::getStandard(CalEvent::ARCHIVED);
    if (!collection.isValid())
        return;
    KAEvent::List events = AlarmCalendar::resources()->events(collection);
    for (int i = 0;  i < events.count();  )
    {
        if (purgeDays  &&  events.at(i)->createdDateTime().date() >= cutoff)
            events.remove(i);
        else
            ++i;
    }
    if (!events.isEmpty())
        AlarmCalendar::resources()->purgeEvents(events);   // delete the events and save the calendar
}

/******************************************************************************
* Display an error message about an error when saving an event.
* If 'model' is non-null, the AlarmListModel* which it points to is used; if
* that is null, it is created.
*/
QVector<KAEvent> getSortedActiveEvents(QObject* parent, AlarmListModel** model)
{
    AlarmListModel* mdl = nullptr;
    if (!model)
        model = &mdl;
    if (!*model)
    {
        *model = new AlarmListModel(parent);
        (*model)->setEventTypeFilter(CalEvent::ACTIVE);
        (*model)->sort(AlarmListModel::TimeColumn);
    }
    QVector<KAEvent> result;
    for (int i = 0, count = (*model)->rowCount();  i < count;  ++i)
    {
        const KAEvent event = (*model)->event(i);
        if (event.enabled()  &&  !event.expired())
            result += event;
    }
    return result;
}

/******************************************************************************
* Display an error message corresponding to a specified alarm update error code.
*/
void displayKOrgUpdateError(QWidget* parent, UpdateError code, const UpdateResult& korgError, int nAlarms)
{
    QString errmsg;
    switch (code)
    {
        case ERR_ADD:
        case ERR_REACTIVATE:
            errmsg = (nAlarms > 1) ? i18nc("@info", "Unable to show alarms in KOrganizer")
                                   : i18nc("@info", "Unable to show alarm in KOrganizer");
            break;
        case ERR_MODIFY:
            errmsg = i18nc("@info", "Unable to update alarm in KOrganizer");
            break;
        case ERR_DELETE:
            errmsg = (nAlarms > 1) ? i18nc("@info", "Unable to delete alarms from KOrganizer")
                                   : i18nc("@info", "Unable to delete alarm from KOrganizer");
            break;
        case ERR_TEMPLATE:
            return;
    }
    bool showDetail = !korgError.message.isEmpty();
    QString msg;
    switch (korgError.status)
    {
        case UPDATE_KORG_ERRINIT:
            msg = xi18nc("@info", "<para>%1</para><para>(Could not start KOrganizer)</para>", errmsg);
            break;
        case UPDATE_KORG_ERRSTART:
            msg = xi18nc("@info", "<para>%1</para><para>(KOrganizer not fully started)</para>", errmsg);
            break;
        case UPDATE_KORG_ERR:
            msg = xi18nc("@info", "<para>%1</para><para>(Error communicating with KOrganizer)</para>", errmsg);
            break;
        default:
            msg = errmsg;
            showDetail = false;
            break;
    }
    if (showDetail)
        KAMessageBox::detailedError(parent, msg, korgError.message);
    else
        KAMessageBox::error(parent, msg);
}

/******************************************************************************
* Execute a New Alarm dialog for the specified alarm type.
*/
void editNewAlarm(EditAlarmDlg::Type type, QWidget* parent)
{
    execNewAlarmDlg(EditAlarmDlg::create(false, type, parent));
}

/******************************************************************************
* Execute a New Alarm dialog for the specified alarm type.
*/
void editNewAlarm(KAEvent::SubAction action, QWidget* parent, const AlarmText* text)
{
    bool setAction = false;
    EditAlarmDlg::Type type;
    switch (action)
    {
        case KAEvent::MESSAGE:
        case KAEvent::FILE:
            type = EditAlarmDlg::DISPLAY;
            setAction = true;
            break;
        case KAEvent::COMMAND:
            type = EditAlarmDlg::COMMAND;
            break;
        case KAEvent::EMAIL:
            type = EditAlarmDlg::EMAIL;
            break;
        case KAEvent::AUDIO:
            type = EditAlarmDlg::AUDIO;
            break;
        default:
            return;
    }
    EditAlarmDlg* editDlg = EditAlarmDlg::create(false, type, parent);
    if (setAction  ||  text)
        editDlg->setAction(action, *text);
    execNewAlarmDlg(editDlg);
}

/******************************************************************************
* Execute a New Alarm dialog, optionally either presetting it to the supplied
* event, or setting the action and text.
*/
void editNewAlarm(const KAEvent* preset, QWidget* parent)
{
    execNewAlarmDlg(EditAlarmDlg::create(false, preset, true, parent));
}

/******************************************************************************
* Common code for editNewAlarm() variants.
*/
void execNewAlarmDlg(EditAlarmDlg* editDlg)
{
    // Create a PrivateNewAlarmDlg parented by editDlg.
    // It will be deleted when editDlg is closed.
    new PrivateNewAlarmDlg(editDlg);
    editDlg->show();
    editDlg->raise();
    editDlg->activateWindow();
}

PrivateNewAlarmDlg::PrivateNewAlarmDlg(EditAlarmDlg* dlg)
    : QObject(dlg)
{
    connect(dlg, &QDialog::accepted, this, &PrivateNewAlarmDlg::okClicked);
    connect(dlg, &QDialog::rejected, this, &PrivateNewAlarmDlg::cancelClicked);
}

/******************************************************************************
* Called when the dialogue is accepted (e.g. by clicking the OK button).
* Creates the event specified in the instance's dialogue.
*/
void PrivateNewAlarmDlg::okClicked()
{
    accept(static_cast<EditAlarmDlg*>(parent()));
}

/******************************************************************************
* Creates the event specified in a given dialogue.
*/
void PrivateNewAlarmDlg::accept(EditAlarmDlg* editDlg)
{
    KAEvent event;
    Collection calendar;
    editDlg->getEvent(event, calendar);

    // Add the alarm to the displayed lists and to the calendar file
    const UpdateResult status = addEvent(event, &calendar, editDlg);
    switch (status.status)
    {
        case UPDATE_FAILED:
            return;
        case UPDATE_KORG_ERR:
        case UPDATE_KORG_ERRINIT:
        case UPDATE_KORG_ERRSTART:
        case UPDATE_KORG_FUNCERR:
            displayKOrgUpdateError(editDlg, ERR_ADD, status);
            break;
        default:
            break;
    }
    Undo::saveAdd(event, calendar);

    outputAlarmWarnings(editDlg, &event);

    editDlg->deleteLater();
}

/******************************************************************************
* Called when the dialogue is rejected (e.g. by clicking the Cancel button).
*/
void PrivateNewAlarmDlg::cancelClicked()
{
    static_cast<EditAlarmDlg*>(parent())->deleteLater();
}

/******************************************************************************
* Display the alarm edit dialog to edit a new alarm, preset with a template.
*/
bool editNewAlarm(const QString& templateName, QWidget* parent)
{
    if (!templateName.isEmpty())
    {
        KAEvent* templateEvent = AlarmCalendar::resources()->templateEvent(templateName);
        if (templateEvent->isValid())
        {
            editNewAlarm(templateEvent, parent);
            return true;
        }
        qCWarning(KALARM_LOG) << "KAlarm::editNewAlarm:" << templateName << ": template not found";
    }
    return false;
}

/******************************************************************************
* Create a new template.
*/
void editNewTemplate(EditAlarmDlg::Type type, QWidget* parent)
{
    ::editNewTemplate(type, nullptr, parent);
}

/******************************************************************************
* Create a new template, based on an existing event or template.
*/
void editNewTemplate(const KAEvent* preset, QWidget* parent)
{
    ::editNewTemplate(EditAlarmDlg::Type(0), preset, parent);
}

/******************************************************************************
* Find the identity of the desktop we are running on.
*/
QString currentDesktopIdentityName()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CURRENT_DESKTOP"));
}

/******************************************************************************
* Find the identity of the desktop we are running on.
*/
Desktop currentDesktopIdentity()
{
    const QString desktop = currentDesktopIdentityName();
    if (desktop == QLatin1String("KDE"))    return Desktop::Kde;
    if (desktop == QLatin1String("Unity"))  return Desktop::Unity;
    return Desktop::Other;
}

/******************************************************************************
* Check the config as to whether there is a wake-on-suspend alarm pending, and
* if so, delete it from the config if it has expired.
* If 'checkExists' is true, the config entry will only be returned if the
* event exists.
* Reply = config entry: [0] = event's collection ID (Akonadi only),
*                       [1] = event ID,
*                       [2] = trigger time (int64 seconds since epoch).
*       = empty list if none or expired.
*/
QStringList checkRtcWakeConfig(bool checkEventExists)
{
    KConfigGroup config(KSharedConfig::openConfig(), "General");
    const QStringList params = config.readEntry("RtcWake", QStringList());
#if KALARMCAL_VERSION >= QT_VERSION_CHECK(5,12,1)
    if (params.count() == 3  &&  params[2].toLongLong() > KADateTime::currentUtcDateTime().toSecsSinceEpoch())
#else
    if (params.count() == 3  &&  params[2].toUInt() > KADateTime::currentUtcDateTime().toTime_t())
#endif
    {
        if (checkEventExists  &&  !AlarmCalendar::getEvent(EventId(params[0].toLongLong(), params[1])))
            return QStringList();
        return params;                   // config entry is valid
    }
    if (!params.isEmpty())
    {
        config.deleteEntry("RtcWake");   // delete the expired config entry
        config.sync();
    }
    return QStringList();
}

/******************************************************************************
* Delete any wake-on-suspend alarm from the config.
*/
void deleteRtcWakeConfig()
{
    KConfigGroup config(KSharedConfig::openConfig(), "General");
    config.deleteEntry("RtcWake");
    config.sync();
}

/******************************************************************************
* Delete any wake-on-suspend alarm, optionally only for a specified event.
*/
void cancelRtcWake(QWidget* msgParent, const QString& eventId)
{
    const QStringList wakeup = checkRtcWakeConfig();
    if (!wakeup.isEmpty()  &&  (eventId.isEmpty() || wakeup[0] == eventId))
    {
        Private::instance()->mMsgParent = msgParent ? msgParent : MainWindow::mainMainWindow();
        QTimer::singleShot(0, Private::instance(), &Private::cancelRtcWake);
    }
}

/******************************************************************************
* Delete any wake-on-suspend alarm.
*/
void Private::cancelRtcWake()
{
    // setRtcWakeTime will only work with a parent window specified
    setRtcWakeTime(0, mMsgParent);
    deleteRtcWakeConfig();
    KAMessageBox::information(mMsgParent, i18nc("info", "The scheduled Wake from Suspend has been cancelled."));
}

/******************************************************************************
* Set the wakeup time for the system.
* Set 'triggerTime' to zero to cancel the wakeup.
* Reply = true if successful.
*/
bool setRtcWakeTime(unsigned triggerTime, QWidget* parent)
{
    QVariantMap args;
    args[QStringLiteral("time")] = triggerTime;
    KAuth::Action action(QStringLiteral("org.kde.kalarm.rtcwake.settimer"));
    action.setHelperId(QStringLiteral("org.kde.kalarm.rtcwake"));
    action.setParentWidget(parent);
    action.setArguments(args);
    KAuth::ExecuteJob* job = action.execute();
    if (!job->exec())
    {
        QString errmsg = job->errorString();
        qCDebug(KALARM_LOG) << "KAlarm::setRtcWakeTime: Error code=" << job->error() << errmsg;
        if (errmsg.isEmpty())
        {
            int errcode = job->error();
            switch (errcode)
            {
                case KAuth::ActionReply::AuthorizationDeniedError:
                case KAuth::ActionReply::UserCancelledError:
                    qCDebug(KALARM_LOG) << "KAlarm::setRtcWakeTime: Authorization error:" << errcode;
                    return false;   // the user should already know about this
                default:
                    break;
            }
            errmsg = i18nc("@info", "Error obtaining authorization (%1)", errcode);
        }
        KAMessageBox::information(parent, errmsg);
        return false;
    }
    return true;
}

} // namespace KAlarm
namespace
{

/******************************************************************************
* Create a new template.
* 'preset' is non-null to base it on an existing event or template; otherwise,
* the alarm type is set to 'type'.
*/
void editNewTemplate(EditAlarmDlg::Type type, const KAEvent* preset, QWidget* parent)
{
    if (CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty())
    {
        KAMessageBox::sorry(parent, i18nc("@info", "You must enable a template calendar to save the template in"));
        return;
    }
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg;
    if (preset)
        editDlg = EditAlarmDlg::create(true, preset, true, parent);
    else
        editDlg = EditAlarmDlg::create(true, type, parent);
    if (editDlg->exec() == QDialog::Accepted)
    {
        KAEvent event;
        Akonadi::Collection calendar;
        editDlg->getEvent(event, calendar);

        // Add the template to the displayed lists and to the calendar file
        KAlarm::addTemplate(event, &calendar, editDlg);
        Undo::saveAdd(event, calendar);
    }
}

} // namespace
namespace KAlarm
{

/******************************************************************************
* Open the Edit Alarm dialog to edit the specified alarm.
* If the alarm is read-only or archived, the dialog is opened read-only.
*/
void editAlarm(KAEvent* event, QWidget* parent)
{
    if (event->expired()  ||  AlarmCalendar::resources()->eventReadOnly(event->itemId()))
    {
        viewAlarm(event, parent);
        return;
    }
    const EventId id(*event);
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
    if (editDlg->exec() == QDialog::Accepted)
    {
        if (!AlarmCalendar::resources()->event(id))
        {
            // Event has been deleted while the user was editing the alarm,
            // so treat it as a new alarm.
            PrivateNewAlarmDlg().accept(editDlg);
            return;
        }
        KAEvent newEvent;
        Collection calendar;
        bool changeDeferral = !editDlg->getEvent(newEvent, calendar);

        // Update the event in the displays and in the calendar file
        const Undo::Event undo(*event, calendar);
        if (changeDeferral)
        {
            // The only change has been to an existing deferral
            if (updateEvent(newEvent, editDlg, true) != UPDATE_OK)   // keep the same event ID
                return;   // failed to save event
        }
        else
        {
            const UpdateResult status = modifyEvent(*event, newEvent, editDlg);
            if (status.status != UPDATE_OK  &&  status.status <= UPDATE_KORG_ERR)
                displayKOrgUpdateError(editDlg, ERR_MODIFY, status);
        }
        Undo::saveEdit(undo, newEvent);

        outputAlarmWarnings(editDlg, &newEvent);
    }
}

/******************************************************************************
* Display the alarm edit dialog to edit the alarm with the specified ID.
* An error occurs if the alarm is not found, if there is more than one alarm
* with the same ID, or if it is read-only or expired.
*/
bool editAlarmById(const EventId& id, QWidget* parent)
{
    const QString eventID(id.eventId());
    KAEvent* event = AlarmCalendar::resources()->event(id, true);
    if (!event)
    {
        if (id.collectionId() != -1)    
            qCWarning(KALARM_LOG) << "KAlarm::editAlarmById: Event ID not found, or duplicated:" << eventID;
        else
            qCWarning(KALARM_LOG) << "KAlarm::editAlarmById: Event ID not found:" << eventID;
        return false;
    }
    if (AlarmCalendar::resources()->eventReadOnly(event->itemId()))
    {
        qCCritical(KALARM_LOG) << "KAlarm::editAlarmById:" << eventID << ": read-only";
        return false;
    }
    switch (event->category())
    {
        case CalEvent::ACTIVE:
        case CalEvent::TEMPLATE:
            break;
        default:
            qCCritical(KALARM_LOG) << "KAlarm::editAlarmById:" << eventID << ": event not active or template";
            return false;
    }
    editAlarm(event, parent);
    return true;
}

/******************************************************************************
* Open the Edit Alarm dialog to edit the specified template.
* If the template is read-only, the dialog is opened read-only.
*/
void editTemplate(KAEvent* event, QWidget* parent)
{
    if (AlarmCalendar::resources()->eventReadOnly(event->itemId()))
    {
        // The template is read-only, so make the dialogue read-only.
        // Use AutoQPointer to guard against crash on application exit while
        // the dialogue is still open. It prevents double deletion (both on
        // deletion of parent, and on return from this function).
        AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
        editDlg->exec();
        return;
    }
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
    if (editDlg->exec() == QDialog::Accepted)
    {
        KAEvent newEvent;
        Akonadi::Collection calendar;
        editDlg->getEvent(newEvent, calendar);
        const QString id = event->id();
        newEvent.setEventId(id);
        newEvent.setCollectionId(event->collectionId());
        newEvent.setItemId(event->itemId());

        // Update the event in the displays and in the calendar file
        const Undo::Event undo(*event, calendar);
        updateTemplate(newEvent, editDlg);
        Undo::saveEdit(undo, newEvent);
    }
}

/******************************************************************************
* Open the Edit Alarm dialog to view the specified alarm (read-only).
*/
void viewAlarm(const KAEvent* event, QWidget* parent)
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
    editDlg->exec();
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog invoked by the Edit button
* in an alarm message window.
* Updates the alarm calendar and closes the dialog.
*/
void updateEditedAlarm(EditAlarmDlg* editDlg, KAEvent& event, Collection& calendar)
{
    qCDebug(KALARM_LOG) << "KAlarm::updateEditedAlarm";
    KAEvent newEvent;
    Akonadi::Collection cal;
    editDlg->getEvent(newEvent, cal);

    // Update the displayed lists and the calendar file
    UpdateResult status;
    if (AlarmCalendar::resources()->event(EventId(event)))
    {
        // The old alarm hasn't expired yet, so replace it
        const Undo::Event undo(event, calendar);
        status = modifyEvent(event, newEvent, editDlg);
        Undo::saveEdit(undo, newEvent);
    }
    else
    {
        // The old event has expired, so simply create a new one
        status = addEvent(newEvent, &calendar, editDlg);
        Undo::saveAdd(newEvent, calendar);
    }

    if (status.status != UPDATE_OK  &&  status.status <= UPDATE_KORG_ERR)
        displayKOrgUpdateError(editDlg, ERR_MODIFY, status);
    outputAlarmWarnings(editDlg, &newEvent);

    editDlg->close();
}

/******************************************************************************
* Returns a list of all alarm templates.
* If shell commands are disabled, command alarm templates are omitted.
*/
KAEvent::List templateList()
{
    KAEvent::List templates;
    const bool includeCmdAlarms = ShellProcess::authorised();
    const KAEvent::List events = AlarmCalendar::resources()->events(CalEvent::TEMPLATE);
    for (KAEvent* event : events)
    {
        if (includeCmdAlarms  ||  !(event->actionTypes() & KAEvent::ACT_COMMAND))
            templates.append(event);
    }
    return templates;
}

/******************************************************************************
* To be called after an alarm has been edited.
* Prompt the user to re-enable alarms if they are currently disabled, and if
* it's an email alarm, warn if no 'From' email address is configured.
*/
void outputAlarmWarnings(QWidget* parent, const KAEvent* event)
{
    if (event  &&  event->actionTypes() == KAEvent::ACT_EMAIL
    &&  Preferences::emailAddress().isEmpty())
        KAMessageBox::information(parent, xi18nc("@info Please set the 'From' email address...",
                                                "<para>%1</para><para>Please set it in the Configuration dialog.</para>", KAMail::i18n_NeedFromEmailAddress()));

    if (!theApp()->alarmsEnabled())
    {
        if (KAMessageBox::warningYesNo(parent, xi18nc("@info", "<para>Alarms are currently disabled.</para><para>Do you want to enable alarms now?</para>"),
                                       QString(), KGuiItem(i18nc("@action:button", "Enable")), KGuiItem(i18nc("@action:button", "Keep Disabled")),
                                       QStringLiteral("EditEnableAlarms"))
                        == KMessageBox::Yes)
            theApp()->setAlarmsEnabled(true);
    }
}

/******************************************************************************
* Reload the calendar.
*/
void refreshAlarms()
{
    qCDebug(KALARM_LOG) << "KAlarm::refreshAlarms";
    if (!refreshAlarmsQueued)
    {
        refreshAlarmsQueued = true;
        theApp()->processQueue();
    }
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* If refreshAlarms() has been called, reload the calendars.
*/
void refreshAlarmsIfQueued()
{
    if (refreshAlarmsQueued)
    {
        qCDebug(KALARM_LOG) << "KAlarm::refreshAlarmsIfQueued";
        AlarmCalendar::resources()->reload();

        // Close any message windows for alarms which are now disabled
        const KAEvent::List events = AlarmCalendar::resources()->events(CalEvent::ACTIVE);
        for (KAEvent* event : events)
        {
            if (!event->enabled()  &&  (event->actionTypes() & KAEvent::ACT_DISPLAY))
            {
                MessageWin* win = MessageWin::findEvent(EventId(*event));
                delete win;
            }
        }

        MainWindow::refresh();
        refreshAlarmsQueued = false;
    }
}

/******************************************************************************
* Start KMail if it isn't already running, optionally minimised.
* Reply = reason for failure to run KMail
*       = null string if success.
*/
QString runKMail()
{
    const QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(KMAIL_DBUS_SERVICE);
    if (!reply.isValid()  ||  !reply.value())
    {
        // Program is not already running, so start it
        const QDBusReply<void> startReply = QDBusConnection::sessionBus().interface()->startService(KMAIL_DBUS_SERVICE);
        if (!startReply.isValid())
        {
            const QString errmsg = startReply.error().message();
            qCCritical(KALARM_LOG) << "Couldn't start KMail (" << errmsg << ")";
            return xi18nc("@info", "Unable to start <application>KMail</application><nl/>(<message>%1</message>)", errmsg);
        }
    }
    return QString();
}

/******************************************************************************
* The "Don't show again" option for error messages is personal to the user on a
* particular computer. For example, he may want to inhibit error messages only
* on his laptop. So the status is not stored in the alarm calendar, but in the
* user's local KAlarm data directory.
******************************************************************************/

/******************************************************************************
* Return the Don't-show-again error message tags set for a specified alarm ID.
*/
QStringList dontShowErrors(const EventId& eventId)
{
    if (eventId.isEmpty())
        return QStringList();
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
    return group.readEntry(id, QStringList());
}

/******************************************************************************
* Check whether the specified Don't-show-again error message tag is set for an
* alarm ID.
*/
bool dontShowErrors(const EventId& eventId, const QString& tag)
{
    if (tag.isEmpty())
        return false;
    const QStringList tags = dontShowErrors(eventId);
    return tags.indexOf(tag) >= 0;
}

/******************************************************************************
* Reset the Don't-show-again error message tags for an alarm ID.
* If 'tags' is empty, the config entry is deleted.
*/
void setDontShowErrors(const EventId& eventId, const QStringList& tags)
{
    if (eventId.isEmpty())
        return;
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
    if (tags.isEmpty())
        group.deleteEntry(id);
    else
        group.writeEntry(id, tags);
    group.sync();
}

/******************************************************************************
* Set the specified Don't-show-again error message tag for an alarm ID.
* Existing tags are unaffected.
*/
void setDontShowErrors(const EventId& eventId, const QString& tag)
{
    if (eventId.isEmpty()  ||  tag.isEmpty())
        return;
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
    QStringList tags = group.readEntry(id, QStringList());
    if (tags.indexOf(tag) < 0)
    {
        tags += tag;
        group.writeEntry(id, tags);
        group.sync();
    }
}

/******************************************************************************
* Read the size for the specified window from the config file, for the
* current screen resolution.
* Reply = true if size set in the config file, in which case 'result' is set
*       = false if no size is set, in which case 'result' is unchanged.
*/
bool readConfigWindowSize(const char* window, QSize& result, int* splitterWidth)
{
    KConfigGroup config(KSharedConfig::openConfig(), window);
    const QWidget* desktop = QApplication::desktop();
    const QSize s = QSize(config.readEntry(QStringLiteral("Width %1").arg(desktop->width()), (int)0),
                          config.readEntry(QStringLiteral("Height %1").arg(desktop->height()), (int)0));
    if (s.isEmpty())
        return false;
    result = s;
    if (splitterWidth)
        *splitterWidth = config.readEntry(QStringLiteral("Splitter %1").arg(desktop->width()), -1);
    return true;
}

/******************************************************************************
* Write the size for the specified window to the config file, for the
* current screen resolution.
*/
void writeConfigWindowSize(const char* window, const QSize& size, int splitterWidth)
{
    KConfigGroup config(KSharedConfig::openConfig(), window);
    const QWidget* desktop = QApplication::desktop();
    config.writeEntry(QStringLiteral("Width %1").arg(desktop->width()), size.width());
    config.writeEntry(QStringLiteral("Height %1").arg(desktop->height()), size.height());
    if (splitterWidth >= 0)
        config.writeEntry(QStringLiteral("Splitter %1").arg(desktop->width()), splitterWidth);
    config.sync();
}

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
FileType fileType(const QMimeType& mimetype)
{
    if (mimetype.inherits(QStringLiteral("text/html")))
        return TextFormatted;
    if (mimetype.inherits(QStringLiteral("application/x-executable")))
        return TextApplication;
    if (mimetype.inherits(QStringLiteral("text/plain")))
        return TextPlain;
    if (mimetype.name().startsWith(QLatin1String("image/")))
        return Image;
    return Unknown;
}

/******************************************************************************
* Check that a file exists and is a plain readable file.
* Updates 'filename' and 'url' even if an error occurs, since 'filename' may
* be needed subsequently by showFileErrMessage().
* 'filename' is in user input format and may be a local file path or URL.
*/
FileErr checkFileExists(QString& filename, QUrl& url)
{
    // Convert any relative file path to absolute
    // (using home directory as the default).
    // This also supports absolute paths and absolute urls.
    FileErr err = FileErr_None;
    url = QUrl::fromUserInput(filename, QDir::homePath(), QUrl::AssumeLocalFile);
    if (filename.isEmpty())
    {
        url = QUrl();
        err = FileErr_Blank;    // blank file name
    }
    else if (!url.isValid())
        err = FileErr_Nonexistent;
    else if (url.isLocalFile())
    {
        // It's a local file
        filename = url.toLocalFile();
        QFileInfo info(filename);
        if      (info.isDir())        err = FileErr_Directory;
        else if (!info.exists())      err = FileErr_Nonexistent;
        else if (!info.isReadable())  err = FileErr_Unreadable;
    }
    else
    {
        filename = url.toDisplayString();
        auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, 2);
        KJobWidgets::setWindow(statJob, MainWindow::mainMainWindow());
        if (!statJob->exec())
            err = FileErr_Nonexistent;
        else
        {
            KFileItem fi(statJob->statResult(), url);
            if (fi.isDir())             err = FileErr_Directory;
            else if (!fi.isReadable())  err = FileErr_Unreadable;
        }
    }
    return err;
}

/******************************************************************************
* Display an error message appropriate to 'err'.
* Display a Continue/Cancel error message if 'errmsgParent' non-null.
* Reply = true to continue, false to cancel.
*/
bool showFileErrMessage(const QString& filename, FileErr err, FileErr blankError, QWidget* errmsgParent)
{
    if (err != FileErr_None)
    {
        // If file is a local file, remove "file://" from name
        QString file = filename;
        const QRegExp f(QStringLiteral("^file:/+"));
        if (f.indexIn(file) >= 0)
            file = file.mid(f.matchedLength() - 1);

        QString errmsg;
        switch (err)
        {
            case FileErr_Blank:
                if (blankError == FileErr_BlankDisplay)
                    errmsg = i18nc("@info", "Please select a file to display");
                else if (blankError == FileErr_BlankPlay)
                    errmsg = i18nc("@info", "Please select a file to play");
                else
                    qFatal("showFileErrMessage: Program error");
                KAMessageBox::sorry(errmsgParent, errmsg);
                return false;
            case FileErr_Directory:
                KAMessageBox::sorry(errmsgParent, xi18nc("@info", "<filename>%1</filename> is a folder", file));
                return false;
            case FileErr_Nonexistent:   errmsg = xi18nc("@info", "<filename>%1</filename> not found", file);  break;
            case FileErr_Unreadable:    errmsg = xi18nc("@info", "<filename>%1</filename> is not readable", file);  break;
            case FileErr_NotTextImage:  errmsg = xi18nc("@info", "<filename>%1</filename> appears not to be a text or image file", file);  break;
            default:
                break;
        }
        if (KAMessageBox::warningContinueCancel(errmsgParent, errmsg)
            == KMessageBox::Cancel)
            return false;
    }
    return true;
}

/******************************************************************************
* If a url string is a local file, strip off the 'file:/' prefix.
*/
QString pathOrUrl(const QString& url)
{
    static const QRegExp localfile(QStringLiteral("^file:/+"));
    return (localfile.indexIn(url) >= 0) ? url.mid(localfile.matchedLength() - 1) : url;
}

/******************************************************************************
* Display a modal dialog to choose an existing file, initially highlighting
* any specified file.
* @param file        Updated with the file which was selected, or empty if no file
*                    was selected.
* @param initialFile The file to initially highlight - must be a full path name or URL.
* @param defaultDir The directory to start in if @p initialFile is empty. If empty,
*                   the user's home directory will be used. Updated to the
*                   directory containing the selected file, if a file is chosen.
* @param existing true to return only existing files, false to allow new ones.
* Reply = true if 'file' value can be used.
*       = false if the dialogue was deleted while visible (indicating that
*         the parent widget was probably also deleted).
*/
bool browseFile(QString& file, const QString& caption, QString& defaultDir,
                const QString& initialFile, const QString& filter, bool existing, QWidget* parent)
{
    file.clear();
    const QString initialDir = !initialFile.isEmpty() ? QString(initialFile).remove(QRegExp(QLatin1String("/[^/]*$")))
                             : !defaultDir.isEmpty()  ? defaultDir
                             :                          QDir::homePath();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<QFileDialog> fileDlg = new QFileDialog(parent, caption, initialDir, filter);
    fileDlg->setAcceptMode(existing ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
    fileDlg->setFileMode(existing ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
    if (!initialFile.isEmpty())
        fileDlg->selectFile(initialFile);
    if (fileDlg->exec() != QDialog::Accepted)
        return static_cast<bool>(fileDlg);   // return false if dialog was deleted
    const QList<QUrl> urls = fileDlg->selectedUrls();
    if (urls.isEmpty())
        return true;
    const QUrl& url = urls[0];
    defaultDir = url.isLocalFile() ? KIO::upUrl(url).toLocalFile() : url.adjusted(QUrl::RemoveFilename).path();
    bool localOnly = true;
    file = localOnly ? url.toDisplayString(QUrl::PreferLocalFile) : url.toDisplayString();
    return true;
}

/******************************************************************************
* Return a prompt string to ask the user whether to convert the calendar to the
* current format.
* If 'whole' is true, the whole calendar needs to be converted; else only some
* alarms may need to be converted.
*
* Note: This method is defined here to avoid duplicating the i18n string
*       definition between the Akonadi and KResources code.
*/
QString conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole)
{
    const QString msg = whole
                ? xi18nc("@info", "Calendar <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), "
                       "and will be read-only unless you choose to update it to the current format.",
                       calendarName, calendarVersion)
                : xi18nc("@info", "Some or all of the alarms in calendar <resource>%1</resource> are in an old <application>KAlarm</application> format, "
                       "and will be read-only unless you choose to update them to the current format.",
                       calendarName);
    return xi18nc("@info", "<para>%1</para><para>"
                 "<warning>Do not update the calendar if it is also used with an older version of <application>KAlarm</application> "
                 "(e.g. on another computer). If you do so, the calendar may become unusable there.</warning></para>"
                 "<para>Do you wish to update the calendar?</para>", msg);
}

#ifndef NDEBUG
/******************************************************************************
* Set up KAlarm test conditions based on environment variables.
* KALARM_TIME: specifies current system time (format [[[yyyy-]mm-]dd-]hh:mm [TZ]).
*/
void setTestModeConditions()
{
    const QByteArray newTime = qgetenv("KALARM_TIME");
    if (!newTime.isEmpty())
    {
        KADateTime dt;
        if (AlarmTime::convertTimeString(newTime, dt, KADateTime::realCurrentLocalDateTime(), true))
            setSimulatedSystemTime(dt);
    }
}

/******************************************************************************
* Set the simulated system time.
*/
void setSimulatedSystemTime(const KADateTime& dt)
{
    KADateTime::setSimulatedSystemTime(dt);
    qCDebug(KALARM_LOG) << "New time =" << qPrintable(KADateTime::currentLocalDateTime().toString(QStringLiteral("%Y-%m-%d %H:%M %:Z")));
}
#endif

} // namespace KAlarm
namespace
{

/******************************************************************************
* Display an error message about an error when saving an event.
*/
void displayUpdateError(QWidget* parent, KAlarm::UpdateError code, const UpdateStatusData& status, bool showKOrgError)
{
    QString errmsg;
    if (status.status.status > KAlarm::UPDATE_KORG_ERR)
    {
        switch (code)
        {
            case KAlarm::ERR_ADD:
            case KAlarm::ERR_MODIFY:
                errmsg = (status.warnErr > 1) ? i18nc("@info", "Error saving alarms")
                                              : i18nc("@info", "Error saving alarm");
                break;
            case KAlarm::ERR_DELETE:
                errmsg = (status.warnErr > 1) ? i18nc("@info", "Error deleting alarms")
                                              : i18nc("@info", "Error deleting alarm");
                break;
            case KAlarm::ERR_REACTIVATE:
                errmsg = (status.warnErr > 1) ? i18nc("@info", "Error saving reactivated alarms")
                                              : i18nc("@info", "Error saving reactivated alarm");
                break;
            case KAlarm::ERR_TEMPLATE:
                errmsg = (status.warnErr > 1) ? i18nc("@info", "Error saving alarm templates")
                                              : i18nc("@info", "Error saving alarm template");
                break;
        }
        KAMessageBox::error(parent, errmsg);
    }
    else if (showKOrgError)
        displayKOrgUpdateError(parent, code, status.status, status.warnKOrg);
}

/******************************************************************************
* Tell KOrganizer to put an alarm in its calendar.
* It will be held by KOrganizer as a simple event, without alarms - KAlarm
* is still responsible for alarming.
*/
KAlarm::UpdateResult sendToKOrganizer(const KAEvent& event)
{
    Event::Ptr kcalEvent(new KCalendarCore::Event);
    event.updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
    // Change the event ID to avoid duplicating the same unique ID as the original event
    const QString uid = uidKOrganizer(event.id());
    kcalEvent->setUid(uid);
    kcalEvent->clearAlarms();
    QString userEmail;
    switch (event.actionTypes())
    {
        case KAEvent::ACT_DISPLAY:
        case KAEvent::ACT_COMMAND:
        case KAEvent::ACT_DISPLAY_COMMAND:
            kcalEvent->setSummary(event.cleanText());
            userEmail = Preferences::emailAddress();
            break;
        case KAEvent::ACT_EMAIL:
        {
            const QString from = event.emailFromId()
                               ? Identities::identityManager()->identityForUoid(event.emailFromId()).fullEmailAddr()
                               : Preferences::emailAddress();
            AlarmText atext;
            atext.setEmail(event.emailAddresses(QStringLiteral(", ")), from, QString(), QString(), event.emailSubject(), QString());
            kcalEvent->setSummary(atext.displayText());
            userEmail = from;
            break;
        }
        case KAEvent::ACT_AUDIO:
            kcalEvent->setSummary(event.audioFile());
            break;
        default:
            break;
    }
    const Person person(QString(), userEmail);
    kcalEvent->setOrganizer(person);
    kcalEvent->setDuration(Duration(Preferences::kOrgEventDuration() * 60, Duration::Seconds));

    // Translate the event into string format
    ICalFormat format;
    format.setTimeZone(Preferences::timeSpecAsZone());
    const QString iCal = format.toICalString(kcalEvent);

    // Send the event to KOrganizer
    KAlarm::UpdateResult status = runKOrganizer();   // start KOrganizer if it isn't already running, and create its D-Bus interface
    if (status != KAlarm::UPDATE_OK)
        return status;
    QDBusInterface korgInterface(KORG_DBUS_SERVICE, QStringLiteral(KORG_DBUS_PATH), KORG_DBUS_IFACE);
    const QList<QVariant> args{iCal};
    QDBusReply<bool> reply = korgInterface.callWithArgumentList(QDBus::Block, QStringLiteral("addIncidence"), args);
    if (!reply.isValid())
    {
        if (reply.error().type() == QDBusError::UnknownObject)
        {
            status =  KAlarm::UPDATE_KORG_ERRSTART;
            qCCritical(KALARM_LOG) << "KAlarm::sendToKOrganizer: addIncidence() D-Bus error: still starting";
        }
        else
        {
            status.set(KAlarm::UPDATE_KORG_ERR, reply.error().message());
            qCCritical(KALARM_LOG) << "KAlarm::sendToKOrganizer: addIncidence(" << uid << ") D-Bus call failed:" << status.message;
        }
    }
    else if (!reply.value())
    {
        status = KAlarm::UPDATE_KORG_FUNCERR;
        qCDebug(KALARM_LOG) << "KAlarm::sendToKOrganizer: addIncidence(" << uid << ") D-Bus call returned false";
    }
    else
        qCDebug(KALARM_LOG) << "KAlarm::sendToKOrganizer:" << uid << ": success";
    return status;
}

/******************************************************************************
* Tell KOrganizer to delete an event from its calendar.
*/
KAlarm::UpdateResult deleteFromKOrganizer(const QString& eventID)
{
    const QString newID = uidKOrganizer(eventID);
    new CollectionSearch(KORG_MIME_TYPE, QString(), newID, true);  // this auto-deletes when complete
    // Ignore errors
    return KAlarm::UpdateResult(KAlarm::UPDATE_OK);
}

/******************************************************************************
* Start KOrganizer if not already running, and create its D-Bus interface.
*/
KAlarm::UpdateResult runKOrganizer()
{
    KAlarm::UpdateResult status;

    // If Kontact is running, there is a load() method which needs to be called to
    // load KOrganizer into Kontact. But if KOrganizer is running independently,
    // the load() method doesn't exist. This call starts korganizer if needed, too.
    QDBusInterface iface(KORG_DBUS_SERVICE, QStringLiteral(KORG_DBUS_LOAD_PATH), QStringLiteral("org.kde.PIMUniqueApplication"));
    QDBusReply<bool> reply = iface.call(QStringLiteral("load"));
    if ((!reply.isValid() || !reply.value())
    &&  iface.lastError().type() != QDBusError::UnknownMethod)
    {
        status.set(KAlarm::UPDATE_KORG_ERR, iface.lastError().message());
        qCWarning(KALARM_LOG) << "Loading KOrganizer failed:" << status.message;
        return status;
    }

    return status;
}

/******************************************************************************
* Insert a KOrganizer string after the hyphen in the supplied event ID.
*/
QString uidKOrganizer(const QString& id)
{
    if (id.startsWith(KORGANIZER_UID))
        return id;
    QString result = id;
    return result.insert(0, KORGANIZER_UID);
}

} // namespace

/******************************************************************************
* Case insensitive comparison for use by qSort().
*/
bool caseInsensitiveLessThan(const QString& s1, const QString& s2)
{
    return s1.toLower() < s2.toLower();
}

// vim: et sw=4:
