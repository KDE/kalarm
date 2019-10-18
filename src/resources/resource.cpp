/*
 *  resource.cpp  -  generic class containing an alarm calendar resource
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

#include "resource.h"

Resource::Resource()
    : mResource()
{
}

Resource::Resource(ResourceBase* r)
    : mResource(r)
{
}

bool Resource::operator==(const Resource& other) const
{
    return mResource == other.mResource;
}

bool Resource::operator==(const ResourceBase* other) const
{
    return mResource.data() == other;
}

Resource null()
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

ResourceBase::Ptr Resource::resource() const
{
    return mResource;
}

ResourceId Resource::id() const
{
    return mResource.isNull() ? -1 : mResource->id();
}

QString Resource::storageType(bool description) const
{
    return mResource.isNull() ? QString() : mResource->storageType(description);
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

bool Resource::load(bool readThroughCache)
{
    return mResource.isNull() ? false : mResource->load(readThroughCache);
}

bool Resource::isLoaded() const
{
    return mResource.isNull() ? false : mResource->isLoaded();
}

bool Resource::save(bool writeThroughCache)
{
    return mResource.isNull() ? false : mResource->save(writeThroughCache);
}

bool Resource::close()
{
    return mResource.isNull() ? false : mResource->close();
}

KACalendar::Compat Resource::compatibility() const
{
    return mResource.isNull() ? KACalendar::Incompatible : mResource->compatibility();
}

bool Resource::isCompatible() const
{
    return mResource.isNull() ? false : mResource->isCompatible();
}

QList<KAEvent> Resource::events() const
{
    return mResource.isNull() ? QList<KAEvent>() : mResource->events();
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

// vim: et sw=4:
