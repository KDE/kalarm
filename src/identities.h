/*
 *  identities.h  -  email identity functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2004-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARM_IDENTITIES_H
#define KALARM_IDENTITIES_H

#include "kalarmcal_export.h"
#include <qglobal.h>

class QString;

namespace KIdentityManagement
{
class IdentityManager;
}

namespace KAlarmCal
{

/**
 * Functions to facilitate use of KDE email identities.
 *
 * @author David Jarvie <djarvie@kde.org>
 */
namespace Identities
{
/** Return the unique identity manager instance. It is created if it does not already exist. */
KALARMCAL_EXPORT KIdentityManagement::IdentityManager *identityManager();

/** Return whether any identities exist. */
KALARMCAL_EXPORT bool identitiesExist();

/** Fetch the uoid of an identity name or uoid string. */
KALARMCAL_EXPORT uint identityUoid(const QString &identityUoidOrName);
}

}

#endif // KALARM_IDENTITIES_H

