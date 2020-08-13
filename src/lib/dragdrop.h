/*
 *  dragdrop.h  -  drag and drop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
