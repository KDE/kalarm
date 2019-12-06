/*
 *  dbushandler.h  -  handler for D-Bus calls by other applications
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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
 *  You should have received a copy of the GNU General Public License
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DBUSHANDLER_H
#define DBUSHANDLER_H

#include "kalarmiface.h"

#include <KAlarmCal/KAEvent>

#include <KCalendarCore/Duration>

class QUrl;

using namespace KAlarmCal;


class DBusHandler : public QObject, public KAlarmIface
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kalarm.kalarm")
    public:
        DBusHandler();

    public Q_SLOTS:
        Q_SCRIPTABLE bool cancelEvent(const QString& eventId);
        Q_SCRIPTABLE bool triggerEvent(const QString& eventId);
        Q_SCRIPTABLE QString list();

        Q_SCRIPTABLE bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                          const QString& bgColor, const QString& fgColor, const QString& font,
                                          const QString& audioUrl, int reminderMins, const QString& recurrence,
                                          int subRepeatInterval, int subRepeatCount);
        Q_SCRIPTABLE bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                          const QString& bgColor, const QString& fgColor, const QString& font,
                                          const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount);
        Q_SCRIPTABLE bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                          const QString& bgColor, const QString& fgColor, const QString& font,
                                          const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
        Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                       const QString& audioUrl, int reminderMins, const QString& recurrence,
                                       int subRepeatInterval, int subRepeatCount);
        Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                       const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount);
        Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                       const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
        Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                          const QString& recurrence, int subRepeatInterval, int subRepeatCount);
        Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                          int recurType, int recurInterval, int recurCount);
        Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                          int recurType, int recurInterval, const QString& endDateTime);
        Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                        const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                        const QString& recurrence, int subRepeatInterval, int subRepeatCount);
        Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                        const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                        int recurType, int recurInterval, int recurCount);
        Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                        const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                        int recurType, int recurInterval, const QString& endDateTime);
        Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                        unsigned flags, const QString& recurrence, int subRepeatInterval, int subRepeatCount);
        Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                        unsigned flags, int recurType, int recurInterval, int recurCount);
        Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                        unsigned flags, int recurType, int recurInterval, const QString& endDateTime);
        Q_SCRIPTABLE bool edit(const QString& eventID);
        Q_SCRIPTABLE bool editNew(int type);
        Q_SCRIPTABLE bool editNew(const QString& templateName);

    private:
        static bool scheduleMessage(const QString& message, const KADateTime& start, int lateCancel, unsigned flags,
                                    const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                    const QUrl& audioFile, int reminderMins, const KARecurrence&,
                                    const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
        static bool scheduleFile(const QUrl& file, const KADateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                                 const QUrl& audioFile, int reminderMins, const KARecurrence&,
                                 const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
        static bool scheduleCommand(const QString& commandLine, const KADateTime& start, int lateCancel, unsigned flags,
                                    const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
        static bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                  const QString& attachments, const KADateTime& start, int lateCancel, unsigned flags,
                                  const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
        static bool scheduleAudio(const QString& audioUrl, int volumePercent, const KADateTime& start, int lateCancel, unsigned flags,
                                  const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
        static KADateTime convertDateTime(const QString& dateTime, const KADateTime& = KADateTime());
        static KAEvent::Flags convertStartFlags(const KADateTime& start, unsigned flags);
        static QColor    convertBgColour(const QString& bgColor);
        static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, const QString& icalRecurrence, int subRepeatInterval, KCalendarCore::Duration& subRepeatDuration);
        static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, int recurCount);
        static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, const QString& endDateTime);
        static bool      convertRecurrence(KARecurrence&, const KADateTime& start, int recurType, int recurInterval, int recurCount, const KADateTime& end);
};

#endif // DBUSHANDLER_H

// vim: et sw=4:
