/*
 *  identities.h  -  email identity functions
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

#ifndef IDENTITIES_H
#define IDENTITIES_H

#include "kalarm_cal_export.h"

#include <QString>

namespace KPIMIdentities { class IdentityManager; }

namespace Identities
{
    KALARM_CAL_EXPORT KPIMIdentities::IdentityManager* identityManager();
    KALARM_CAL_EXPORT bool identitiesExist();
    KALARM_CAL_EXPORT uint identityUoid(const QString& identityUoidOrName);
}

#endif // IDENTITIES_H

// vim: et sw=4:
