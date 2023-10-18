/*
 *  functions.cpp  -  miscellaneous functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "functions.h"
#include "functions_p.h"

#include "displaycalendar.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "mainwindow.h"
#include "preferences.h"
#include "resourcescalendar.h"
#include "resources/calendarfunctions.h"
#include "resources/datamodel.h"
#include "resources/eventmodel.h"
#include "resources/resources.h"
#include "lib/autoqpointer.h"
#include "lib/dragdrop.h"
#include "lib/filedialog.h"
#include "lib/messagebox.h"
#include "lib/shellprocess.h"
#include "kalarmcalendar/identities.h"
#include "akonadiplugin/akonadiplugin.h"
#include "kalarm_debug.h"

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/Person>
#include <KCalendarCore/Duration>
#include <KCalendarCore/MemoryCalendar>
using namespace KCalendarCore;
#include <KIdentityManagementCore/IdentityManager>
#include <KIdentityManagementCore/Identity>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KToggleAction>
#include <KLocalizedString>

#if ENABLE_RTC_WAKE_FROM_SUSPEND
#include <KAuth/ActionReply>
#include <KAuth/ExecuteJob>
#endif
#include <KStandardShortcut>
#include <KFileItem>
#include <KJobWidgets>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KFileCustomDialog>
#include <KWindowSystem>
#if ENABLE_X11
#include <KWindowInfo>
#include <KX11Extras>
#endif

#include <QAction>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QTimer>
#include <QMimeData>
#include <QStandardPaths>
#include <QPushButton>
#include <QTemporaryFile>

//clazy:excludeall=non-pod-global-static

namespace
{
bool refreshAlarmsQueued = false;
QUrl lastImportUrl;     // last URL for Import Alarms file dialogue
QUrl lastExportUrl;     // last URL for Export Alarms file dialogue

struct UpdateStatusData
{
    KAlarm::UpdateResult status;   // status code and error message if any
    int                  warnErr;
    int                  warnKOrg;

    explicit UpdateStatusData(KAlarm::UpdateStatus s = KAlarm::UPDATE_OK) : status(s), warnErr(0), warnKOrg(0) {}
    int  failedCount() const      { return status.failed.count(); }
    void appendFailed(int index)  { status.failed.append(index); }
    // Set an error status and increment to number of errors to warn about
    void setError(KAlarm::UpdateStatus st, int errorCount = -1, const QString& msg = QString())
    {
        status.set(st, msg);
        if (errorCount < 0)
            ++warnErr;
        else
            warnErr = errorCount;
    }
    // Set an error status and increment to number of errors to warn about,
    // without changing the error message
    void updateError(KAlarm::UpdateStatus st, int errorCount = -1)
    {
        status.status = st;
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

const QLatin1String KORG_DBUS_SERVICE("org.kde.korganizer");
const QLatin1String KORG_DBUS_IFACE("org.kde.korganizer.Korganizer");
// D-Bus object path of KOrganizer's notification interface
#define       KORG_DBUS_PATH            "/Korganizer"
#define       KORG_DBUS_LOAD_PATH       "/korganizer_PimApplication"
const QLatin1String KORG_MIME_TYPE("application/x-vnd.akonadi.calendar.event");
const QLatin1String KORGANIZER_UID("korg-");

const QLatin1String ALARM_OPTS_FILE("alarmopts");
const char*         DONT_SHOW_ERRORS_GROUP = "DontShowErrors";

KAlarm::UpdateResult updateEvent(KAEvent&, KAlarm::UpdateError, QWidget* msgParent, bool saveIfReadOnly);
void editNewTemplate(EditAlarmDlg::Type, const KAEvent& preset, QWidget* parent);
void displayUpdateError(QWidget* parent, KAlarm::UpdateError, const UpdateStatusData&, bool showKOrgError = true);
KAlarm::UpdateResult sendToKOrganizer(const KAEvent&);
KAlarm::UpdateResult deleteFromKOrganizer(const QString& eventID);
KAlarm::UpdateResult runKOrganizer();
QString uidKOrganizer(const QString& eventID);
}


namespace KAlarm
{

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString i18n_act_StopPlay()    { return i18nc("@action", "Stop Play"); }

Private* Private::mInstance = nullptr;

/******************************************************************************
* Display a main window with the specified event selected.
*/
MainWindow* displayMainWindowSelected(const QString& eventId)
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
#if ENABLE_X11
        KWindowInfo wi(win->winId(), NET::WMDesktop);
        if (!wi.valid()  ||  !wi.isOnDesktop(KX11Extras::currentDesktop()))
        {
            // The main window isn't on the current virtual desktop. Hide it
            // first so that it will be shown on the current desktop when it is
            // shown again. Note that this shifts the window's position, so
            // don't hide it if it's already on the current desktop.
            win->hide();
        }
#endif
        win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
        win->show();
        win->raise();
        KWindowSystem::activateWindow(win->windowHandle());
    }
    if (win)
        win->selectEvent(eventId);
    return win;
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
KToggleAction* createAlarmEnableAction(QObject* parent)
{
    KToggleAction* action = new KToggleAction(i18nc("@action", "Enable Alarms"), parent);
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
    QAction* action = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), i18n_act_StopPlay(), parent);
    action->setEnabled(MessageDisplay::isAudioPlaying());
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
* Add a new active (non-archived) alarm, and save it in its resource.
* Parameters: msgParent = parent widget for any calendar selection prompt or
*                         error message.
*             event - is updated with the actual event ID.
*/
UpdateResult addEvent(KAEvent& event, Resource& resource, QWidget* msgParent, int options, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::addEvent:" << event.id();
    bool cancelled = false;
    UpdateStatusData status;
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status.status = UPDATE_FAILED;
    else
    {
        // Save the event details in the calendar file, and get the new event ID
        // Note that ResourcesCalendar::addEvent() updates 'event'.
        ResourcesCalendar::AddEventOptions rc_options = {};
        if (options & USE_EVENT_ID)
            rc_options |= ResourcesCalendar::UseEventId;
        if (options & NO_RESOURCE_PROMPT)
            rc_options |= ResourcesCalendar::NoResourcePrompt;
        if (!ResourcesCalendar::addEvent(event, resource, msgParent, rc_options, &cancelled))
        {
            status.status = UPDATE_FAILED;
        }
        else
        {
            if (!resource.save(&status.status.message))
            {
                resource.reload(true);   // discard the new event
                status.status.status = SAVE_FAILED;
            }
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
* Add a list of new active (non-archived) alarms, and save them in the resource.
* The events are updated with their actual event IDs.
*/
UpdateResult addEvents(QList<KAEvent>& events, Resource& resource, QWidget* msgParent,
                       bool allowKOrgUpdate, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::addEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    if (!theApp()->checkCalendar())    // ensure calendar is open
        status.status = UPDATE_FAILED;
    else
    {
        if (!resource.isValid())
            resource = Resources::destination(CalEvent::ACTIVE, msgParent);
        if (!resource.isValid())
        {
            qCDebug(KALARM_LOG) << "KAlarm::addEvents: No calendar";
            status.status = UPDATE_FAILED;
        }
        else
        {
            for (int i = 0, end = events.count();  i < end;  ++i)
            {
                // Save the event details in the calendar file, and get the new event ID
                KAEvent& event = events[i];
                if (!ResourcesCalendar::addEvent(event, resource, msgParent))
                {
                    status.appendFailed(i);
                    status.setError(UPDATE_ERROR);
                    continue;
                }
                if (allowKOrgUpdate  &&  event.copyToKOrganizer())
                {
                    UpdateResult st = sendToKOrganizer(event);    // tell KOrganizer to show the event
                    status.korgUpdate(st);
                }

            }
            if (status.failedCount() == events.count())
                status.status = UPDATE_FAILED;
            else if (!resource.save(&status.status.message))
            {
                resource.reload(true);   // discard the new events
                status.updateError(SAVE_FAILED, events.count());  // everything failed
            }
        }
    }

    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_ADD, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Save the event in the archived calendar.
* The event's ID is changed to an archived ID if necessary.
*/
bool addArchivedEvent(KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "KAlarm::addArchivedEvent:" << event.id();
    bool archiving = (event.category() == CalEvent::ACTIVE);
    if (archiving  &&  !Preferences::archivedKeepDays())
        return false;   // expired alarms aren't being kept
    KAEvent newevent(event);
    KAEvent* const newev = &newevent;
    if (archiving)
    {
        newev->setCategory(CalEvent::ARCHIVED);    // this changes the event ID
        newev->setCreatedDateTime(KADateTime::currentUtcDateTime());   // time stamp to control purging
    }
    // Add the event from the archived resource. It's not too important whether
    // the resource is saved successfully after the deletion, so allow it to be
    // saved automatically.
    if (!ResourcesCalendar::addEvent(newevent, resource))
        return false;
    event = *newev;   // update event ID etc.

    return true;
}

/******************************************************************************
* Add a new template.
* 'event' is updated with the actual event ID.
* Parameters: promptParent = parent widget for any calendar selection prompt.
*/
UpdateResult addTemplate(KAEvent& event, Resource& resource, QWidget* msgParent)
{
    qCDebug(KALARM_LOG) << "KAlarm::addTemplate:" << event.id();
    UpdateStatusData status;

    // Add the template to the calendar file
    KAEvent newev(event);
    if (!ResourcesCalendar::addEvent(newev, resource, msgParent))
        status.status = UPDATE_FAILED;
    else
    {
        event = newev;   // update event ID etc.
        if (!resource.save(&status.status.message))
        {
            resource.reload(true);   // discard the new template
            status.status.status = SAVE_FAILED;
        }
        else
            return UpdateResult(UPDATE_OK);
    }

    if (msgParent)
        displayUpdateError(msgParent, ERR_TEMPLATE, status);
    return status.status;
}

/******************************************************************************
* Modify an active (non-archived) alarm in a resource.
* The new event must have a different event ID from the old one.
*/
UpdateResult modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::modifyEvent:" << oldEvent.id();

    UpdateStatusData status;
    Resource resource = Resources::resource(oldEvent.resourceId());
    if (!newEvent.isValid())
    {
        deleteEvent(oldEvent, resource, true);
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
        if (!ResourcesCalendar::modifyEvent(oldId, newEvent))
            status.status = UPDATE_FAILED;
        else
        {
            if (!resource.save(&status.status.message))
            {
                resource.reload(true);   // retrieve the pre-update version of the event
                status.status.status = SAVE_FAILED;
            }
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
* Update an active (non-archived) alarm in its resource.
* The new event will have the same event ID as the old one.
* The event is not updated in KOrganizer, since this function is called when an
* existing alarm is rescheduled (due to recurrence or deferral).
*/
UpdateResult updateEvent(KAEvent& event, QWidget* msgParent, bool archiveOnDelete, bool saveIfReadOnly)
{
    qCDebug(KALARM_LOG) << "KAlarm::updateEvent:" << event.id();

    if (!event.isValid())
    {
        Resource resource;
        deleteEvent(event, resource, archiveOnDelete);
        return UpdateResult(UPDATE_OK);
    }

    // Update the event in the resource.
    return ::updateEvent(event, ERR_MODIFY, msgParent, saveIfReadOnly);
}

/******************************************************************************
* Update a template in its resource.
*/
UpdateResult updateTemplate(KAEvent& event, QWidget* msgParent)
{
    return ::updateEvent(event, ERR_TEMPLATE, msgParent, true);
}

/******************************************************************************
* Delete alarms from a resource.
* If the events are archived, the events' IDs are changed to archived IDs if necessary.
*/
UpdateResult deleteEvent(KAEvent& event, Resource& resource, bool archive, QWidget* msgParent, bool showKOrgErr)
{
    QList<KAEvent> events(1, event);
    return deleteEvents(events, resource, archive, msgParent, showKOrgErr);
}

UpdateResult deleteEvents(QList<KAEvent>& events, Resource& resource, bool archive, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::deleteEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
#if ENABLE_RTC_WAKE_FROM_SUSPEND
    bool deleteWakeFromSuspendAlarm = false;
    const QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
#endif
    QSet<Resource> resources;   // resources which events have been deleted from
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
                Resource archResource;
                KAEvent ev(*event);
                addArchivedEvent(ev, archResource);  // this changes the event ID to an archived ID
            }
        }
        Resource res = resource;
        if (ResourcesCalendar::deleteEvent(*event, res, false))   // don't save calendar after deleting
            resources.insert(res);
        else
            status.appendFailed(i);

#if ENABLE_RTC_WAKE_FROM_SUSPEND
        if (id == wakeFromSuspendId)
            deleteWakeFromSuspendAlarm = true;
#endif

        // Remove "Don't show error messages again" for this alarm
        setDontShowErrors(EventId(*event));
    }

    if (!resource.isValid()  &&  resources.size() == 1)
        resource = *resources.constBegin();

    if (status.failedCount())
        status.setError(status.failedCount() == events.count() ? UPDATE_FAILED : UPDATE_ERROR, status.failedCount());
    if (status.failedCount() < events.count())
    {
        QString msg;
        for (Resource res : resources)
            if (!res.save(&msg))
            {
                res.reload(true);    // retrieve the events which couldn't be deleted
                status.setError(SAVE_FAILED, status.failedCount(), msg);
            }
    }
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_DELETE, status, showKOrgErr);

#if ENABLE_RTC_WAKE_FROM_SUSPEND
    // Remove any wake-from-suspend scheduled for a deleted alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);
#endif

    return status.status;
}

/******************************************************************************
* Delete templates from the calendar file.
*/
UpdateResult deleteTemplates(const KAEvent::List& events, QWidget* msgParent)
{
    int count = events.count();
    qCDebug(KALARM_LOG) << "KAlarm::deleteTemplates:" << count;
    if (!count)
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    QSet<Resource> resources;   // resources which events have been deleted from
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        // Update the window lists
        // Delete the template from the calendar file
        const KAEvent* event = events[i];
        Resource resource;
        if (ResourcesCalendar::deleteEvent(*event, resource, false))   // don't save calendar after deleting
            resources.insert(resource);
        else
            status.appendFailed(i);
    }

    if (status.failedCount())
        status.setError(status.failedCount() == events.count() ? UPDATE_FAILED : UPDATE_ERROR, status.failedCount());
    if (status.failedCount() < events.count())
    {
        QString msg;
        for (Resource res : resources)
            if (!res.save(&msg))
            {
                res.reload(true);    // retrieve the templates which couldn't be deleted
                status.status.message = msg;
                status.setError(SAVE_FAILED, status.failedCount(), msg);
            }
    }

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
    if (DisplayCalendar::open())
        DisplayCalendar::deleteEvent(eventID, true);   // save calendar after deleting
}

/******************************************************************************
* Undelete archived alarms.
* The archive bit is set to ensure that they get re-archived if deleted again.
* Parameters:
*   calendar - the active alarms calendar to restore the alarms into, or null
*              to use the default way of determining the active alarm calendar.
*   ineligibleIndexes - will be filled in with the indexes to any ineligible events.
*/
UpdateResult reactivateEvent(KAEvent& event, Resource& resource, QWidget* msgParent, bool showKOrgErr)
{
    QList<int> ids;
    QList<KAEvent> events(1, event);
    return reactivateEvents(events, ids, resource, msgParent, showKOrgErr);
}

UpdateResult reactivateEvents(QList<KAEvent>& events, QList<int>& ineligibleIndexes,
                              Resource& resource, QWidget* msgParent, bool showKOrgErr)
{
    qCDebug(KALARM_LOG) << "KAlarm::reactivateEvents:" << events.count();
    ineligibleIndexes.clear();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
    if (!resource.isValid())
        resource = Resources::destination(CalEvent::ACTIVE, msgParent);
    if (!resource.isValid())
    {
        qCDebug(KALARM_LOG) << "KAlarm::reactivateEvents: No calendar";
        status.setError(UPDATE_FAILED, events.count());
    }
    else
    {
        int count = 0;
        const KADateTime now = KADateTime::currentUtcDateTime();
        QList<KAEvent> eventsToDelete;
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            // Delete the event from the archived resource
            KAEvent* event = &events[i];
            if (event->category() != CalEvent::ARCHIVED
            ||  !event->occursAfter(now, true))
            {
                ineligibleIndexes += i;
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
            if (!ResourcesCalendar::addEvent(newevent, resource, msgParent, ResourcesCalendar::UseEventId))
            {
                status.appendFailed(i);
                continue;
            }
            if (newev->copyToKOrganizer())
            {
                UpdateResult st = sendToKOrganizer(*newev);    // tell KOrganizer to show the event
                status.korgUpdate(st);
            }

            if (ResourcesCalendar::event(EventId(*event)).isValid())  // no error if event doesn't exist in archived resource
                eventsToDelete.append(*event);
            events[i] = newevent;
        }

        if (status.failedCount())
            status.setError(status.failedCount() == events.count() ? UPDATE_FAILED : UPDATE_ERROR, status.failedCount());
        if (status.failedCount() < events.count())
        {
            if (!resource.save(&status.status.message))
            {
                resource.reload(true);
                status.updateError(SAVE_FAILED, count);
            }
            else
            {
                // Delete the events from the archived resources. It's not too
                // important whether the resources are saved successfully after
                // the deletion, so allow them to be saved automatically.
                for (const KAEvent& event : eventsToDelete)
                {
                    Resource res = Resources::resource(event.resourceId());
                    if (!ResourcesCalendar::deleteEvent(event, res, false))
                        status.setError(UPDATE_ERROR);
                }
            }
        }
    }
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_REACTIVATE, status, showKOrgErr);
    return status.status;
}

/******************************************************************************
* Enable or disable alarms.
* The new events will have the same event IDs as the old ones.
*/
UpdateResult enableEvents(QList<KAEvent>& events, bool enable, QWidget* msgParent)
{
    qCDebug(KALARM_LOG) << "KAlarm::enableEvents:" << events.count();
    if (events.isEmpty())
        return UpdateResult(UPDATE_OK);
    UpdateStatusData status;
#if ENABLE_RTC_WAKE_FROM_SUSPEND
    bool deleteWakeFromSuspendAlarm = false;
    const QString wakeFromSuspendId = checkRtcWakeConfig().value(0);
#endif
    QSet<ResourceId> resourceIds;   // resources whose events have been updated
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        KAEvent* event = &events[i];
        if (event->category() == CalEvent::ACTIVE
        &&  enable != event->enabled())
        {
            event->setEnabled(enable);

#if ENABLE_RTC_WAKE_FROM_SUSPEND
            if (!enable  &&  event->id() == wakeFromSuspendId)
                deleteWakeFromSuspendAlarm = true;
#endif

            // Update the event in the calendar file
            const KAEvent newev = ResourcesCalendar::updateEvent(*event);
            if (!newev.isValid())
            {
                qCCritical(KALARM_LOG) << "KAlarm::enableEvents: Error updating event in calendar:" << event->id();
                status.appendFailed(i);
            }
            else
            {
                resourceIds.insert(event->resourceId());
                ResourcesCalendar::disabledChanged(newev);

                // If we're disabling a display alarm, close any message display
                if (!enable  &&  (event->actionTypes() & KAEvent::Action::Display))
                {
                    MessageDisplay* win = MessageDisplay::findEvent(EventId(*event));
                    delete win;
                }
            }
        }
    }

    if (status.failedCount())
        status.setError(status.failedCount() == events.count() ? UPDATE_FAILED : UPDATE_ERROR, status.failedCount());
    if (status.failedCount() < events.count())
    {
        QString msg;
        for (ResourceId id : resourceIds)
        {
            Resource res = Resources::resource(id);
            if (!res.save(&msg))
            {
                // Don't reload resource after failed save. It's better to
                // keep the new enabled status of the alarms at least until
                // KAlarm is restarted.
                status.setError(SAVE_FAILED, status.failedCount(), msg);
            }
        }
    }
    if (status.status != UPDATE_OK  &&  msgParent)
        displayUpdateError(msgParent, ERR_ADD, status);

#if ENABLE_RTC_WAKE_FROM_SUSPEND
    // Remove any wake-from-suspend scheduled for a disabled alarm
    if (deleteWakeFromSuspendAlarm  &&  !wakeFromSuspendId.isEmpty())
        cancelRtcWake(msgParent, wakeFromSuspendId);
#endif

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
    const Resource resource = Resources::getStandard(CalEvent::ARCHIVED, true);
    if (!resource.isValid())
        return;
    QList<KAEvent> events = ResourcesCalendar::events(resource);
    for (int i = 0;  i < events.count();  )
    {
        if (purgeDays  &&  events.at(i).createdDateTime().date() >= cutoff)
            events.remove(i);
        else
            ++i;
    }
    if (!events.isEmpty())
        ResourcesCalendar::purgeEvents(events);   // delete the events and save the calendar
}

/******************************************************************************
* Return whether an event is read-only.
*/
bool eventReadOnly(const QString& eventId)
{
    KAEvent event;
    const Resource resource = Resources::resourceForEvent(eventId, event);
    return !event.isValid()  ||  event.isReadOnly()
       ||  !resource.isWritable(event.category());
}

/******************************************************************************
* Display an error message about an error when saving an event.
* If 'model' is non-null, the AlarmListModel* which it points to is used; if
* that is null, it is created.
*/
QList<KAEvent> getSortedActiveEvents(QObject* parent, AlarmListModel** model)
{
    AlarmListModel* mdl = nullptr;
    if (!model)
        model = &mdl;
    if (!*model)
    {
        *model = DataModel::createAlarmListModel(parent);
        (*model)->setEventTypeFilter(CalEvent::ACTIVE);
        (*model)->sort(AlarmListModel::TimeColumn);
    }
    int count = (*model)->rowCount();
    QList<KAEvent> result;
    result.reserve(count);
    for (int i = 0;  i < count;  ++i)
    {
        const KAEvent event = (*model)->event(i);
        if (event.enabled()  &&  !event.expired())
            result += event;
    }
    return result;
}

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool importAlarms(Resource& resource, QWidget* parent)
{
    qCDebug(KALARM_LOG) << "KAlarm::importAlarms" << resource.displayId();
    // Use KFileCustomDialog to allow files' mime types to be determined by
    // both file name and content, instead of QFileDialog which only looks at
    // files' names. This is needed in particular when importing an old KAlarm
    // calendar directory, in order to list the calendar files within it, since
    // each calendar file name is simply the UID of the event within it, without
    // a .ics extension.
    AutoQPointer<KFileCustomDialog> dlg = new KFileCustomDialog(lastImportUrl, parent);
    dlg->setWindowTitle(i18nc("@title:window", "Import Calendar Files"));
    KFileWidget* widget = dlg->fileWidget();
    widget->setOperationMode(KFileWidget::Opening);
    widget->setMode(KFile::Files | KFile::ExistingOnly);
    widget->setMimeFilter({QStringLiteral("text/calendar")});
    dlg->setWindowModality(Qt::WindowModal);
    dlg->exec();
    if (!dlg)
        return false;
    const QList<QUrl> urls = widget->selectedUrls();
    if (urls.isEmpty())
        return false;
    lastImportUrl = urls[0].adjusted(QUrl::RemoveFilename);

    const CalEvent::Types alarmTypes = resource.isValid() ? resource.alarmTypes() : CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE;

    // Read all the selected calendar files and extract their alarms.
    QHash<CalEvent::Type, QList<KAEvent>> events;
    for (const QUrl& url : urls)
    {
        if (!url.isValid())
        {
            qCDebug(KALARM_LOG) << "KAlarm::importAlarms: Invalid URL";
            continue;
        }
        qCDebug(KALARM_LOG) << "KAlarm::importAlarms:" << url.toDisplayString();
        importCalendarFile(url, alarmTypes, true, parent, events);
    }
    if (events.isEmpty())
        return false;

    // Add the alarms to the destination resource.
    bool success = true;
    for (auto it = events.constBegin();  it != events.constEnd();  ++it)
    {
        Resource res;
        if (resource.isValid())
            res = resource;
        else
            res = Resources::destination(it.key());

        for (const KAEvent& event : std::as_const(it.value()))
        {
            if (!res.addEvent(event))
                success = false;
        }
    }
    return success;
}

/******************************************************************************
* Export all selected alarms to an external calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully exported
*       = false if any alarms failed to be exported.
*/
bool exportAlarms(const QList<KAEvent>& events, QWidget* parent)
{
    bool append;
    QString file = FileDialog::getSaveFileName(lastExportUrl,
                                               QStringLiteral("*.ics|%1").arg(i18nc("@item:inlistbox File type selection filter", "Calendar Files")),
                                               parent, i18nc("@title:window", "Choose Export Calendar"),
                                               &append);
    if (file.isEmpty())
        return false;
    const QUrl url = QUrl::fromLocalFile(file);
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "KAlarm::exportAlarms: Invalid URL" << url;
        return false;
    }
    lastExportUrl = url.adjusted(QUrl::RemoveFilename);
    qCDebug(KALARM_LOG) << "KAlarm::exportAlarms:" << url.toDisplayString();

    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(calendar, file));
    if (append  &&  !calStorage->load())
    {
        auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatDetail::StatDefaultDetails);
        KJobWidgets::setWindow(statJob, parent);
        statJob->exec();
        KFileItem fi(statJob->statResult(), url);
        if (fi.size())
        {
            qCCritical(KALARM_LOG) << "KAlarm::exportAlarms: Error loading calendar file" << file << "for append";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                xi18nc("@info", "Error loading calendar to append to:<nl/><filename>%1</filename>", url.toDisplayString()));
            return false;
        }
    }
    KACalendar::setKAlarmVersion(calendar);

    // Add the alarms to the calendar
    bool success = true;
    bool exported = false;
    for (const KAEvent& event : events)
    {
        Event::Ptr kcalEvent(new Event);
        const CalEvent::Type type = event.category();
        const QString id = CalEvent::uid(kcalEvent->uid(), type);
        kcalEvent->setUid(id);
        event.updateKCalEvent(kcalEvent, KAEvent::UidAction::Ignore);
        if (calendar->addEvent(kcalEvent))
            exported = true;
        else
            success = false;
    }

    if (exported)
    {
        // One or more alarms have been exported to the calendar.
        // Save the calendar to file.
        QTemporaryFile* tempFile = nullptr;
        bool local = url.isLocalFile();
        if (!local)
        {
            tempFile = new QTemporaryFile;
            file = tempFile->fileName();
        }
        calStorage->setFileName(file);
        calStorage->setSaveFormat(new ICalFormat);
        if (!calStorage->save())
        {
            qCCritical(KALARM_LOG) << "KAlarm::exportAlarms:" << file << ": failed";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                xi18nc("@info", "Failed to save new calendar to:<nl/><filename>%1</filename>", url.toDisplayString()));
            success = false;
        }
        else if (!local)
        {
            QFile qFile(file);
            qFile.open(QIODevice::ReadOnly);
            auto uploadJob = KIO::storedPut(&qFile, url, -1);
            KJobWidgets::setWindow(uploadJob, parent);
            if (!uploadJob->exec())
            {
                qCCritical(KALARM_LOG) << "KAlarm::exportAlarms:" << file << ": upload failed";
                KAMessageBox::error(MainWindow::mainMainWindow(),
                                    xi18nc("@info", "Cannot upload new calendar to:<nl/><filename>%1</filename>", url.toDisplayString()));
                success = false;
            }
        }
        delete tempFile;
    }
    return success;
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
    editNewAlarm(type, QDate(), parent);
}
void editNewAlarm(EditAlarmDlg::Type type, const QDate& startDate, QWidget* parent)
{
    EditAlarmDlg* editDlg = EditAlarmDlg::create(false, type, parent);
    if (editDlg)
        execNewAlarmDlg(editDlg, startDate);
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
        case KAEvent::SubAction::Message:
        case KAEvent::SubAction::File:
            type = EditAlarmDlg::DISPLAY;
            setAction = true;
            break;
        case KAEvent::SubAction::Command:
            type = EditAlarmDlg::COMMAND;
            break;
        case KAEvent::SubAction::Email:
            type = EditAlarmDlg::EMAIL;
            break;
        case KAEvent::SubAction::Audio:
            type = EditAlarmDlg::AUDIO;
            break;
        default:
            return;
    }
    EditAlarmDlg* editDlg = EditAlarmDlg::create(false, type, parent);
    if (!editDlg)
        return;
    if (setAction  ||  text)
        editDlg->setAction(action, *text);
    execNewAlarmDlg(editDlg);
}

/******************************************************************************
* Execute a New Alarm dialog, optionally either presetting it to the supplied
* event, or setting the action and text.
*/
void editNewAlarm(const KAEvent& preset, QWidget* parent)
{
    editNewAlarm(preset, QDate(), parent);
}
void editNewAlarm(const KAEvent& preset, const QDate& startDate, QWidget* parent)
{
    EditAlarmDlg* editDlg = EditAlarmDlg::create(false, preset, true, parent);
    if (editDlg)
        execNewAlarmDlg(editDlg, startDate);
}

/******************************************************************************
* Common code for editNewAlarm() variants.
*/
void execNewAlarmDlg(EditAlarmDlg* editDlg, const QDate& startDate)
{
    if (startDate.isValid())
    {
        KADateTime defaultTime = editDlg->time();
        defaultTime.setDate(startDate);
        editDlg->setTime(defaultTime);
    }

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
    Resource resource;
    editDlg->getEvent(event, resource);

    // Add the alarm to the displayed lists and to the calendar file
    const UpdateResult status = addEvent(event, resource, editDlg);
    switch (status.status)
    {
        case UPDATE_FAILED:
        case SAVE_FAILED:
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
    Undo::saveAdd(event, resource);

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
        KAEvent templateEvent = ResourcesCalendar::templateEvent(templateName);
        if (templateEvent.isValid())
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
    ::editNewTemplate(type, KAEvent(), parent);
}

/******************************************************************************
* Create a new template, based on an existing event or template.
*/
void editNewTemplate(const KAEvent& preset, QWidget* parent)
{
    ::editNewTemplate(EditAlarmDlg::Type(0), preset, parent);
}

#if ENABLE_RTC_WAKE_FROM_SUSPEND
/******************************************************************************
* Check the config as to whether there is a wake-on-suspend alarm pending, and
* if so, delete it from the config if it has expired.
* If 'checkExists' is true, the config entry will only be returned if the
* event exists.
* Reply = config entry: [0] = event's resource ID,
*                       [1] = event ID,
*                       [2] = trigger time (int64 seconds since epoch).
*       = empty list if none or expired.
*/
QStringList checkRtcWakeConfig(bool checkEventExists)
{
    KConfigGroup config(KSharedConfig::openConfig(), "General");
    const QStringList params = config.readEntry("RtcWake", QStringList());
    if (params.count() == 3  &&  params[2].toLongLong() > KADateTime::currentUtcDateTime().toSecsSinceEpoch())
    {
        if (checkEventExists)
        {
            Resource resource = Resources::resource(params[0].toLongLong());
            if (!resource.event(params[1]).isValid())
                return {};
        }
        return params;                   // config entry is valid
    }
    if (!params.isEmpty())
    {
        config.deleteEntry("RtcWake");   // delete the expired config entry
        config.sync();
    }
    return {};
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
        QTimer::singleShot(0, Private::instance(), &Private::cancelRtcWake);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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
    parent->window()->winId();
    action.setParentWindow(parent->window()->windowHandle());
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
#endif // ENABLE_RTC_WAKE_FROM_SUSPEND

} // namespace KAlarm
namespace
{

/******************************************************************************
* Update an event in its resource.
*/
KAlarm::UpdateResult updateEvent(KAEvent& event, KAlarm::UpdateError err, QWidget* msgParent, bool saveIfReadOnly)
{
    UpdateStatusData status;
    const KAEvent newEvent = ResourcesCalendar::updateEvent(event, saveIfReadOnly);
    if (!newEvent.isValid())
        status.status = KAlarm::UPDATE_FAILED;
    else
    {
        Resource resource = Resources::resource(event.resourceId());
        if ((saveIfReadOnly || !resource.readOnly())
        &&  !resource.save(&status.status.message))
        {
            resource.reload(true);   // retrieve the pre-update version of the event
            status.status.status = KAlarm::SAVE_FAILED;
        }
    }
    if (status.status != KAlarm::UPDATE_OK)
    {
        if (msgParent)
            displayUpdateError(msgParent, err, status);
    }
    return status.status;
}

/******************************************************************************
* Create a new template.
* 'preset' is non-null to base it on an existing event or template; otherwise,
* the alarm type is set to 'type'.
*/
void editNewTemplate(EditAlarmDlg::Type type, const KAEvent& preset, QWidget* parent)
{
    if (Resources::enabledResources(CalEvent::TEMPLATE, true).isEmpty())
    {
        KAMessageBox::error(parent, i18nc("@info", "You must enable a template calendar to save the template in"));
        return;
    }
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg;
    if (preset.isValid())
        editDlg = EditAlarmDlg::create(true, preset, true, parent);
    else
        editDlg = EditAlarmDlg::create(true, type, parent);
    if (editDlg  &&  editDlg->exec() == QDialog::Accepted)
    {
        KAEvent event;
        Resource resource;
        editDlg->getEvent(event, resource);

        // Add the template to the displayed lists and to the calendar file
        KAlarm::addTemplate(event, resource, editDlg);
        Undo::saveAdd(event, resource);
    }
}

} // namespace
namespace KAlarm
{

/******************************************************************************
* Open the Edit Alarm dialog to edit the specified alarm.
* If the alarm is read-only or archived, the dialog is opened read-only.
*/
void editAlarm(KAEvent& event, QWidget* parent)
{
    if (event.expired()  ||  eventReadOnly(event.id()))
    {
        viewAlarm(event, parent);
        return;
    }
    const EventId id(event);
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
    if (editDlg  &&  editDlg->exec() == QDialog::Accepted)
    {
        if (!ResourcesCalendar::event(id).isValid())
        {
            // Event has been deleted while the user was editing the alarm,
            // so treat it as a new alarm.
            PrivateNewAlarmDlg().accept(editDlg);
            return;
        }
        KAEvent newEvent;
        Resource resource;
        bool changeDeferral = !editDlg->getEvent(newEvent, resource);

        // Update the event in the displays and in the calendar file
        const Undo::Event undo(event, resource);
        if (changeDeferral)
        {
            // The only change has been to an existing deferral
            if (updateEvent(newEvent, editDlg, true, true) != UPDATE_OK)   // keep the same event ID
                return;   // failed to save event
        }
        else
        {
            const UpdateResult status = modifyEvent(event, newEvent, editDlg);
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
    KAEvent event = ResourcesCalendar::event(id, true);
    if (!event.isValid())
    {
        if (id.resourceId() != -1)
            qCWarning(KALARM_LOG) << "KAlarm::editAlarmById: Event ID not found, or duplicated:" << eventID;
        else
            qCWarning(KALARM_LOG) << "KAlarm::editAlarmById: Event ID not found:" << eventID;
        return false;
    }
    if (eventReadOnly(event.id()))
    {
        qCCritical(KALARM_LOG) << "KAlarm::editAlarmById:" << eventID << ": read-only";
        return false;
    }
    switch (event.category())
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
void editTemplate(const KAEvent& event, QWidget* parent)
{
    if (eventReadOnly(event.id()))
    {
        // The template is read-only, so make the dialogue read-only.
        // Use AutoQPointer to guard against crash on application exit while
        // the dialogue is still open. It prevents double deletion (both on
        // deletion of parent, and on return from this function).
        AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
        if (editDlg)
            editDlg->exec();
        return;
    }
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(true, event, false, parent, EditAlarmDlg::RES_USE_EVENT_ID);
    if (editDlg  &&  editDlg->exec() == QDialog::Accepted)
    {
        KAEvent newEvent;
        Resource resource;
        editDlg->getEvent(newEvent, resource);
        const QString id = event.id();
        newEvent.setEventId(id);
        newEvent.setResourceId(event.resourceId());

        // Update the event in the displays and in the calendar file
        const Undo::Event undo(event, resource);
        updateTemplate(newEvent, editDlg);
        Undo::saveEdit(undo, newEvent);
    }
}

/******************************************************************************
* Open the Edit Alarm dialog to view the specified alarm (read-only).
*/
void viewAlarm(const KAEvent& event, QWidget* parent)
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(false, event, false, parent, EditAlarmDlg::RES_PROMPT, true);
    if (editDlg)
        editDlg->exec();
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog invoked by the Edit button
* in an alarm message window.
* Updates the alarm calendar and closes the dialog.
*/
void updateEditedAlarm(EditAlarmDlg* editDlg, KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "KAlarm::updateEditedAlarm";
    KAEvent newEvent;
    Resource res;
    editDlg->getEvent(newEvent, res);

    // Update the displayed lists and the calendar file
    UpdateResult status;
    if (ResourcesCalendar::event(EventId(event)).isValid())
    {
        // The old alarm hasn't expired yet, so replace it
        const Undo::Event undo(event, resource);
        status = modifyEvent(event, newEvent, editDlg);
        Undo::saveEdit(undo, newEvent);
    }
    else
    {
        // The old event has expired, so simply create a new one
        status = addEvent(newEvent, resource, editDlg);
        Undo::saveAdd(newEvent, resource);
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
QList<KAEvent> templateList()
{
    QList<KAEvent> templates;
    const bool includeCmdAlarms = ShellProcess::authorised();
    const QList<KAEvent> events = ResourcesCalendar::events(CalEvent::TEMPLATE);
    for (const KAEvent& event : events)
    {
        if (includeCmdAlarms  ||  !(event.actionTypes() & KAEvent::Action::Command))
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
    if (event  &&  event->actionTypes() == KAEvent::Action::Email
    &&  Preferences::emailAddress().isEmpty())
        KAMessageBox::information(parent, xi18nc("@info Please set the 'From' email address...",
                                                "<para>%1</para><para>Please set it in the Configuration dialog.</para>", KAMail::i18n_NeedFromEmailAddress()));

    if (!theApp()->alarmsEnabled())
    {
        if (KAMessageBox::warningYesNo(parent, xi18nc("@info", "<para>Alarms are currently disabled.</para><para>Do you want to enable alarms now?</para>"),
                                       QString(), KGuiItem(i18nc("@action:button", "Enable")), KGuiItem(i18nc("@action:button", "Keep Disabled")),
                                       QStringLiteral("EditEnableAlarms"))
                        == KMessageBox::ButtonCode::PrimaryAction)
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
        QList<Resource> resources = Resources::enabledResources();
        for (Resource& resource : resources)
            resource.reload();

        // Close any message displays for alarms which are now disabled
        const QList<KAEvent> events = ResourcesCalendar::events(CalEvent::ACTIVE);
        for (const KAEvent& event : events)
        {
            if (!event.enabled()  &&  (event.actionTypes() & KAEvent::Action::Display))
            {
                MessageDisplay* win = MessageDisplay::findEvent(EventId(event));
                delete win;
            }
        }

        MainWindow::refresh();
        refreshAlarmsQueued = false;
    }
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
        return {};
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.resourceId()).arg(eventId.eventId());
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
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.resourceId()).arg(eventId.eventId());
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
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + ALARM_OPTS_FILE);
    KConfigGroup group(&config, DONT_SHOW_ERRORS_GROUP);
    const QString id = QStringLiteral("%1:%2").arg(eventId.resourceId()).arg(eventId.eventId());
    QStringList tags = group.readEntry(id, QStringList());
    if (tags.indexOf(tag) < 0)
    {
        tags += tag;
        group.writeEntry(id, tags);
        group.sync();
    }
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
bool convertTimeString(const QByteArray& timeString, KADateTime& dateTime, const KADateTime& defaultDt, bool allowTZ)
{
#define MAX_DT_LEN 19
    int i = timeString.indexOf(' ');
    if (i > MAX_DT_LEN  ||  (i >= 0 && !allowTZ))
        return false;
    QString zone = (i >= 0) ? QString::fromLatin1(timeString.mid(i)) : QString();
    char timeStr[MAX_DT_LEN+1];
    strncpy(timeStr, timeString.left(i >= 0 ? i : MAX_DT_LEN).constData(), MAX_DT_LEN);
    timeStr[MAX_DT_LEN] = '\0';
    int dt[5] = { -1, -1, -1, -1, -1 };
    char* s;
    char* end;
    bool noTime;
    // Get the minute value
    if ((s = strchr(timeStr, ':')) == nullptr)
        noTime = true;
    else
    {
        noTime = false;
        *s++ = 0;
        dt[4] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[4] >= 60)
            return false;
        // Get the hour value
        if ((s = strrchr(timeStr, '-')) == nullptr)
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
        if ((s = strrchr(timeStr, '-')) == nullptr)
            s = timeStr;
        else
            *s++ = 0;
        dt[2] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
            return false;
        if (s != timeStr)
        {
            // Get the month value
            if ((s = strrchr(timeStr, '-')) == nullptr)
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
        dateTime = applyTimeZone(zone, date, time, false, defaultDt);
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
                date.setDate(dt[0],
                            (dt[1] < 0 ? defaultDt.date().month() : dt[1]),
                            (dt[2] < 0 ? defaultDt.date().day() : dt[2]));
            }
            else
                date.setDate(2000, 1, 1);  // temporary substitute for date
        }
        dateTime = applyTimeZone(zone, date, time, true, defaultDt);
        if (!dateTime.isValid())
            return false;
        if (dt[0] < 0)
        {
            // Some or all of the date was omitted.
            // Use the current date in the specified time zone as default.
            const KADateTime now = KADateTime::currentDateTime(dateTime.timeSpec());
            date = dateTime.date();
            date.setDate(now.date().year(),
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
* Extract dragged and dropped Akonadi RFC822 message data.
*/
bool dropAkonadiEmail(const QMimeData* data, QUrl& url, AlarmText& alarmText)
{
    alarmText.clear();
    const QList<QUrl> urls = data->urls();
    if (urls.isEmpty())
        url = QUrl();
    else
    {
        url  = urls.at(0);
        AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
        if (akonadiPlugin)
        {
            KAEvent::EmailId emailId;
            KMime::Message::Ptr message = akonadiPlugin->fetchAkonadiEmail(url, emailId);
            if (message)
            {
                // It's an email held in Akonadi
                if (message->hasContent())
                    alarmText = DragDrop::kMimeEmailToAlarmText(*message, emailId);
                return true;
            }
        }
    }
    return false;
}

/******************************************************************************
* Convert a time zone specifier string and apply it to a given date and/or time.
* The time zone specifier is a system time zone name, e.g. "Europe/London" or
* "UTC". If no time zone is specified, it defaults to the local time zone.
* If 'defaultDt' is valid, it supplies the time spec and default date.
*/
KADateTime applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                         bool haveTime, const KADateTime& defaultDt)
{
    bool error = false;
    KADateTime::Spec spec = KADateTime::LocalZone;
    const QString zone = tzstring.trimmed();
    if (!zone.isEmpty())
    {
        if (zone == QLatin1String("UTC"))
            spec = KADateTime::UTC;
        else
        {
            const QTimeZone tz(zone.toLatin1());
            error = !tz.isValid();
            if (!error)
                spec = tz;
        }
    }
    else if (defaultDt.isValid())
        spec = defaultDt.timeSpec();

    KADateTime result;
    if (!error)
    {
        if (!date.isValid())
        {
            // It's a time without a date
            if (defaultDt.isValid())
                   result = KADateTime(defaultDt.date(), time, spec);
            else if (spec == KADateTime::LocalZone)
                result = KADateTime(KADateTime::currentLocalDate(), time, spec);
        }
        else if (haveTime)
        {
            // It's a date and time
            result = KADateTime(date, time, spec);
        }
        else
        {
            // It's a date without a time
            result = KADateTime(date, spec);
        }
    }
    return result;
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
        if (convertTimeString(newTime, dt, KADateTime::realCurrentLocalDateTime(), true))
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
        if (!status.status.message.isEmpty())
            KAMessageBox::detailedError(parent, errmsg, status.status.message);
        else
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
    event.updateKCalEvent(kcalEvent, KAEvent::UidAction::Ignore);
    // Change the event ID to avoid duplicating the same unique ID as the original event
    const QString uid = uidKOrganizer(event.id());
    kcalEvent->setUid(uid);
    kcalEvent->clearAlarms();
    QString userEmail;
    switch (event.actionTypes())
    {
        case KAEvent::Action::Display:
        case KAEvent::Action::Command:
        case KAEvent::Action::DisplayCommand:
            kcalEvent->setSummary(event.cleanText());
            userEmail = Preferences::emailAddress();
            break;
        case KAEvent::Action::Email:
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
        case KAEvent::Action::Audio:
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
    AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
    if (!akonadiPlugin)
        return KAlarm::UpdateResult(KAlarm::UPDATE_KORG_ERR);

    const QString newID = uidKOrganizer(eventID);
    akonadiPlugin->deleteEvent(KORG_MIME_TYPE, QString(), newID);
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

#include "moc_functions_p.cpp"

// vim: et sw=4:
