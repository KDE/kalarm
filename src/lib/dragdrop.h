/*
 *  dragdrop.h  -  drag and drop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kaevent.h"

#include <QtGlobal>

namespace KAlarmCal { class AlarmText; }
namespace KMime { class Content; }
class QString;
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

/** Convert a KMime email instance to AlarmText.
 *  @param content  email content.
 *  @param emailId  kmail email ID.
 *  @return email contents in AlarmText format.
 */
KAlarmCal::AlarmText kMimeEmailToAlarmText(KMime::Content& content, KAlarmCal::KAEvent::EmailId emailId);

} // namespace DragDrop

// vim: et sw=4:
