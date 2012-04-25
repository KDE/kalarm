/*
 *  functions.cpp  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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

#ifdef USE_AKONADI
#include "collectionmodel.h"
#else
#include "alarmresources.h"
#include "eventlistmodel.h"
#endif
#include "alarmcalendar.h"
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

#include <kalarmcal/identities.h>
#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <kcalcore/event.h>
#include <kcalcore/icalformat.h>
#include <kcalcore/person.h>
#include <kcalcore/duration.h>
using namespace KCalCore;
#else
#include <kcal/event.h>
#include <kcal/icalformat.h>
#include <kcal/person.h>
#include <kcal/duration.h>
using namespace KCal;
#endif
#include <kpimidentities/identitymanager.h>
#include <kpimidentities/identity.h>
#include <kholidays/holidays.h>

#include <kconfiggroup.h>
#include <kaction.h>
#include <ktoggleaction.h>
#include <kactioncollection.h>
#include <kdbusservicestarter.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kauth.h>
#include <ksystemtimezone.h>
#include <kstandardguiitem.h>
#include <kstandardshortcut.h>
#include <kfiledialog.h>
#include <kicon.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <kdebug.h>
#include <ktoolinvocation.h>

#ifdef Q_WS_X11
#include <kwindowsystem.h>
#include <kxmessages.h>
#include <kstartupinfo.h>
#include <netwm.h>
#include <QX11Info>
#endif

#include <QDir>
#include <QRegExp>
#include <QDesktopWidget>
#include <QtDBus/QtDBus>
#include <QTimer>
#include <qglobal.h>

#ifdef USE_AKONADI
using namespace Akonadi;
#endif


namespace
{
bool          refreshAlarmsQueued = false;
QString       korganizerName    = "korganizer";
QString       korgStartError;
QDBusInterface* korgInterface = 0;

const char*   KMAIL_DBUS_SERVICE      = "org.kde.kmail";
//const char*   KMAIL_DBUS_IFACE        = "org.kde.kmail.kmail";
//const char*   KMAIL_DBUS_WINDOW_PATH  = "/kmail/kmail_mainwindow_1";
const char*   KORG_DBUS_SERVICE       = "org.kde.korganizer";
const char*   KORG_DBUS_IFACE         = "org.kde.korganizer.Korganizer";
// D-Bus object path of KOrganizer's notification interface
#define       KORG_DBUS_PATH            "/Korganizer"
#define       KORG_DBUS_LOAD_PATH       "/korganizer_PimApplication"
//const char*   KORG_DBUS_WINDOW_PATH   = "/korganizer/MainWindow_1";
const QString KORGANIZER_UID         = QString::fromLatin1("-korg");

const char*   ALARM_OPTS_FILE        = "alarmopts";
const char*   DONT_SHOW_ERRORS_GROUP = "DontShowErrors";

void editNewTemplate(EditAlarmDlg::Type, const KAEvent* preset, QWidget* parent);
KAlarm::UpdateStatus sendToKOrganizer(const KAEvent*);
KAlarm::UpdateStatus deleteFromKOrganizer(const QString& eventID);
KAlarm::UpdateStatus runKOrganizer();
QString uidKOrganizer(const QString& eventID);
}


namespace KAlarm
{

Private* Private::mInstance = 0;

/******************************************************************************
* Display a main window with the specified event selected.
*/
#ifdef USE_AKONADI
MainWindow* displayMainWindowSelected(Akonadi::Item::Id eventId)
#else
MainWindow* displayMainWindowSelected(const QString& eventId)
#endif
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
#ifdef USE_AKONADI
    if (win  &&  eventId >= 0)
        win->selectEvent(eventId);
#else
    if (win  &&  !eventId.isEmpty())
        win->selectEvent(eventId);
#endif
    return win;
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
KToggleAction* createAlarmEnableAction(QObject* parent)
{
    KToggleAction* action = new KToggleAction(i18nc("@action", "Enable &Alarms"), parent);
    action->setChecked(theApp()->alarmsEnabled());
    QObject::connect(action, SIGNAL(toggled(bool)), theApp(), SLOT(setAlarmsEnabled(bool)));
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), action, SLOT(setChecked(bool)));
    return action;
}

/******************************************************************************
* Create a "Stop Play" action.
*/
KAction* createStopPlayAction(QObject* parent)
{
    KAction* action = new KAction(KIcon("media-playback-stop"), i18nc("@action", "Stop Play"), parent);
    action->setEnabled(MessageWin::isAudioPlaying());
    QObject::connect(action, SIGNAL(triggered(bool)), theApp(), SLOT(stopAudio()));
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), SIGNAL(audioPlaying(bool)), action, SLOT(setEnabled(bool)));
    return action;
}

/******************************************************************************
* Create a "Spread Windows" action.
*/
KToggleAction* createSpreadWindowsAction(QObject* parent)
{
    KToggleAction* action = new KToggleAction(i18nc("@action", "Spread Windows"), parent);
    QObject::connect(action, SIGNAL(triggered(bool)), theApp(), SLOT(spreadWindows(bool)));
    // The following line ensures that all instances are kept in the same state
    QObject::connect(theApp(), SIGNAL(spreadWindowsToggled(bool)), action, SLOT(setChecked(bool)));
    return action;
}

/******************************************************************************
* Add a new active (non-archived) alarm.
* Save it in the calendar file and add it to every main window instance.
* Parameters: msgParent = parent widget for any calendar selection prompt or
*                         error message.
*             event - is updated with the actual event ID.
*/
#ifdef USE_AKONADI
UpdateStatus addEvent(KAEvent& event, Collection* calendar, QWidget* msgParent, int options, bool showKOrgErr)
#else
UpdateStatus addEvent(KAEvent& event, AlarmResource* calendar, QWidget* msgParent, int options, bool showKOrgErr)
#endif
{
    kDebug() << event.id();
    bool cancelled = false;
    UpdateStatus status = UPDATE_OK;
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status = UPDATE_FAILED;
    else
    {
        // Save the event details in the calendar file, and get the new event ID
        AlarmCalendar* cal = AlarmCalendar::resources();
        KAEvent* newev = new KAEvent(event);
#ifdef USE_AKONADI
        if (!cal->addEvent(*newev, msgParent, (options & USE_EVENT_ID), calendar, (options & NO_RESOURCE_PROMPT), &cancelled))
#else
        if (!cal->addEvent(newev, msgParent, (options & USE_EVENT_ID), calendar, (options & NO_RESOURCE_PROMPT), &cancelled))
#endif
        {
            delete newev;
            status = UPDATE_FAILED;
        }
        else
        {
            event = *newev;   // update event ID etc.
            if (!cal->save())
                status = SAVE_FAILED;
        }
        if (status == UPDATE_OK)
        {
            if ((options & ALLOW_KORG_UPDATE)  &&  event.copyToKOrganizer())
            {
                UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
                if (st > status)
                    status = st;
            }

#ifndef USE_AKONADI
            // Update the window lists
            EventListModel::alarms()->addEvent(newev);
#endif
        }
    }

    if (status != UPDATE_OK  &&  !cancelled  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_ADD, 1, 1, showKOrgErr);
    return status;
}

/******************************************************************************
* Add a list of new active (non-archived) alarms.
* Save them in the calendar file and add them to every main window instance.
* The events are updated with their actual event IDs.
*/
UpdateStatus addEvents(QVector<KAEvent>& events, QWidget* msgParent, bool allowKOrgUpdate, bool showKOrgErr)
{
    kDebug() << events.count();
    if (events.isEmpty())
        return UPDATE_OK;
    int warnErr = 0;
    int warnKOrg = 0;
    UpdateStatus status = UPDATE_OK;
#ifdef USE_AKONADI
    Collection collection;
#else
    AlarmResource* resource;
#endif
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status = UPDATE_FAILED;
    else
    {
#ifdef USE_AKONADI
        collection = CollectionControlModel::instance()->destination(CalEvent::ACTIVE, msgParent);
        if (!collection.isValid())
#else
        resource = AlarmResources::instance()->destination(CalEvent::ACTIVE, msgParent);
        if (!resource)
#endif
        {
            kDebug() << "No calendar";
            status = UPDATE_FAILED;
        }
    }
    if (status == UPDATE_OK)
    {
        QString selectID;
        AlarmCalendar* cal = AlarmCalendar::resources();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            // Save the event details in the calendar file, and get the new event ID
#ifdef USE_AKONADI
            KAEvent* const newev = &events[i];
            if (!cal->addEvent(events[i], msgParent, false, &collection))
#else
            KAEvent* newev = new KAEvent(events[i]);
            if (!cal->addEvent(newev, msgParent, false, resource))
#endif
            {
#ifndef USE_AKONADI
                delete newev;
#endif
                status = UPDATE_ERROR;
                ++warnErr;
                continue;
            }
#ifndef USE_AKONADI
            events[i] = *newev;   // update event ID etc.
#endif
            if (allowKOrgUpdate  &&  newev->copyToKOrganizer())
            {
                UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
                if (st != UPDATE_OK)
                {
                    ++warnKOrg;
                    if (st > status)
                        status = st;
                }
            }

#ifndef USE_AKONADI
            // Update the window lists, but not yet which item is selected
            EventListModel::alarms()->addEvent(newev);
//            selectID = newev->id();
#endif
        }
        if (warnErr == events.count())
            status = UPDATE_FAILED;
        else if (!cal->save())
        {
            status = SAVE_FAILED;
            warnErr = 0;    // everything failed
        }
    }

    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_ADD, (warnErr ? warnErr : events.count()), warnKOrg, showKOrgErr);
    return status;
}

/******************************************************************************
* Save the event in the archived calendar and adjust every main window instance.
* The event's ID is changed to an archived ID if necessary.
*/
#ifdef USE_AKONADI
bool addArchivedEvent(KAEvent& event, Collection* collection)
#else
bool addArchivedEvent(KAEvent& event, AlarmResource* resource)
#endif
{
    kDebug() << event.id();
    QString oldid = event.id();
    bool archiving = (event.category() == CalEvent::ACTIVE);
    if (archiving  &&  !Preferences::archivedKeepDays())
        return false;   // expired alarms aren't being kept
    AlarmCalendar* cal = AlarmCalendar::resources();
#ifdef USE_AKONADI
    KAEvent newevent(event);
    newevent.setItemId(-1);    // invalidate the Akonadi item ID since it's a new item
    KAEvent* const newev = &newevent;
#else
    KAEvent* newev = new KAEvent(event);
#endif
    if (archiving)
    {
        newev->setCategory(CalEvent::ARCHIVED);    // this changes the event ID
        newev->setCreatedDateTime(KDateTime::currentUtcDateTime());   // time stamp to control purging
    }
    // Note that archived resources are automatically saved after changes are made
#ifdef USE_AKONADI
    if (!cal->addEvent(newevent, 0, false, collection))
        return false;
#else
    if (!cal->addEvent(newev, 0, false, resource))
    {
        delete newev;     // failed to add to calendar - leave event in its original state
        return false;
    }
#endif
    event = *newev;   // update event ID etc.

#ifndef USE_AKONADI
    // Update window lists.
    // Note: updateEvent() is not used here since that doesn't trigger refiltering
    // of the alarm list, resulting in the archived event still remaining visible
    // even if archived events are supposed to be hidden.
    if (archiving)
        EventListModel::alarms()->removeEvent(oldid);
    EventListModel::alarms()->addEvent(newev);
#endif
    return true;
}

/******************************************************************************
* Add a new template.
* Save it in the calendar file and add it to every template list view.
* 'event' is updated with the actual event ID.
* Parameters: promptParent = parent widget for any calendar selection prompt.
*/
#ifdef USE_AKONADI
UpdateStatus addTemplate(KAEvent& event, Collection* collection, QWidget* msgParent)
#else
UpdateStatus addTemplate(KAEvent& event, AlarmResource* resource, QWidget* msgParent)
#endif
{
    kDebug() << event.id();
    UpdateStatus status = UPDATE_OK;

    // Add the template to the calendar file
    AlarmCalendar* cal = AlarmCalendar::resources();
#ifdef USE_AKONADI
    KAEvent newev(event);
    if (!cal->addEvent(newev, msgParent, false, collection))
        status = UPDATE_FAILED;
#else
    KAEvent* newev = new KAEvent(event);
    if (!cal->addEvent(newev, msgParent, false, resource))
    {
        delete newev;
        status = UPDATE_FAILED;
    }
#endif
    else
    {
#ifdef USE_AKONADI
        event = newev;   // update event ID etc.
#else
        event = *newev;   // update event ID etc.
#endif
        if (!cal->save())
            status = SAVE_FAILED;
        else
        {
#ifndef USE_AKONADI
            // Update the window lists
            EventListModel::templates()->addEvent(newev);
#endif
            return UPDATE_OK;
        }
    }

    if (msgParent)
        displayUpdateError(msgParent, status, ERR_TEMPLATE, 1);
    return status;
}

/******************************************************************************
* Modify an active (non-archived) alarm in the calendar file and in every main
* window instance.
* The new event must have a different event ID from the old one.
*/
UpdateStatus modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent, bool showKOrgErr)
{
    kDebug() << oldEvent.id();

    UpdateStatus status = UPDATE_OK;
    if (!newEvent.isValid())
    {
        deleteEvent(oldEvent, true);
        status = UPDATE_FAILED;
    }
    else
    {
#ifdef USE_AKONADI
        EventId oldId(oldEvent);
#else
        QString oldId = oldEvent.id();
#endif
        if (oldEvent.copyToKOrganizer())
        {
            // Tell KOrganizer to delete its old event.
            // But ignore errors, because the user could have manually
            // deleted it since KAlarm asked KOrganizer to set it up.
#ifdef USE_AKONADI
            deleteFromKOrganizer(oldId.eventId());
#else
            deleteFromKOrganizer(oldId);
#endif
        }
#ifdef USE_AKONADI
        // Update the event in the calendar file, and get the new event ID
        AlarmCalendar* cal = AlarmCalendar::resources();
        if (!cal->modifyEvent(oldId, newEvent))
            status = UPDATE_FAILED;
#else
        // Delete from the window lists to prevent the event's invalid
        // pointer being accessed.
        EventListModel::alarms()->removeEvent(oldId);

        // Update the event in the calendar file, and get the new event ID
        KAEvent* newev = new KAEvent(newEvent);
        AlarmCalendar* cal = AlarmCalendar::resources();
        if (!cal->modifyEvent(oldId, newev))
        {
            delete newev;
            status = UPDATE_FAILED;
        }
#endif
        else
        {
#ifndef USE_AKONADI
            newEvent = *newev;
#endif
            if (!cal->save())
                status = SAVE_FAILED;
            if (status == UPDATE_OK)
            {
                if (newEvent.copyToKOrganizer())
                {
                    UpdateStatus st = sendToKOrganizer(&newEvent);    // tell KOrganizer to show the new event
                    if (st > status)
                        status = st;
                }

                // Remove "Don't show error messages again" for the old alarm
                setDontShowErrors(oldId);

#ifndef USE_AKONADI
                // Update the window lists
                EventListModel::alarms()->addEvent(newev);
#endif
            }
        }
    }

    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_MODIFY, 1, 1, showKOrgErr);
    return status;
}

/******************************************************************************
* Update an active (non-archived) alarm from the calendar file and from every
* main window instance.
* The new event will have the same event ID as the old one.
* The event is not updated in KOrganizer, since this function is called when an
* existing alarm is rescheduled (due to recurrence or deferral).
*/
UpdateStatus updateEvent(KAEvent& event, QWidget* msgParent, bool archiveOnDelete)
{
    kDebug() << event.id();

    if (!event.isValid())
        deleteEvent(event, archiveOnDelete);
    else
    {
        // Update the event in the calendar file.
        AlarmCalendar* cal = AlarmCalendar::resources();
#ifdef USE_AKONADI
        cal->updateEvent(event);
#else
        KAEvent* newEvent = cal->updateEvent(event);
#endif
        if (!cal->save())
        {
            if (msgParent)
                displayUpdateError(msgParent, SAVE_FAILED, ERR_ADD, 1);
            return SAVE_FAILED;
        }

#ifndef USE_AKONADI
        // Update the window lists
        EventListModel::alarms()->updateEvent(newEvent);
#endif
    }
    return UPDATE_OK;
}

/******************************************************************************
* Update a template in the calendar file and in every template list view.
* If 'selectionView' is non-null, the selection highlight is moved to the
* updated event in that listView instance.
*/
UpdateStatus updateTemplate(KAEvent& event, QWidget* msgParent)
{
    AlarmCalendar* cal = AlarmCalendar::resources();
    KAEvent* newEvent = cal->updateEvent(event);
    UpdateStatus status = UPDATE_OK;
    if (!newEvent)
        status = UPDATE_FAILED;
    else if (!cal->save())
        status = SAVE_FAILED;
    if (status != UPDATE_OK)
    {
        if (msgParent)
            displayUpdateError(msgParent, SAVE_FAILED, ERR_TEMPLATE, 1);
        return status;
    }

#ifndef USE_AKONADI
    EventListModel::templates()->updateEvent(newEvent);
#endif
    return UPDATE_OK;
}

/******************************************************************************
* Delete alarms from the calendar file and from every main window instance.
* If the events are archived, the events' IDs are changed to archived IDs if necessary.
*/
UpdateStatus deleteEvent(KAEvent& event, bool archive, QWidget* msgParent, bool showKOrgErr)
{
#ifdef USE_AKONADI
    QVector<KAEvent> events(1, event);
#else
    KAEvent::List events;
    events += &event;
#endif
    return deleteEvents(events, archive, msgParent, showKOrgErr);
}

#ifdef USE_AKONADI
UpdateStatus deleteEvents(QVector<KAEvent>& events, bool archive, QWidget* msgParent, bool showKOrgErr)
#else
UpdateStatus deleteEvents(KAEvent::List& events, bool archive, QWidget* msgParent, bool showKOrgErr)
#endif
{
    kDebug() << events.count();
    if (events.isEmpty())
        return UPDATE_OK;
    int warnErr = 0;
    int warnKOrg = 0;
    UpdateStatus status = UPDATE_OK;
    AlarmCalendar* cal = AlarmCalendar::resources();
    bool deleteWakeFromSuspendAlarm = false;
    QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        // Save the event details in the calendar file, and get the new event ID
#ifdef USE_AKONADI
        KAEvent* event = &events[i];
#else
        KAEvent* event = events[i];
#endif
        QString id = event->id();

#ifndef USE_AKONADI
        // Update the window lists and clear stored command errors
        EventListModel::alarms()->removeEvent(id);
        event->setCommandError(KAEvent::CMD_NO_ERROR);
#endif

        // Delete the event from the calendar file
        if (event->category() != CalEvent::ARCHIVED)
        {
            if (event->copyToKOrganizer())
            {
                // The event was shown in KOrganizer, so tell KOrganizer to
                // delete it. But ignore errors, because the user could have
                // manually deleted it from KOrganizer since it was set up.
                UpdateStatus st = deleteFromKOrganizer(id);
                if (st != UPDATE_OK)
                {
                    ++warnKOrg;
                    if (st > status)
                        status = st;
                }
            }
            if (archive  &&  event->toBeArchived())
            {
                KAEvent ev(*event);
                addArchivedEvent(ev);     // this changes the event ID to an archived ID
            }
        }
#ifdef USE_AKONADI
        if (!cal->deleteEvent(*event, false))   // don't save calendar after deleting
#else
        if (!cal->deleteEvent(id, false))   // don't save calendar after deleting
#endif
        {
            status = UPDATE_ERROR;
            ++warnErr;
        }

        if (id == wakeFromSuspendId)
            deleteWakeFromSuspendAlarm = true;

        // Remove "Don't show error messages again" for this alarm
#ifdef USE_AKONADI
        setDontShowErrors(EventId(*event));
#else
        setDontShowErrors(id);
#endif
    }

    if (warnErr == events.count())
        status = UPDATE_FAILED;
    else if (!cal->save())      // save the calendars now
    {
        status = SAVE_FAILED;
        warnErr = events.count();
    }
    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_DELETE, warnErr, warnKOrg, showKOrgErr);

    // Remove any wake-from-suspend scheduled for a deleted alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);

    return status;
}

/******************************************************************************
* Delete templates from the calendar file and from every template list view.
*/
#ifdef USE_AKONADI
UpdateStatus deleteTemplates(const KAEvent::List& events, QWidget* msgParent)
#else
UpdateStatus deleteTemplates(const QStringList& eventIDs, QWidget* msgParent)
#endif
{
#ifdef USE_AKONADI
    int count = events.count();
#else
    int count = eventIDs.count();
#endif
    kDebug() << count;
    if (!count)
        return UPDATE_OK;
    int warnErr = 0;
    UpdateStatus status = UPDATE_OK;
    AlarmCalendar* cal = AlarmCalendar::resources();
    for (int i = 0, end = count;  i < end;  ++i)
    {
        // Update the window lists
#ifndef USE_AKONADI
        QString id = eventIDs[i];
        EventListModel::templates()->removeEvent(id);
#endif

        // Delete the template from the calendar file
        AlarmCalendar* cal = AlarmCalendar::resources();
#ifdef USE_AKONADI
        if (!cal->deleteEvent(*events[i], false))   // don't save calendar after deleting
#else
        if (!cal->deleteEvent(id, false))    // don't save calendar after deleting
#endif
        {
            status = UPDATE_ERROR;
            ++warnErr;
        }
    }

    if (warnErr == count)
        status = UPDATE_FAILED;
    else if (!cal->save())      // save the calendars now
    {
        status = SAVE_FAILED;
        warnErr = count;
    }
    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_TEMPLATE, warnErr);
    return status;
}

/******************************************************************************
* Delete an alarm from the display calendar.
*/
void deleteDisplayEvent(const QString& eventID)
{
    kDebug() << eventID;
    AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
    if (cal)
#ifdef USE_AKONADI
        cal->deleteDisplayEvent(eventID, true);   // save calendar after deleting
#else
        cal->deleteEvent(eventID, true);   // save calendar after deleting
#endif
}

/******************************************************************************
* Undelete archived alarms, and update every main window instance.
* The archive bit is set to ensure that they get re-archived if deleted again.
* 'ineligibleIDs' is filled in with the IDs of any ineligible events.
*/
#ifdef USE_AKONADI
UpdateStatus reactivateEvent(KAEvent& event, Collection* calendar, QWidget* msgParent, bool showKOrgErr)
#else
UpdateStatus reactivateEvent(KAEvent& event, AlarmResource* calendar, QWidget* msgParent, bool showKOrgErr)
#endif
{
#ifdef USE_AKONADI
    QVector<EventId> ids;
    QVector<KAEvent> events(1, event);
#else
    QStringList ids;
    KAEvent::List events;
    events += &event;
#endif
    return reactivateEvents(events, ids, calendar, msgParent, showKOrgErr);
}

#ifdef USE_AKONADI
UpdateStatus reactivateEvents(QVector<KAEvent>& events, QVector<EventId>& ineligibleIDs, Collection* col, QWidget* msgParent, bool showKOrgErr)
#else
UpdateStatus reactivateEvents(KAEvent::List& events, QStringList& ineligibleIDs, AlarmResource* resource, QWidget* msgParent, bool showKOrgErr)
#endif
{
    kDebug() << events.count();
    ineligibleIDs.clear();
    if (events.isEmpty())
        return UPDATE_OK;
    int warnErr = 0;
    int warnKOrg = 0;
    UpdateStatus status = UPDATE_OK;
#ifdef USE_AKONADI
    Collection collection;
    if (col)
        collection = *col;
    if (!collection.isValid())
        collection = CollectionControlModel::instance()->destination(CalEvent::ACTIVE, msgParent);
    if (!collection.isValid())
#else
    if (!resource)
        resource = AlarmResources::instance()->destination(CalEvent::ACTIVE, msgParent);
    if (!resource)
#endif
    {
        kDebug() << "No calendar";
        status = UPDATE_FAILED;
        warnErr = events.count();
    }
    else
    {
        QString selectID;
        int count = 0;
        AlarmCalendar* cal = AlarmCalendar::resources();
        KDateTime now = KDateTime::currentUtcDateTime();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            // Delete the event from the archived resource
#ifdef USE_AKONADI
            KAEvent* event = &events[i];
#else
            KAEvent* event = events[i];
#endif
            if (event->category() != CalEvent::ARCHIVED
            ||  !event->occursAfter(now, true))
            {
#ifdef USE_AKONADI
                ineligibleIDs += EventId(*event);
#else
                ineligibleIDs += event->id();
#endif
                continue;
            }
            ++count;

#ifdef USE_AKONADI
            KAEvent newevent(*event);
            KAEvent* const newev = &newevent;
#else
            KAEvent* newev = new KAEvent(*event);
            QString oldid = event->id();
#endif
            newev->setCategory(CalEvent::ACTIVE);    // this changes the event ID
            if (newev->recurs()  ||  newev->repetition())
                newev->setNextOccurrence(now);   // skip any recurrences in the past
            newev->setArchive();    // ensure that it gets re-archived if it is deleted

            // Save the event details in the calendar file.
            // This converts the event ID.
#ifdef USE_AKONADI
            if (!cal->addEvent(newevent, msgParent, true, &collection))
#else
            if (!cal->addEvent(newev, msgParent, true, resource))
#endif
            {
#ifndef USE_AKONADI
                delete newev;
#endif
                status = UPDATE_ERROR;
                ++warnErr;
                continue;
            }
            if (newev->copyToKOrganizer())
            {
                UpdateStatus st = sendToKOrganizer(newev);    // tell KOrganizer to show the event
                if (st != UPDATE_OK)
                {
                    ++warnKOrg;
                    if (st > status)
                        status = st;
                }
            }

#ifndef USE_AKONADI
            // Update the window lists
            EventListModel::alarms()->updateEvent(oldid, newev);
//            selectID = newev->id();
#endif

#ifdef USE_AKONADI
            if (cal->event(EventId(*event))  // no error if event doesn't exist in archived resource
            &&  !cal->deleteEvent(*event, false))   // don't save calendar after deleting
#else
            if (cal->event(oldid)    // no error if event doesn't exist in archived resource
            &&  !cal->deleteEvent(oldid, false))   // don't save calendar after deleting
#endif
            {
                status = UPDATE_ERROR;
                ++warnErr;
            }
#ifdef USE_AKONADI
            events[i] = newevent;
#else
            events[i] = newev;
#endif
        }

        if (warnErr == count)
            status = UPDATE_FAILED;
        // Save the calendars, even if all events failed, since more than one calendar was updated
        if (!cal->save()  &&  status != UPDATE_FAILED)
        {
            status = SAVE_FAILED;
            warnErr = count;
        }
    }
    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_REACTIVATE, warnErr, warnKOrg, showKOrgErr);
    return status;
}

/******************************************************************************
* Enable or disable alarms in the calendar file and in every main window instance.
* The new events will have the same event IDs as the old ones.
*/
#ifdef USE_AKONADI
UpdateStatus enableEvents(QVector<KAEvent>& events, bool enable, QWidget* msgParent)
#else
UpdateStatus enableEvents(KAEvent::List& events, bool enable, QWidget* msgParent)
#endif
{
    kDebug() << events.count();
    if (events.isEmpty())
        return UPDATE_OK;
    UpdateStatus status = UPDATE_OK;
    AlarmCalendar* cal = AlarmCalendar::resources();
    bool deleteWakeFromSuspendAlarm = false;
    QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
#ifdef USE_AKONADI
        KAEvent* event = &events[i];
#else
        KAEvent* event = events[i];
#endif
        if (event->category() == CalEvent::ACTIVE
        &&  enable != event->enabled())
        {
            event->setEnabled(enable);

            if (!enable  &&  event->id() == wakeFromSuspendId)
                deleteWakeFromSuspendAlarm = true;

            // Update the event in the calendar file
            KAEvent* newev = cal->updateEvent(event);
            if (!newev)
                kError() << "Error updating event in calendar:" << event->id();
            else
            {
                cal->disabledChanged(newev);

                // If we're disabling a display alarm, close any message window
                if (!enable  &&  (event->actionTypes() & KAEvent::ACT_DISPLAY))
                {
#ifdef USE_AKONADI
                    MessageWin* win = MessageWin::findEvent(EventId(*event));
#else
                    MessageWin* win = MessageWin::findEvent(event->id());
#endif
                    delete win;
                }

#ifndef USE_AKONADI
                // Update the window lists
                EventListModel::alarms()->updateEvent(newev);
#endif
            }
        }
    }

    if (!cal->save())
        status = SAVE_FAILED;
    if (status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, status, ERR_ADD, events.count(), 0);

    // Remove any wake-from-suspend scheduled for a disabled alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);

    return status;
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
    kDebug() << purgeDays;
    QDate cutoff = KDateTime::currentLocalDate().addDays(-purgeDays);
#ifdef USE_AKONADI
    Collection collection = CollectionControlModel::getStandard(CalEvent::ARCHIVED);
    if (!collection.isValid())
        return;
    KAEvent::List events = AlarmCalendar::resources()->events(collection);
    for (int i = 0;  i < events.count();  )
    {
        if (purgeDays  &&  events[i]->createdDateTime().date() >= cutoff)
            events.remove(i);
        else
            ++i;
    }
#else
    AlarmResource* resource = AlarmResources::instance()->getStandardResource(CalEvent::ARCHIVED);
    if (!resource)
        return;
    KAEvent::List events = AlarmCalendar::resources()->events(resource);
    for (int i = 0;  i < events.count();  )
    {
        KAEvent* event = events[i];
        Incidence* kcalIncidence = resource->incidence(event->id());
        if (purgeDays  &&  kcalIncidence  &&  kcalIncidence->created().date() >= cutoff)
            events.remove(i);
        else
            EventListModel::alarms()->removeEvent(events[i++]);   // update the window lists
    }
#endif
    if (!events.isEmpty())
        AlarmCalendar::resources()->purgeEvents(events);   // delete the events and save the calendar
}

/******************************************************************************
* Display an error message about an error when saving an event.
*/
void displayUpdateError(QWidget* parent, UpdateStatus status, UpdateError code, int nAlarms, int nKOrgAlarms, bool showKOrgError)
{
    QString errmsg;
    if (status > UPDATE_KORG_ERR)
    {
        switch (code)
        {
            case ERR_ADD:
            case ERR_MODIFY:
                errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving alarms")
                                       : i18nc("@info", "Error saving alarm");
                break;
            case ERR_DELETE:
                errmsg = (nAlarms > 1) ? i18nc("@info", "Error deleting alarms")
                                       : i18nc("@info", "Error deleting alarm");
                break;
            case ERR_REACTIVATE:
                errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving reactivated alarms")
                                       : i18nc("@info", "Error saving reactivated alarm");
                break;
            case ERR_TEMPLATE:
                errmsg = (nAlarms > 1) ? i18nc("@info", "Error saving alarm templates")
                                       : i18nc("@info", "Error saving alarm template");
                break;
        }
        KAMessageBox::error(parent, errmsg);
    }
    else if (showKOrgError)
        displayKOrgUpdateError(parent, code, status, nKOrgAlarms);
}

/******************************************************************************
* Display an error message corresponding to a specified alarm update error code.
*/
void displayKOrgUpdateError(QWidget* parent, UpdateError code, UpdateStatus korgError, int nAlarms)
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
    QString msg;
    if (korgError == UPDATE_KORG_ERRSTART)
        msg = i18nc("@info", "<para>%1</para><para>(KOrganizer not fully started)</para>", errmsg);
    else if (korgError == UPDATE_KORG_ERR)
        msg = i18nc("@info", "<para>%1</para><para>(Error communicating with KOrganizer)</para>", errmsg);
    else
        msg = errmsg;
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
    connect(dlg, SIGNAL(accepted()), SLOT(okClicked()));
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
#ifdef USE_AKONADI
    Collection calendar;
#else
    AlarmResource* calendar;
#endif
    editDlg->getEvent(event, calendar);

    // Add the alarm to the displayed lists and to the calendar file
#ifdef USE_AKONADI
    UpdateStatus status = addEvent(event, &calendar, editDlg);
#else
    UpdateStatus status = addEvent(event, calendar, editDlg);
#endif
    switch (status)
    {
        case UPDATE_FAILED:
            return;
        case UPDATE_KORG_ERR:
        case UPDATE_KORG_ERRSTART:
        case UPDATE_KORG_FUNCERR:
            displayKOrgUpdateError(editDlg, ERR_ADD, status, 1);
            break;
        default:
            break;
    }
    Undo::saveAdd(event, calendar);

    outputAlarmWarnings(editDlg, &event);
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
        kWarning() << templateName << ": template not found";
    }
    return false;
}

/******************************************************************************
* Create a new template.
*/
void editNewTemplate(EditAlarmDlg::Type type, QWidget* parent)
{
    ::editNewTemplate(type, 0, parent);
}

/******************************************************************************
* Create a new template, based on an existing event or template.
*/
void editNewTemplate(const KAEvent* preset, QWidget* parent)
{
    ::editNewTemplate(EditAlarmDlg::Type(0), preset, parent);
}

/******************************************************************************
* Check the config as to whether there is a wake-on-suspend alarm pending, and
* if so, delete it from the config if it has expired.
* If 'checkExists' is true, the config entry will only be returned if the
* event exists.
* Reply = config entry: [0] = event's collection ID (Akonadi only),
*                       [1] = event ID,
*                       [2] = trigger time (time_t).
*       = empty list if none or expired.
*/
QStringList checkRtcWakeConfig(bool checkEventExists)
{
    KConfigGroup config(KGlobal::config(), "General");
    QStringList params = config.readEntry("RtcWake", QStringList());
#ifdef USE_AKONADI
    if (params.count() == 3  &&  params[2].toUInt() > KDateTime::currentUtcDateTime().toTime_t())
#else
    if (params.count() == 2  &&  params[1].toUInt() > KDateTime::currentUtcDateTime().toTime_t())
#endif
    {
#ifdef USE_AKONADI
        if (checkEventExists  &&  !AlarmCalendar::getEvent(EventId(params[0].toLongLong(), params[1])))
#else
        if (checkEventExists  &&  !AlarmCalendar::getEvent(params[0]))
#endif
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
    KConfigGroup config(KGlobal::config(), "General");
    config.deleteEntry("RtcWake");
    config.sync();
}

/******************************************************************************
* Delete any wake-on-suspend alarm, optionally only for a specified event.
*/
void cancelRtcWake(QWidget* msgParent, const QString& eventId)
{
    QStringList wakeup = checkRtcWakeConfig();
    if (!wakeup.isEmpty()  &&  (eventId.isEmpty() || wakeup[0] == eventId))
    {
        Private::instance()->mMsgParent = msgParent ? msgParent : MainWindow::mainMainWindow();
        QTimer::singleShot(0, Private::instance(), SLOT(cancelRtcWake()));
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
    args["time"] = triggerTime;
    KAuth::Action action("org.kde.kalarmrtcwake.settimer");
    action.setHelperID("org.kde.kalarmrtcwake");
    action.setParentWidget(parent);
    action.setArguments(args);
    KAuth::ActionReply reply = action.execute();
    if (reply.failed())
    {
        QString errmsg = reply.errorDescription();
        kDebug() << "Error code=" << reply.errorCode() << errmsg;
        if (errmsg.isEmpty())
        {
            int errcode = reply.errorCode();
            switch (reply.type())
            {
                case KAuth::ActionReply::KAuthError:
                    kDebug() << "Authorisation error:" << errcode;
                    switch (errcode)
                    {
                        case KAuth::ActionReply::AuthorizationDenied:
                        case KAuth::ActionReply::UserCancelled:
                            return false;   // the user should already know about this
                        default:
                            break;
                    }
                    break;
                case KAuth::ActionReply::HelperError:
                    kDebug() << "Helper error:" << errcode;
                    errcode += 100;    // make code distinguishable from KAuthError type
                    break;
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
#ifdef USE_AKONADI
    if (CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty())
#else
    if (!AlarmResources::instance()->activeCount(CalEvent::TEMPLATE, true))
#endif
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
#ifdef USE_AKONADI
        Akonadi::Collection calendar;
#else
        AlarmResource* calendar;
#endif
        editDlg->getEvent(event, calendar);

        // Add the template to the displayed lists and to the calendar file
#ifdef USE_AKONADI
        KAlarm::addTemplate(event, &calendar, editDlg);
#else
        KAlarm::addTemplate(event, calendar, editDlg);
#endif
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
#ifdef USE_AKONADI
    if (event->expired()  ||  AlarmCalendar::resources()->eventReadOnly(event->itemId()))
#else
    if (event->expired()  ||  AlarmCalendar::resources()->eventReadOnly(event->id()))
#endif
    {
        viewAlarm(event, parent);
        return;
    }
#ifdef USE_AKONADI
    EventId id(*event);
#else
    QString id = event->id();
#endif
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
#ifdef USE_AKONADI
        Collection calendar;
#else
        AlarmResource* calendar;
#endif
        bool changeDeferral = !editDlg->getEvent(newEvent, calendar);

        // Update the event in the displays and in the calendar file
        Undo::Event undo(*event, calendar);
        if (changeDeferral)
        {
            // The only change has been to an existing deferral
            if (updateEvent(newEvent, editDlg, true) != UPDATE_OK)   // keep the same event ID
                return;   // failed to save event
        }
        else
        {
            UpdateStatus status = modifyEvent(*event, newEvent, editDlg);
            if (status != UPDATE_OK  &&  status <= UPDATE_KORG_ERR)
                displayKOrgUpdateError(editDlg, ERR_MODIFY, status, 1);
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
bool editAlarmById(const QString& eventID, QWidget* parent)
{
#ifdef USE_AKONADI
    KAEvent::List events = AlarmCalendar::resources()->events(eventID);
    if (events.count() > 1)
    {
        kWarning() << eventID << ": multiple events found";
        return false;
    }
    if (events.isEmpty())
#else
    KAEvent* event = AlarmCalendar::resources()->event(eventID);
    if (!event)
#endif
    {
        kError() << eventID << ": event ID not found";
        return false;
    }
#ifdef USE_AKONADI
    KAEvent* event = events[0];
    if (AlarmCalendar::resources()->eventReadOnly(event->itemId()))
#else
    if (AlarmCalendar::resources()->eventReadOnly(eventID))
#endif
    {
        kError() << eventID << ": read-only";
        return false;
    }
    switch (event->category())
    {
        case CalEvent::ACTIVE:
        case CalEvent::TEMPLATE:
            break;
        default:
            kError() << eventID << ": event not active or template";
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
#ifdef USE_AKONADI
    if (AlarmCalendar::resources()->eventReadOnly(event->itemId()))
#else
    if (AlarmCalendar::resources()->eventReadOnly(event->id()))
#endif
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
#ifdef USE_AKONADI
        Akonadi::Collection calendar;
#else
        AlarmResource* calendar;
#endif
        editDlg->getEvent(newEvent, calendar);
        QString id = event->id();
        newEvent.setEventId(id);

        // Update the event in the displays and in the calendar file
        Undo::Event undo(*event, calendar);
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
#ifdef USE_AKONADI
void updateEditedAlarm(EditAlarmDlg* editDlg, KAEvent& event, Collection& calendar)
#else
void updateEditedAlarm(EditAlarmDlg* editDlg, KAEvent& event, AlarmResource* calendar)
#endif
{
    kDebug();
    KAEvent newEvent;
#ifdef USE_AKONADI
    Akonadi::Collection cal;
#else
    AlarmResource* cal;
#endif
    editDlg->getEvent(newEvent, cal);

    // Update the displayed lists and the calendar file
    UpdateStatus status;
#ifdef USE_AKONADI
    if (AlarmCalendar::resources()->event(EventId(event)))
#else
    if (AlarmCalendar::resources()->event(event.id()))
#endif
    {
        // The old alarm hasn't expired yet, so replace it
        Undo::Event undo(event, calendar);
        status = modifyEvent(event, newEvent, editDlg);
        Undo::saveEdit(undo, newEvent);
    }
    else
    {
        // The old event has expired, so simply create a new one
#ifdef USE_AKONADI
        status = addEvent(newEvent, &calendar, editDlg);
#else
        status = addEvent(newEvent, calendar, editDlg);
#endif
        Undo::saveAdd(newEvent, calendar);
    }

    if (status != UPDATE_OK  &&  status <= UPDATE_KORG_ERR)
        displayKOrgUpdateError(editDlg, ERR_MODIFY, status, 1);
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
    bool includeCmdAlarms = ShellProcess::authorised();
    KAEvent::List events = AlarmCalendar::resources()->events(CalEvent::TEMPLATE);
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        KAEvent* event = events[i];
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
        KAMessageBox::information(parent, i18nc("@info Please set the 'From' email address...",
                                                "<para>%1</para><para>Please set it in the Configuration dialog.</para>", KAMail::i18n_NeedFromEmailAddress()));

    if (!theApp()->alarmsEnabled())
    {
        if (KAMessageBox::warningYesNo(parent, i18nc("@info", "<para>Alarms are currently disabled.</para><para>Do you want to enable alarms now?</para>"),
                                       QString(), KGuiItem(i18nc("@action:button", "Enable")), KGuiItem(i18nc("@action:button", "Keep Disabled")),
                                       QLatin1String("EditEnableAlarms"))
                        == KMessageBox::Yes)
            theApp()->setAlarmsEnabled(true);
    }
}

/******************************************************************************
* Reload the calendar.
*/
void refreshAlarms()
{
    kDebug();
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
        kDebug();
        AlarmCalendar::resources()->reload();

        // Close any message windows for alarms which are now disabled
        KAEvent::List events = AlarmCalendar::resources()->events(CalEvent::ACTIVE);
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            if (!event->enabled()  &&  (event->actionTypes() & KAEvent::ACT_DISPLAY))
            {
#ifdef USE_AKONADI
                MessageWin* win = MessageWin::findEvent(EventId(*event));
#else
                MessageWin* win = MessageWin::findEvent(event->id());
#endif
                delete win;
            }
        }

        MainWindow::refresh();
        refreshAlarmsQueued = false;
    }
}

/******************************************************************************
* Start KMail if it isn't already running, optionally minimised.
* Reply = reason for failure to run KMail (which may be the empty string)
*       = null string if success.
*/
QString runKMail(bool minimise)
{
    QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(KMAIL_DBUS_SERVICE);
    if (!reply.isValid()  ||  !reply.value())
    {
        // Program is not already running, so start it
        QString errmsg;
        if (minimise  &&  Private::startKMailMinimised())
            return QString();
        if (KToolInvocation::startServiceByDesktopName(QLatin1String("kmail"), QString(), &errmsg))
        {
            kError() << "Couldn't start KMail (" << errmsg << ")";
            return i18nc("@info", "Unable to start <application>KMail</application><nl/>(<message>%1</message>)", errmsg);
        }
    }
    return QString();
}

/******************************************************************************
* Start KMail, minimised.
* This code is taken from kstart in kdebase.
*/
bool Private::startKMailMinimised()
{
#ifdef Q_WS_X11
    NETRootInfo i(QX11Info::display(), NET::Supported);
    if (i.isSupported(NET::WM2KDETemporaryRules))
    {
        kDebug() << "using rules";
        KXMessages msg;
        QString message = "wmclass=kmail\nwmclassmatch=1\n"  // 1 = exact match
                          "wmclasscomplete=false\n"
                          "minimize=true\nminimizerule=3\n"
                          "type=" + QString().setNum(NET::Normal) + "\ntyperule=2";
        msg.broadcastMessage("_KDE_NET_WM_TEMPORARY_RULES", message, -1, false);
        qApp->flush();
    }
    else
    {
        // Connect to window add to get the NEW windows
        kDebug() << "connecting to window add";
        connect(KWindowSystem::self(), SIGNAL(windowAdded(WId)), instance(), SLOT(windowAdded(WId)));
    }
    // Propagate the app startup notification info to the started app.
    // We are not using KApplication, so the env remained set.
    KStartupInfoId id = KStartupInfo::currentStartupIdEnv();
    KProcess* proc = new KProcess;
    (*proc) << "kmail";
    int pid = proc->startDetached();
    if (!pid)
    {
        KStartupInfo::sendFinish(id); // failed to start
        return false;
    }
    KStartupInfoData data;
    data.addPid(pid);
    data.setName("kmail");
    data.setBin("kmail");
    KStartupInfo::sendChange(id, data);
    return true;
#else
    return false;
#endif
}

/******************************************************************************
* Called when a window is created, to minimise it.
* This code is taken from kstart in kdebase.
*/
void Private::windowAdded(WId w)
{
#ifdef Q_WS_X11
    static const int SUPPORTED_TYPES = NET::NormalMask | NET::DesktopMask | NET::DockMask
                                     | NET::ToolbarMask | NET::MenuMask | NET::DialogMask
                                     | NET::OverrideMask | NET::TopMenuMask | NET::UtilityMask | NET::SplashMask;
    KWindowInfo kwinfo = KWindowSystem::windowInfo(w, NET::WMWindowType | NET::WMName);
    if (kwinfo.windowType(SUPPORTED_TYPES) == NET::TopMenu
    ||  kwinfo.windowType(SUPPORTED_TYPES) == NET::Toolbar
    ||  kwinfo.windowType(SUPPORTED_TYPES) == NET::Desktop)
        return;   // always ignore these window types

    QX11Info qxinfo;
    XWithdrawWindow(QX11Info::display(), w, qxinfo.screen());
    QApplication::flush();

    NETWinInfo info(QX11Info::display(), w, QX11Info::appRootWindow(), NET::WMState);
    XWMHints* hints = XGetWMHints(QX11Info::display(), w);
    if (hints)
    {
        hints->flags |= StateHint;
        hints->initial_state = IconicState;
        XSetWMHints(QX11Info::display(), w, hints);
        XFree(hints);
    }
    info.setWindowType(NET::Normal);

    XSync(QX11Info::display(), False);
    XMapWindow(QX11Info::display(), w);
    XSync(QX11Info::display(), False);
    QApplication::flush();
#endif
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
#ifdef USE_AKONADI
QStringList dontShowErrors(const EventId& eventId)
#else
QStringList dontShowErrors(const QString& eventId)
#endif
{
    if (eventId.isEmpty())
        return QStringList();
    KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
#ifdef USE_AKONADI
    const QString id = QString("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
#else
    const QString id(eventId);
#endif
    return group.readEntry(id, QStringList());
}

/******************************************************************************
* Check whether the specified Don't-show-again error message tag is set for an
* alarm ID.
*/
#ifdef USE_AKONADI
bool dontShowErrors(const EventId& eventId, const QString& tag)
#else
bool dontShowErrors(const QString& eventId, const QString& tag)
#endif
{
    if (tag.isEmpty())
        return false;
    QStringList tags = dontShowErrors(eventId);
    return tags.indexOf(tag) >= 0;
}

/******************************************************************************
* Reset the Don't-show-again error message tags for an alarm ID.
* If 'tags' is empty, the config entry is deleted.
*/
#ifdef USE_AKONADI
void setDontShowErrors(const EventId& eventId, const QStringList& tags)
#else
void setDontShowErrors(const QString& eventId, const QStringList& tags)
#endif
{
    if (eventId.isEmpty())
        return;
    KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
#ifdef USE_AKONADI
    const QString id = QString("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
#else
    const QString id(eventId);
#endif
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
#ifdef USE_AKONADI
void setDontShowErrors(const EventId& eventId, const QString& tag)
#else
void setDontShowErrors(const QString& eventId, const QString& tag)
#endif
{
    if (eventId.isEmpty()  ||  tag.isEmpty())
        return;
    KConfig config(KStandardDirs::locateLocal("appdata", ALARM_OPTS_FILE));
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
#ifdef USE_AKONADI
    const QString id = QString("%1:%2").arg(eventId.collectionId()).arg(eventId.eventId());
#else
    const QString id(eventId);
#endif
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
    KConfigGroup config(KGlobal::config(), window);
    QWidget* desktop = KApplication::desktop();
    QSize s = QSize(config.readEntry(QString::fromLatin1("Width %1").arg(desktop->width()), (int)0),
                    config.readEntry(QString::fromLatin1("Height %1").arg(desktop->height()), (int)0));
    if (s.isEmpty())
        return false;
    result = s;
    if (splitterWidth)
        *splitterWidth = config.readEntry(QString::fromLatin1("Splitter %1").arg(desktop->width()), -1);
    return true;
}

/******************************************************************************
* Write the size for the specified window to the config file, for the
* current screen resolution.
*/
void writeConfigWindowSize(const char* window, const QSize& size, int splitterWidth)
{
    KConfigGroup config(KGlobal::config(), window);
    QWidget* desktop = KApplication::desktop();
    config.writeEntry(QString::fromLatin1("Width %1").arg(desktop->width()), size.width());
    config.writeEntry(QString::fromLatin1("Height %1").arg(desktop->height()), size.height());
    if (splitterWidth >= 0)
        config.writeEntry(QString::fromLatin1("Splitter %1").arg(desktop->width()), splitterWidth);
    config.sync();
}

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
FileType fileType(const KMimeType::Ptr& mimetype)
{
    if (mimetype->is("text/html"))
        return TextFormatted;
    if (mimetype->is("application/x-executable"))
        return TextApplication;
    if (mimetype->is("text/plain"))
        return TextPlain;
    if (mimetype->name().startsWith(QLatin1String("image/")))
        return Image;
    return Unknown;
}

/******************************************************************************
* Check that a file exists and is a plain readable file.
* Updates 'filename' and 'url' even if an error occurs, since 'filename' may
* be needed subsequently by showFileErrMessage().
*/
FileErr checkFileExists(QString& filename, KUrl& url)
{
    url = KUrl();
    FileErr err = FileErr_None;
    QString file = filename;
    QRegExp f("^file:/+");
    if (f.indexIn(file) >= 0)
        file = file.mid(f.matchedLength() - 1);
    // Convert any relative file path to absolute
    // (using home directory as the default)
    int i = file.indexOf(QLatin1Char('/'));
    if (i > 0  &&  file[i - 1] == QLatin1Char(':'))
    {
        url = file;
        url.cleanPath();
        filename = url.prettyUrl();
        KIO::UDSEntry uds;
        if (!KIO::NetAccess::stat(url, uds, MainWindow::mainMainWindow()))
            err = FileErr_Nonexistent;
        else
        {
            KFileItem fi(uds, url);
            if (fi.isDir())             err = FileErr_Directory;
            else if (!fi.isReadable())  err = FileErr_Unreadable;
        }
    }
    else if (file.isEmpty())
        err = FileErr_Blank;    // blank file name
    else
    {
        // It's a local file - convert to absolute path & check validity
        QFileInfo info(file);
        QDir::setCurrent(QDir::homePath());
        filename = info.absoluteFilePath();
        url.setPath(filename);
        if      (info.isDir())        err = FileErr_Directory;
        else if (!info.exists())      err = FileErr_Nonexistent;
        else if (!info.isReadable())  err = FileErr_Unreadable;
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
        QRegExp f("^file:/+");
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
                    kFatal() << "Program error";
                KAMessageBox::sorry(errmsgParent, errmsg);
                return false;
            case FileErr_Directory:
                KAMessageBox::sorry(errmsgParent, i18nc("@info", "<filename>%1</filename> is a folder", file));
                return false;
            case FileErr_Nonexistent:   errmsg = i18nc("@info", "<filename>%1</filename> not found", file);  break;
            case FileErr_Unreadable:    errmsg = i18nc("@info", "<filename>%1</filename> is not readable", file);  break;
            case FileErr_NotTextImage:  errmsg = i18nc("@info", "<filename>%1</filename> appears not to be a text or image file", file);  break;
            case FileErr_None:
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
    static const QRegExp localfile("^file:/+");
    return (localfile.indexIn(url) >= 0) ? url.mid(localfile.matchedLength() - 1) : url;
}

/******************************************************************************
* Display a modal dialog to choose an existing file, initially highlighting
* any specified file.
* @param initialFile The file to initially highlight - must be a full path name or URL.
* @param defaultDir The directory to start in if @p initialFile is empty. If empty,
*                   the user's home directory will be used. Updated to the
*                   directory containing the selected file, if a file is chosen.
* @param mode OR of KFile::Mode values, e.g. ExistingOnly, LocalOnly.
* Reply = URL selected.
*       = empty, non-null string if no file was selected.
*       = null string if dialogue was deleted while visible (indicating that
*         the parent widget was probably also deleted).
*/
QString browseFile(const QString& caption, QString& defaultDir, const QString& initialFile,
                   const QString& filter, KFile::Modes mode, QWidget* parent)
{
    QString initialDir = !initialFile.isEmpty() ? QString(initialFile).remove(QRegExp("/[^/]*$"))
                       : !defaultDir.isEmpty()  ? defaultDir
                       :                          QDir::homePath();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<KFileDialog> fileDlg = new KFileDialog(initialDir, filter, parent);
    fileDlg->setOperationMode(mode & KFile::ExistingOnly ? KFileDialog::Opening : KFileDialog::Saving);
    fileDlg->setMode(KFile::File | mode);
    fileDlg->setCaption(caption);
    if (!initialFile.isEmpty())
        fileDlg->setSelection(initialFile);
    if (fileDlg->exec() != QDialog::Accepted)
        return fileDlg ? QString("") : QString();  // return null only if dialog was deleted
    KUrl url = fileDlg->selectedUrl();
    if (url.isEmpty())
        return QString("");   // return empty, non-null string
    defaultDir = url.isLocalFile() ? url.upUrl().toLocalFile() : url.directory();
    return (mode & KFile::LocalOnly) ? url.pathOrUrl() : url.prettyUrl();
}

/******************************************************************************
* Convert a date/time specification string into a local date/time or date value.
* Parameters:
*   timeString  = in the form [[[yyyy-]mm-]dd-]hh:mm [TZ] or yyyy-mm-dd [TZ].
*   dateTime  = receives converted date/time value.
*   defaultDt = default date/time used for missing parts of timeString, or null
*               to use current date/time.
*   allowTZ   = whether to allow a time zone specifier in timeString.
* Reply = true if successful.
*/
bool convertTimeString(const QByteArray& timeString, KDateTime& dateTime, const KDateTime& defaultDt, bool allowTZ)
{
#define MAX_DT_LEN 19
    int i = timeString.indexOf(' ');
    if (i > MAX_DT_LEN  ||  (i >= 0 && !allowTZ))
        return false;
    QString zone = (i >= 0) ? QString::fromLatin1(timeString.mid(i)) : QString();
    char timeStr[MAX_DT_LEN+1];
    strcpy(timeStr, timeString.left(i >= 0 ? i : MAX_DT_LEN));
    int dt[5] = { -1, -1, -1, -1, -1 };
    char* s;
    char* end;
    bool noTime;
    // Get the minute value
    if ((s = strchr(timeStr, ':')) == 0)
        noTime = true;
    else
    {
        noTime = false;
        *s++ = 0;
        dt[4] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[4] >= 60)
            return false;
        // Get the hour value
        if ((s = strrchr(timeStr, '-')) == 0)
            s = timeStr;
        else
            *s++ = 0;
        dt[3] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[3] >= 24)
            return false;
    }
    bool noDate = true;
    if (s != timeStr)
    {
        noDate = false;
        // Get the day value
        if ((s = strrchr(timeStr, '-')) == 0)
            s = timeStr;
        else
            *s++ = 0;
        dt[2] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
            return false;
        if (s != timeStr)
        {
            // Get the month value
            if ((s = strrchr(timeStr, '-')) == 0)
                s = timeStr;
            else
                *s++ = 0;
            dt[1] = strtoul(s, &end, 10);
            if (end == s  ||  *end  ||  dt[1] == 0  ||  dt[1] > 12)
                return false;
            if (s != timeStr)
            {
                // Get the year value
                dt[0] = strtoul(timeStr, &end, 10);
                if (end == timeStr  ||  *end)
                    return false;
            }
        }
    }

    QDate date;
    if (dt[0] >= 0)
        date = QDate(dt[0], dt[1], dt[2]);
    QTime time(0, 0, 0);
    if (noTime)
    {
        // No time was specified, so the full date must have been specified
        if (dt[0] < 0  ||  !date.isValid())
            return false;
        dateTime = KAlarm::applyTimeZone(zone, date, time, false, defaultDt);
    }
    else
    {
        // Compile the values into a date/time structure
        time.setHMS(dt[3], dt[4], 0);
        if (dt[0] < 0)
        {
            // Some or all of the date was omitted.
            // Use the default date/time if provided.
            if (defaultDt.isValid())
            {
                dt[0] = defaultDt.date().year();
                date.setYMD(dt[0],
                            (dt[1] < 0 ? defaultDt.date().month() : dt[1]),
                            (dt[2] < 0 ? defaultDt.date().day() : dt[2]));
            }
            else
                date.setYMD(2000, 1, 1);  // temporary substitute for date
        }
        dateTime = KAlarm::applyTimeZone(zone, date, time, true, defaultDt);
        if (!dateTime.isValid())
            return false;
        if (dt[0] < 0)
        {
            // Some or all of the date was omitted.
            // Use the current date in the specified time zone as default.
            KDateTime now = KDateTime::currentDateTime(dateTime.timeSpec());
            date = dateTime.date();
            date.setYMD(now.date().year(),
                        (dt[1] < 0 ? now.date().month() : dt[1]),
                        (dt[2] < 0 ? now.date().day() : dt[2]));
            if (!date.isValid())
                return false;
            if (noDate  &&  time < now.time())
                date = date.addDays(1);
            dateTime.setDate(date);
        }
    }
    return dateTime.isValid();
}

/******************************************************************************
* Convert a time zone specifier string and apply it to a given date and/or time.
* The time zone specifier is a system time zone name, e.g. "Europe/London",
* "UTC" or "Clock". If no time zone is specified, it defaults to the local time
* zone.
* If 'defaultDt' is valid, it supplies the time spec and default date.
*/
KDateTime applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                        bool haveTime, const KDateTime& defaultDt)
{
    bool error = false;
    KDateTime::Spec spec = KDateTime::LocalZone;
    QString zone = tzstring.trimmed();
    if (!zone.isEmpty())
    {
        if (zone == QLatin1String("Clock"))
            spec = KDateTime::ClockTime;
        else if (zone == QLatin1String("UTC"))
            spec = KDateTime::UTC;
        else
        {
            KTimeZone tz = KSystemTimeZones::zone(zone);
            error = !tz.isValid();
            if (!error)
                spec = tz;
        }
    }
    else if (defaultDt.isValid())
        spec = defaultDt.timeSpec();

    KDateTime result;
    if (!error)
    {
        if (!date.isValid())
        {
            // It's a time without a date
            if (defaultDt.isValid())
                   result = KDateTime(defaultDt.date(), time, spec);
            else if (spec == KDateTime::LocalZone  ||  spec == KDateTime::ClockTime)
                result = KDateTime(KDateTime::currentLocalDate(), time, spec);
        }
        else if (haveTime)
        {
            // It's a date and time
            result = KDateTime(date, time, spec);
        }
        else
        {
            // It's a date without a time
            result = KDateTime(date, spec);
        }
    }
    return result;
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
    QString msg = whole
                ? i18nc("@info", "Calendar <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), "
                       "and will be read-only unless you choose to update it to the current format.",
                       calendarName, calendarVersion)
                : i18nc("@info", "Some or all of the alarms in calendar <resource>%1</resource> are in an old <application>KAlarm</application> format, "
                       "and will be read-only unless you choose to update them to the current format.",
                       calendarName);
    return i18nc("@info", "<para>%1</para><para>"
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
        KDateTime dt;
        if (convertTimeString(newTime, dt, KDateTime::realCurrentLocalDateTime(), true))
            setSimulatedSystemTime(dt);
    }
}

/******************************************************************************
* Set the simulated system time.
*/
void setSimulatedSystemTime(const KDateTime& dt)
{
    KDateTime::setSimulatedSystemTime(dt);
    kDebug() << "New time =" << qPrintable(KDateTime::currentLocalDateTime().toString("%Y-%m-%d %H:%M %:Z"));
}
#endif

} // namespace KAlarm
namespace
{

/******************************************************************************
* Tell KOrganizer to put an alarm in its calendar.
* It will be held by KOrganizer as a simple event, without alarms - KAlarm
* is still responsible for alarming.
*/
KAlarm::UpdateStatus sendToKOrganizer(const KAEvent* event)
{
#ifdef USE_AKONADI
    Event::Ptr kcalEvent(new KCalCore::Event);
        event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
#else
    Event* kcalEvent = AlarmCalendar::resources()->createKCalEvent(event);
#endif
    // Change the event ID to avoid duplicating the same unique ID as the original event
    QString uid = uidKOrganizer(event->id());
    kcalEvent->setUid(uid);
    kcalEvent->clearAlarms();
    QString userEmail;
    switch (event->actionTypes())
    {
        case KAEvent::ACT_DISPLAY:
        case KAEvent::ACT_COMMAND:
        case KAEvent::ACT_DISPLAY_COMMAND:
            kcalEvent->setSummary(event->cleanText());
            userEmail = Preferences::emailAddress();
            break;
        case KAEvent::ACT_EMAIL:
        {
            QString from = event->emailFromId()
                         ? Identities::identityManager()->identityForUoid(event->emailFromId()).fullEmailAddr()
                         : Preferences::emailAddress();
            AlarmText atext;
            atext.setEmail(event->emailAddresses(", "), from, QString(), QString(), event->emailSubject(), QString());
            kcalEvent->setSummary(atext.displayText());
            userEmail = from;
            break;
        }
        case KAEvent::ACT_AUDIO:
            kcalEvent->setSummary(event->audioFile());
            break;
        default:
            break;
    }
#ifdef USE_AKONADI
    Person::Ptr person(new Person(QString(), userEmail));
    kcalEvent->setOrganizer(person);
#else
    kcalEvent->setOrganizer(KCal::Person(QString(), userEmail));
#endif
    kcalEvent->setDuration(Duration(Preferences::kOrgEventDuration() * 60, Duration::Seconds));

    // Translate the event into string format
    ICalFormat format;
    format.setTimeSpec(Preferences::timeZone(true));
    QString iCal = format.toICalString(kcalEvent);
#ifndef USE_AKONADI
    delete kcalEvent;
#endif

    // Send the event to KOrganizer
    KAlarm::UpdateStatus st = runKOrganizer();   // start KOrganizer if it isn't already running, and create its D-Bus interface
    if (st != KAlarm::UPDATE_OK)
        return st;
    QList<QVariant> args;
    args << iCal;
    QDBusReply<bool> reply = korgInterface->callWithArgumentList(QDBus::Block, QLatin1String("addIncidence"), args);
    if (!reply.isValid())
    {
        if (reply.error().type() == QDBusError::UnknownObject)
        {
            kError()<<"addIncidence() D-Bus error: still starting";
            return KAlarm::UPDATE_KORG_ERRSTART;
        }
        kError() << "addIncidence(" << uid << ") D-Bus call failed:" << reply.error().message();
        return KAlarm::UPDATE_KORG_ERR;
    }
    if (!reply.value())
    {
        kDebug() << "addIncidence(" << uid << ") D-Bus call returned false";
        return KAlarm::UPDATE_KORG_FUNCERR;
    }
    kDebug() << uid << ": success";
    return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Tell KOrganizer to delete an event from its calendar.
*/
KAlarm::UpdateStatus deleteFromKOrganizer(const QString& eventID)
{
    KAlarm::UpdateStatus st = runKOrganizer();   // start KOrganizer if it isn't already running, and create its D-Bus interface
    if (st != KAlarm::UPDATE_OK)
        return st;
    QString newID = uidKOrganizer(eventID);
    QList<QVariant> args;
    args << newID << true;
    QDBusReply<bool> reply = korgInterface->callWithArgumentList(QDBus::Block, QLatin1String("deleteIncidence"), args);
    if (!reply.isValid())
    {
        if (reply.error().type() == QDBusError::UnknownObject)
        {
            kError()<<"deleteIncidence() D-Bus error: still starting";
            return KAlarm::UPDATE_KORG_ERRSTART;
        }
        kError() << "deleteIncidence(" << newID << ") D-Bus call failed:" << reply.error().message();
        return KAlarm::UPDATE_KORG_ERR;
    }
    if (!reply.value())
    {
        kDebug() << "deleteIncidence(" << newID << ") D-Bus call returned false";
        return KAlarm::UPDATE_KORG_FUNCERR;
    }
    kDebug() << newID << ": success";
    return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Start KOrganizer if not already running, and create its D-Bus interface.
*/
KAlarm::UpdateStatus runKOrganizer()
{
    QString error, dbusService;
    int result = KDBusServiceStarter::self()->findServiceFor("DBUS/Organizer", QString(), &error, &dbusService);
    if (result)
    {
        kWarning() << "Unable to start DBUS/Organizer:" << dbusService << error;
        return KAlarm::UPDATE_KORG_ERR;
    }
    // If Kontact is running, there is be a load() method which needs to be called
    // to load KOrganizer into Kontact. But if KOrganizer is running independently,
    // the load() method doesn't exist.
    QDBusInterface iface(KORG_DBUS_SERVICE, KORG_DBUS_LOAD_PATH, "org.kde.KUniqueApplication");
    if (!iface.isValid())
    {
        kWarning() << "Unable to access "KORG_DBUS_LOAD_PATH" D-Bus interface:" << iface.lastError().message();
        return KAlarm::UPDATE_KORG_ERR;
    }
    QDBusReply<bool> reply = iface.call("load");
    if ((!reply.isValid() || !reply.value())
    &&  iface.lastError().type() != QDBusError::UnknownMethod)
    {
        kWarning() << "Loading KOrganizer failed:" << iface.lastError().message();
        return KAlarm::UPDATE_KORG_ERR;
    }

    // KOrganizer has been started, but it may not have the necessary
    // D-Bus interface available yet.
    if (!korgInterface  ||  !korgInterface->isValid())
    {
        delete korgInterface;
        korgInterface = new QDBusInterface(KORG_DBUS_SERVICE, KORG_DBUS_PATH, KORG_DBUS_IFACE);
        if (!korgInterface->isValid())
        {
            kWarning() << "Unable to access "KORG_DBUS_PATH" D-Bus interface:" << korgInterface->lastError().message();
            delete korgInterface;
            korgInterface = 0;
            return KAlarm::UPDATE_KORG_ERRSTART;
        }
    }
    return KAlarm::UPDATE_OK;
}

/******************************************************************************
* Insert a KOrganizer string after the hyphen in the supplied event ID.
*/
QString uidKOrganizer(const QString& id)
{
    QString result = id;
    int i = result.lastIndexOf('-');
    if (i < 0)
        i = result.length();
    return result.insert(i, KORGANIZER_UID);
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
