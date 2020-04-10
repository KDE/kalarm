/*
 *  resource.cpp  -  generic class containing an alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019-2020 David Jarvie <djarvie@kde.org>
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

#include "resources.h"

#include "resource.h"
#include "resourcedatamodelbase.h"
#include "resourcemodel.h"
#include "resourceselectdialog.h"

#include "mainwindow.h"
#include "preferences.h"
#include "lib/autoqpointer.h"
#include "lib/filedialog.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
#include <KLocalizedString>
#include <KFileItem>
#include <KJobWidgets>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <kio_version.h>

#include <QTemporaryFile>
#include <QFileDialog>

using namespace KCalendarCore;

namespace
{
bool updateCalendarFormat(const FileStorage::Ptr&);
}

Resources* Resources::mInstance {nullptr};

// Copy of all ResourceType instances with valid ID, wrapped in the Resource
// container which manages the instance.
QHash<ResourceId, Resource> Resources::mResources;

bool Resources::mCreated {false};
bool Resources::mPopulated {false};
QUrl Resources::mLastImportUrl;


Resources* Resources::instance()
{
    if (!mInstance)
        mInstance = new Resources;
    return mInstance;
}

Resources::Resources()
{
    qRegisterMetaType<ResourceType::MessageType>();
}

Resources::~Resources()
{
    qCDebug(KALARM_LOG) << "Resources::~Resources";
    for (auto it = mResources.begin();  it != mResources.end();  ++it)
        it.value().close();
}

Resource Resources::resource(ResourceId id)
{
    return mResources.value(id, Resource::null());
}

/******************************************************************************
* Return the resources which are enabled for a specified alarm type.
* If 'writable' is true, only writable resources are included.
*/
QVector<Resource> Resources::enabledResources(CalEvent::Type type, bool writable)
{
    const CalEvent::Types types = (type == CalEvent::EMPTY)
                                ? CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE
                                : type;

    QVector<Resource> result;
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (writable  &&  !res.isWritable())
            continue;
        if (res.enabledTypes() & types)
            result += res;
    }
    return result;
}

/******************************************************************************
* Return the standard resource for an alarm type.
*/
Resource Resources::getStandard(CalEvent::Type type)
{
    Resources* manager = instance();
    bool wantDefaultArchived = (type == CalEvent::ARCHIVED);
    Resource defaultArchived;
    for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.isWritable(type))
        {
            if (res.configIsStandard(type))
                return res;
            if (wantDefaultArchived)
            {
                if (defaultArchived.isValid())
                    wantDefaultArchived = false;   // found two archived alarm resources
                else
                    defaultArchived = res;   // this is the first archived alarm resource
            }
        }
    }

    if (wantDefaultArchived  &&  defaultArchived.isValid())
    {
        // There is no resource specified as the standard archived alarm
        // resource, but there is exactly one writable archived alarm
        // resource. Set that resource to be the standard.
        defaultArchived.configSetStandard(CalEvent::ARCHIVED, true);
        return defaultArchived;
    }

    return Resource();
}

/******************************************************************************
* Return whether a collection is the standard collection for a specified
* mime type.
*/
bool Resources::isStandard(const Resource& resource, CalEvent::Type type)
{
    // If it's for archived alarms, get and also set the standard resource if
    // necessary.
    if (type == CalEvent::ARCHIVED)
        return getStandard(type) == resource;

    return resource.configIsStandard(type) && resource.isWritable(type);
}

/******************************************************************************
* Return the alarm types for which a resource is the standard resource.
*/
CalEvent::Types Resources::standardTypes(const Resource& resource, bool useDefault)
{
    if (!resource.isWritable())
        return CalEvent::EMPTY;

    Resources* manager = instance();
    auto it = manager->mResources.constFind(resource.id());
    if (it == manager->mResources.constEnd())
        return CalEvent::EMPTY;
    CalEvent::Types stdTypes = resource.configStandardTypes() & resource.enabledTypes();
    if (useDefault)
    {
        // Also return alarm types for which this is the only resource.
        // Check if it is the only writable resource for these type(s).

        if (!(stdTypes & CalEvent::ARCHIVED)  &&  resource.isEnabled(CalEvent::ARCHIVED))
        {
            // If it's the only enabled archived alarm resource, set it as standard.
            getStandard(CalEvent::ARCHIVED);
            stdTypes = resource.configStandardTypes() & resource.enabledTypes();
        }
        CalEvent::Types enabledNotStd = resource.enabledTypes() & ~stdTypes;
        if (enabledNotStd)
        {
            // The resource is enabled for type(s) for which it is not the standard.
            for (auto itr = manager->mResources.constBegin();  itr != manager->mResources.constEnd() && enabledNotStd;  ++itr)
            {
                const Resource& res = itr.value();
                if (res != resource  &&  res.isWritable())
                {
                    const CalEvent::Types en = res.enabledTypes() & enabledNotStd;
                    if (en)
                        enabledNotStd &= ~en;   // this resource handles the same alarm type
                }
            }
        }
        stdTypes |= enabledNotStd;
    }
    return stdTypes;
}

/******************************************************************************
* Set or clear the standard status for a resource.
*/
void Resources::setStandard(Resource& resource, CalEvent::Type type, bool standard)
{
    if (!(type & resource.enabledTypes()))
        return;

    Resources* manager = instance();
    auto it = manager->mResources.find(resource.id());
    if (it == manager->mResources.end())
        return;
    resource = it.value();   // just in case it's a different object!
    if (standard == resource.configIsStandard(type))
        return;

    if (!standard)
        resource.configSetStandard(type, false);
    else if (resource.isWritable(type))
    {
        // Clear the standard status for any other resources.
        for (auto itr = manager->mResources.begin();  itr != manager->mResources.end();  ++itr)
        {
            Resource& res = itr.value();
            if (res != resource)
                res.configSetStandard(type, false);
        }
        resource.configSetStandard(type, true);
    }
}

/******************************************************************************
* Set the alarm types for which a resource the standard resource.
*/
void Resources::setStandard(Resource& resource, CalEvent::Types types)
{
    types &= resource.enabledTypes();

    Resources* manager = instance();
    auto it = manager->mResources.find(resource.id());
    if (it == manager->mResources.end())
        return;
    resource = it.value();   // just in case it's a different object!
    if (types != resource.configStandardTypes()
    &&  (!types  ||  resource.isWritable()))
    {
        if (types)
        {
            // Clear the standard status for any other resources.
            for (auto itr = manager->mResources.begin();  itr != manager->mResources.end();  ++itr)
            {
                Resource& res = itr.value();
                if (res != resource)
                {
                    const CalEvent::Types rtypes = res.configStandardTypes();
                    if (rtypes & types)
                        res.configSetStandard(rtypes & ~types);
                }
            }
        }
        resource.configSetStandard(types);
    }
}

/******************************************************************************
* Find the resource to be used to store an event of a given type.
* This will be the standard resource for the type, but if this is not valid,
* the user will be prompted to select a resource.
*/
Resource Resources::destination(CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Resource standard;
    if (type == CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == CalEvent::ARCHIVED  ||  noPrompt
    ||  (!Preferences::askResource()  &&  standard.isValid()))
        return standard;

    // Prompt for which collection to use
    ResourceListModel* model = DataModel::createResourceListModel(promptParent);
    model->setFilterWritable(true);
    model->setFilterEnabled(true);
    model->setEventTypeFilter(type);
    model->useResourceColour(false);
    Resource res;
    switch (model->rowCount())
    {
        case 0:
            break;
        case 1:
            res = model->resource(0);
            break;
        default:
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of 'promptParent', and on return from this function).
            AutoQPointer<ResourceSelectDialog> dlg = new ResourceSelectDialog(model, promptParent);
            dlg->setWindowTitle(i18nc("@title:window", "Choose Calendar"));
            dlg->setDefaultResource(standard);
            if (dlg->exec())
                res = dlg->selectedResource();
            if (!res.isValid()  &&  cancelled)
                *cancelled = true;
        }
    }
    return res;
}

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool Resources::importAlarms(Resource& resource, QWidget* parent)
{
    qCDebug(KALARM_LOG) << "Resources::importAlarms";
    const QUrl url = QFileDialog::getOpenFileUrl(parent, QString(), mLastImportUrl,
                                                 QStringLiteral("%1 (*.vcs *.ics)").arg(i18nc("@info", "Calendar Files")));
    if (url.isEmpty())
    {
        qCCritical(KALARM_LOG) << "Resources::importAlarms: Empty URL";
        return false;
    }
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "Resources::importAlarms: Invalid URL";
        return false;
    }
    mLastImportUrl = url.adjusted(QUrl::RemoveFilename);
    qCDebug(KALARM_LOG) << "Resources::importAlarms:" << url.toDisplayString();

    // If the URL is remote, download it into a temporary local file.
    QString filename;
    bool local = url.isLocalFile();
    if (local)
    {
        filename = url.toLocalFile();
        if (!QFile::exists(filename))
        {
            qCDebug(KALARM_LOG) << "Resources::importAlarms: File '" << url.toDisplayString() <<"' not found";
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
            qCCritical(KALARM_LOG) << "Resources::accessUrl: Download failure";
            KAMessageBox::error(parent, xi18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.toDisplayString()));
            return false;
        }
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        tmpFile.write(getJob->data());
        tmpFile.seek(0);
        filename = tmpFile.fileName();
        qCDebug(KALARM_LOG) << "Resources::accessUrl: --- Downloaded to" << filename;
    }

    // Read the calendar and add its alarms to the current calendars
    MemoryCalendar::Ptr cal(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(cal, filename));
    bool success = calStorage->load();
    if (!success)
    {
        qCDebug(KALARM_LOG) << "Resources::importAlarms: Error loading calendar '" << filename <<"'";
        KAMessageBox::error(parent, xi18nc("@info", "Could not load calendar <filename>%1</filename>.", url.toDisplayString()));
    }
    else
    {
        const bool currentFormat = updateCalendarFormat(calStorage);
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
                if (!currentFormat)
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
//TODO: does this prompt for every alarm if no default is set?
                res = Resources::destination(type);
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
bool Resources::exportAlarms(const KAEvent::List& events, QWidget* parent)
{
    bool append;
//TODO: exportalarms shows up afterwards in other file dialogues
    QString file = FileDialog::getSaveFileName(QUrl(QStringLiteral("kfiledialog:///exportalarms")),
                                               QStringLiteral("*.ics|%1").arg(i18nc("@info", "Calendar Files")),
                                               parent, i18nc("@title:window", "Choose Export Calendar"),
                                               &append);
    if (file.isEmpty())
        return false;
    const QUrl url = QUrl::fromLocalFile(file);
    if (!url.isValid())
    {
        qCDebug(KALARM_LOG) << "Resources::exportAlarms: Invalid URL" << url;
        return false;
    }
    qCDebug(KALARM_LOG) << "Resources::exportAlarms:" << url.toDisplayString();

    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
    FileStorage::Ptr calStorage(new FileStorage(calendar, file));
    if (append  &&  !calStorage->load())
    {
#if KIO_VERSION < QT_VERSION_CHECK(5, 69, 0)
        auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, 2);
#else
        auto statJob = KIO::statDetails(url, KIO::StatJob::SourceSide, KIO::StatDetail::StatDefaultDetails);
#endif
        KJobWidgets::setWindow(statJob, parent);
        statJob->exec();
        KFileItem fi(statJob->statResult(), url);
        if (fi.size())
        {
            qCCritical(KALARM_LOG) << "Resources::exportAlarms: Error loading calendar file" << file << "for append";
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
            qCCritical(KALARM_LOG) << "Resources::exportAlarms:" << file << ": failed";
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
                qCCritical(KALARM_LOG) << "Resources::exportAlarms:" << file << ": upload failed";
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
* Return whether all configured resources have been created.
*/
bool Resources::allCreated()
{
    return instance()->mCreated;
}

/******************************************************************************
* Return whether all configured resources have been loaded at least once.
*/
bool Resources::allPopulated()
{
    return instance()->mPopulated;
}

/******************************************************************************
* Return the resource which an event belongs to, provided its alarm type is
* enabled.
*/
Resource Resources::resourceForEvent(const QString& eventId)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.containsEvent(eventId))
            return res;
    }
    return Resource::null();
}

/******************************************************************************
* Return the resource which an event belongs to, and the event, provided its
* alarm type is enabled.
*/
Resource Resources::resourceForEvent(const QString& eventId, KAEvent& event)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        event = res.event(eventId);
        if (event.isValid())
            return res;
    }
    if (mResources.isEmpty())   // otherwise, 'event' was set invalid in the loop
        event = KAEvent();
    return Resource::null();
}

/******************************************************************************
* Return the resource which has a given configuration identifier.
*/
Resource Resources::resourceForConfigName(const QString& configName)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.configName() == configName)
            return res;
    }
    return Resource::null();
}

/******************************************************************************
* Called after a new resource has been created, when it has completed its
* initialisation.
*/
void Resources::notifyNewResourceInitialised(Resource& res)
{
    if (res.isValid())
        Q_EMIT instance()->resourceAdded(res);
}

/******************************************************************************
* Called when all configured resources have been created for the first time.
*/
void Resources::notifyResourcesCreated()
{
    mCreated = true;
    Q_EMIT instance()->resourcesCreated();
    checkResourcesPopulated();
}

/******************************************************************************
* Called when a resource's events have been loaded.
* Emits a signal if all collections have been populated.
*/
void Resources::notifyResourcePopulated(const ResourceType* res)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->resourcePopulated(r);
    }

    // Check whether all resources have now loaded at least once.
    checkResourcesPopulated();
}

/******************************************************************************
* Called to notify that migration/creation of resources has completed.
*/
void Resources::notifyResourcesMigrated()
{
    Q_EMIT instance()->migrationCompleted();
}

/******************************************************************************
* Called to notify that a resource is about to be removed.
*/
void Resources::notifyResourceToBeRemoved(ResourceType* res)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->resourceToBeRemoved(r);
    }
}

/******************************************************************************
* Called by a resource to notify that its settings have changed.
* Emits the settingsChanged() signal.
* If the resource is now read-only and was standard, clear its standard status.
* If the resource has newly enabled alarm types, ensure that it doesn't
* duplicate any existing standard setting.
*/
void Resources::notifySettingsChanged(ResourceType* res, ResourceType::Changes change, CalEvent::Types oldEnabled)
{
    if (!res)
        return;
    Resource r = resource(res->id());
    if (!r.isValid())
        return;

    Resources* manager = instance();

    if (change & ResourceType::Enabled)
    {
        ResourceType::Changes change = ResourceType::Enabled;

        // Find which alarm types (if any) have been newly enabled.
        const CalEvent::Types extra    = res->enabledTypes() & ~oldEnabled;
        CalEvent::Types       std      = res->configStandardTypes();
        const CalEvent::Types extraStd = std & extra;
        if (extraStd  &&  res->isWritable())
        {
            // Alarm type(s) have been newly enabled, and are set as standard.
            // Don't allow the resource to be set as standard for those types if
            // another resource is already the standard.
            CalEvent::Types disallowedStdTypes{};
            for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
            {
                const Resource& resit = it.value();
                if (resit.id() != res->id()  &&  resit.isWritable())
                {
                    disallowedStdTypes |= extraStd & resit.configStandardTypes() & resit.enabledTypes();
                    if (extraStd == disallowedStdTypes)
                        break;   // all the resource's newly enabled standard types are disallowed
                }
            }
            if (disallowedStdTypes)
            {
                std &= ~disallowedStdTypes;
                res->configSetStandard(std);
            }
        }
        if (std)
            change |= ResourceType::Standard;
    }

    Q_EMIT manager->settingsChanged(r, change);

    if ((change & ResourceType::ReadOnly)  &&  res->readOnly())
    {
        qCDebug(KALARM_LOG) << "Resources::notifySettingsChanged:" << res->displayId() << "ReadOnly";
        // A read-only resource can't be the default for any alarm type
        const CalEvent::Types std = standardTypes(r, false);
        if (std != CalEvent::EMPTY)
        {
            setStandard(r, CalEvent::EMPTY);
            bool singleType = true;
            QString msg;
            switch (std)
            {
                case CalEvent::ACTIVE:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for active alarms.",
                            res->displayName());
                    break;
                case CalEvent::ARCHIVED:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for archived alarms.",
                            res->displayName());
                    break;
                case CalEvent::TEMPLATE:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for alarm templates.",
                            res->displayName());
                    break;
                default:
                    msg = xi18nc("@info", "<para>The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for:%2</para>"
                            "<para>Please select new default calendars.</para>",
                            res->displayName(), ResourceDataModelBase::typeListForDisplay(std));
                    singleType = false;
                    break;
            }
            if (singleType)
                msg = xi18nc("@info", "<para>%1</para><para>Please select a new default calendar.</para>", msg);
            notifyResourceMessage(res->id(), ResourceType::MessageType::Info, msg, QString());
        }
    }
}

void Resources::notifyResourceMessage(ResourceType* res, ResourceType::MessageType type, const QString& message, const QString& details)
{
    if (res)
        notifyResourceMessage(res->id(), type, message, details);
}

void Resources::notifyResourceMessage(ResourceId id, ResourceType::MessageType type, const QString& message, const QString& details)
{
    if (resource(id).isValid())
        Q_EMIT instance()->resourceMessage(type, message, details);
}

void Resources::notifyEventsAdded(ResourceType* res, const QList<KAEvent>& events)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventsAdded(r, events);
    }
}

void Resources::notifyEventUpdated(ResourceType* res, const KAEvent& event)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventUpdated(r, event);
    }
}

void Resources::notifyEventsToBeRemoved(ResourceType* res, const QList<KAEvent>& events)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventsToBeRemoved(r, events);
    }
}

bool Resources::addResource(ResourceType* instance, Resource& resource)
{
    if (!instance  ||  instance->id() < 0)
    {
        // Instance is invalid - return an invalid resource.
        delete instance;
        resource = Resource::null();
        return false;
    }
    auto it = mResources.constFind(instance->id());
    if (it != mResources.constEnd())
    {
        // Instance ID already exists - return the existing resource.
        delete instance;
        resource = it.value();
        return false;
    }
    // Add a new resource.
    resource = Resource(instance);
    mResources[instance->id()] = resource;
    return true;
}

void Resources::removeResource(ResourceId id)
{
    if (mResources.remove(id) > 0)
        Q_EMIT instance()->resourceRemoved(id);
}

/******************************************************************************
* To be called when a resource has been created or loaded.
* If all resources have now loaded for the first time, emit signal.
*/
void Resources::checkResourcesPopulated()
{
    if (!mPopulated  &&  mCreated)
    {
        // Check whether all resources have now loaded at least once.
        for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
        {
            const Resource& res = it.value();
            if (res.isEnabled(CalEvent::EMPTY)  &&  !res.isPopulated())
                return;
        }
        mPopulated = true;
        Q_EMIT instance()->resourcesPopulated();
    }
}

#if 0
/******************************************************************************
* Return whether one or all enabled collections have been loaded.
*/
bool Resources::isPopulated(ResourceId id)
{
    if (id >= 0)
    {
        const Resource res = resource(id);
        return res.isPopulated()
           ||  res.enabledTypes() == CalEvent::EMPTY;
    }

    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (!res.isPopulated()
        &&  res.enabledTypes() != CalEvent::EMPTY)
            return false;
    }
    return true;
}
#endif

namespace
{

/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format.
*/
bool updateCalendarFormat(const FileStorage::Ptr& fileStorage)
{
    QString versionString;
    int version = KACalendar::updateVersion(fileStorage, versionString);
    if (version == KACalendar::IncompatibleFormat)
        return false;  // calendar was created by another program, or an unknown version of KAlarm
    return true;
}

}

// vim: et sw=4:
