/*
 *  kaevent.h  -  represents calendar events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2001-2013 by David Jarvie <djarvie@kde.org>
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

#ifndef KAEVENT_H
#define KAEVENT_H

#include "kalarmcal_export.h"

#include "datetime.h"
#include "karecurrence.h"
#include "kacalendar.h"
#include "repetition.h"

#include <collection.h>
#include <item.h>
#include <kcalcore/person.h>
#include <kcalcore/calendar.h>

#include <QtCore/QBitArray>
#include <QColor>
#include <QFont>
#include <QtCore/QVector>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QMetaType>

namespace KHolidays
{
class HolidayRegion;
}

namespace KAlarmCal
{

/**
 * @short KAAlarm represents individual alarms within a KAEvent.
 *
 * The KAAlarm class represents one of the main or subsidiary alarms in
 * a KAEvent instance. It contains the alarm's type and trigger time.
 *
 * Note that valid KAAlarm instances can only be created by the KAEvent
 * class.
 *
 * @see KAEvent::alarm(), KAEvent::firstAlarm(), KAEvent::nextAlarm().
 *
 * @author David Jarvie <djarvie@kde.org>
 */
class KALARMCAL_EXPORT KAAlarm
{
public:
    /** The basic KAAlarm action types. */
    enum Action {
        MESSAGE,   //!< KCal::Alarm::Display type: display a text message
        FILE,      //!< KCal::Alarm::Display type: display a file (URL given by the alarm text)
        COMMAND,   //!< KCal::Alarm::Procedure type: execute a shell command
        EMAIL,     //!< KCal::Alarm::Email type: send an email
        AUDIO      //!< KCal::Alarm::Audio type: play a sound file
    };

    /** Alarm types.
     *  KAAlarm's of different types may be contained in a KAEvent,
     *  each KAAlarm defining a different component of the overall alarm.
     */
    enum Type {
        INVALID_ALARM       = 0,     //!< Not an alarm
        MAIN_ALARM          = 1,     //!< THE real alarm. Must be the first in the enumeration.
        REMINDER_ALARM      = 0x02,  //!< Reminder in advance of/after the main alarm
        DEFERRED_ALARM      = 0x04,  //!< Deferred alarm
        DEFERRED_REMINDER_ALARM = REMINDER_ALARM | DEFERRED_ALARM,  //!< Deferred reminder alarm
        // The following values must be greater than the preceding ones, to
        // ensure that in ordered processing they are processed afterwards.
        AT_LOGIN_ALARM      = 0x10,  //!< Additional repeat-at-login trigger
        DISPLAYING_ALARM    = 0x20   //!< Copy of the alarm currently being displayed

                              // IMPORTANT: if any values are added to this list, ensure that the
                              //            KAEventPrivate::AlarmType enum is adjusted similarly.
    };

    /** Default constructor, which creates an invalid instance. */
    KAAlarm();

    /** Copy constructor. */
    KAAlarm(const KAAlarm &other);

    /** Destructor. */
    ~KAAlarm();

    /** Assignment operator. */
    KAAlarm &operator=(const KAAlarm &other);

    /** Return the action type for the alarm. */
    Action action() const;

    /** Return whether the alarm is valid, i.e. whether it contains any alarm data. */
    bool isValid() const;

    /** Return the alarm's type (main, reminder, etc.). */
    Type type() const;

    /** Return the trigger time for the alarm.
     *  Sub-repetitions can optionally be ignored; in this case, if a sub-repetition
     *  is due next, the last main recurrence will be returned instead.
     *  @param withRepeats if true, returns the next sub-repetition time where appropriate;
     *                     if false, ignores sub-repetitions.
     */
    DateTime dateTime(bool withRepeats = false) const;

    /** Return the trigger date for the alarm.
     *  Sub-repetitions are ignored: if a sub-repetition is due next, the
     *  last main recurrence will be returned instead.
     */
    QDate date() const;

    /** Return the trigger time-of-day for the alarm.
     *  Sub-repetitions are ignored: if a sub-repetition is due next, the
     *  last main recurrence will be returned instead.
     *  @return trigger time-of-day. If the alarm is date-only, this will be
     *          the user-defined start-of-day time.
     */
    QTime time() const;

    /** Set the alarm's trigger time. */
    void setTime(const DateTime &dt);
    /** Set the alarm's trigger time. */
    void setTime(const KDateTime &dt);

    /** Return whether this is a repeat-at-login alarm. */
    bool repeatAtLogin() const;

    /** Return whether this is a reminder alarm. */
    bool isReminder() const;

    /** Return whether this is a deferred alarm. */
    bool deferred() const;

    /** Return whether in the case of a deferred alarm, it is timed (as
     *  opposed to date-only).
     *  @return @c true if a timed deferral alarm, @c false if date-only or not a deferred alarm.
     */
    bool timedDeferral() const;

    /** Return an alarm type as a string.
     *  @return alarm type string, or the empty string if debug output is disabled.
     */
    static const char *debugType(Type);

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond

    friend class KAEvent;
    friend class KAEventPrivate;
};

class KAEventPrivate;

/**
 * @short KAEvent represents a KAlarm event
 *
 * KAEvent represents a KAlarm event. An event contains a main alarm together
 * with optional subsidiary alarms such as reminders and deferrals. Individual
 * alarms are represented by the KAAlarm class. KAEvent includes the complete
 * definition of the event including recurrence information, and also holds
 * current status information such as the next due occurrence and command
 * execution error status. It provides methods to set and get the event
 * properties, to defer the alarm, to convert it for storage in the displaying
 * calendar.
 *
 * Methods which act globally or on multiple KAEvent instances include
 * convertKCalEvents() which converts events stored in an older KAlarm
 * calendar format to the current format; setStartOfDay() and adjustStartOfDay()
 * which set a new start-of-day time for date-only alarms; setHolidays()
 * and setWorkTime() which set holiday region and working days/hours.
 *
 * @author David Jarvie <djarvie@kde.org>
 */
class KALARMCAL_EXPORT KAEvent
{
public:
    /** A list of pointers to KAEvent objects. */
    typedef QVector<KAEvent *> List;

    /** Flags for use in D-Bus calls, etc. Flags may be combined by OR'ing them together. */
    enum Flag {
        BEEP            = 0x02,    //!< sound an audible beep when the alarm is displayed
        REPEAT_AT_LOGIN = 0x04,    //!< repeat the alarm at every login
        ANY_TIME        = 0x08,    //!< only a date is specified for the alarm, not a time
        CONFIRM_ACK     = 0x10,    //!< closing the alarm message window requires a confirmation prompt
        EMAIL_BCC       = 0x20,    //!< blind copy the email to the user
        DEFAULT_FONT    = 0x40,    //!< use the default alarm message font
        REPEAT_SOUND    = 0x80,    //!< repeat the sound file while the alarm is displayed
        DISABLED        = 0x100,   //!< the alarm is currently disabled
        AUTO_CLOSE      = 0x200,   //!< auto-close the alarm window after the late-cancel period
        SCRIPT          = 0x400,   //!< the command is a script, not a shell command line
        EXEC_IN_XTERM   = 0x800,   //!< execute the command in a terminal window
        SPEAK           = 0x1000,  //!< speak the message when the alarm is displayed
        COPY_KORGANIZER = 0x2000,  //!< KOrganizer should hold a copy of the event
        EXCL_HOLIDAYS   = 0x4000,  //!< don't trigger the alarm on holidays
        WORK_TIME_ONLY  = 0x8000,  //!< trigger the alarm only during working hours
        DISPLAY_COMMAND = 0x10000, //!< display command output in the alarm window
        REMINDER_ONCE   = 0x20000  //!< only trigger the reminder on the first recurrence

                          // IMPORTANT: if any values are added to this list, ensure that the
                          //            additional enum values in KAEventPrivate are also adjusted.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /** The basic action type(s) for the event's main alarm.
     *  Values may be combined by OR'ing them together. */
    enum Actions {
        ACT_NONE            = 0,      //!< invalid
        ACT_DISPLAY         = 0x01,   //!< the alarm displays something
        ACT_COMMAND         = 0x02,   //!< the alarm executes a command
        ACT_EMAIL           = 0x04,   //!< the alarm sends an email
        ACT_AUDIO           = 0x08,   //!< the alarm plays an audio file (without any display)
        ACT_DISPLAY_COMMAND = ACT_DISPLAY | ACT_COMMAND,  //!< the alarm displays command output
        ACT_ALL             = ACT_DISPLAY | ACT_COMMAND | ACT_EMAIL | ACT_AUDIO   //!< all types mask
    };

    /** The sub-action type for the event's main alarm. */
    enum SubAction {
        MESSAGE = KAAlarm::MESSAGE,    //!< display a message text
        FILE    = KAAlarm::FILE,       //!< display the contents of a file
        COMMAND = KAAlarm::COMMAND,    //!< execute a command
        EMAIL   = KAAlarm::EMAIL,      //!< send an email
        AUDIO   = KAAlarm::AUDIO       //!< play an audio file
    };

    /** What type of occurrence is due. */
    enum OccurType {
        NO_OCCURRENCE            = 0,      //!< no occurrence is due
        FIRST_OR_ONLY_OCCURRENCE = 0x01,   //!< the first occurrence (takes precedence over LAST_RECURRENCE)
        RECURRENCE_DATE          = 0x02,   //!< a recurrence with only a date, not a time
        RECURRENCE_DATE_TIME     = 0x03,   //!< a recurrence with a date and time
        LAST_RECURRENCE          = 0x04,   //!< the last recurrence
        OCCURRENCE_REPEAT        = 0x10,   //!< (bitmask for a sub-repetition of an occurrence)
        FIRST_OR_ONLY_OCCURRENCE_REPEAT = OCCURRENCE_REPEAT | FIRST_OR_ONLY_OCCURRENCE, //!< a sub-repetition of the first occurrence
        RECURRENCE_DATE_REPEAT          = OCCURRENCE_REPEAT | RECURRENCE_DATE,          //!< a sub-repetition of a date-only recurrence
        RECURRENCE_DATE_TIME_REPEAT     = OCCURRENCE_REPEAT | RECURRENCE_DATE_TIME,     //!< a sub-repetition of a date/time recurrence
        LAST_RECURRENCE_REPEAT          = OCCURRENCE_REPEAT | LAST_RECURRENCE           //!< a sub-repetition of the last recurrence
    };

    /** How to treat sub-repetitions in nextOccurrence(). */
    enum OccurOption {
        IGNORE_REPETITION,    //!< check for recurrences only, ignore sub-repetitions
        RETURN_REPETITION,    //!< return a sub-repetition if it's the next occurrence
        ALLOW_FOR_REPETITION  //!< if a sub-repetition is the next occurrence, return the previous recurrence, not the sub-repetition
    };

    /** What type of occurrence currently limits how long the alarm can be deferred. */
    enum DeferLimitType {
        LIMIT_NONE,        //!< there is no limit
        LIMIT_MAIN,        //!< the main alarm
        LIMIT_RECURRENCE,  //!< a recurrence
        LIMIT_REPETITION,  //!< a sub-repetition
        LIMIT_REMINDER     //!< a reminder
    };

    /** Alarm trigger type. */
    enum TriggerType {
        ALL_TRIGGER,       //!< next trigger, including reminders, ignoring working hours & holidays
        MAIN_TRIGGER,      //!< next trigger, excluding reminders, ignoring working hours & holidays
        WORK_TRIGGER,      //!< next main working time trigger, excluding reminders
        ALL_WORK_TRIGGER,  //!< next actual working time trigger, including reminders
        DISPLAY_TRIGGER    //!< next trigger time for display purposes (i.e. excluding reminders)
    };

    /** Command execution error type for last time the alarm was triggered. */
    enum CmdErrType {
        CMD_NO_ERROR   = 0,      //!< no error
        CMD_ERROR      = 0x01,   //!< command alarm execution failed
        CMD_ERROR_PRE  = 0x02,   //!< pre-alarm command execution failed
        CMD_ERROR_POST = 0x04,   //!< post-alarm command execution failed
        CMD_ERROR_PRE_POST = CMD_ERROR_PRE | CMD_ERROR_POST
    };

    /** Options for pre- or post-alarm actions. These may be OR'ed together.
     *  @since 4.9
     */
    enum ExtraActionOption {
        CancelOnPreActError  = 0x01,   //!< cancel alarm on pre-alarm action error
        DontShowPreActError  = 0x02,   //!< do not notify pre-alarm action errors to user
        ExecPreActOnDeferral = 0x04    //!< execute pre-alarm action also for deferred alarms
    };
    Q_DECLARE_FLAGS(ExtraActionOptions, ExtraActionOption)

    /** How to deal with the event UID in updateKCalEvent(). */
    enum UidAction {
        UID_IGNORE,        //!< leave KCal::Event UID unchanged
        UID_CHECK,         //!< verify that the KCal::Event UID is already the same as the KAEvent ID, if the latter is non-empty
        UID_SET            //!< set the KCal::Event UID to the KAEvent ID
    };

    /** Default constructor which creates an invalid event. */
    KAEvent();

    /** Construct an event and initialise with the specified parameters.
     *  @param dt    start date/time.
     *  @param text  alarm message (@p action = #MESSAGE);
     *               file to display (@p action = #FILE);
     *               command to execute (@p action = #COMMAND);
     *               email body (@p action = #EMAIL);
     *               audio file (@p action = #AUDIO).
     *  @param bg    background color (for display alarms, ignored otherwise).
     *  @param fg    foreground color (for display alarms, ignored otherwise).
     *  @param font  font (for display alarms, ignored otherwise).
     *  @param action         alarm action type.
     *  @param lateCancel     late-cancellation period (minutes), else 0.
     *  @param flags          OR of #Flag enum values.
     *  @param changesPending true to inhibit automatic calculations and data
     *                        updates until further changes have been applied
     *                        to the instance; call endChanges() when changes
     *                        are complete.
     */
    KAEvent(const KDateTime &, const QString &text, const QColor &bg, const QColor &fg,
            const QFont &f, SubAction, int lateCancel, Flags flags, bool changesPending = false);
    /** Construct an event and initialise it from a KCalCore::Event. */
    explicit KAEvent(const KCalCore::Event::Ptr &);

    /** Initialise the instance from a KCalCore::Event. */
    void set(const KCalCore::Event::Ptr &);

    KAEvent(const KAEvent &other);
    ~KAEvent();

    KAEvent &operator=(const KAEvent &other);

    /** Initialise the instance with the specified parameters.
     *  @param dt    start date/time
     *  @param text  alarm message (@p action = #MESSAGE);
     *               file to display (@p action = #FILE);
     *               command to execute (@p action = #COMMAND);
     *               email body (@p action = #EMAIL);
     *               audio file (@p action = #AUDIO)
     *  @param bg    background color (for display alarms, ignored otherwise)
     *  @param fg    background color (for display alarms, ignored otherwise)
     *  @param font  font (for display alarms, ignored otherwise)
     *  @param action         alarm action type
     *  @param lateCancel     late-cancellation period (minutes), else 0
     *  @param flags          OR of #Flag enum values
     *  @param changesPending true to inhibit automatic data updates until
     *                        further changes have been applied to the instance;
     *                        call endChanges() when changes are complete.
     */
    void set(const KDateTime &dt, const QString &text, const QColor &bg,
             const QColor &fg, const QFont &font, SubAction action, int lateCancel,
             Flags flags, bool changesPending = false);

    /** Update an existing KCalCore::Event with the KAEvent data.
     *  @param event  Event to update.
     *  @param u      how to deal with the Event's UID.
     *  @param setCustomProperties if true, all the Event's existing custom
     *                             properties are cleared and replaced with the
     *                             KAEvent's custom properties. If false, the
     *                             KCal::Event's non-KAlarm custom properties
     *                             are left untouched.
     */
    bool updateKCalEvent(const KCalCore::Event::Ptr &event, UidAction u, bool setCustomProperties = true) const;

    /** Return whether the instance represents a valid event. */
    bool isValid() const;

    /** Enable or disable the alarm. */
    void setEnabled(bool enable);
    /** Return the enabled status of the alarm. */
    bool enabled() const;

    /** Set the read-only status of the alarm. */
    void setReadOnly(bool ro);
    /** Return the read-only status of the alarm. */
    bool isReadOnly() const;

    /** Set the event to be archived when it expires or is deleted.
     *  Normally this is set when the event has triggered at least once.
     */
    void setArchive();
    /** Return whether the event should be archived when it expires or is deleted. */
    bool toBeArchived() const;

    /** Return whether the event's main alarm has expired. If so, a deferral alarm will exist. */
    bool mainExpired() const;
    /** Return whether the event has expired.
     *  @return @c true if the event has expired and is currently being displayed,
     *               or it is an archived event.
     */
    bool expired() const;

    /** Return the OR of various Flag enum status values. */
    Flags flags() const;

    /** Set the alarm category (active/archived/template, or displaying). */
    void setCategory(CalEvent::Type type);

    /** Return the alarm category (active/archived/template, or displaying). */
    CalEvent::Type category() const;

    /** Set the event's unique identifier. Note that the UID is guaranteed to be unique
     *  only within the calendar containing the event.
     */
    void setEventId(const QString &id);

    /** Return the event's unique identifier. Note that the UID is guaranteed to be unique
     *  only within the calendar containing the event.
     */
    QString id() const;

    /** Increment the revision number of the event (SEQUENCE property in iCalendar). */
    void incrementRevision();
    /** Return the revision number of the event (SEQUENCE property in iCalendar). */
    int revision() const;

    /** Set the ID of the Akonadi Collection which contains the event. */
    void setCollectionId(Akonadi::Collection::Id id);
    /** Set the ID of the Akonadi Collection which contains the event.
     *  @warning This is a const method, which means that any other instance
     *           which references the same shared data will also be
     *           updated. It is designed to be used when a KAEvent is
     *           being created from an Akonadi Item, to avoid unnecessary
     *           copying. Use with caution!
     */
    void setCollectionId_const(Akonadi::Collection::Id id) const;
    /** Return the ID of the Akonadi Collection which contains the event. */
    Akonadi::Collection::Id  collectionId() const;

    /** Set the ID of the Akonadi Item which contains the event. */
    void setItemId(Akonadi::Item::Id id);
    /** Return the ID of the Akonadi Item which contains the event. */
    Akonadi::Item::Id  itemId() const;

    /** Initialise an Akonadi::Item with the event's data.
     *  Note that the event is not updated with the Item ID.
     *  @return @c true if successful; @c false if the event's category does not match the
     *          collection's mime types.
     */
    bool setItemPayload(Akonadi::Item &, const QStringList &collectionMimeTypes) const;

    /** Note the event's storage format compatibility compared to the current KAlarm calendar format. */
    void setCompatibility(KACalendar::Compat c);
    /** Return the event's storage format compatibility compared to the current KAlarm calendar format. */
    KACalendar::Compat compatibility() const;

    /** Return the original KCalCore::Event's custom properties in the source calendar. */
    QMap<QByteArray, QString> customProperties() const;

    /** Return the action sub-type of the event's main alarm. For display alarms,
     *  this is MESSAGE or FILE, while other types of alarm simply return the
     *  basic action type (COMMAND, EMAIL, AUDIO).
     *  Note that for a display alarm whose text is generated by a command, the
     *  returned type is COMMAND.
     */
    SubAction actionSubType() const;

    /** Return the OR of the basic action types of the event's main alarm (display,
     *  command, email, audio).
     *  Note that for a display alarm whose text is generated by a command, the
     *  returned type is @c ACT_DISPLAY|ACT_COMMAND.
     */
    Actions actionTypes() const;

    /** Set or clear the late-cancel option. This determines whether the alarm
     *  will be cancelled if it is late in triggering.
     *  @param minutes  late cancellation period in minutes, or 0 to clear
     *  @see lateCancel()
     */
    void setLateCancel(int minutes);

    /** Get the late cancellation period. This is how late the alarm can
     *  trigger after its scheduled time, before it will be cancelled.
     *  @return period in minutes, or 0 if no late cancellation is specified
     *  @see setLateCancel()
     */
    int lateCancel() const;

    /** Enable or disable auto-close for a display alarm, i.e. whether the
     *  alarm window will be closed on expiry of the late-cancellation
     *  time. Note that auto-close will only take effect if the late-cancel
     *  option is also set.
     *  @see setLateCancel(), autoClose()
     */
    void setAutoClose(bool autoclose);

    /** Return whether auto-close is enabled, i.e. whether the alarm window
     *  will be closed on expiry of the late-cancellation time. Note that
     *  auto-close will only operate if in addition to being enabled,
     *  late-cancel is also set.
     *  @return @c true if it is a display alarm and auto-close is enabled.
     *  @see lateCancel(), setAutoClose()
     */
    bool autoClose() const;

    void               setKMailSerialNumber(unsigned long n);
    unsigned long      kmailSerialNumber() const;

    /** Return the alarm's text. Its significance depends on the type of alarm;
     *  alternatively, use message(), displayMessage(), fileName() or command(),
     *  which incorporate checks on alarm type.
     */
    QString cleanText() const;
    /** Return the message text for a display alarm, or the email body for
     *  an email alarm.
     *  @return message/email text, or empty if not a display or email alarm. */
    QString message() const;
    /** Return the message text for a display alarm.
     *  @return message text, or empty if not a text display alarm. */
    QString displayMessage() const;
    /** Return the path of the file whose contents are to be shown, for a display alarm.
     *  @return file path, or empty if not a file display alarm. */
    QString fileName() const;

    /** Return the message window background color, for a display alarm. */
    QColor bgColour() const;
    /** Return the message window foreground color, for a display alarm. */
    QColor fgColour() const;

    /** Set the global default font for alarm message texts. */
    static void setDefaultFont(const QFont &font);
    /** Return whether to use the default font (as set by setDefaultFont())
     *  for alarm message texts. */
    bool useDefaultFont() const;
    /** Return the font to use for alarm message texts. */
    QFont font() const;

    /** Return the command or script to execute, for a command alarm.
     *  @return command, or empty if not a command alarm. */
    QString command() const;
    /** Return whether a command script is specified, for a command alarm. */
    bool commandScript() const;
    /** Return whether to execute the command in a terminal window, for a command alarm. */
    bool commandXterm() const;
    /** Return whether the command output is to be displayed in an alarm message window. */
    bool commandDisplay() const;
    /** Set or clear the command execution error for the last time the alarm triggered. */
    void setCommandError(CmdErrType error) const;
    /** Return the command execution error for the last time the alarm triggered. */
    CmdErrType commandError() const;

    /** Set the log file to write command alarm output to.
     *  @param logfile  log file path
     */
    void setLogFile(const QString &logfile);
    /** Return the log file which command alarm output should be written to.
     *  @return log file path, or empty if no log file. */
    QString logFile() const;

    /** Return whether alarm acknowledgement must be confirmed by the user, for a display alarm. */
    bool confirmAck() const;

    /** Return whether KOrganizer should hold a copy of the event. */
    bool copyToKOrganizer() const;

    /** Set the email related data for the event. */
    void setEmail(uint from, const KCalCore::Person::List &, const QString &subject,
                  const QStringList &attachments);

    /** Return the email message body, for an email alarm.
     *  @return email body, or empty if not an email alarm
     */
    QString emailMessage() const;

    /** Return the email identity to be used as the sender, for an email alarm.
     *  @return email UOID
     */
    uint emailFromId() const;

    /** Return the list of email addressees, including names, for an email alarm. */
    KCalCore::Person::List emailAddressees() const;

    /** Return a list of the email addresses, including names, for an email alarm. */
    QStringList emailAddresses() const;

    /** Return a string containing the email addressees, including names, for an email alarm.
     *  @param sep  separator string to insert between addresses.
     */
    QString emailAddresses(const QString &sep) const;

    /** Concatenate a list of email addresses into a string.
     *  @param sep  separator string to insert between addresses.
     */
    static QString joinEmailAddresses(const KCalCore::Person::List &addresses, const QString &sep);
    /** Return the list of email addressees, excluding names, for an email alarm. */
    QStringList emailPureAddresses() const;

    /** Return a string containing the email addressees, excluding names, for an email alarm.
     *  @param sep  separator string to insert between addresses.
     */
    QString emailPureAddresses(const QString &sep) const;

    /** Return the email subject line, for an email alarm. */
    QString emailSubject() const;

    /** Return the list of file paths of the attachments, for an email alarm. */
    QStringList emailAttachments() const;

    /** Return the file paths of the attachments, as a string, for an email alarm.
     *  @param sep  string separator
     */
    QString emailAttachments(const QString &sep) const;

    /** Return whether to send a blind copy of the email to the sender, for an email alarm. */
    bool emailBcc() const;

    /** Set the audio file related data for the event.
     *  @param filename       audio file path
     *  @param volume         final volume (0 - 1), or -1 for default volume
     *  @param fadeVolume     initial volume (0 - 1), or -1 for no fade
     *  @param fadeSeconds    number of seconds to fade from @p fadeVolume to @p volume
     *  @param repeatPause    number of seconds to pause between repetitions, or -1 if no repeat
     *  @param allowEmptyFile true to set the volume levels even if @p filename is empty
     *  @see audioFile(), soundVolume(), fadeVolume(), fadeSeconds()
     */
    void setAudioFile(const QString &filename, float volume, float fadeVolume,
                      int fadeSeconds, int repeatPause = -1, bool allowEmptyFile = false);

    /** Return the audio file path.
     *  @see setAudioFile()
     */
    QString audioFile() const;

    /** Return the sound volume (the final volume if fade is specified).
     *  @return volume in range 0 - 1, or -1 for default volume.
     *  @see setAudioFile()
     */
    float soundVolume() const;

    /** Return the initial volume which will fade to the final volume.
     *  @return volume in range 0 - 1, or -1 if no fade specified.
     *  @see setAudioFile()
     */
    float fadeVolume() const;

    /** Return the fade period in seconds, or 0 if no fade is specified.
     *  @see setAudioFile()
     */
    int fadeSeconds() const;

    /** Return whether the sound file will be repeated indefinitely. */
    bool repeatSound() const;

    /** Return how many seconds to pause between repetitions of the sound file.
     *  @return pause interval, or -1 if sound does not repeat.
     */
    int repeatSoundPause() const;

    /** Return whether a beep should sound when the alarm is displayed. */
    bool beep() const;

    /** Return whether the displayed alarm text should be spoken. */
    bool speak() const;

    /** Set the event to be an alarm template.
     *  @param name      template's name
     *  @param afterTime number of minutes after default time to schedule alarm for, or
     *                   -1 to not use 'time from now'
     *  @see isTemplate(), templateName()
     */
    void setTemplate(const QString &name, int afterTime = -1);

    /** Return whether the event is an alarm template.
     *  @see setTemplate()
     */
    bool isTemplate() const;

    /** Return the alarm template's name.
     *  @return template name, or empty if not a template
     *  @see setTemplate()
     */
    QString templateName() const;

    /** Return whether the alarm template does not specify a time.
     *  @return @c true if no time is specified, i.e. the normal default alarm time will
     *          be used, @c false if the template specifies a time.
     */
    bool usingDefaultTime() const;

    /** Return the number of minutes (>= 0) after the default alarm time which is
     *  specified in the alarm template. If this is specified, an alarm based on
     *  this template wll have the "Time from now" radio button enabled in the alarm
     *  edit dialog.
     *  @return minutes after the default time, or -1 if the template specifies a time
     *          of day or a date-only alarm.
     */
    int templateAfterTime() const;

    /** Set the pre-alarm and post-alarm actions, and their options.
     *  @param pre  shell command to execute before the alarm is displayed
     *  @param post shell command to execute after the alarm is acknowledged
     *  @param options options for pre- or post-alarm actions
     *  @see preAction(), postAction(), cancelOnPreActionError(), dontShowPreActionError()
     *  @since 4.9
     */
    void setActions(const QString &pre, const QString &post, ExtraActionOptions options);

    /** Set the pre-alarm and post-alarm actions, and their options.
     *  @param pre  shell command to execute before the alarm is displayed
     *  @param post shell command to execute after the alarm is acknowledged
     *  @param cancelOnError true to cancel the alarm if the pre-alarm action fails
     *  @param dontShowError true to not notify the error if the pre-alarm action fails
     *  @see preAction(), postAction(), cancelOnPreActionError(), dontShowPreActionError()
     *  @deprecated Use alternative form of setActions() instead.
     */
    void setActions(const QString &pre, const QString &post, bool cancelOnError, bool dontShowError);

    /** Return the shell command to execute before the alarm is displayed. */
    QString preAction() const;

    /** Return the shell command to execute after the display alarm is acknowledged.
     *  @see setActions()
     */
    QString postAction() const;

    /** Return the pre- and post-alarm action options.
     *  @see preAction(), postAction(), setActions()
     *  @since 4.9
     */
    ExtraActionOptions extraActionOptions() const;

    /** Return whether the alarm is to be cancelled if the pre-alarm action fails.
     *  @see preAction(), setActions()
     *  @deprecated Use preActionOptions() instead
     */
    bool cancelOnPreActionError() const;

    /** Return whether the user should not be notified if the pre-alarm action fails.
     *  @return @c true if the user will not be notified, @c false if the user will be notified
     *  @see preAction(), setActions()
     *  @deprecated Use preActionOptions() instead
     */
    bool dontShowPreActionError() const;

    /** Set an additional reminder alarm.
     *  @param minutes  number of minutes BEFORE the main alarm; if negative, the reminder
     *                  will occur AFTER the main alarm.
     *                  0 = clear the reminder.
     *  @param onceOnly true to trigger a reminder only for the first recurrence.
     *  @see reminderMinutes(), reminderOnceOnly()
     */
    void setReminder(int minutes, bool onceOnly);

    /** If there is a reminder which occurs AFTER the main alarm,
     *  activate the event's reminder which occurs after the given main alarm time.
     *  If there is no reminder after the main alarm, this method does nothing.
     *  @return @c true if successful (i.e. reminder falls before the next main alarm).
     */
    void activateReminderAfter(const DateTime &mainAlarmTime);

    /** Return the number of minutes BEFORE the main alarm when a reminder alarm is set.
     *  @return >0 if the reminder is before the main alarm;
     *          <0 if the reminder is after the main alarm;
     *          0  if no reminder is configured.
     *  @see setReminder()
     */
    int reminderMinutes() const;
    /** Return whether a reminder is currently due (before the next, or after the last,
     *  main alarm/recurrence).
     *  @see reminderDeferral()
     */
    bool reminderActive() const;
    /** Return whether the reminder alarm is triggered only for the first recurrence.
     *  @see setReminder()
     */
    bool reminderOnceOnly() const;
    /** Return whether there is currently a deferred reminder alarm pending. */
    bool reminderDeferral() const;

    /** Defer the event to the specified time.
     *  If the main alarm time has passed, the main alarm is marked as expired.
     *  @param dt               date/time to defer the event to
     *  @param reminder         true if deferring a reminder alarm
     *  @param adjustRecurrence if true, ensure that the next scheduled recurrence is
     *                          after the current time.
     *
     *  @see cancelDefer(), deferred(), deferDateTime()
     */
    void defer(const DateTime &dt, bool reminder, bool adjustRecurrence = false);

    /** Cancel any deferral alarm which is pending.
     *  @see defer()
     */
    void cancelDefer();
    /** Set defaults for the deferral dialog.
     *  @param minutes   default number of minutes, or 0 to select time control.
     *  @param dateOnly  true to select date-only by default.
     *  @see deferDefaultMinutes()
     */
    void setDeferDefaultMinutes(int minutes, bool dateOnly = false);
    /** Return whether there is currently a deferred alarm pending.
     *  @see defer(), deferDateTime()
     */
    bool deferred() const;
    /** Return the time at which the currently pending deferred alarm should trigger.
     *  @return trigger time, or invalid if no deferral pending.
     *  @see defer(), deferred()
     */
    DateTime deferDateTime() const;

    /** Return the latest time which the alarm can currently be deferred to.
     *  @param limitType  if non-null, pointer to variable which will be updated to hold
     *                    the type of occurrence which currently limits the deferral.
     *  @return deferral limit, or invalid if no limit
     */
    DateTime deferralLimit(DeferLimitType *limitType = 0) const;

    /** Return the default deferral interval used in the deferral dialog.
     *  @see setDeferDefaultMinutes()
     */
    int deferDefaultMinutes() const;
    /** Return the default date-only setting used in the deferral dialog. */
    bool deferDefaultDateOnly() const;

    /** Return the start time for the event. If the event recurs, this is the
     *  time of the first recurrence.
     *  @see mainDateTime()
     */
    DateTime startDateTime() const;
    /** Set the next time to trigger the alarm (excluding sub-repetitions).
     *  Note that for a recurring event, this should match one of the
     *  recurrence times.
     */
    void setTime(const KDateTime &dt);
    /** Return the next time the main alarm will trigger.
     *  @param withRepeats  true to include sub-repetitions, false to exclude them.
     *  @see mainTime(), startDateTime(), setTime()
     */
    DateTime mainDateTime(bool withRepeats = false) const;

    /** Return the time at which the main alarm will next trigger.
     *  Sub-repetitions are ignored. */
    QTime mainTime() const;
    /** Return the time at which the last sub-repetition of the main
     *  alarm will occur.
     *  @return last sub-repetition time, or main alarm time if no
     *          sub-repetitions are configured.
     */
    DateTime mainEndRepeatTime() const;

    /** Set the start-of-day time used by all date-only alarms.
     *  Note that adjustStartOfDay() should be called immediately after this,
     *  to adjust all events' internal data.
     */
    static void setStartOfDay(const QTime &);

    /** Call when the user changes the start-of-day time, to adjust the data
     *  for each date-only event in a list.
     *  @param events list of events. Any date-time events in the list are ignored.
     *  @see setStartOfDay()
     */
    static void adjustStartOfDay(const KAEvent::List &events);

    /** Return the next time the alarm will trigger.
     *  @param type specifies whether to ignore reminders, working time
     *              restrictions, etc.
     */
    DateTime nextTrigger(TriggerType type) const;

    /** Set the date/time the event was created, or saved in the archive calendar.
     *  @see createdDateTime()
     */
    void setCreatedDateTime(const KDateTime &dt);
    /** Return the date/time the event was created, or saved in the archive calendar.
     *  @see setCreatedDateTime()
     */
    KDateTime createdDateTime() const;

    /** Enable or disable repeat-at-login.
     *  If @p repeat is true, any existing pre-alarm reminder, late-cancel and
     *  copy-to-KOrganizer will all be disabled.
     *  @see repeatAtLogin()
     */
    void setRepeatAtLogin(bool repeat);

    /** Return whether the alarm repeats at login.
     *  @param includeArchived  true to also test for archived repeat-at-login status,
     *                          false to test only for a current repeat-at-login alarm.
     *  @see setRepeatAtLogin()
     */
    bool repeatAtLogin(bool includeArchived = false) const;

    /** Enable or disable the alarm on holiday dates. The currently selected
     *  holiday region determines which dates are holidays.
     *  Note that this option only has any effect for recurring alarms.
     *  @param exclude  true to disable on holidays, false to enable
     *  @see holidaysExcluded(), setHolidays()
     */
    void setExcludeHolidays(bool exclude);
    /** Return whether the alarm is disabled on holiday dates.
     *  @see setExcludeHolidays()
     */
    bool holidaysExcluded() const;

    /** Set the holiday region to be used by all KAEvent instances.
     *  Alarms which exclude holidays record the pointer to the holiday definition
     *  at the time their next trigger times were last calculated. The change in
     *  holiday definition pointer will cause their next trigger times to be
     *  recalculated.
     *  @param region  the holiday region data. The data object must persist for
     *                 the lifetime of the application, since this class just
     *                 stores a pointer to @p region.
     *  @see setExcludeHolidays()
     */
    static void setHolidays(const KHolidays::HolidayRegion &region);

    /** Enable or disable the alarm on non-working days and outside working hours.
     *  Note that this option only has any effect for recurring alarms.
     *  @param exclude  true to restrict to working time, false to enable any time
     *  @see workTimeOnly(), setWorkTime()
     */
    void setWorkTimeOnly(bool wto);
    /** Return whether the alarm is disabled on non-working days and outside working hours.
     *  @see setWorkTimeOnly()
     */
    bool workTimeOnly() const;

    /** Check whether a date/time is during working hours and/or holidays, depending
     *  on the flags set for the specified event. */
    bool isWorkingTime(const KDateTime &dt) const;

    /** Set working days and times, to be used by all KAEvent instances.
     *  @param days   bits set to 1 for each working day. Array element 0 = Monday ... 6 = Sunday.
     *  @param start  start time in working day.
     *  @param end    end time in working day.
     *  @see setWorkTimeOnly(), isWorkingTime()
     */
    static void setWorkTime(const QBitArray &days, const QTime &start, const QTime &end);

    /** Clear the event's recurrence and sub-repetition data.
     *  @see setRecurrence(), recurs()
     */
    void setNoRecur();

    /** Initialise the event's recurrence from a KARecurrence.
     *  The event's start date/time is not changed.
     *  @see setRecurMinutely(), setRecurDaily(), setRecurWeekly(), setRecurMonthlyByDate(), setRecurMonthlyByPos(), setRecurAnnualByDate(), setRecurAnnualByPos(), setFirstRecurrence()
     */
    void setRecurrence(const KARecurrence &r);

    /** Set the recurrence to recur at a minutes interval.
     *  @param freq   how many minutes between recurrences.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date/time (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMinutely(int freq, int count, const KDateTime &end);

    /** Set the recurrence to recur daily.
     *  @param freq   how many days between recurrences.
     *  @param days   which days of the week alarms are allowed to occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurDaily(int freq, const QBitArray &days, int count, const QDate &end);

    /** Set the recurrence to recur weekly, on the specified weekdays.
     *  @param freq   how many weeks between recurrences.
     *  @param days   which days of the week alarms are allowed to occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurWeekly(int freq, const QBitArray &days, int count, const QDate &end);

    /** Set the recurrence to recur monthly, on the specified days within the month.
     *  @param freq   how many months between recurrences.
     *  @param days   which days of the month alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMonthlyByDate(int freq, const QVector<int> &days, int count, const QDate &end);

    /** Holds days of the week combined with a week number in the month,
     *  used to specify some monthly or annual recurrences. */
    struct MonthPos {
        MonthPos() : days(7) {}    //krazy:exclude=inline (need default constructor)
        int        weeknum;     //!< Week in month, or < 0 to count from end of month.
        QBitArray  days;        //!< Days in week, element 0 = Monday.
    };

    /** Set the recurrence to recur monthly, on the specified weekdays in the
     *  specified weeks of the month.
     *  @param freq   how many months between recurrences.
     *  @param days   which days of the week/weeks of the month alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMonthlyByPos(int freq, const QVector<MonthPos> &pos, int count, const QDate &end);

    /** Set the recurrence to recur annually, on the specified day in each
     *  of the specified months.
     *  @param freq   how many years between recurrences.
     *  @param months which months of the year alarms should occur on.
     *  @param day    day of month, or 0 to use event start date.
     *  @param feb29  for a February 29th recurrence, when February 29th should
     *                recur in non-leap years.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurAnnualByDate(int freq, const QVector<int> &months, int day, KARecurrence::Feb29Type, int count, const QDate &end);

    /** Set the recurrence to recur annually, on the specified weekdays in the
     *  specified weeks of the specified months.
     *  @param freq   how many years between recurrences.
     *  @param days   which days of the week/weeks of the month alarms should occur on.
     *  @param months which months of the year alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurAnnualByPos(int freq, const QVector<MonthPos> &pos, const QVector<int> &months, int count, const QDate &end);

    /** Return whether the event recurs.
     *  @see recurType()
     */
    bool recurs() const;
    /** Return the recurrence period type for the event.
     *  Note that this does not test for repeat-at-login.
     *  @see recurInterval()
     */
    KARecurrence::Type recurType() const;
    /** Return the full recurrence data for the event.
     *  @return recurrence data, or null if none.
     *  @see recurrenceText()
     */
    KARecurrence *recurrence() const;

    /** Return the recurrence interval in units of the recurrence period type
     *  (minutes, days, etc).
     *  @see longestRecurrenceInterval()
     */
    int recurInterval() const;

    /** Return the longest interval which can occur between consecutive recurrences.
     *  @see recurInterval()
     */
    KCalCore::Duration longestRecurrenceInterval() const;

    /** Adjust the event date/time to the first recurrence of the event, on or after
     *  the event start date/time. The event start date may not be a recurrence date,
     *  in which case a later date will be set.
     */
    void setFirstRecurrence();

    /** Return the recurrence interval as text suitable for display. */
    QString recurrenceText(bool brief = false) const;

    /** Initialise the event's sub-repetition.
    *  The repetition length is adjusted if necessary to fit the recurrence interval.
    *  If the event doesn't recur, the sub-repetition is cleared.
    *  @return @c false if a non-daily interval was specified for a date-only recurrence.
    *  @see repetition()
    */
    bool setRepetition(const Repetition &r);

    /** Return the event's sub-repetition data.
     *  @see setRepetition(), repetitionText()
     */
    Repetition repetition() const;

    /** Return the count of the next sub-repetition which is due.
     *  @return sub-repetition count (>=1), or 0 for the main recurrence.
     *  @see nextOccurrence()
     */
    int nextRepetition() const;

    /** Return the repetition interval as text suitable for display. */
    QString repetitionText(bool brief = false) const;

    /** Determine whether the event will occur after the specified date/time.
     *  @param includeRepetitions if true and the alarm has a sub-repetition, the
     *                            method will return true if any sub-repetitions
     *                            occur after @p preDateTime.
     *  @see nextOccurrence()
     */
    bool occursAfter(const KDateTime &preDateTime, bool includeRepetitions) const;

    /** Set the date/time of the event to the next scheduled occurrence after a
     *  specified date/time, provided that this is later than its current date/time.
     *  Any reminder alarm is adjusted accordingly.
     *  If the alarm has a sub-repetition, and a sub-repetition of a previous
     *  recurrence occurs after the specified date/time, that sub-repetition is
     *  set as the next occurrence.
     *  @see nextOccurrence()
     */
    OccurType setNextOccurrence(const KDateTime &preDateTime);

    /** Get the date/time of the next occurrence of the event, after the specified
     *  date/time.
     *  @param result  date/time of next occurrence, or invalid date/time if none.
     *  @param option  how/whether to make allowance for sub-repetitions.
     *  @see nextRepetition(), setNextOccurrence(), previousOccurrence(), occursAfter()
     */
    OccurType nextOccurrence(const KDateTime &preDateTime, DateTime &result, OccurOption option = IGNORE_REPETITION) const;

    /** Get the date/time of the last previous occurrence of the event, before the
     *  specified date/time.
     *  @param result              date/time of previous occurrence, or invalid
     *                             date/time if none.
     *  @param includeRepetitions  if true and the alarm has a sub-repetition, the
     *                             last previous repetition is returned if
     *                             appropriate.
     *  @see nextOccurrence()
     */
    OccurType previousOccurrence(const KDateTime &afterDateTime, DateTime &result, bool includeRepetitions = false) const;

    /** Set the event to be a copy of the specified event, making the specified
     *  alarm the 'displaying' alarm.
     *  The purpose of setting up a 'displaying' alarm is to be able to reinstate
     *  the alarm message in case of a crash, or to reinstate it should the user
     *  choose to defer the alarm. Note that even repeat-at-login alarms need to be
     *  saved in case their end time expires before the next login.
     *  @param event      the event to copy
     *  @param type       the alarm type (main, reminder, deferred etc.)
     *  @param colId      the ID of the collection which originally contained the event
     *  @param repeatAtLoginTime repeat-at-login time if @p type == AT_LOGIN_ALARM, else ignored
     *  @param showEdit   whether the Edit button was displayed
     *  @param showDefer  whether the Defer button was displayed
     *  @return @c true if successful, @c false if alarm was not copied.
     */
    bool setDisplaying(const KAEvent &event, KAAlarm::Type type, Akonadi::Collection::Id colId, const KDateTime &repeatAtLoginTime, bool showEdit, bool showDefer);

    /** Reinstate the original event from the 'displaying' event.
     *  This instance is initialised from the supplied displaying @p event,
     *  and appropriate adjustments are made to convert it back to the
     *  original pre-displaying state.
     *  @param event      the displaying event
     *  @param colId      updated to the ID of the collection which originally contained the event
     *  @param showEdit   updated to true if Edit button was displayed, else false
     *  @param showDefer  updated to true if Defer button was displayed, else false
     */
    void reinstateFromDisplaying(const KCalCore::Event::Ptr &event, Akonadi::Collection::Id &colId, bool &showEdit, bool &showDefer);

    /** Return the original alarm which the displaying alarm refers to.
     *  Note that the caller is responsible for ensuring that the event was
     *  a displaying event; this check is not made in convertDisplayingAlarm()
     *  since it is normally called after reinstateFromDisplaying(), which
     *  resets the instance so that displaying() returns false.
     */
    KAAlarm convertDisplayingAlarm() const;

    /** Return whether the alarm is currently being displayed, i.e. is in the displaying calendar. */
    bool displaying() const;

    /** Return the alarm of a specified type.
     *  @param type  alarm type to return.
     *  @see nextAlarm(), alarmCount()
     */
    KAAlarm alarm(KAAlarm::Type type) const;

    /** Return the main alarm for the event.
     *  If the main alarm does not exist, one of the subsidiary ones is returned if
     *  possible.
     *  N.B. a repeat-at-login alarm can only be returned if it has been read from/
     *  written to the calendar file.
     *  @see nextAlarm()
     */
    KAAlarm firstAlarm() const;

    /** Return the next alarm for the event, after the specified alarm.
     *  @see firstAlarm()
     */
    KAAlarm nextAlarm(const KAAlarm &previousAlarm) const;
    /** Return the next alarm for the event, after the specified alarm type.
     *  @see firstAlarm()
     */
    KAAlarm nextAlarm(KAAlarm::Type previousType) const;

    /** Return the number of alarms in the event, i.e. the count of:
     *   - main alarm
     *   - repeat-at-login alarm
     *   - deferral alarm
     *   - reminder alarm
     *   - displaying alarm
     */
    int alarmCount() const;

    /** Remove the alarm of the specified type from the event.
     *  This must only be called to remove an alarm which has expired, not to
     *  reconfigure the event.
     */
    void removeExpiredAlarm(KAAlarm::Type type);

    /** Call before making a group of changes to the event, to avoid unnecessary
     *  calculation intensive recalculations of trigger times from being
     *  performed until all the changes have been applied. When the changes
     *  are complete, endChanges() should be called to allow resultant
     *  updates to occur.
     */
    void startChanges();
    /** Call when a group of changes preceded by startChanges() is complete, to
     *  allow resultant updates to occur.
     */
    void endChanges();

    /** Return the current KAlarm calendar storage format version.
     *  @return version in the format returned by KAlarmCal::Version().
     *  @see currentCalendarVersionString()
     */
    static int currentCalendarVersion();

    /** Return the current KAlarm calendar storage format version.
     *  @return version as a string in the format "1.2.3".
     *  @see currentCalendarVersion()
     */
    static QByteArray currentCalendarVersionString();

    /** If a calendar was written by a previous version of KAlarm, do any
     *  necessary format conversions on the events to ensure that when the calendar
     *  is saved, no information is lost or corrupted.
     *  @param calendar         calendar whose events are to be converted.
     *  @param calendarVersion  KAlarm calendar format version of @p calendar, in the
     *                          format returned by KAlarmCal::Version(). The KDE 3.0.0
     *                          version 0.5.7 requires a special adjustment for
     *                          summer time and should be passed negated (-507) to
     *                          distinguish it from the KDE 3.0.1 version 0.5.7
     *                          which does not require the adjustment.
     *  @return @c true if any conversions were done.
     */
    static bool convertKCalEvents(const KCalCore::Calendar::Ptr &, int calendarVersion);

    /** Return a list of pointers to a list of KAEvent objects. */
    static List ptrList(QVector<KAEvent> &events);

    /** Output the event's data as debug output. */
    void dumpDebug() const;

private:
    QSharedDataPointer<KAEventPrivate> d;
};

} // namespace KAlarmCal

Q_DECLARE_OPERATORS_FOR_FLAGS(KAlarmCal::KAEvent::Flags)
Q_DECLARE_METATYPE(KAlarmCal::KAEvent)

#endif // KAEVENT_H

