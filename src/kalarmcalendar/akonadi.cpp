/*
 *  akonadi.cpp  -  Akonadi object functions
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011, 2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "akonadi.h"
#include "kaevent.h"

#include <Akonadi/Item>

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

// vim: et sw=4:
