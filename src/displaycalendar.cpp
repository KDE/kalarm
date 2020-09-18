/*
 *  displaycalendar.cpp  -  KAlarm display calendar file access
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "displaycalendar.h"

#include "mainwindow.h"
#include "preferences.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>

#include <KLocalizedString>

#include <QStandardPaths>
#include <QDir>

using namespace KCalendarCore;
using namespace KAlarmCal;


namespace
{
const QString displayCalendarName = QStringLiteral("displaying.ics");
}

bool                            DisplayCalendar::mInitialised {false};
KAEvent::List                   DisplayCalendar::mEventList;
QHash<QString, KAEvent*>        DisplayCalendar::mEventMap;
KCalendarCore::FileStorage::Ptr DisplayCalendar::mCalendarStorage;
QString                         DisplayCalendar::mDisplayCalPath;
QString                         DisplayCalendar::mDisplayICalPath;
DisplayCalendar::CalType        DisplayCalendar::mCalType;
bool                            DisplayCalendar::mOpen {false};


/******************************************************************************
* Initialise the display alarm calendar.
* It is user-specific, and contains details of alarms which are currently being
* displayed to that user and which have not yet been acknowledged;
*/
void DisplayCalendar::initialise()
{
    QDir dir;
    dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    mDisplayCalPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + displayCalendarName;
    mDisplayICalPath = mDisplayCalPath;
    mDisplayICalPath.replace(QStringLiteral("\\.vcs$"), QStringLiteral(".ics"));
    mCalType = (mDisplayCalPath == mDisplayICalPath) ? LOCAL_ICAL : LOCAL_VCAL;    // is the calendar in ICal or VCal format?
    mInitialised = true;
}

/******************************************************************************
* Terminate access to the display calendar.
*/
void DisplayCalendar::terminate()
{
    close();
    mInitialised = false;
}

/******************************************************************************
* Open the calendar if not already open, and load it into memory.
*/
bool DisplayCalendar::open()
{
    if (mOpen)
        return true;

    // Open the display calendar.
    qCDebug(KALARM_LOG) << "DisplayCalendar::open:" << mDisplayCalPath;
    if (!mCalendarStorage)
    {
        MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
        mCalendarStorage = FileStorage::Ptr(new FileStorage(calendar, mDisplayCalPath));
    }

    // Check for file's existence, assuming that it does exist when uncertain,
    // to avoid overwriting it.
    QFileInfo fi(mDisplayCalPath);
    if (!fi.exists()  ||  !fi.isFile()  ||  load() == 0)
    {
        // The calendar file doesn't yet exist, or it's zero length, so create a new one
        if (saveCal(mDisplayICalPath))
            load();
    }

    if (!mOpen)
    {
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }
    return mOpen;
}

/******************************************************************************
* Load the calendar into memory.
* Reply = 1 if success
*       = 0 if zero-length file exists.
*       = -1 if failure to load calendar file
*       = -2 if instance uninitialised.
*/
int DisplayCalendar::load()
{
    // Load the display calendar.
    if (!mCalendarStorage)
        return -2;

    qCDebug(KALARM_LOG) << "DisplayCalendar::load:" << mDisplayCalPath;
    if (!mCalendarStorage->load())
    {
        // Load error. Check if the file is zero length
        QFileInfo fi(mDisplayCalPath);
        if (fi.exists()  &&  !fi.size())
            return 0;     // file is zero length

        qCCritical(KALARM_LOG) << "DisplayCalendar::load: Error loading calendar file '" << mDisplayCalPath <<"'";
        KAMessageBox::error(MainWindow::mainMainWindow(),
                            xi18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mDisplayCalPath));
        // load() could have partially populated the calendar, so clear it out
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
        mOpen = false;
        return -1;
    }
    QString versionString;
    KACalendar::updateVersion(mCalendarStorage, versionString);   // convert events to current KAlarm format for when calendar is saved
    updateKAEvents();

    mOpen = true;
    return 1;
}

/******************************************************************************
* Save the calendar.
*/
bool DisplayCalendar::save()
{
    return saveCal();
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool DisplayCalendar::saveCal(const QString& newFile)
{
    if (!mCalendarStorage)
        return false;
    if (!mOpen  &&  newFile.isEmpty())
        return false;

    qCDebug(KALARM_LOG) << "DisplayCalendar::saveCal:" << "\"" << newFile;
    QString saveFilename = newFile.isEmpty() ? mDisplayCalPath : newFile;
    if (mCalType == LOCAL_VCAL  &&  newFile.isNull())
        saveFilename = mDisplayICalPath;
    mCalendarStorage->setFileName(saveFilename);
    mCalendarStorage->setSaveFormat(new ICalFormat);
    if (!mCalendarStorage->save())
    {
        qCCritical(KALARM_LOG) << "DisplayCalendar::saveCal: Saving" << saveFilename << "failed.";
        KAMessageBox::error(MainWindow::mainMainWindow(),
                            xi18nc("@info", "Failed to save calendar to <filename>%1</filename>", mDisplayICalPath));
        return false;
    }

    if (mCalType == LOCAL_VCAL)
    {
        // The file was in vCalendar format, but has now been saved in iCalendar format.
        mDisplayCalPath = mDisplayICalPath;
        mCalType = LOCAL_ICAL;
    }
    return true;
}

/******************************************************************************
* Close display calendar file at program exit.
*/
void DisplayCalendar::close()
{
    if (mCalendarStorage)
    {
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }

    // Flag as closed now to prevent removeKAEvents() doing silly things
    // when it's called again
    mOpen = false;

    // Events list should be empty, but just in case...
    for (KAEvent* event : qAsConst(mEventList))
    {
        mEventMap.remove(event->id());
        delete event;
    }
    mEventList.clear();
}

/******************************************************************************
* Create a KAEvent instance corresponding to each KCalendarCore::Event in the
* display calendar, and store them in the event map in place of the old set.
* Called after the display calendar has completed loading.
*/
void DisplayCalendar::updateKAEvents()
{
    qCDebug(KALARM_LOG) << "DisplayCalendar::updateKAEvents";
    for (KAEvent* event : qAsConst(mEventList))
    {
        mEventMap.remove(event->id());
        delete event;
    }
    mEventList.clear();
    Calendar::Ptr cal = mCalendarStorage->calendar();
    if (!cal)
        return;

    const Event::List kcalevents = cal->rawEvents();
    for (const Event::Ptr& kcalevent : kcalevents)
    {
        if (kcalevent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEvent* event = new KAEvent(kcalevent);
        if (!event->isValid())
        {
            qCWarning(KALARM_LOG) << "DisplayCalendar::updateKAEvents: Ignoring unusable event" << kcalevent->uid();
            delete event;
            continue;    // ignore events without usable alarms
        }
        mEventList += event;
        mEventMap[kcalevent->uid()] = event;
    }
}

/******************************************************************************
* Add the specified event to the calendar.
* Reply = true if 'evnt' was written to the calendar. 'evnt' is updated.
*       = false if an error occurred, in which case 'evnt' is unchanged.
*/
bool DisplayCalendar::addEvent(KAEvent& evnt)
{
    if (!mOpen)
        return false;
    qCDebug(KALARM_LOG) << "DisplayCalendar::addEvent:" << evnt.id();
    // Check that the event type is valid for the calendar
    const CalEvent::Type type = evnt.category();
    if (type != CalEvent::DISPLAYING)
        return false;

    Event::Ptr kcalEvent(new Event);
    KAEvent* event = new KAEvent(evnt);
    QString id = event->id();
    if (id.isEmpty())
        id = kcalEvent->uid();
    id = CalEvent::uid(id, type);   // include the alarm type tag in the ID
    kcalEvent->setUid(id);
    event->setEventId(id);
    event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);

    bool ok = false;
    bool remove = false;
    if (!mEventMap.contains(event->id()))
    {
        mEventList += event;
        mEventMap[event->id()] = event;
        ok = mCalendarStorage->calendar()->addEvent(kcalEvent);
        remove = !ok;
    }
    if (!ok)
    {
        if (remove)
        {
            // Adding to mCalendar failed, so undo DisplayCalendar::addEvent()
            mEventMap.remove(event->id());
            int i = mEventList.indexOf(event);
            if (i >= 0)
                mEventList.remove(i);
        }
        delete event;
        return false;
    }
    evnt = *event;
    if (remove)
        delete event;
    return true;
}

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
bool DisplayCalendar::deleteEvent(const QString& eventID, bool saveit)
{
    if (mOpen)
    {
        Event::Ptr kcalEvent;
        if (mCalendarStorage)
            kcalEvent = mCalendarStorage->calendar()->event(eventID);   // display calendar

        // Make a copy of the ID QString, since the supplied reference might be
        // destructed when the event is deleted below.
        const QString id = eventID;

        auto it = mEventMap.find(id);
        if (it != mEventMap.end())
        {
            KAEvent* ev = it.value();
            mEventMap.erase(it);
            int i = mEventList.indexOf(ev);
            if (i >= 0)
                mEventList.remove(i);
            delete ev;
        }

        CalEvent::Type status = CalEvent::EMPTY;
        if (kcalEvent)
        {
            status = CalEvent::status(kcalEvent);
            mCalendarStorage->calendar()->deleteEvent(kcalEvent);
        }

        if (status != CalEvent::EMPTY)
        {
            if (saveit)
                return saveCal();
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Return the event with the specified ID.
* This method is for the display calendar only.
*/
Event::Ptr DisplayCalendar::kcalEvent(const QString& uniqueID)
{
    if (!mCalendarStorage)
        return Event::Ptr();
    return mCalendarStorage->calendar()->event(uniqueID);
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
* This method is for the display calendar only.
* Optionally the event type can be filtered, using an OR of event types.
*/
Event::List DisplayCalendar::kcalEvents(CalEvent::Type type)
{
    Event::List list;
    if (!mCalendarStorage)
        return list;
    list = mCalendarStorage->calendar()->rawEvents();
    for (int i = 0;  i < list.count();  )
    {
        Event::Ptr event = list.at(i);
        if (event->alarms().isEmpty()
        ||  (type != CalEvent::EMPTY  &&  !(type & CalEvent::status(event)))
        ||  !KAEvent(event).isValid())
            list.remove(i);
        else
            ++i;
    }
    return list;
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start times of all date-only alarms' recurrences.
*/
void DisplayCalendar::adjustStartOfDay()
{
    if (!isValid())
        return;
    KAEvent::adjustStartOfDay(mEventList);
}

// vim: et sw=4:
