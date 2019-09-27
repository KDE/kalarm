/*
 *  akonadi.h  -  Akonadi object functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2011,2019 David Jarvie <djarvie@kde.org>
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

#ifndef KALARMCAL_AKONADI_H
#define KALARMCAL_AKONADI_H

#include "kalarmcal_export.h"

class QStringList;
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
 *  @param event                the event whose data will be used to intialise the Item.
 *  @param collectionMimeTypes  the mime types for the Collection which will contain the Item.
 *  @return @c true if successful; @c false if the event's category does not match the
 *          collection's mime types.
 */
bool setItemPayload(Akonadi::Item &item, const KAEvent &event, const QStringList &collectionMimeTypes);

}

#endif // KALARMCAL_AKONADI_H
