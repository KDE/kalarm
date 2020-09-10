/*
 *  messagedisplay.cpp  -  base class to display an alarm or error message
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagedisplayhelper.h"
#include "messagewindow.h"
#include "messagenotification.h"

#include "deferdlg.h"
#include "displaycalendar.h"
#include "functions.h"
#include "kalarmapp.h"
#include "resourcescalendar.h"
#include "resources/resources.h"
#include "lib/messagebox.h"

#include <KLocalizedString>

using namespace KAlarmCal;
using namespace KCalendarCore;


bool MessageDisplay::mRedisplayed = false;

/******************************************************************************
* Create a new instance of a MessageDisplay, the derived class being dependent
* on 'event.notify()'.
*/
MessageDisplay* MessageDisplay::create(const KAEvent& event, const KAAlarm& alarm, int flags)
{
    if (event.notify())
        return new MessageNotification(event, alarm, flags);
    else
        return new MessageWindow(event, alarm, flags);
}

/******************************************************************************
* Show an error message about the execution of an alarm.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
void MessageDisplay::showError(const KAEvent& event, const DateTime& alarmDateTime,
                               const QStringList& errmsgs, const QString& dontShowAgain)
{
    if (MessageDisplayHelper::shouldShowError(event, errmsgs, dontShowAgain))
    {
        MessageDisplay* disp;
        if (event.notify())
            disp = new MessageNotification(event, alarmDateTime, errmsgs, dontShowAgain);
        else
            disp = new MessageWindow(event, alarmDateTime, errmsgs, dontShowAgain);
        disp->showDisplay();
    }
}

/******************************************************************************
* Constructors.
*/
MessageDisplay::MessageDisplay()
    : mHelper(new MessageDisplayHelper(this))
{
}

MessageDisplay::MessageDisplay(const KAEvent& event, const KAAlarm& alarm, int flags)
    : mHelper(new MessageDisplayHelper(this, event, alarm, flags))
{
}

MessageDisplay::MessageDisplay(const KAEvent& event, const DateTime& alarmDateTime,
                               const QStringList& errmsgs, const QString& dontShowAgain)
    : mHelper(new MessageDisplayHelper(this, event, alarmDateTime, errmsgs, dontShowAgain))
{
}

MessageDisplay::MessageDisplay(MessageDisplayHelper* helper)
    : mHelper(helper)
{
    mHelper->setParent(this);
}

MessageDisplay::~MessageDisplay()
{
    delete mHelper;
    if (!instanceCount(true))
        theApp()->quitIf();   // no visible displays remain - check whether to quit
}

/******************************************************************************
* Redisplay alarms which were being shown when the program last exited.
* Normally, these alarms will have been displayed by session restoration, but
* if the program crashed or was killed, we can redisplay them here so that
* they won't be lost.
*/
void MessageDisplay::redisplayAlarms()
{
    if (mRedisplayed)
        return;
    qCDebug(KALARM_LOG) << "MessageDisplay::redisplayAlarms";
    mRedisplayed = true;
    if (DisplayCalendar::isOpen())
    {
        KAEvent event;
        Resource resource;
        const Event::List kcalEvents = DisplayCalendar::kcalEvents();
        for (const Event::Ptr& kcalEvent : kcalEvents)
        {
            bool showDefer, showEdit;
            reinstateFromDisplaying(kcalEvent, event, resource, showEdit, showDefer);
            const EventId eventId(event);
            if (findEvent(eventId))
                qCDebug(KALARM_LOG) << "MessageDisplay::redisplayAlarms: Message display already exists:" << eventId;
            else
            {
                // This event should be displayed, but currently isn't being
                const KAAlarm alarm = event.convertDisplayingAlarm();
                if (alarm.type() == KAAlarm::INVALID_ALARM)
                {
                    qCCritical(KALARM_LOG) << "MessageDisplay::redisplayAlarms: Invalid alarm: id=" << eventId;
                    continue;
                }
                qCDebug(KALARM_LOG) << "MessageDisplay::redisplayAlarms:" << eventId;
                const bool login = alarm.repeatAtLogin();
                const int flags = NoReschedule | (login ? NoDefer : 0) | NoInitView;
                MessageDisplay* d = create(event, alarm, flags);
                MessageDisplayHelper* h = d->mHelper;
                h->mResource = resource;
                const bool rw = resource.isWritable(event.category());
                h->mShowEdit = rw ? showEdit : false;
                h->mNoDefer  = (rw && !login) ? !showDefer : true;
                d->setUpDisplay();
                d->showDisplay();
            }
        }
    }
}

/******************************************************************************
* Retrieves the event with the current ID from the displaying calendar file,
* or if not found there, from the archive calendar. 'resource' is set to the
* resource which originally contained the event, or invalid if not known.
*/
bool MessageDisplay::retrieveEvent(const EventId& evntId, KAEvent& event, Resource& resource, bool& showEdit, bool& showDefer)
{
    const QString eventId = evntId.eventId();
    const Event::Ptr kcalEvent = DisplayCalendar::kcalEvent(CalEvent::uid(eventId, CalEvent::DISPLAYING));
    if (!reinstateFromDisplaying(kcalEvent, event, resource, showEdit, showDefer))
    {
        // The event isn't in the displaying calendar.
        // Try to retrieve it from the archive calendar.
        KAEvent ev;
        Resource archiveRes = Resources::getStandard(CalEvent::ARCHIVED);
        if (archiveRes.isValid())
            ev = ResourcesCalendar::event(EventId(archiveRes.id(), CalEvent::uid(eventId, CalEvent::ARCHIVED)));
        if (!ev.isValid())
            return false;
        event = ev;
        event.setArchive();     // ensure that it gets re-archived if it's saved
        event.setCategory(CalEvent::ACTIVE);
        if (eventId != event.id())
            qCCritical(KALARM_LOG) << "MessageDisplay::retrieveEvent: Wrong event ID";
        event.setEventId(eventId);
        resource  = Resource();
        showEdit  = false;
        showDefer = false;
        qCDebug(KALARM_LOG) << "MessageDisplay::retrieveEvent:" << event.id() << ": success";
    }
    return true;
}

/******************************************************************************
* Retrieves the displayed event from the calendar file, or if not found there,
* from the displaying calendar. 'resource' is set to the resource which
* originally contained the event.
*/
bool MessageDisplay::reinstateFromDisplaying(const Event::Ptr& kcalEvent, KAEvent& event, Resource& resource, bool& showEdit, bool& showDefer)
{
    if (!kcalEvent)
        return false;
    ResourceId resourceId;
    event.reinstateFromDisplaying(kcalEvent, resourceId, showEdit, showDefer);
    event.setResourceId(resourceId);
    resource = Resources::resource(resourceId);
    qCDebug(KALARM_LOG) << "MessageDisplay::reinstateFromDisplaying:" << EventId(event) << ": success";
    return true;
}

/******************************************************************************
* Display the main window, with the appropriate alarm selected.
*/
void MessageDisplay::displayMainWindow()
{
    KAlarm::displayMainWindowSelected(mEventId().eventId());
}

MessageDisplay::DeferDlgData::~DeferDlgData()
{
    delete dlg;
}

/******************************************************************************
* Create a defer message dialog.
*/
MessageDisplay::DeferDlgData* MessageDisplay::createDeferDlg(bool displayClosing)
{
    DeferAlarmDlg* dlg = new DeferAlarmDlg(KADateTime::currentDateTime(Preferences::timeSpec()).addSecs(60), mDateTime().isDateOnly(), false, displayParent());
    dlg->setObjectName(QStringLiteral("DeferDlg"));    // used by LikeBack
    dlg->setDeferMinutes(mDefaultDeferMinutes() > 0 ? mDefaultDeferMinutes() : Preferences::defaultDeferTime());
    dlg->setLimit(mEvent());
    DeferDlgData* data = new DeferDlgData(dlg);
    data->eventId      = mEventId();
    data->alarmType    = mAlarmType();
    data->commandError = mCommandError();
    data->displayOpen  = !displayClosing;
    return data;
}

/******************************************************************************
* Display a defer message dialog.
*/
void MessageDisplay::executeDeferDlg(DeferDlgData* data)
{
    if (data->dlg->exec() == QDialog::Accepted)
    {
        const DateTime dateTime  = data->dlg->getDateTime();
        const int      delayMins = data->dlg->deferMinutes();
        // Fetch the up-to-date alarm from the calendar. Note that it could have
        // changed since it was displayed.
        KAEvent event;
        if (!data->eventId.isEmpty())
            event = ResourcesCalendar::event(data->eventId);
        if (event.isValid())
        {
            // The event still exists in the active calendar
            qCDebug(KALARM_LOG) << "MessageDisplay::executeDeferDlg: Deferring event" << data->eventId;
            KAEvent newev(event);
            newev.defer(dateTime, (data->alarmType & KAAlarm::REMINDER_ALARM), true);
            newev.setDeferDefaultMinutes(delayMins);
            KAlarm::updateEvent(newev, data->dlg, true);
            if (data->displayOpen)
            {
                if (newev.deferred())
                    mNoPostAction() = true;
            }
        }
        else
        {
            // Try to retrieve the event from the displaying or archive calendars
            Resource resource;   // receives the event's original resource, if known
            KAEvent event2;
            bool showEdit, showDefer;
            if (!retrieveEvent(data->eventId, event2, resource, showEdit, showDefer))
            {
                // The event doesn't exist any more !?!, so recurrence data,
                // flags, and more, have been lost.
                KAMessageBox::error(displayParent(), xi18nc("@info", "<para>Cannot defer alarm:</para><para>Alarm not found.</para>"));
                if (data->displayOpen)
                {
                    raiseDisplay();
                    enableDeferButton(false);
                    enableEditButton(false);
                }
                delete data;
                return;
            }
            qCDebug(KALARM_LOG) << "MessageDisplay::executeDeferDlg: Deferring retrieved event" << data->eventId;
            event2.defer(dateTime, (data->alarmType & KAAlarm::REMINDER_ALARM), true);
            event2.setDeferDefaultMinutes(delayMins);
            event2.setCommandError(data->commandError);
            // Add the event back into the calendar file, retaining its ID
            // and not updating KOrganizer.
            KAlarm::addEvent(event2, resource, data->dlg, KAlarm::USE_EVENT_ID);
            if (data->displayOpen)
            {
                if (event2.deferred())
                    mNoPostAction() = true;
            }
            // Finally delete it from the archived calendar now that it has
            // been reactivated.
            event2.setCategory(CalEvent::ARCHIVED);
            Resource res;
            KAlarm::deleteEvent(event2, res, false);
        }
        if (theApp()->wantShowInSystemTray())
        {
            // Alarms are to be displayed only if the system tray icon is running,
            // so start it if necessary so that the deferred alarm will be shown.
            theApp()->displayTrayIcon(true);
        }
        if (data->displayOpen)
        {
            mHelper->mNoCloseConfirm = true;   // allow window to close without confirmation prompt
            closeDisplay();
        }
    }
    else
    {
        if (data->displayOpen)
            raiseDisplay();
    }
    delete data;
}

// vim: et sw=4:
