/*
 *  resource.cpp  -  generic class containing an alarm calendar resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resource.h"


Resource::Resource()
    : mResource()
{
}

Resource::Resource(ResourceType* r)
    : mResource(r)
{
}

Resource::~Resource()
{
}

bool Resource::operator==(const Resource& other) const
{
    return mResource == other.mResource;
}

bool Resource::operator==(const ResourceType* other) const
{
    return mResource.data() == other;
}

Resource Resource::null()
{
    static Resource nullResource(nullptr);
    return nullResource;
}

bool Resource::isNull() const
{
    return mResource.isNull();
}

bool Resource::isValid() const
{
    return mResource.isNull() ? false : mResource->isValid();
}

bool Resource::failed() const
{
    return mResource.isNull() ? true : mResource->failed();
}

ResourceId Resource::id() const
{
    return mResource.isNull() ? -1 : mResource->id();
}

ResourceId Resource::displayId() const
{
    return mResource.isNull() ? -1 : mResource->displayId();
}

Resource::StorageType Resource::storageType() const
{
    return mResource.isNull() ? NoStorage : static_cast<StorageType>(mResource->storageType());
}

QString Resource::storageTypeString(bool description) const
{
    return mResource.isNull() ? QString() : mResource->storageTypeString(description);
}

QString Resource::storageTypeString(ResourceType::StorageType type)
{
    return ResourceType::storageTypeString(type);
}

QUrl Resource::location() const
{
    return mResource.isNull() ? QUrl() : mResource->location();
}

QString Resource::displayLocation() const
{
    return mResource.isNull() ? QString() : mResource->displayLocation();
}

QString Resource::displayName() const
{
    return mResource.isNull() ? QString() : mResource->displayName();
}

QString Resource::configName() const
{
    return mResource.isNull() ? QString() : mResource->configName();
}

CalEvent::Types Resource::alarmTypes() const
{
    return mResource.isNull() ? CalEvent::EMPTY : mResource->alarmTypes();
}

bool Resource::isEnabled(CalEvent::Type type) const
{
    return mResource.isNull() ? false : mResource->isEnabled(type);
}

CalEvent::Types Resource::enabledTypes() const
{
    return mResource.isNull() ? CalEvent::EMPTY : mResource->enabledTypes();
}

void Resource::setEnabled(CalEvent::Type type, bool enabled)
{
    if (!mResource.isNull())
        mResource->setEnabled(type, enabled);
}

void Resource::setEnabled(CalEvent::Types types)
{
    if (!mResource.isNull())
        mResource->setEnabled(types);
}

bool Resource::readOnly() const
{
    return mResource.isNull() ? true : mResource->readOnly();
}

int Resource::writableStatus(CalEvent::Type type) const
{
    return mResource.isNull() ? -1 : mResource->writableStatus(type);
}

bool Resource::isWritable(CalEvent::Type type) const
{
    return mResource.isNull() ? false : mResource->isWritable(type);
}

bool Resource::keepFormat() const
{
    return mResource.isNull() ? false : mResource->keepFormat();
}

void Resource::setKeepFormat(bool keep)
{
    if (!mResource.isNull())
        mResource->setKeepFormat(keep);
}

QColor Resource::backgroundColour() const
{
    return mResource.isNull() ? QColor() : mResource->backgroundColour();
}

void Resource::setBackgroundColour(const QColor& colour)
{
    if (!mResource.isNull())
        mResource->setBackgroundColour(colour);
}

QColor Resource::foregroundColour(CalEvent::Types types) const
{
    return mResource.isNull() ? QColor() : mResource->foregroundColour(types);
}

bool Resource::configIsStandard(CalEvent::Type type) const
{
    return mResource.isNull() ? false : mResource->configIsStandard(type);
}

CalEvent::Types Resource::configStandardTypes() const
{
    return mResource.isNull() ? CalEvent::EMPTY : mResource->configStandardTypes();
}

void Resource::configSetStandard(CalEvent::Type type, bool standard)
{
    if (!mResource.isNull())
        mResource->configSetStandard(type, standard);
}

void Resource::configSetStandard(CalEvent::Types types)
{
    if (!mResource.isNull())
        mResource->configSetStandard(types);
}

bool Resource::isCompatible() const
{
    return mResource.isNull() ? false : mResource->isCompatible();
}

KACalendar::Compat Resource::compatibility() const
{
    return mResource.isNull() ? KACalendar::Incompatible : mResource->compatibility();
}

KACalendar::Compat Resource::compatibilityVersion(QString& versionString) const
{
    if (mResource.isNull())
    {
        versionString.clear();
        return KACalendar::Incompatible;
    }
    return mResource->compatibilityVersion(versionString);
}

void Resource::editResource(QWidget* dialogParent)
{
    if (!mResource.isNull())
        mResource->editResource(dialogParent);
}

bool Resource::removeResource()
{
    return mResource.isNull() ? false : mResource->removeResource();
}

bool Resource::load(bool readThroughCache)
{
    return mResource.isNull() ? false : mResource->load(readThroughCache);
}

bool Resource::reload(bool discardMods)
{
    return mResource.isNull() ? false : mResource->reload(discardMods);
}

bool Resource::isPopulated() const
{
    return mResource.isNull() ? false : mResource->isPopulated();
}

bool Resource::save(QString* errorMessage, bool writeThroughCache)
{
    return mResource.isNull() ? false : mResource->save(errorMessage, writeThroughCache);
}

bool Resource::isSaving() const
{
    return mResource.isNull() ? false : mResource->isSaving();
}

void Resource::close()
{
    if (!mResource.isNull())
        mResource->close();
}

QList<KAEvent> Resource::events() const
{
    return mResource.isNull() ? QList<KAEvent>() : mResource->events();
}

KAEvent Resource::event(const QString& eventId, bool allowDisabled) const
{
    return mResource.isNull() ? KAEvent() : mResource->event(eventId, allowDisabled);
}

bool Resource::containsEvent(const QString& eventId) const
{
    return mResource.isNull() ? false : mResource->containsEvent(eventId);
}

bool Resource::addEvent(const KAEvent& event)
{
    return mResource.isNull() ? false : mResource->addEvent(event);
}

bool Resource::updateEvent(const KAEvent& event, bool saveIfReadOnly)
{
    return mResource.isNull() ? false : mResource->updateEvent(event, saveIfReadOnly);
}

bool Resource::deleteEvent(const KAEvent& event)
{
    return mResource.isNull() ? false : mResource->deleteEvent(event);
}

void Resource::adjustStartOfDay()
{
    if (!mResource.isNull())
        mResource->adjustStartOfDay();
}

void Resource::handleCommandErrorChange(const KAEvent& event)
{
    if (!mResource.isNull())
        mResource->handleCommandErrorChange(event);
}

void Resource::notifyDeletion()
{
    if (!mResource.isNull())
        mResource->notifyDeletion();
}

bool Resource::isBeingDeleted() const
{
    return mResource.isNull() ? false : mResource->isBeingDeleted();
}

// vim: et sw=4:
