/*
 *  resourcecreator.cpp  -  base class to interactively create a resource
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#include "resourcecreator.h"

#include "kalarm_debug.h"

#include <QTimer>

using namespace KAlarmCal;


ResourceCreator::ResourceCreator(CalEvent::Type defaultType, QWidget* parent)
    : QObject()
    , mParent(parent)
    , mDefaultType(defaultType)
{
}

/******************************************************************************
* Create a new resource. The user will be prompted to enter its configuration.
*/
void ResourceCreator::createResource()
{
    QTimer::singleShot(0, this, &ResourceCreator::doCreateResource);
}

// vim: et sw=4:
