/*
 *  messagedisplay.cpp  -  base class to display an alarm or error message
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagedisplayhelper.h"
#include "messagewindow.h"
//#include "messagenotification.h"

#include "displaycalendar.h"
#include "kalarmapp.h"
#include "resourcescalendar.h"
#include "resources/resources.h"

using namespace KAlarmCal;
using namespace KCalendarCore;


bool MessageDisplay::mRedisplayed = false;

MessageDisplay::MessageDisplay(const QString& displayType)
    : mDisplayType(displayType)
    , mHelper(new MessageDisplayHelper(this))
{
}

MessageDisplay::MessageDisplay(const QString& displayType, const KAEvent* event, const KAAlarm& alarm, int flags)
    : mDisplayType(displayType)
    , mHelper(new MessageDisplayHelper(this, event, alarm, flags))
{
}

MessageDisplay::MessageDisplay(const QString& displayType, const KAEvent* event,
                               const DateTime& alarmDateTime,
                               const QStringList& errmsgs, const QString& dontShowAgain)
    : mDisplayType(displayType)
    , mHelper(new MessageDisplayHelper(this, event, alarmDateTime, errmsgs, dontShowAgain))
{
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
                const int flags = NO_RESCHEDULE | (login ? NO_DEFER : 0) | NO_INIT_VIEW;
                MessageDisplay* d = new MessageWindow(&event, alarm, flags);
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
bool MessageDisplay::retrieveEvent(KAEvent& event, Resource& resource, bool& showEdit, bool& showDefer)
{
    const QString eventId = mHelper->mEventId.eventId();
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

// vim: et sw=4:
