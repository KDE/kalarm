/*
 *  dragdrop.h  -  drag and drop functions
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#ifndef DRAGDROP_H
#define DRAGDROP_H

namespace KAlarmCal { class AlarmText; }
namespace Akonadi { class Item; }
class QString;
class QUrl;
class QMimeData;

namespace DragDrop
{

/** Get plain text from a drag-and-drop object.
 *  @param data  Dropped data.
 *  @param text  Extracted text.
 *  @return  true if @p data contained plain text data, false if not.
*/
bool dropPlainText(const QMimeData* data, QString& text);

/** Check whether drag-and-drop data may contain an RFC822 message (Akonadi or not). */
bool mayHaveRFC822(const QMimeData* data);

/** Extract dragged and dropped RFC822 message data.
 *  If there is more than one message, only the first is extracted.
 *  @param data       Dropped data.
 *  @param alarmText  Extracted email data.
 *  @return  true if @p data contained RFC822 message data, false if not.
 */
bool dropRFC822(const QMimeData* data, KAlarmCal::AlarmText& alarmText);

/** Extract dragged and dropped Akonadi RFC822 message data.
 *  @param data       Dropped data.
 *  @param url        Receives the first URL in the data, or empty if data does
 *                    not provide URLs.
 *  @param item       Receives the Akonadi Item specified by @p url, or invalid
 *                    if not an Akonadi URL.
 *  @param alarmText  Extracted email data.
 *  @return  true if @p data contained RFC822 message data, false if not.
 */
bool dropAkonadiEmail(const QMimeData* data, QUrl& url, Akonadi::Item& item, KAlarmCal::AlarmText& alarmText);

} // namespace KAlarm

#endif // DRAGDROP_H

// vim: et sw=4:
