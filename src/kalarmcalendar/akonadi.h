/*
 *  akonadi.h  -  Akonadi object functions
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011, 2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"

#include <QStringList>

namespace Akonadi
{
class Item;
}

namespace KAlarmCal
{
class KAEvent;

/** Initialise an Akonadi::Item with the event's data.
 *  Note that the event is not updated with the Item ID, and the Item is not
 *  added to the Collection.
 *  @param item                 the Item to initialise.
 *  @param event                the event whose data will be used to initialise the Item.
 *  @param collectionMimeTypes  the mime types for the Collection which will contain the Item.
 *  @return @c true if successful; @c false if the event's category does not match the
 *          collection's mime types.
 */
KALARMCAL_EXPORT bool setItemPayload(Akonadi::Item& item, const KAEvent& event, const QStringList& collectionMimeTypes);

}

// vim: et sw=4:
