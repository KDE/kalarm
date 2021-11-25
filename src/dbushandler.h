/*
 *  dbushandler.h  -  handler for D-Bus calls by other applications
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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

    // Create a display alarm with a specified text message.
    Q_SCRIPTABLE bool scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                      const QString& bgColor, const QString& fgColor, const QString& font,
                                      const QString& audioUrl, int reminderMins, const QString& recurrence,
                                      int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                      const QString& bgColor, const QString& fgColor, const QString& font,
                                      const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                      const QString& bgColor, const QString& fgColor, const QString& font,
                                      const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
    // Deprecated: Create a display alarm with a specified text message.
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

    // Create a display alarm showing a file's contents.
    Q_SCRIPTABLE bool scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, const QString& recurrence,
                                   int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);
    // Deprecated: Create a display alarm showing a file's contents.
    Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, const QString& recurrence,
                                   int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                                   const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime);

    // Create a command alarm.
    Q_SCRIPTABLE bool scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      int recurType, int recurInterval, const QString& endDateTime);
    // Deprecated: Create a command alarm.
    Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                      int recurType, int recurInterval, const QString& endDateTime);

    // Create an email alarm.
    Q_SCRIPTABLE bool scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    int recurType, int recurInterval, const QString& endDateTime);
    // Deprecated: Create an email alarm.
    Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                    const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                    int recurType, int recurInterval, const QString& endDateTime);

    // Create an audio alarm.
    Q_SCRIPTABLE bool scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, int recurType, int recurInterval, const QString& endDateTime);
    // Deprecated: Create an audio alarm.
    Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, const QString& recurrence, int subRepeatInterval, int subRepeatCount);
    Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, int recurType, int recurInterval, int recurCount);
    Q_SCRIPTABLE bool scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                    unsigned flags, int recurType, int recurInterval, const QString& endDateTime);

    // Edit an alarm.
    Q_SCRIPTABLE bool edit(const QString& eventID);
    Q_SCRIPTABLE bool editNew(int type);
    Q_SCRIPTABLE bool editNew(const QString& templateName);

private:
    static bool scheduleMessage(const QString& name, const QString& message, const KADateTime& start, int lateCancel, unsigned flags,
                                const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                const QUrl& audioFile, int reminderMins, const KARecurrence&,
                                const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
    static bool scheduleFile(const QString& name, const QUrl& file, const KADateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                             const QUrl& audioFile, int reminderMins, const KARecurrence&,
                             const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
    static bool scheduleCommand(const QString& name, const QString& commandLine, const KADateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
    static bool scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                              const QString& attachments, const KADateTime& start, int lateCancel, unsigned flags,
                              const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
    static bool scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const KADateTime& start, int lateCancel, unsigned flags,
                              const KARecurrence&, const KCalendarCore::Duration& subRepeatDuration = KCalendarCore::Duration(0), int subRepeatCount = 0);
    static KADateTime convertDateTime(const QString& dateTime, const KADateTime& = KADateTime());
    static KAEvent::Flags convertStartFlags(const KADateTime& start, unsigned flags);
    static QColor    convertBgColour(const QString& bgColor);
    static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, const QString& icalRecurrence, int subRepeatInterval, KCalendarCore::Duration& subRepeatDuration);
    static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, int recurCount);
    static bool      convertRecurrence(KADateTime& start, KARecurrence&, const QString& startDateTime, int recurType, int recurInterval, const QString& endDateTime);
    static bool      convertRecurrence(KARecurrence&, const KADateTime& start, int recurType, int recurInterval, int recurCount, const KADateTime& end);
};

// vim: et sw=4:
