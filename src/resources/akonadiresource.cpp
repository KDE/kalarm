/*
 *  akonadiresource.cpp  -  class for an Akonadi alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#include "akonadiresource.h"
#include "akonadimodel.h"
#include "kalarm_debug.h"

#include <kalarmcal/compatibilityattribute.h>

#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/collectionmodifyjob.h>
using namespace Akonadi;

#include <KLocalizedString>

#include <QFileInfo>

namespace
{
Collection::Rights WritableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;
}

AkonadiResource::AkonadiResource(const Collection& collection)
    : mCollection(collection)
    , mValid(collection.id() >= 0)
{
    if (mValid)
    {
        // Fetch collection data, including remote ID, resource and mime types and
        // current attributes.
        fetchCollectionAttribute(false);
        // If the collection doesn't belong to a resource, it can't be used.
        mValid = AgentManager::self()->instance(mCollection.resource()).isValid();
    }
}

AkonadiResource::~AkonadiResource()
{
}

Resource AkonadiResource::nullResource()
{
    static Resource nullRes(new AkonadiResource(Collection()));
    return nullRes;
}

bool AkonadiResource::isValid() const
{
    return mValid;
}

ResourceId AkonadiResource::id() const
{
    return mCollection.id();
}

Akonadi::Collection AkonadiResource::collection() const
{
    return mCollection;
}

QString AkonadiResource::storageType(bool description) const
{
    if (description)
        return AgentManager::self()->instance(mCollection.resource()).type().name();
    const QUrl url = location();
    bool local = url.isLocalFile();
    bool dir   = local && QFileInfo(url.toLocalFile()).isDir();
    return storageTypeString(false, !dir, local);
}

QUrl AkonadiResource::location() const
{
    return QUrl::fromUserInput(mCollection.remoteId(), QString(), QUrl::AssumeLocalFile);
}

QString AkonadiResource::displayLocation() const
{
    // Don't simply use remoteId() since that may contain "file://" prefix.
    return location().toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile);
}

QString AkonadiResource::displayName() const
{
    return mCollection.displayName();
}

QString AkonadiResource::configName() const
{
    return mCollection.resource();
}

CalEvent::Types AkonadiResource::alarmTypes() const
{
    if (!mValid)
        return CalEvent::EMPTY;
    return CalEvent::types(mCollection.contentMimeTypes());
}

CalEvent::Types AkonadiResource::enabledTypes() const
{
//TODO: see CollectionControlModel::isEnabled()
    if (!mValid)  // ||  !instance()->collectionIds().contains(mCollection.id()))
        return CalEvent::EMPTY;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.enabled();
}

void AkonadiResource::setEnabled(CalEvent::Type type, bool enabled)
{
    const CalEvent::Types types = enabledTypes();
    const CalEvent::Types newTypes = enabled ? types | type : types & ~type;
    if (newTypes != types)
        setEnabled(newTypes);
}

void AkonadiResource::setEnabled(CalEvent::Types types)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    const bool newAttr = !mCollection.hasAttribute<CollectionAttribute>();
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.enabled() == types)
        return;   // no change
    qCDebug(KALARM_LOG) << "AkonadiResource:" << mCollection.id() << "Set enabled:" << types << " was=" << mCollectionAttribute.enabled();
    mCollectionAttribute.setEnabled(types);
    mHaveCollectionAttribute = true;
    if (newAttr)
    {
        // Akonadi often doesn't notify changes to the enabled status
        // (surely a bug?), so ensure that the change is noticed.
        mNewEnabled = true;
    }
    modifyCollectionAttribute();
}

bool AkonadiResource::readOnly() const
{
    AkonadiModel::instance()->refresh(mCollection);    // update with latest data
    return (mCollection.rights() & WritableRights) != WritableRights;
}

int AkonadiResource::writableStatus(CalEvent::Type type) const
{
//TODO: don't call multiple methods which contain refresh()
    if (!mValid)
        return -1;
    AkonadiModel::instance()->refresh(mCollection);    // update with latest data
    if ((type == CalEvent::EMPTY  && !enabledTypes())
    ||  (type != CalEvent::EMPTY  && !isEnabled(type)))
        return -1;
    if ((mCollection.rights() & WritableRights) != WritableRights)
        return -1;
    if (!mCollection.hasAttribute<CompatibilityAttribute>())
        return -1;
    switch (mCollection.attribute<CompatibilityAttribute>()->compatibility())
    {
        case KACalendar::Current:
            return 1;
        case KACalendar::Converted:
        case KACalendar::Convertible:
            return 0;
        default:
            return -1;
    }
}

bool AkonadiResource::keepFormat() const
{
    if (!mValid)
        return false;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.keepFormat();
}

void AkonadiResource::setKeepFormat(bool keep)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.keepFormat() == keep)
        return;   // no change
    mCollectionAttribute.setKeepFormat(keep);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

QColor AkonadiResource::backgroundColour() const
{
    if (!mValid)
        return QColor();
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.backgroundColor();
}

void AkonadiResource::setBackgroundColour(const QColor& colour)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.backgroundColor() == colour)
        return;   // no change
    mCollectionAttribute.setBackgroundColor(colour);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

bool AkonadiResource::configIsStandard(CalEvent::Type type) const
{
    if (!mValid)
        return false;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.isStandard(type);
}

CalEvent::Types AkonadiResource::configStandardTypes() const
{
    if (!mValid)
        return CalEvent::EMPTY;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.standard();
}

void AkonadiResource::configSetStandard(CalEvent::Type type, bool standard)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.isStandard(type) == standard)
        return;   // no change
    mCollectionAttribute.setStandard(type, standard);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

void AkonadiResource::configSetStandard(CalEvent::Types types)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.standard() == types)
        return;   // no change
    mCollectionAttribute.setStandard(types);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

bool AkonadiResource::load(bool readThroughCache)
{
    Q_UNUSED(readThroughCache);
    AgentManager::self()->instance(mCollection.resource()).synchronize();
    return true;
}

bool AkonadiResource::isLoaded() const
{
    return AkonadiModel::instance()->isCollectionPopulated(mCollection.id());
}

bool AkonadiResource::save(bool writeThroughCache)
{
    Q_UNUSED(writeThroughCache);
    AgentManager::self()->instance(mCollection.resource()).synchronize();
    return true;
}

#if 0
        /** Reload the resource. Any cached data is first discarded. */
        bool reload() override;
#endif

/******************************************************************************
* Close the resource.
*/
bool AkonadiResource::close()
{
    qCDebug(KALARM_LOG) << "AkonadiResource::close:" << displayName();
    mCollection.setId(-1);
    mCollectionAttribute     = CollectionAttribute();
    mValid                   = false;
    mHaveCollectionAttribute = false;
    mNewEnabled              = false;
    return true;
}

KACalendar::Compat AkonadiResource::compatibility() const
{
    if (!mValid)
        return KACalendar::Incompatible;
    AkonadiModel::instance()->refresh(mCollection);    // update with latest data
    if (!mCollection.hasAttribute<CompatibilityAttribute>())
        return KACalendar::Incompatible;
    return mCollection.attribute<CompatibilityAttribute>()->compatibility();
}

/******************************************************************************
* Save a command error change to Akonadi.
*/
void AkonadiResource::handleCommandErrorChange(const KAEvent& event)
{
    AkonadiModel::instance()->updateCommandError(event);
}

/******************************************************************************
* Update the stored CollectionAttribute value from the Akonadi database.
*/
void AkonadiResource::fetchCollectionAttribute(bool refresh) const
{
    if (refresh)
        AkonadiModel::instance()->refresh(mCollection);    // update with latest data
    if (!mCollection.hasAttribute<CollectionAttribute>())
    {
        mCollectionAttribute = CollectionAttribute();
        mHaveCollectionAttribute = false;
    }
    else
    {
        mCollectionAttribute = *mCollection.attribute<CollectionAttribute>();
        mHaveCollectionAttribute = true;
    }
}

/******************************************************************************
* Update the CollectionAttribute value in the Akonadi database.
*/
void AkonadiResource::modifyCollectionAttribute()
{
    // Note that we can't supply 'mCollection' to CollectionModifyJob since that
    // also contains the CompatibilityAttribute value, which is read-only for
    // applications. So create a new Collection instance and only set a value
    // for CollectionAttribute.
    Collection c(mCollection.id());
    CollectionAttribute* att = c.attribute<CollectionAttribute>(Collection::AddIfMissing);
    *att = mCollectionAttribute;
    CollectionModifyJob* job = new CollectionModifyJob(c, this);
    connect(job, &CollectionModifyJob::result, this, &AkonadiResource::modifyCollectionAttrJobDone);
}

/******************************************************************************
* Called when a CollectionAttribute modification job has completed.
* Checks for any error.
*/
void AkonadiResource::modifyCollectionAttrJobDone(KJob* j)
{
    Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
    const Collection::Id id = collection.id();
    const bool newEnabled = mNewEnabled;
    mNewEnabled = false;
    if (j->error())
    {
        if (AkonadiModel::instance()->resource(id).isValid()   // if collection has been deleted, ignore error
        &&  id == mCollection.id())
        {
            qCCritical(KALARM_LOG) << "AkonadiModel::modifyCollectionAttrJobDone:" << collection.id() << "Failed to update calendar" << displayName() << ":" << j->errorString();
            AkonadiModel::notifyResourceError(this, i18nc("@info", "Failed to update calendar \"%1\".", displayName()), j->errorString());
        }
    }
    else
    {
        AkonadiModel::instance()->refresh(mCollection);   // pick up the modified attribute
        if (newEnabled)
            AkonadiModel::notifySettingsChanged(this, AkonadiModel::Enabled);
    }
}

/******************************************************************************
* Return a reference to the Collection held by a resource.
*/
Collection& AkonadiResource::collection(Resource& resource)
{
    static Collection nullCollection;
    AkonadiResource* akres = qobject_cast<AkonadiResource*>(resource.resource().data());
    return akres ? akres->mCollection : nullCollection;
}
const Collection& AkonadiResource::collection(const Resource& resource)
{
    static const Collection nullCollection;
    AkonadiResource* akres = qobject_cast<AkonadiResource*>(resource.resource().data());
    return akres ? akres->mCollection : nullCollection;
}

// vim: et sw=4:
