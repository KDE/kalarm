/*
 *  identities.cpp  -  email identity functions
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "identities.h"

#include <KIdentityManagementCore/IdentityManager>
#include <KIdentityManagementCore/Identity>

namespace KAlarmCal
{
namespace Identities
{

KIdentityManagementCore::IdentityManager* identityManager()
{
    static KIdentityManagementCore::IdentityManager* manager = nullptr;

    if (!manager)
        manager = new KIdentityManagementCore::IdentityManager(true);   // create a read-only kmail identity manager
    return manager;
}

/******************************************************************************
* Return whether any email identities exist.
*/
bool identitiesExist()
{
    KIdentityManagementCore::IdentityManager* manager = identityManager();   // create identity manager if not already done
    return manager->begin() != manager->end();
}

/******************************************************************************
* Fetch the uoid of an email identity name or uoid string.
*/
uint identityUoid(const QString& identityUoidOrName)
{
    bool ok;
    uint id = identityUoidOrName.toUInt(&ok);
    if (!ok  ||  identityManager()->identityForUoid(id).isNull())
    {
        KIdentityManagementCore::IdentityManager* manager = identityManager();  // fetch it if not already done
        for (KIdentityManagementCore::IdentityManager::ConstIterator it = manager->begin();
             it != manager->end();  ++it)
        {
            if ((*it).identityName() == identityUoidOrName)
            {
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
