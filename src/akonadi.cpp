/*
 *  akonadi.cpp  -  Akonadi object functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2011, 2019 David Jarvie <djarvie@kde.org>
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

#include "akonadi.h"
#include "kaevent.h"

#include <AkonadiCore/Item>

#include <QStringList>

namespace KAlarmCal
{

/******************************************************************************
* Initialise an Item with the event.
* Note that the event is not updated with the Item ID.
* Reply = true if successful,
*         false if event's category does not match collection's mime types.
*/
bool setItemPayload(Akonadi::Item &item, const KAEvent &event, const QStringList &collectionMimeTypes)
{
    QString mimetype;
    switch (event.category()) {
        case CalEvent::ACTIVE:    mimetype = MIME_ACTIVE;    break;
        case CalEvent::ARCHIVED:  mimetype = MIME_ARCHIVED;  break;
        case CalEvent::TEMPLATE:  mimetype = MIME_TEMPLATE;  break;
        default:                  Q_ASSERT(0);  return false;
    }
    if (!collectionMimeTypes.contains(mimetype)) {
        return false;
    }
    item.setMimeType(mimetype);
    item.setPayload<KAEvent>(event);
    return true;
}

}
