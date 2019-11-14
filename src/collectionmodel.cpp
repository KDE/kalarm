/*
 *  collectionmodel.cpp  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#include "collectionmodel.h"

#include "akonadimodel.h"
#include "messagebox.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/collectionfetchjob.h>
#include <AkonadiCore/entitymimetypefiltermodel.h>

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QApplication>
#include <QUrl>

using namespace Akonadi;
using namespace KAlarmCal;


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
=============================================================================*/

CollectionControlModel*                             CollectionControlModel::mInstance = nullptr;
QHash<QString, CollectionControlModel::ResourceCol> CollectionControlModel::mAgentPaths;
EntityMimeTypeFilterModel*                          CollectionControlModel::mFilterModel;

static QRegularExpression matchMimeType(QStringLiteral("^application/x-vnd\\.kde\\.alarm.*"),
                                        QRegularExpression::DotMatchesEverythingOption);

CollectionControlModel* CollectionControlModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionControlModel(qApp);
    return mInstance;
}

CollectionControlModel::CollectionControlModel(QObject* parent)
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KSharedConfig::openConfig(), "Collections"), parent)
{
    // Initialise the list of enabled collections
    mFilterModel = new EntityMimeTypeFilterModel(this);
    mFilterModel->addMimeTypeInclusionFilter(Collection::mimeType());
    mFilterModel->setSourceModel(AkonadiModel::instance());

    QList<Collection::Id> collectionIds;
    findEnabledCollections(QModelIndex(), collectionIds);
    setCollections(Collection::List());
    for (Collection::Id id : qAsConst(collectionIds))
        addCollection(Collection(id));

    connect(this, &QAbstractItemModel::rowsInserted,
            this, &CollectionControlModel::slotRowsInserted);

    connect(AkonadiModel::instance(), &AkonadiModel::serverStopped,
                                this, &CollectionControlModel::reset);
    connect(Resources::instance(), &Resources::settingsChanged,
                             this, &CollectionControlModel::resourceStatusChanged);
}

/******************************************************************************
* Recursive function to check all collections' enabled status, and to compile a
* list of all collections which have any alarm types enabled.
* Collections which duplicate the same backend storage are filtered out, to
* avoid crashes due to duplicate events in different resources.
*/
void CollectionControlModel::findEnabledCollections(const QModelIndex& parent, QList<Collection::Id>& collectionIds) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = mFilterModel->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix   = mFilterModel->index(row, 0, parent);
        const QModelIndex AMix = mFilterModel->mapToSource(ix);
        Resource resource = model->resource(AMix);
        if (!resource.isValid())
            continue;
        const CalEvent::Types enabled = resource.enabledTypes();
        const CalEvent::Types canEnable = checkTypesToEnable(resource, collectionIds, enabled);
        if (canEnable != enabled)
        {
            // There is another resource which uses the same backend
            // storage. Disable alarm types enabled in the other resource.
            if (!resource.isBeingDeleted())
                resource.setEnabled(canEnable);
        }
        if (canEnable)
            collectionIds += resource.id();
        if (mFilterModel->rowCount(ix) > 0)
            findEnabledCollections(ix, collectionIds);
    }
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Connect to signals from the inserted resources.
*/
void CollectionControlModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix   = mFilterModel->index(row, 0, parent);
        const QModelIndex AMix = mFilterModel->mapToSource(ix);
        Resource resource = model->resource(AMix);
        setEnabledStatus(resource, resource.enabledTypes(), true);
    }
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If it's the enabled status, add or remove the collection to/from the enabled
* list.
*/
void CollectionControlModel::resourceStatusChanged(Resource& res, ResourceType::Changes change)
{
    if (!res.isValid())
        return;

    if (change & ResourceType::Enabled)
    {
        const CalEvent::Types enabled = res.enabledTypes();
        qCDebug(KALARM_LOG) << "CollectionControlModel::resourceStatusChanged:" << res.id() << ", enabled=" << enabled;
        setEnabledStatus(res, enabled, false);
    }
    if (change & ResourceType::ReadOnly)
    {
        bool readOnly = res.readOnly();
        qCDebug(KALARM_LOG) << "CollectionControlModel::resourceStatusChanged:" << res.id() << ", readOnly=" << readOnly;
        if (readOnly)
        {
            // A read-only resource can't be the default for any alarm type
            const CalEvent::Types std = Resources::standardTypes(res, false);
            if (std != CalEvent::EMPTY)
            {
                Resources::setStandard(res, CalEvent::Types(CalEvent::EMPTY));
                QWidget* messageParent = qobject_cast<QWidget*>(QObject::parent());
                bool singleType = true;
                QString msg;
                switch (std)
                {
                    case CalEvent::ACTIVE:
                        msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                                    "This was the default calendar for active alarms.",
                                    res.displayName());
                        break;
                    case CalEvent::ARCHIVED:
                        msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                                    "This was the default calendar for archived alarms.",
                                    res.displayName());
                        break;
                    case CalEvent::TEMPLATE:
                        msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                                    "This was the default calendar for alarm templates.",
                                    res.displayName());
                        break;
                    default:
                        msg = xi18nc("@info", "<para>The calendar <resource>%1</resource> has been made read-only. "
                                              "This was the default calendar for:%2</para>"
                                              "<para>Please select new default calendars.</para>",
                                     res.displayName(), CalendarDataModel::typeListForDisplay(std));
                        singleType = false;
                        break;
                }
                if (singleType)
                    msg = xi18nc("@info", "<para>%1</para><para>Please select a new default calendar.</para>", msg);
                KAMessageBox::information(messageParent, msg);
            }
        }
    }
}

/******************************************************************************
* Change the collection's enabled status.
* Add or remove the collection to/from the enabled list.
* Reply = alarm types which can be enabled
*/
CalEvent::Types CollectionControlModel::setEnabledStatus(Resource& resource, CalEvent::Types types, bool inserted)
{
    qCDebug(KALARM_LOG) << "CollectionControlModel::setEnabledStatus:" << resource.id() << ", types=" << types;
    CalEvent::Types disallowedStdTypes{};
    CalEvent::Types stdTypes{};

    // Prevent the enabling of duplicate alarm types if another collection
    // uses the same backend storage.
    const QList<Collection::Id> colIds = collectionIds();
    const CalEvent::Types canEnable = checkTypesToEnable(resource, colIds, types);

    // Update the list of enabled collections
    if (canEnable)
    {
        if (!colIds.contains(resource.id()))
        {
            // It's a new collection.
            // Prevent duplicate standard collections being created for any alarm type.
            stdTypes = resource.configStandardTypes();
            if (stdTypes)
            {
                for (const Collection::Id& id : colIds)
                {
                    const Resource res = AkonadiModel::instance()->resource(id);
                    if (res.isValid())
                    {
                        const CalEvent::Types t = stdTypes & res.alarmTypes();
                        if (t  &&  resource.isCompatible())
                        {
                            disallowedStdTypes |= res.configStandardTypes() & t;
                            if (disallowedStdTypes == stdTypes)
                                break;
                        }
                    }
                }
            }
            addCollection(Collection(resource.id()));
        }
    }
    else
        removeCollection(Collection(resource.id()));

    if (disallowedStdTypes  ||  !inserted  ||  canEnable != types)
    {
        // Update the collection's status
        if (!resource.isBeingDeleted())
        {
            if (!inserted  ||  canEnable != types)
                resource.setEnabled(canEnable);
            if (disallowedStdTypes)
                resource.configSetStandard(stdTypes & ~disallowedStdTypes);
        }
    }
    return canEnable;
}

/******************************************************************************
* Check which alarm types can be enabled for a specified collection.
* If the collection uses the same backend storage as another collection, any
* alarm types already enabled in the other collection must be disabled in this
* collection. This is to avoid duplicating events between different resources,
* which causes user confusion and annoyance, and causes crashes.
* Parameters:
*   collectionIds = list of collection IDs to search for duplicates.
*   types         = alarm types to be enabled for the collection.
* Reply = alarm types which can be enabled without duplicating other collections.
*/
CalEvent::Types CollectionControlModel::checkTypesToEnable(const Resource& resource, const QList<Collection::Id>& collectionIds, CalEvent::Types types)
{
    types &= (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    if (types)
    {
        // At least one alarm type is to be enabled
        const QUrl location = resource.location();
        for (const Collection::Id& id : collectionIds)
        {
            const Resource res = AkonadiModel::instance()->resource(id);
            const QUrl cLocation = res.location();
            if (id != resource.id()  &&  cLocation == location)
            {
                // The collection duplicates the backend storage
                // used by another enabled collection.
                // N.B. don't refresh this collection - assume no change.
                qCDebug(KALARM_LOG) << "CollectionControlModel::checkTypesToEnable:" << id << "duplicates backend for" << resource.id();
                types &= ~res.enabledTypes();
                if (!types)
                    break;
            }
        }
    }
    return types;
}

/******************************************************************************
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Resource CollectionControlModel::destination(CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    return Resources::destination<AkonadiModel>(type, promptParent, noPrompt, cancelled);
}

/******************************************************************************
* Return all collections which belong to a resource and which optionally
* contain a specified mime type.
*/
Collection::List CollectionControlModel::allCollections(CalEvent::Type type)
{
    const bool allTypes = (type == CalEvent::EMPTY);
    const QString mimeType = CalEvent::mimeType(type);
    AkonadiModel* model = AkonadiModel::instance();
    Collection::List result;
    const QList<Collection::Id> colIds = instance()->collectionIds();
    for (Collection::Id colId : colIds)
    {
        Resource res = model->resource(colId);
        if (res.isValid()  &&  (allTypes  ||  res.alarmTypes() & type))
        {
            Collection* c = model->collection(colId);
            if (c)
                result += *c;
        }
    }
    return result;
}

/******************************************************************************
* Return the collection ID for a given Akonadi resource ID.
*/
Collection::Id CollectionControlModel::collectionForResourceName(const QString& resourceName)
{
    const QList<Collection::Id> colIds = instance()->collectionIds();
    for (Collection::Id colId : colIds)
    {
        Resource res = AkonadiModel::instance()->resource(colId);
        if (res.configName() == resourceName)
            return res.id();
    }
    return -1;
}

/******************************************************************************
* Check for, and remove, any Akonadi resources which duplicate use of calendar
* files/directories.
*/
void CollectionControlModel::removeDuplicateResources()
{
    mAgentPaths.clear();
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        if (agent.type().mimeTypes().indexOf(matchMimeType) >= 0)
        {
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
            job->fetchScope().setResource(agent.identifier());
            connect(job, &CollectionFetchJob::result, instance(), &CollectionControlModel::collectionFetchResult);
        }
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
*/
void CollectionControlModel::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCCritical(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        AgentManager* agentManager = AgentManager::self();
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            if (c.contentMimeTypes().indexOf(matchMimeType) >= 0)
            {
                ResourceCol thisRes(job->fetchScope().resource(), c.id());
                auto it = mAgentPaths.constFind(c.remoteId());
                if (it != mAgentPaths.constEnd())
                {
                    // Remove the resource containing the higher numbered Collection
                    // ID, which is likely to be the more recently created.
                    const ResourceCol prevRes = it.value();
                    if (thisRes.collectionId > prevRes.collectionId)
                    {
                        qCWarning(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: Removing duplicate resource" << thisRes.resourceId;
                        agentManager->removeInstance(agentManager->instance(thisRes.resourceId));
                        continue;
                    }
                    qCWarning(KALARM_LOG) << "CollectionControlModel::collectionFetchResult: Removing duplicate resource" << prevRes.resourceId;
                    agentManager->removeInstance(agentManager->instance(prevRes.resourceId));
                }
                mAgentPaths[c.remoteId()] = thisRes;
            }
        }
    }
}

/******************************************************************************
* Called when the Akonadi server has stopped. Reset the model.
*/
void CollectionControlModel::reset()
{
    // Clear the collections list. This is required because addCollection() or
    // setCollections() don't work if the collections which they specify are
    // already in the list.
    setCollections(Collection::List());
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant CollectionControlModel::data(const QModelIndex& index, int role) const
{
    return sourceModel()->data(mapToSource(index), role);
}

// vim: et sw=4:
