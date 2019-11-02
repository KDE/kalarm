/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
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

#include "alarmcalendar.h"

#include "kalarm.h"
#include "collectionmodel.h"
#include "filedialog.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>

#include <KLocalizedString>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <kfileitem.h>
#include <KSharedConfig>

#include <QTemporaryFile>
#include <QStandardPaths>
#include <QFileDialog>

using namespace Akonadi;
using namespace KCalendarCore;
using namespace KAlarmCal;

static KACalendar::Compat fix(const KCalendarCore::FileStorage::Ptr&);

static const QString displayCalendarName = QStringLiteral("displaying.ics");
static const Collection::Id DISPLAY_COL_ID = -1;   // resource ID used for displaying calendar

AlarmCalendar* AlarmCalendar::mResourcesCalendar = nullptr;
AlarmCalendar* AlarmCalendar::mDisplayCalendar = nullptr;
QUrl           AlarmCalendar::mLastImportUrl;


/******************************************************************************
* Initialise the alarm calendars, and ensure that their file names are different.
* There are 2 calendars:
*  1) A resources calendar containing the active alarms, archived alarms and
*     alarm templates;
*  2) A user-specific one which contains details of alarms which are currently
*     being displayed to that user and which have not yet been acknowledged;
* Reply = true if success, false if calendar name error.
*/
bool AlarmCalendar::initialiseCalendars()
{
    QDir dir;
    dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    QString displayCal = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + displayCalendarName;
    AkonadiModel::instance();
    Preferences::setBackend(Preferences::Akonadi);
    Preferences::self()->save();
    mResourcesCalendar = new AlarmCalendar();
    mDisplayCalendar = new AlarmCalendar(displayCal, CalEvent::DISPLAYING);
    KACalendar::setProductId(KALARM_NAME, KALARM_VERSION);
    CalFormat::setApplication(QStringLiteral(KALARM_NAME), QString::fromLatin1(KACalendar::icalProductId()));
    return true;
}

/******************************************************************************
* Terminate access to all calendars.
*/
void AlarmCalendar::terminateCalendars()
{
    delete mResourcesCalendar;
    mResourcesCalendar = nullptr;
    delete mDisplayCalendar;
    mDisplayCalendar = nullptr;
}

/******************************************************************************
* Return the display calendar, opening it first if necessary.
*/
AlarmCalendar* AlarmCalendar::displayCalendarOpen()
{
    if (mDisplayCalendar->open())
        return mDisplayCalendar;
    qCCritical(KALARM_LOG) << "AlarmCalendar::displayCalendarOpen: Open error";
    return nullptr;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
KAEvent* AlarmCalendar::getEvent(const EventId& eventId)
{
    if (eventId.eventId().isEmpty())
        return nullptr;
    return mResourcesCalendar->event(eventId);
}

/******************************************************************************
* Constructor for the resources calendar.
*/
AlarmCalendar::AlarmCalendar()
    : mCalType(RESOURCES)
    , mEventType(CalEvent::EMPTY)
{
    AkonadiModel* model = AkonadiModel::instance();
    connect(model, &AkonadiModel::eventsAdded, this, &AlarmCalendar::slotEventsAdded);
    connect(model, &AkonadiModel::eventsToBeRemoved, this, &AlarmCalendar::slotEventsToBeRemoved);
    connect(model, &AkonadiModel::eventChanged, this, &AlarmCalendar::slotEventChanged);
    connect(model, &AkonadiModel::collectionsPopulated, this, &AlarmCalendar::slotCollectionsPopulated);
    connect(Resources::instance(), &Resources::settingsChanged, this, &AlarmCalendar::slotResourceSettingsChanged);
}

/******************************************************************************
* Constructor for a calendar file.
*/
AlarmCalendar::AlarmCalendar(const QString& path, CalEvent::Type type)
    : mEventType(type)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
        case CalEvent::DISPLAYING:
            break;
        default:
            Q_ASSERT(false);   // invalid event type for a calendar
            break;
    }
    mUrl = QUrl::fromUserInput(path, QString(), QUrl::AssumeLocalFile);
    QString icalPath = path;
    icalPath.replace(QStringLiteral("\\.vcs$"), QStringLiteral(".ics"));
    mICalUrl = QUrl::fromUserInput(icalPath, QString(), QUrl::AssumeLocalFile);
    mCalType = (path == icalPath) ? LOCAL_ICAL : LOCAL_VCAL;    // is the calendar in ICal or VCal format?
}

AlarmCalendar::~AlarmCalendar()
{
    close();
}

/******************************************************************************
* Check whether the calendar is open.
*/
bool AlarmCalendar::isOpen()
{
    return mOpen;
}

/******************************************************************************
* Open the calendar if not already open, and load it into memory.
*/
bool AlarmCalendar::open()
{
    if (isOpen())
        return true;
    if (mCalType == RESOURCES)
    {
        mOpen = true;
    }
    else
    {
        if (!mUrl.isValid())
            return false;

        qCDebug(KALARM_LOG) << "AlarmCalendar::open:" << mUrl.toDisplayString();
        if (!mCalendarStorage)
        {
            MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
            mCalendarStorage = FileStorage::Ptr(new FileStorage(calendar));
        }

        // Check for file's existence, assuming that it does exist when uncertain,
        // to avoid overwriting it.
        auto statJob = KIO::stat(mUrl, KIO::StatJob::SourceSide, 2);
        KJobWidgets::setWindow(statJob, MainWindow::mainMainWindow());
        if (!statJob->exec() ||  load() == 0)
        {
            // The calendar file doesn't yet exist, or it's zero length, so create a new one
            bool created = false;
            if (mICalUrl.isLocalFile())
                created = saveCal(mICalUrl.toLocalFile());
            else
            {
                QTemporaryFile tmpFile;
                tmpFile.setAutoRemove(false);
                tmpFile.open();
                created = saveCal(tmpFile.fileName());
            }
            if (created)
                load();
        }
    }
    if (!mOpen)
    {
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }
    return isOpen();
}

/******************************************************************************
* Load the calendar into memory.
* Reply = 1 if success
*       = 0 if zero-length file exists.
*       = -1 if failure to load calendar file
*       = -2 if instance uninitialised.
*/
int AlarmCalendar::load()
{
    if (mCalType == RESOURCES)
    {
    }
    else
    {
        if (!mCalendarStorage)
            return -2;

        QString filename;
        qCDebug(KALARM_LOG) << "AlarmCalendar::load:" << mUrl.toDisplayString();
        if (!mUrl.isLocalFile())
        {
            auto getJob = KIO::storedGet(mUrl);
            KJobWidgets::setWindow(getJob, MainWindow::mainMainWindow());
            if (!getJob->exec())
            {
                qCCritical(KALARM_LOG) << "AlarmCalendar::load: Download failure";
                KAMessageBox::error(MainWindow::mainMainWindow(),
                                    xi18nc("@info", "Cannot download calendar: <filename>%1</filename>", mUrl.toDisplayString()));
                return -1;
            }
            QTemporaryFile tmpFile;
            tmpFile.setAutoRemove(false);
            tmpFile.write(getJob->data());
            qCDebug(KALARM_LOG) << "--- Downloaded to" << tmpFile.fileName();
            filename = tmpFile.fileName();
        }
        else
            filename = mUrl.toLocalFile();
        mCalendarStorage->calendar()->setTimeZone(Preferences::timeSpecAsZone());
        mCalendarStorage->setFileName(filename);
        if (!mCalendarStorage->load())
        {
            // Check if the file is zero length
            if (mUrl.isLocalFile())
            {
                auto statJob = KIO::stat(KIO::upUrl(mUrl));
                KJobWidgets::setWindow(statJob, MainWindow::mainMainWindow());
                statJob->exec();
                KFileItem fi(statJob->statResult(), mUrl);
                if (!fi.size())
                    return 0;     // file is zero length
            }

            qCCritical(KALARM_LOG) << "AlarmCalendar::load: Error loading calendar file '" << filename <<"'";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                xi18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mUrl.toDisplayString()));
            // load() could have partially populated the calendar, so clear it out
            mCalendarStorage->calendar()->close();
            mCalendarStorage->calendar().clear();
            mCalendarStorage.clear();
            mOpen = false;
            return -1;
        }
        if (!mLocalFile.isEmpty())
        {
            if (mLocalFile.startsWith(QDir::tempPath()))
                QFile::remove(mLocalFile);
        }
        mLocalFile = filename;
        fix(mCalendarStorage);   // convert events to current KAlarm format for when calendar is saved
        updateDisplayKAEvents();
    }
    mOpen = true;
    return 1;
}

/******************************************************************************
* Reload the calendar file into memory.
*/
bool AlarmCalendar::reload()
{
    if (mCalType == RESOURCES)
        return true;
    if (!mCalendarStorage)
        return false;
    {
        qCDebug(KALARM_LOG) << "AlarmCalendar::reload:" << mUrl.toDisplayString();
        close();
        return open();
    }
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool AlarmCalendar::saveCal(const QString& newFile)
{
    if (mCalType == RESOURCES)
        return true;
    if (!mCalendarStorage)
        return false;
    {
        if (!mOpen && newFile.isNull())
            return false;

        qCDebug(KALARM_LOG) << "AlarmCalendar::saveCal:" << "\"" << newFile << "\"," << mEventType;
        QString saveFilename = newFile.isNull() ? mLocalFile : newFile;
        if (mCalType == LOCAL_VCAL  &&  newFile.isNull()  &&  mUrl.isLocalFile())
            saveFilename = mICalUrl.toLocalFile();
        mCalendarStorage->setFileName(saveFilename);
        mCalendarStorage->setSaveFormat(new ICalFormat);
        if (!mCalendarStorage->save())
        {
            qCCritical(KALARM_LOG) << "AlarmCalendar::saveCal: Saving" << saveFilename << "failed.";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                xi18nc("@info", "Failed to save calendar to <filename>%1</filename>", mICalUrl.toDisplayString()));
            return false;
        }

        if (!mICalUrl.isLocalFile())
        {
            QFile file(saveFilename);
            file.open(QIODevice::ReadOnly);
            auto putJob = KIO::storedPut(&file, mICalUrl, -1);
            KJobWidgets::setWindow(putJob, MainWindow::mainMainWindow());
            if (!putJob->exec())
            {
                qCCritical(KALARM_LOG) << "AlarmCalendar::saveCal:" << saveFilename << "upload failed.";
                KAMessageBox::error(MainWindow::mainMainWindow(),
                                    xi18nc("@info", "Cannot upload calendar to <filename>%1</filename>", mICalUrl.toDisplayString()));
                return false;
            }
        }

        if (mCalType == LOCAL_VCAL)
        {
            // The file was in vCalendar format, but has now been saved in iCalendar format.
            mUrl  = mICalUrl;
            mCalType = LOCAL_ICAL;
        }
        Q_EMIT calendarSaved(this);
    }

    mUpdateSave = false;
    return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
    if (mCalType != RESOURCES)
    {
        if (!mLocalFile.isEmpty())
        {
            if (mLocalFile.startsWith(QDir::tempPath())) // removes it only if it IS a temporary file
                QFile::remove(mLocalFile);
            mLocalFile = QStringLiteral("");
        }
    }
    // Flag as closed now to prevent removeKAEvents() doing silly things
    // when it's called again
    mOpen = false;
    if (mCalendarStorage)
    {
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }
    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
        removeKAEvents(mResourceMap.begin().key(), true, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
}

/******************************************************************************
* Create a KAEvent instance corresponding to each KCalendarCore::Event in the
* display calendar, and store them in the event map in place of the old set.
* Called after the display calendar has completed loading.
*/
void AlarmCalendar::updateDisplayKAEvents()
{
    if (mCalType == RESOURCES)
        return;
    qCDebug(KALARM_LOG) << "AlarmCalendar::updateDisplayKAEvents";
    const Collection::Id key = DISPLAY_COL_ID;
    KAEvent::List& events = mResourceMap[key];
    for (KAEvent* event : events)
    {
        mEventMap.remove(EventId(key, event->id()));
        delete event;
    }
    events.clear();
    mEarliestAlarm[key] = nullptr;
    Calendar::Ptr cal = mCalendarStorage->calendar();
    if (!cal)
        return;

    const Event::List kcalevents = cal->rawEvents();
    for (Event::Ptr kcalevent : kcalevents)
    {
        if (kcalevent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEvent* event = new KAEvent(kcalevent);
        if (!event->isValid())
        {
            qCWarning(KALARM_LOG) << "AlarmCalendar::updateDisplayKAEvents: Ignoring unusable event" << kcalevent->uid();
            delete event;
            continue;    // ignore events without usable alarms
        }
        event->setCollectionId(key);
        events += event;
        mEventMap[EventId(key, kcalevent->uid())] = event;
    }

}

/******************************************************************************
* Delete a calendar and all its KAEvent instances of specified alarm types from
* the lists.
* Called after the calendar is deleted or alarm types have been disabled, or
* the AlarmCalendar is closed.
*/
void AlarmCalendar::removeKAEvents(Collection::Id key, bool closing, CalEvent::Types types)
{
    bool removed = false;
    ResourceMap::Iterator rit = mResourceMap.find(key);
    if (rit != mResourceMap.end())
    {
        KAEvent::List retained;
        KAEvent::List& events = rit.value();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            bool remove = (event->collectionId() != key);
            if (remove)
            {
                if (key != DISPLAY_COL_ID)
                    qCCritical(KALARM_LOG) << "AlarmCalendar::removeKAEvents: Event" << event->id() << ", resource" << event->collectionId() << "Indexed under resource" << key;
            }
            else
                remove = event->category() & types;
            if (remove)
            {
                mEventMap.remove(EventId(key, event->id()));
                delete event;
                removed = true;
            }
            else
                retained.push_back(event);
        }
        if (retained.empty())
            mResourceMap.erase(rit);
        else
            events.swap(retained);
    }
    if (removed)
    {
        mEarliestAlarm.remove(key);
        // Emit signal only if we're not in the process of closing the calendar
        if (!closing  &&  mOpen)
        {
            Q_EMIT earliestAlarmChanged();
            if (mHaveDisabledAlarms)
                checkForDisabledAlarms();
        }
    }
}

/******************************************************************************
* Called when the enabled or read-only status of a resource has changed.
* If the resource is now disabled, remove its events from the calendar.
*/
void AlarmCalendar::slotResourceSettingsChanged(Resource& resource, ResourceType::Changes change)
{
    if (change & ResourceType::Enabled)
    {
        // For each alarm type which has been disabled, remove the collection's
        // events from the map, but not from AkonadiModel.
        if (resource.isValid())
        {
            const CalEvent::Types enabled = resource.enabledTypes();
            const CalEvent::Types disabled = ~enabled & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
            removeKAEvents(resource.id(), false, disabled);
        }
    }
}

/******************************************************************************
* Called when all resources have been populated for the first time.
*/
void AlarmCalendar::slotCollectionsPopulated()
{
    // Now that all calendars have been processed, all repeat-at-login alarms
    // will have been triggered. Prevent any new or updated repeat-at-login
    // alarms (e.g. when they are edited by the user) triggering from now on.
    mIgnoreAtLogin = true;
}

/******************************************************************************
* Called when events have been added to AkonadiModel.
* Add corresponding KAEvent instances to those held by AlarmCalendar.
*/
void AlarmCalendar::slotEventsAdded(const AkonadiModel::EventList& events)
{
    for (const AkonadiModel::Event& event : events)
        slotEventChanged(event);
}

/******************************************************************************
* Called when an event has been changed in AkonadiModel.
* Change the corresponding KAEvent instance held by AlarmCalendar.
*/
void AlarmCalendar::slotEventChanged(const AkonadiModel::Event& event)
{
    Resource resource = AkonadiModel::instance()->resource(event.collection.id());
    if (!event.isConsistent())
    {
        qCCritical(KALARM_LOG) << "AlarmCalendar::slotEventChanged: Inconsistent AkonadiModel::Event: event:" << event.event.collectionId() << ", collection" << event.collection.id();
        return;
    }

    bool added = true;
    bool updated = false;
    KAEventMap::Iterator it = mEventMap.find(event.eventId());
    if (it != mEventMap.end())
    {
        // The event ID already exists - remove the existing event first
        KAEvent* storedEvent = it.value();
        if (event.event.category() == storedEvent->category())
        {
            // The existing event is the same type - update it in place
            *storedEvent = event.event;
            addNewEvent(resource, storedEvent, true);
            updated = true;
        }
        else
            delete storedEvent;
        added = false;
    }
    if (!updated)
        addNewEvent(resource, new KAEvent(event.event));

    if (event.event.category() == CalEvent::ACTIVE)
    {
        bool enabled = event.event.enabled();
        checkForDisabledAlarms(!enabled, enabled);
        if (!mIgnoreAtLogin  &&  added  &&  enabled  &&  event.event.repeatAtLogin())
            Q_EMIT atLoginEventAdded(event.event);
    }
}

/******************************************************************************
* Called when events are about to be removed from AkonadiModel.
* Remove the corresponding KAEvent instances held by AlarmCalendar.
*/
void AlarmCalendar::slotEventsToBeRemoved(const AkonadiModel::EventList& events)
{
    for (const AkonadiModel::Event& event : events)
    {
        if (!event.isConsistent())
            qCCritical(KALARM_LOG) << "AlarmCalendar::slotEventsToBeRemoved: Inconsistent AkonadiModel::Event: event:" << event.event.collectionId() << ", collection" << event.collection.id();
        else if (mEventMap.contains(event.eventId()))
        {
            Resource resource = AkonadiModel::instance()->resource(event.collection.id());
            deleteEventInternal(event.event, resource, false);
        }
    }
}

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool AlarmCalendar::importAlarms(QWidget* parent, Resource* resourceptr)
{
    if (mCalType != RESOURCES)
        return false;
    Resource nullresource;
    Resource& resource(resourceptr ? *resourceptr : nullresource);
    qCDebug(KALARM_LOG) << "AlarmCalendar::importAlarms";
    const QUrl url = QFileDialog::getOpenFileUrl(parent, QString(), mLastImportUrl,
                                                 QStringLiteral("%1 (*.vcs *.ics)").arg(i18nc("@info", "Calendar Files")));
    if (url.isEmpty())
    {
        qCCritical(KALARM_LOG) << "AlarmCalendar::importAlarms: Empty URL";
        return false;
    }
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "AlarmCalendar::importAlarms: Invalid URL";
        return false;
    }
    mLastImportUrl = url.adjusted(QUrl::RemoveFilename);
    qCDebug(KALARM_LOG) << "AlarmCalendar::importAlarms:" << url.toDisplayString();

    bool success = true;
    QString filename;
    bool local = url.isLocalFile();
    if (local)
    {
        filename = url.toLocalFile();
        if (!QFile::exists(filename))
        {
            qCDebug(KALARM_LOG) << "AlarmCalendar::importAlarms: File '" << url.toDisplayString() <<"' not found";
            KAMessageBox::error(parent, xi18nc("@info", "Could not load calendar <filename>%1</filename>.", url.toDisplayString()));
            return false;
        }
    }
    else
    {
        auto getJob = KIO::storedGet(url);
        KJobWidgets::setWindow(getJob, MainWindow::mainMainWindow());
        if (!getJob->exec())
        {
            qCCritical(KALARM_LOG) << "AlarmCalendar::importAlarms: Download failure";
            KAMessageBox::error(parent, xi18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.toDisplayString()));
            return false;
        }
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        tmpFile.write(getJob->data());
        tmpFile.seek(0);
        filename = tmpFile.fileName();
        qCDebug(KALARM_LOG) << "--- Downloaded to" << filename;
    }

    // Read the calendar and add its alarms to the current calendars
    MemoryCalendar::Ptr cal(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(cal, filename));
    success = calStorage->load();
    if (!success)
    {
        qCDebug(KALARM_LOG) << "AlarmCalendar::importAlarms: Error loading calendar '" << filename <<"'";
        KAMessageBox::error(parent, xi18nc("@info", "Could not load calendar <filename>%1</filename>.", url.toDisplayString()));
    }
    else
    {
        const KACalendar::Compat caltype = fix(calStorage);
        const CalEvent::Types wantedTypes = resource.alarmTypes();
        const Event::List events = cal->rawEvents();
        for (Event::Ptr event : events)
        {
            if (event->alarms().isEmpty()  ||  !KAEvent(event).isValid())
                continue;    // ignore events without alarms, or usable alarms
            CalEvent::Type type = CalEvent::status(event);
            if (type == CalEvent::TEMPLATE)
            {
                // If we know the event was not created by KAlarm, don't treat it as a template
                if (caltype == KACalendar::Incompatible)
                    type = CalEvent::ACTIVE;
            }
            Resource res;
            if (resource.isValid())
            {
                if (!(type & wantedTypes))
                    continue;
                res = resource;
            }
            else
            {
                switch (type)
                {
                    case CalEvent::ACTIVE:
                    case CalEvent::ARCHIVED:
                    case CalEvent::TEMPLATE:
                        break;
                    default:
                        continue;
                }
                res = CollectionControlModel::destination(type);
            }

            Event::Ptr newev(new Event(*event));

            // If there is a display alarm without display text, use the event
            // summary text instead.
            if (type == CalEvent::ACTIVE  &&  !newev->summary().isEmpty())
            {
                const Alarm::List& alarms = newev->alarms();
                for (Alarm::Ptr alarm : alarms)
                {
                    if (alarm->type() == Alarm::Display  &&  alarm->text().isEmpty())
                        alarm->setText(newev->summary());
                }
                newev->setSummary(QString());   // KAlarm only uses summary for template names
            }

            // Give the event a new ID and add it to the calendars
            newev->setUid(CalEvent::uid(CalFormat::createUniqueId(), type));
            if (!res.addEvent(KAEvent(newev)))
                success = false;
        }

    }
    if (!local)
        QFile::remove(filename);
    return success;
}

/******************************************************************************
* Export all selected alarms to an external calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully exported
*       = false if any alarms failed to be exported.
*/
bool AlarmCalendar::exportAlarms(const KAEvent::List& events, QWidget* parent)
{
    bool append;
    QString file = FileDialog::getSaveFileName(QUrl(QStringLiteral("kfiledialog:///exportalarms")),
                                               QStringLiteral("*.ics|%1").arg(i18nc("@info", "Calendar Files")),
                                               parent, i18nc("@title:window", "Choose Export Calendar"),
                                               &append);
    if (file.isEmpty())
        return false;
    const QUrl url = QUrl::fromLocalFile(file);
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "AlarmCalendar::exportAlarms: Invalid URL" << url;
        return false;
    }
    qCDebug(KALARM_LOG) << "AlarmCalendar::exportAlarms:" << url.toDisplayString();

    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(calendar, file));
    if (append  &&  !calStorage->load())
    {
        KIO::UDSEntry uds;
        auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, 2);
        KJobWidgets::setWindow(statJob, parent);
        statJob->exec();
        KFileItem fi(statJob->statResult(), url);
        if (fi.size())
        {
            qCCritical(KALARM_LOG) << "AlarmCalendar::exportAlarms: Error loading calendar file" << file << "for append";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                xi18nc("@info", "Error loading calendar to append to:<nl/><filename>%1</filename>", url.toDisplayString()));
            return false;
        }
    }
    KACalendar::setKAlarmVersion(calendar);

    // Add the alarms to the calendar
    bool success = true;
    bool exported = false;
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        const KAEvent* event = events[i];
        Event::Ptr kcalEvent(new Event);
        const CalEvent::Type type = event->category();
        const QString id = CalEvent::uid(kcalEvent->uid(), type);
        kcalEvent->setUid(id);
        event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
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
            qCCritical(KALARM_LOG) << "AlarmCalendar::exportAlarms:" << file << ": failed";
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
                qCCritical(KALARM_LOG) << "AlarmCalendar::exportAlarms:" << file << ": upload failed";
                KAMessageBox::error(MainWindow::mainMainWindow(),
                                    xi18nc("@info", "Cannot upload new calendar to:<nl/><filename>%1</filename>", url.toDisplayString()));
                success = false;
            }
        }
        delete tempFile;
    }
    calendar->close();
    return success;
}

/******************************************************************************
* Flag the start of a group of calendar update calls.
* The purpose is to avoid multiple calendar saves during a group of operations.
*/
void AlarmCalendar::startUpdate()
{
    ++mUpdateCount;
}

/******************************************************************************
* Flag the end of a group of calendar update calls.
* The calendar is saved if appropriate.
*/
bool AlarmCalendar::endUpdate()
{
    if (mUpdateCount > 0)
        --mUpdateCount;
    if (!mUpdateCount)
    {
        if (mUpdateSave)
            return saveCal();
    }
    return true;
}

/******************************************************************************
* Save the calendar, or flag it for saving if in a group of calendar update calls.
* Note that this method has no effect for Akonadi calendars.
*/
bool AlarmCalendar::save()
{
    if (mUpdateCount)
    {
        mUpdateSave = true;
        return true;
    }
    else
        return saveCal();
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge a list of archived events from the calendar.
*/
void AlarmCalendar::purgeEvents(const KAEvent::List& events)
{
    for (const KAEvent* event : events)
    {
        deleteEventInternal(*event);
    }
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
    saveCal();
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is an active event and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'event' (if non-null).
* 'event' is updated with the actual event ID.
* The event is added to 'resource' if specified; otherwise the default resource
* is used or the user is prompted, depending on policy. If 'noPrompt' is true,
* the user will not be prompted so that if no default resource is defined, the
* function will fail.
* Reply = true if 'event' was written to the calendar, in which case (not
*              Akonadi) ownership of 'event' is taken by the calendar. 'event'
*              is updated.
*       = false if an error occurred, in which case 'event' is unchanged.
*/
bool AlarmCalendar::addEvent(KAEvent& evnt, QWidget* promptParent, bool useEventID, Resource* resourceptr, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    if (!mOpen)
        return false;
    // Check that the event type is valid for the calendar
    Resource nullresource;
    Resource& resource(resourceptr ? *resourceptr : nullresource);
    qCDebug(KALARM_LOG) << "AlarmCalendar::addEvent:" << evnt.id() << ", resource" << resource.id();
    const CalEvent::Type type = evnt.category();
    if (type != mEventType)
    {
        switch (type)
        {
            case CalEvent::ACTIVE:
            case CalEvent::ARCHIVED:
            case CalEvent::TEMPLATE:
                if (mEventType == CalEvent::EMPTY)
                    break;
                // fall through to default
                Q_FALLTHROUGH();
            default:
                return false;
        }
    }

    Collection::Id key = resource.id();
    Event::Ptr kcalEvent((mCalType == RESOURCES) ? (Event*)nullptr : new Event);
    KAEvent* event = new KAEvent(evnt);
    QString id = event->id();
    if (type == CalEvent::ACTIVE)
    {
        if (id.isEmpty())
            useEventID = false;
        else if (!useEventID)
            id.clear();
    }
    else
        useEventID = true;
    if (id.isEmpty())
        id = (mCalType == RESOURCES) ? CalFormat::createUniqueId() : kcalEvent->uid();
    if (useEventID)
    {
        id = CalEvent::uid(id, type);
        if (kcalEvent)
            kcalEvent->setUid(id);
    }
    event->setEventId(id);
    bool ok = false;
    bool remove = false;
    if (mCalType == RESOURCES)
    {
        Resource res;
        if (resource.isEnabled(type))
            res = resource;
        else
        {
            res = CollectionControlModel::destination(type, promptParent, noPrompt, cancelled);
            if (!res.isValid())
            {
                const char* typeStr = (type == CalEvent::ACTIVE) ? "Active alarm" : (type == CalEvent::ARCHIVED) ? "Archived alarm" : "alarm Template";
                qCWarning(KALARM_LOG) << "AlarmCalendar::addEvent: Error! Cannot create" << typeStr << "(No default calendar is defined)";
            }
        }
        if (res.isValid())
        {
            // Don't add event to mEventMap yet - its Akonadi item id is not yet known.
            // It will be added once it is inserted into AkonadiModel.
            ok = res.addEvent(*event);
            remove = ok;   // if success, delete the local event instance on exit
            if (ok  &&  type == CalEvent::ACTIVE  &&  !event->enabled())
                checkForDisabledAlarms(true, false);
        }
    }
    else
    {
        // It's the display calendar
        event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
        key = DISPLAY_COL_ID;
        if (!mEventMap.contains(EventId(key, event->id())))
        {
            addNewEvent(Resource(), event);
            ok = mCalendarStorage->calendar()->addEvent(kcalEvent);
            remove = !ok;
        }
    }
    if (!ok)
    {
        if (remove)
        {
            // Adding to mCalendar failed, so undo AlarmCalendar::addEvent()
            mEventMap.remove(EventId(key, event->id()));
            KAEvent::List& events = mResourceMap[key];
            int i = events.indexOf(event);
            if (i >= 0)
                events.remove(i);
            if (mEarliestAlarm[key] == event)
                findEarliestAlarm(key);
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
* Internal method to add an already checked event to the calendar.
* mEventMap takes ownership of the KAEvent.
* If 'replace' is true, an existing event is being updated (NOTE: its category()
* must remain the same).
*/
void AlarmCalendar::addNewEvent(const Resource& resource, KAEvent* event, bool replace)
{
    const Collection::Id key = resource.id();
    event->setCollectionId(key);
    if (!replace)
    {
        mResourceMap[key] += event;
        mEventMap[EventId(key, event->id())] = event;
    }
    if ((resource.alarmTypes() & CalEvent::ACTIVE)
    &&  event->category() == CalEvent::ACTIVE)
    {
        // Update the earliest alarm to trigger
        const KAEvent* earliest = mEarliestAlarm.value(key, (KAEvent*)nullptr);
        if (replace  &&  earliest == event)
            findEarliestAlarm(key);
        else
        {
            const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
            if (dt.isValid()
            &&  (!earliest  ||  dt < earliest->nextTrigger(KAEvent::ALL_TRIGGER)))
            {
                mEarliestAlarm[key] = event;
                Q_EMIT earliestAlarmChanged();
            }
        }
    }
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = true if 'newEvent' was written to the calendar, in which case (not
*              Akonadi) ownership of 'newEvent' is taken by the calendar.
*              'newEvent' is updated.
*       = false if an error occurred, in which case 'newEvent' is unchanged.
*/
bool AlarmCalendar::modifyEvent(const EventId& oldEventId, KAEvent& newEvent)
{
    const EventId newId(oldEventId.collectionId(), newEvent.id());
    qCDebug(KALARM_LOG) << "AlarmCalendar::modifyEvent:" << oldEventId << "->" << newId;
    bool noNewId = newId.isEmpty();
    if (!noNewId  &&  oldEventId == newId)
    {
        qCCritical(KALARM_LOG) << "AlarmCalendar::modifyEvent: Same IDs";
        return false;
    }
    if (!mOpen)
        return false;
    if (mCalType == RESOURCES)
    {
        // Set the event's ID and Akonadi ID, and update the old
        // event in Akonadi.
        const KAEvent* storedEvent = event(oldEventId);
        if (!storedEvent)
        {
            qCCritical(KALARM_LOG) << "AlarmCalendar::modifyEvent: Old event not found";
            return false;
        }
        if (noNewId)
            newEvent.setEventId(CalFormat::createUniqueId());
        Resource resource = AkonadiModel::instance()->resource(oldEventId.collectionId());
        if (!resource.isValid())
            return false;
        // Don't add new event to mEventMap yet - its Akonadi item id is not yet known
        if (!resource.addEvent(newEvent))
            return false;
        // Note: deleteEventInternal() will delete storedEvent before using the
        // event parameter, so need to pass a copy as the parameter.
        deleteEventInternal(KAEvent(*storedEvent), resource);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
    }
    else
    {
        // This functionality isn't needed for the display calendar.
        // The calendar would take ownership of newEvent.
        return false;
    }
    return true;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID. The event must be in the resource calendar.
* Reply = event which has been updated
*       = 0 if error.
*/
KAEvent* AlarmCalendar::updateEvent(const KAEvent& evnt)
{
    return updateEvent(&evnt);
}
KAEvent* AlarmCalendar::updateEvent(const KAEvent* evnt)
{
    if (!mOpen  ||  mCalType != RESOURCES)
        return nullptr;
    KAEvent* kaevnt = event(EventId(*evnt));
    if (kaevnt)
    {
        Resource resource = AkonadiModel::instance()->resourceForEvent(evnt->id());
        if (resource.updateEvent(*evnt))
        {
            *kaevnt = *evnt;
            return kaevnt;
        }
    }
    qCDebug(KALARM_LOG) << "AlarmCalendar::updateEvent: error";
    return nullptr;
}


/******************************************************************************
* Delete the specified event from the resource calendar, if it exists.
* The calendar is then optionally saved.
*/
bool AlarmCalendar::deleteEvent(const KAEvent& event, bool saveit)
{
    if (mOpen  &&  mCalType == RESOURCES)
    {
        const CalEvent::Type status = deleteEventInternal(event);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
        if (status != CalEvent::EMPTY)
        {
            if (saveit)
                return save();
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
bool AlarmCalendar::deleteDisplayEvent(const QString& eventID, bool saveit)
{
    if (mOpen  &&  mCalType != RESOURCES)
    {
        Resource resource;
        const CalEvent::Type status = deleteEventInternal(eventID, KAEvent(), resource, false);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
        if (status != CalEvent::EMPTY)
        {
            if (saveit)
                return save();
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event status, if it was found in the resource calendar/calendar
*         resource or local calendar
*       = CalEvent::EMPTY otherwise.
*/
CalEvent::Type AlarmCalendar::deleteEventInternal(const KAEvent& event, bool deleteFromAkonadi)
{
    Resource resource = AkonadiModel::instance()->resource(event.collectionId());
    if (!resource.isValid())
        return CalEvent::EMPTY;
    return deleteEventInternal(event.id(), event, resource, deleteFromAkonadi);
}

CalEvent::Type AlarmCalendar::deleteEventInternal(const KAEvent& event, Resource& resource, bool deleteFromAkonadi)
{
    if (!resource.isValid())
        return CalEvent::EMPTY;
    if (event.collectionId() != resource.id())
    {
        qCCritical(KALARM_LOG) << "AlarmCalendar::deleteEventInternal: Event" << event.id() << ": resource" << event.collectionId() << "differs from 'resource'" << resource.id();
        return CalEvent::EMPTY;
    }
    return deleteEventInternal(event.id(), event, resource, deleteFromAkonadi);
}

CalEvent::Type AlarmCalendar::deleteEventInternal(const QString& eventID, const KAEvent& event, Resource& resource, bool deleteFromAkonadi)
{
    // Make a copy of the KAEvent and the ID QString, since the supplied
    // references might be destructed when the event is deleted below.
    const QString id = eventID;
    const KAEvent paramEvent = event;

    Event::Ptr kcalEvent;
    if (mCalendarStorage)
        kcalEvent = mCalendarStorage->calendar()->event(id);
    const Collection::Id key = resource.id();
    KAEventMap::Iterator it = mEventMap.find(EventId(key, id));
    if (it != mEventMap.end())
    {
        KAEvent* ev = it.value();
        mEventMap.erase(it);
        KAEvent::List& events = mResourceMap[key];
        int i = events.indexOf(ev);
        if (i >= 0)
            events.remove(i);
        delete ev;
        if (mEarliestAlarm[key] == ev)
            findEarliestAlarm(resource);
    }
    else
    {
        for (EarliestMap::Iterator eit = mEarliestAlarm.begin();  eit != mEarliestAlarm.end();  ++eit)
        {
            KAEvent* ev = eit.value();
            if (ev  &&  ev->id() == id)
            {
                findEarliestAlarm(eit.key());
                break;
            }
        }
    }
    CalEvent::Type status = CalEvent::EMPTY;
    if (kcalEvent)
    {
        status = CalEvent::status(kcalEvent);
        mCalendarStorage->calendar()->deleteEvent(kcalEvent);
    }
    else if (deleteFromAkonadi)
    {
        // It's an Akonadi event
        CalEvent::Type s = paramEvent.category();
        if (resource.deleteEvent(paramEvent))
            status = s;
    }
    return status;
}


/******************************************************************************
* Return the event with the specified ID.
* If 'checkDuplicates' is true, and the resource ID is invalid, if there is
* a unique event with the given ID, it will be returned.
*/
KAEvent* AlarmCalendar::event(const EventId& uniqueID, bool checkDuplicates)
{
    if (!isValid())
        return nullptr;
    const QString eventId = uniqueID.eventId();
    if (uniqueID.collectionId() == -1  &&  checkDuplicates)
    {
        // The resource isn't known, but use the event ID if it is unique among
        // all resources.
        const KAEvent::List list = events(eventId);
        if (list.count() > 1)
        {
            qCWarning(KALARM_LOG) << "AlarmCalendar::event: Multiple events found with ID" << eventId;
            return nullptr;
        }
        if (list.isEmpty())
            return nullptr;
        return list[0];
    }
    KAEventMap::ConstIterator it = mEventMap.constFind(uniqueID);
    if (it == mEventMap.constEnd())
        return nullptr;
    return it.value();
}

/******************************************************************************
* Return the event with the specified ID.
* For the Akonadi version, this method is for the display calendar only.
*/
Event::Ptr AlarmCalendar::kcalEvent(const QString& uniqueID)
{
    Q_ASSERT(mCalType != RESOURCES);   // only allowed for display calendar
    if (!mCalendarStorage)
        return Event::Ptr();
    return mCalendarStorage->calendar()->event(uniqueID);
}

/******************************************************************************
* Find the alarm template with the specified name.
* Reply = 0 if not found.
*/
KAEvent* AlarmCalendar::templateEvent(const QString& templateName)
{
    if (templateName.isEmpty())
        return nullptr;
    const KAEvent::List eventlist = events(CalEvent::TEMPLATE);
    for (KAEvent* event : eventlist)
    {
        if (event->templateName() == templateName)
            return event;
    }
    return nullptr;
}

/******************************************************************************
* Return all events with the specified ID, from all calendars.
*/
KAEvent::List AlarmCalendar::events(const QString& uniqueId) const
{
    KAEvent::List list;
    if (mCalType == RESOURCES  &&  isValid())
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const Collection::Id id = rit.key();
            KAEventMap::ConstIterator it = mEventMap.constFind(EventId(id, uniqueId));
            if (it != mEventMap.constEnd())
                list += it.value();
        }
    }
    return list;
}

/******************************************************************************
* Return all events in the calendar which contain alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
KAEvent::List AlarmCalendar::events(const Resource& resource, CalEvent::Types type) const
{
    KAEvent::List list;
    if (mCalType != RESOURCES  &&  (!mCalendarStorage || resource.isValid()))
        return list;
    if (resource.isValid())
    {
        const Collection::Id key = resource.id();
        ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
        if (rit == mResourceMap.constEnd())
            return list;
        const KAEvent::List events = rit.value();
        if (type == CalEvent::EMPTY)
            return events;
        for (KAEvent* const event : events)
            if (type & event->category())
                list += event;
    }
    else
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const KAEvent::List events = rit.value();
            if (type == CalEvent::EMPTY)
                list += events;
            else
            {
                for (KAEvent* const event : events)
                    if (type & event->category())
                        list += event;
            }
        }
    }
    return list;
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
* For the Akonadi version, this method is for the display calendar only.
* Optionally the event type can be filtered, using an OR of event types.
*/
Event::List AlarmCalendar::kcalEvents(CalEvent::Type type)
{
    Event::List list;
    Q_ASSERT(mCalType != RESOURCES);   // only allowed for display calendar
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
* Return whether an event is read-only.
* Display calendar events are always returned as read-only.
*/
bool AlarmCalendar::eventReadOnly(const QString& eventId) const
{
    if (mCalType != RESOURCES)
        return true;
    AkonadiModel* model = AkonadiModel::instance();
    const Resource resource = model->resourceForEvent(eventId);
    const KAEvent event = model->event(eventId);
    if (!resource.isWritable(event.category()))
        return true;
    return !event.isValid()  ||  event.isReadOnly();
    //   ||  compatibility(event) != KACalendar::Current;
}

/******************************************************************************
* Return the resource containing a specified event.
*/
Resource AlarmCalendar::resourceForEvent(const QString& eventId) const
{
    if (mCalType != RESOURCES)
        return Resource();
    return AkonadiModel::instance()->resourceForEvent(eventId);
}

/******************************************************************************
* Called when an alarm's enabled status has changed.
*/
void AlarmCalendar::disabledChanged(const KAEvent* event)
{
    if (event->category() == CalEvent::ACTIVE)
    {
        bool status = event->enabled();
        checkForDisabledAlarms(!status, status);
    }
}

/******************************************************************************
* Check whether there are any individual disabled alarms, following an alarm
* creation or modification. Must only be called for an ACTIVE alarm.
*/
void AlarmCalendar::checkForDisabledAlarms(bool oldEnabled, bool newEnabled)
{
    if (mCalType == RESOURCES  &&  newEnabled != oldEnabled)
    {
        if (newEnabled  &&  mHaveDisabledAlarms)
            checkForDisabledAlarms();
        else if (!newEnabled  &&  !mHaveDisabledAlarms)
        {
            mHaveDisabledAlarms = true;
            Q_EMIT haveDisabledAlarmsChanged(true);
        }
    }
}

/******************************************************************************
* Check whether there are any individual disabled alarms.
*/
void AlarmCalendar::checkForDisabledAlarms()
{
    if (mCalType != RESOURCES)
        return;
    bool disabled = false;
    const KAEvent::List eventlist = events(CalEvent::ACTIVE);
    for (const KAEvent* const event : eventlist)
    {
        if (!event->enabled())
        {
            disabled = true;
            break;
        }
    }
    if (disabled != mHaveDisabledAlarms)
    {
        mHaveDisabledAlarms = disabled;
        Q_EMIT haveDisabledAlarmsChanged(disabled);
    }
}

/******************************************************************************
* Find and note the active alarm with the earliest trigger time for a calendar.
*/
void AlarmCalendar::findEarliestAlarm(const Resource& resource)
{
    if (mCalType != RESOURCES)
        return;
    if (!(resource.alarmTypes() & CalEvent::ACTIVE))
        return;
    findEarliestAlarm(resource.id());
}

void AlarmCalendar::findEarliestAlarm(Akonadi::Collection::Id key)
{
    EarliestMap::Iterator eit = mEarliestAlarm.find(key);
    if (eit != mEarliestAlarm.end())
        eit.value() = nullptr;
    if (mCalType != RESOURCES
    ||  key < 0)
        return;
    ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
    if (rit == mResourceMap.constEnd())
        return;
    const KAEvent::List& events = rit.value();
    KAEvent* earliest = nullptr;
    KADateTime earliestTime;
    for (KAEvent* event : events)
    {
        if (event->category() != CalEvent::ACTIVE
        ||  mPendingAlarms.contains(event->id()))
            continue;
        const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    mEarliestAlarm[key] = earliest;
    Q_EMIT earliestAlarmChanged();
}

/******************************************************************************
* Return the active alarm with the earliest trigger time.
* Reply = 0 if none.
*/
KAEvent* AlarmCalendar::earliestAlarm() const
{
    KAEvent* earliest = nullptr;
    KADateTime earliestTime;
    for (EarliestMap::ConstIterator eit = mEarliestAlarm.constBegin();  eit != mEarliestAlarm.constEnd();  ++eit)
    {
        KAEvent* event = eit.value();
        if (!event)
            continue;
        const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    return earliest;
}

/******************************************************************************
* Note that an alarm which has triggered is now being processed. While pending,
* it will be ignored for the purposes of finding the earliest trigger time.
*/
void AlarmCalendar::setAlarmPending(KAEvent* event, bool pending)
{
    const QString id = event->id();
    bool wasPending = mPendingAlarms.contains(id);
    qCDebug(KALARM_LOG) << "AlarmCalendar::setAlarmPending:" << id << "," << pending << "(was" << wasPending << ")";
    if (pending)
    {
        if (wasPending)
            return;
        mPendingAlarms += id;
    }
    else
    {
        if (!wasPending)
            return;
        mPendingAlarms.remove(id);
    }
    // Now update the earliest alarm to trigger for its calendar
    findEarliestAlarm(AkonadiModel::instance()->resourceForEvent(event->id()));
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start times of all date-only alarms' recurrences.
*/
void AlarmCalendar::adjustStartOfDay()
{
    if (!isValid())
        return;
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        KAEvent::adjustStartOfDay(rit.value());
}

/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format.
*/
KACalendar::Compat fix(const FileStorage::Ptr& fileStorage)
{
    QString versionString;
    int version = KACalendar::updateVersion(fileStorage, versionString);
    if (version == KACalendar::IncompatibleFormat)
        return KACalendar::Incompatible;  // calendar was created by another program, or an unknown version of KAlarm
    return KACalendar::Current;
}

// vim: et sw=4:
