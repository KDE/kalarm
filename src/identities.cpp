/*
 *  identities.cpp  -  email identity functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "identities.h"

#include <KIdentityManagement/IdentityManager>
#include <KIdentityManagement/Identity>

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
