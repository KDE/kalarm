/*
 *  akonadicollectionsearch.cpp  -  Search Akonadi Collections
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2014-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "akonadicollectionsearch.h"

#include "akonadiplugin_debug.h"

#include <Akonadi/AgentInstance>
#include <Akonadi/AgentManager>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/ItemDeleteJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <KCalendarCore/Event>
using namespace KCalendarCore;

#include <QTimer>

using namespace Akonadi;

/******************************************************************************
* Constructor.
* Creates jobs to fetch all collections for resources containing the mime type.
* Its subsequent actions depend on the parameters:
* - If 'remove' is true, it will locate all Items with the specified 'gid' and
*   delete them. The deleted() signal will be emitted.
* - Otherwise, if 'gid' is specified, it will Q_EMIT the signal items() to
*   notify all Items with that GID.
* - Otherwise, it will Q_EMIT the signal collections() to notify all Collections.
*/
AkonadiCollectionSearch::AkonadiCollectionSearch(const QString& mimeType, const QString& gid, const QString& uid, bool remove)
    : mMimeType(mimeType)
    , mGid(gid)
    , mUid(uid)
    , mDelete(remove && (!mGid.isEmpty() || !mUid.isEmpty()))
{
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        if (agent.type().mimeTypes().contains(mimeType))
        {
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
            job->fetchScope().setResource(agent.identifier());
            mCollectionJobs << job;
            connect(job, &CollectionFetchJob::result, this, &AkonadiCollectionSearch::collectionFetchResult);
        }
    }

    if (mCollectionJobs.isEmpty())
    {
        // There are no resources containing the mime type, so ensure that a
        // signal is emitted after construction.
        QTimer::singleShot(0, this, &AkonadiCollectionSearch::finish);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
*/
void AkonadiCollectionSearch::collectionFetchResult(KJob* j)
{
    auto job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCCritical(AKONADIPLUGIN_LOG) << "AkonadiCollectionSearch::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            if (c.contentMimeTypes().contains(mMimeType))
            {
                ItemFetchJob* ijob;
                if (!mGid.isEmpty())
                {
                    // Search for all Items with the specified GID
                    Item item;
                    item.setGid(mGid);
                    ijob = new ItemFetchJob(item, this);
                    ijob->setCollection(c);
                }
                else if (!mUid.isEmpty())
                {
                    // Search for all Events with the specified UID
                    ijob = new ItemFetchJob(c, this);
                    ijob->fetchScope().fetchFullPayload(true);
                }
                else
                {
                    mCollections << c;
                    continue;
                }
                mItemFetchJobs[ijob] = c.id();
                connect(ijob, &ItemFetchJob::result, this, &AkonadiCollectionSearch::itemFetchResult);
            }
        }
    }
    mCollectionJobs.removeAll(job);

    if (mCollectionJobs.isEmpty())
    {
        // All collections have now been fetched
        if (mGid.isEmpty() && mUid.isEmpty())
            finish();
    }
}

/******************************************************************************
* Called when an ItemFetchJob has completed.
*/
void AkonadiCollectionSearch::itemFetchResult(KJob* j)
{
    auto job = qobject_cast<ItemFetchJob*>(j);
    if (j->error())
    {
        if (!mUid.isEmpty())
            qCDebug(AKONADIPLUGIN_LOG) << "AkonadiCollectionSearch::itemFetchResult: ItemFetchJob: collection" << mItemFetchJobs[job] << "UID" << mUid << "error: " << j->errorString();
        else
            qCDebug(AKONADIPLUGIN_LOG) << "AkonadiCollectionSearch::itemFetchResult: ItemFetchJob: collection" << mItemFetchJobs[job] << "GID" << mGid << "error: " << j->errorString();
    }
    else
    {
        if (mDelete)
        {
            const Item::List items = job->items();
            for (const Item& item : items)
            {
                if (!mUid.isEmpty())
                {
                    if (item.mimeType() == mMimeType  &&  item.hasPayload<KCalendarCore::Event::Ptr>())
                    {
                        const KCalendarCore::Event::Ptr kcalEvent = item.payload<KCalendarCore::Event::Ptr>();
                        if (kcalEvent->uid() != mUid)
                            continue;
                    }
                }
                else if (mGid.isEmpty())
                    continue;
                auto djob = new ItemDeleteJob(item, this);
                mItemDeleteJobs[djob] = mItemFetchJobs.value(job);
                connect(djob, &ItemDeleteJob::result, this, &AkonadiCollectionSearch::itemDeleteResult);
            }
        }
        else
            mItems << job->items();
    }
    mItemFetchJobs.remove(job);

    if (mItemFetchJobs.isEmpty()  &&  mItemDeleteJobs.isEmpty()  &&  mCollectionJobs.isEmpty())
        finish();  // all Items have now been fetched or deleted, so notify the result
}

/******************************************************************************
* Called when an ItemDeleteJob has completed.
*/
void AkonadiCollectionSearch::itemDeleteResult(KJob* j)
{
    auto job = static_cast<ItemDeleteJob*>(j);
    if (j->error())
    {
        if (!mUid.isEmpty())
            qCDebug(AKONADIPLUGIN_LOG) << "AkonadiCollectionSearch::itemDeleteResult: ItemDeleteJob: resource" << mItemDeleteJobs[job] << "UID" << mUid << "error: " << j->errorString();
        else
            qCDebug(AKONADIPLUGIN_LOG) << "AkonadiCollectionSearch::itemDeleteResult: ItemDeleteJob: resource" << mItemDeleteJobs[job] << "GID" << mGid << "error: " << j->errorString();
    }
    else
        ++mDeleteCount;
    mItemDeleteJobs.remove(job);

    if (mItemFetchJobs.isEmpty()  &&  mItemDeleteJobs.isEmpty()  &&  mCollectionJobs.isEmpty())
        finish();  // all Items have now been deleted, so notify the result
}

/******************************************************************************
* Notify the result of the search/delete operation, and delete this instance.
*/
void AkonadiCollectionSearch::finish()
{
    if (mDelete)
        Q_EMIT deleted(mDeleteCount);
    else if (mGid.isEmpty() && mUid.isEmpty())
        Q_EMIT collections(mCollections);
    else
        Q_EMIT items(mItems);
    deleteLater();
}

// vim: et sw=4:
