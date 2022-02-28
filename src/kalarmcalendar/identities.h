/*
 *  identities.h  -  email identity functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2004-2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

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


// vim: et sw=4:
