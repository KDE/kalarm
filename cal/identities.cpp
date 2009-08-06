/*
 *  identities.cpp  -  email identity functions
 *  Program:  kalarm
 *  Copyright © 2004-2009 by David Jarvie <djarvie@kde.org>
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

#include "identities.h"

#include <kpimidentities/identitymanager.h>
#include <kpimidentities/identity.h>


static KPIMIdentities::IdentityManager* mIdentityManager = 0;

namespace Identities
{

KPIMIdentities::IdentityManager* identityManager()
{
	if (!mIdentityManager)
		mIdentityManager = new KPIMIdentities::IdentityManager(true);   // create a read-only kmail identity manager
	return mIdentityManager;
}

/******************************************************************************
*  Return whether any email identities exist.
*/
bool identitiesExist()
{
	identityManager();    // create identity manager if not already done
	return mIdentityManager->begin() != mIdentityManager->end();
}

/******************************************************************************
*  Fetch the uoid of an email identity name or uoid string.
*/
uint identityUoid(const QString& identityUoidOrName)
{
	bool ok;
	uint id = identityUoidOrName.toUInt(&ok);
	if (!ok  ||  identityManager()->identityForUoid(id).isNull())
	{
		identityManager();   // fetch it if not already done
		for (KPIMIdentities::IdentityManager::ConstIterator it = mIdentityManager->begin();
		     it != mIdentityManager->end();  ++it)
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
