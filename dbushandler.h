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

#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <KCalCore/duration.h>
#else
#include <kcal/duration.h>
#endif

class KUrl;

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
        static bool scheduleMessage(const QString& message, const KDateTime& start, int lateCancel, unsigned flags,
                                    const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                    const KUrl& audioFile, int reminderMins, const KARecurrence&,
#ifdef USE_AKONADI
                                    const KCalCore::Duration& subRepeatDuration = KCalCore::Duration(0), int subRepeatCount = 0);
#else
                                    const KCal::Duration& subRepeatDuration = KCal::Duration(0), int subRepeatCount = 0);
#endif
        static bool scheduleFile(const KUrl& file, const KDateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                                 const KUrl& audioFile, int reminderMins, const KARecurrence&,
#ifdef USE_AKONADI
                                 const KCalCore::Duration& subRepeatDuration = KCalCore::Duration(0), int subRepeatCount = 0);
#else
                                 const KCal::Duration& subRepeatDuration = KCal::Duration(0), int subRepeatCount = 0);
#endif
        static bool scheduleCommand(const QString& commandLine, const KDateTime& start, int lateCancel, unsigned flags,
#ifdef USE_AKONADI
                                    const KARecurrence&, const KCalCore::Duration& subRepeatDuration = KCalCore::Duration(0), int subRepeatCount = 0);
#else
                                    const KARecurrence&, const KCal::Duration& subRepeatDuration = KCal::Duration(0), int subRepeatCount = 0);
#endif
        static bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                  const QString& attachments, const KDateTime& start, int lateCancel, unsigned flags,
#ifdef USE_AKONADI
                                  const KARecurrence&, const KCalCore::Duration& subRepeatDuration = KCalCore::Duration(0), int subRepeatCount = 0);
#else
                                  const KARecurrence&, const KCal::Duration& subRepeatDuration = KCal::Duration(0), int subRepeatCount = 0);
#endif
        static bool scheduleAudio(const QString& audioUrl, int volumePercent, const KDateTime& start, int lateCancel, unsigned flags,
#ifdef USE_AKONADI
                                  const KARecurrence&, const KCalCore::Duration& subRepeatDuration = KCalCore::Duration(0), int subRepeatCount = 0);
#else
                                  const KARecurrence&, const KCal::Duration& subRepeatDuration = KCal::Duration(0), int subRepeatCount = 0);
#endif
        static KDateTime convertDateTime(const QString& dateTime, const KDateTime& = KDateTime());
        static KAEvent::Flags convertStartFlags(const KDateTime& start, unsigned flags);
        static QColor    convertBgColour(const QString& bgColor);
#ifdef USE_AKONADI
        static bool      convertRecurrence(KDateTime& start, KARecurrence&, const QString& startDateTime, const QString& icalRecurrence, int subRepeatInterval, KCalCore::Duration& subRepeatDuration);
#else
        static bool      convertRecurrence(KDateTime& start, KARecurrence&, const QString& startDateTime, const QString& icalRecurrence, int subRepeatInterval, KCal::Duration& subRepeatDuration);
#endif
        static bool      convertRecurrence(KDateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, int recurCount);
        static bool      convertRecurrence(KDateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, const QString& endDateTime);
        static bool      convertRecurrence(KARecurrence&, const KDateTime& start, int recurType, int recurInterval, int recurCount, const KDateTime& end);
};

#endif // DBUSHANDLER_H

// vim: et sw=4:
