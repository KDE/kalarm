/*
 *  identities.cpp  -  email identity functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "identities.h"

#include <identitymanager.h>
#include <identity.h>

namespace KAlarmCal
{
namespace Identities
{

KIdentityManagement::IdentityManager *identityManager()
{
    static KIdentityManagement::IdentityManager *manager = nullptr;

    if (!manager) {
        manager = new KIdentityManagement::IdentityManager(true);   // create a read-only kmail identity manager
    }
    return manager;
}

/******************************************************************************
* Return whether any email identities exist.
*/
bool identitiesExist()
{
    KIdentityManagement::IdentityManager *manager = identityManager();   // create identity manager if not already done
    return manager->begin() != manager->end();
}

/******************************************************************************
* Fetch the uoid of an email identity name or uoid string.
*/
uint identityUoid(const QString &identityUoidOrName)
{
    bool ok;
    uint id = identityUoidOrName.toUInt(&ok);
    if (!ok  ||  identityManager()->identityForUoid(id).isNull()) {
        KIdentityManagement::IdentityManager *manager = identityManager();  // fetch it if not already done
        for (KIdentityManagement::IdentityManager::ConstIterator it = manager->begin();
                it != manager->end();  ++it) {
            if ((*it).identityName() == identityUoidOrName) {
                id = (*it).uoid();
                break;
            }
        }
    }
    return id;
}

} // namespace Identities

} // namespace KAlarmCal

// vim: et sw=4:
