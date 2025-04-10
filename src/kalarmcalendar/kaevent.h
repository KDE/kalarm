/*
 *  kaevent.h  -  represents KAlarm calendar events
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"

#include "datetime.h"
#include "karecurrence.h"
#include "kacalendar.h"
#include "repetition.h"

#include <KCalendarCore/Calendar>
#include <KCalendarCore/Person>

#include <QBitArray>
#include <QColor>
#include <QFont>
#include <QList>
#include <QSharedDataPointer>
#include <QMetaType>

namespace KAlarmCal
{
class Holidays;

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
    enum class Action
    {
        Message,   //!< KCal::Alarm::Display type: display a text message
        File,      //!< KCal::Alarm::Display type: display a file (URL given by the alarm text)
        Command,   //!< KCal::Alarm::Procedure type: execute a shell command
        Email,     //!< KCal::Alarm::Email type: send an email
        Audio      //!< KCal::Alarm::Audio type: play a sound file
    };

    /** Alarm types.
     *  KAAlarm's of different types may be contained in a KAEvent,
     *  each KAAlarm defining a different component of the overall alarm.
     */
    enum class Type
    {
        Invalid          = 0,     //!< Not an alarm
        Main             = 0x01,  //!< THE real alarm. Must be the first in the enumeration.
        Reminder         = 0x02,  //!< Reminder in advance of/after the main alarm
        Deferred         = 0x04,  //!< Deferred alarm
        DeferredReminder = Reminder | Deferred,  //!< Deferred reminder alarm
        // The following values must be greater than the preceding ones, to
        // ensure that in ordered processing they are processed afterwards.
        AtLogin          = 0x10,  //!< Additional repeat-at-login trigger
        Displaying       = 0x20   //!< Copy of the alarm currently being displayed

                              // IMPORTANT: if any values are added to this list, ensure that the
                              //            KAEventPrivate::AlarmType enum is adjusted similarly.
    };

    /** Default constructor, which creates an invalid instance. */
    KAAlarm();

    /** Copy constructor. */
    KAAlarm(const KAAlarm& other);

    /** Destructor. */
    ~KAAlarm();

    /** Assignment operator. */
    KAAlarm& operator=(const KAAlarm& other);

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
    void setTime(const DateTime& dt);

    /** Set the alarm's trigger time. */
    void setTime(const KADateTime& dt);

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
    static const char* debugType(Type);

private:
    //@cond PRIVATE
    class Private;
    Private* const d;
    //@endcond

    friend class KAEvent;
    friend class KAEventPrivate;
};

inline int operator&(KAAlarm::Type t1, KAAlarm::Type t2)
{ return static_cast<int>(t1) & static_cast<int>(t2); }

inline QDebug operator<<(QDebug s, KAAlarm::Type type)
{ s << static_cast<int>(type); return s; }

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
    using List = QList<KAEvent*>;

    /** Email ID, equivalent to Akonadi::Item::Id. */
    using EmailId = qint64;

    /** Flags for use in D-Bus calls, etc. Flags may be combined by OR'ing them together. */
    enum Flag
    {
        BEEP            = 0x02,    //!< sound an audible beep when the alarm is displayed
        REPEAT_AT_LOGIN = 0x04,    //!< repeat the alarm at every login
        ANY_TIME        = 0x08,    //!< only a date is specified for the alarm, not a time
        CONFIRM_ACK     = 0x10,    //!< closing the alarm message window requires a confirmation prompt
        EMAIL_BCC       = 0x20,    //!< blind copy the email to the user
        DEFAULT_FONT    = 0x40,    //!< use the default alarm message font. Overrides any specified font.
        REPEAT_SOUND    = 0x80,    //!< repeat the sound file while the alarm is displayed
        DISABLED        = 0x100,   //!< the alarm is currently disabled
        AUTO_CLOSE      = 0x200,   //!< auto-close the alarm window after the late-cancel period
        SCRIPT          = 0x400,   //!< the command is a script, not a shell command line
        EXEC_IN_XTERM   = 0x800,   //!< execute the command in a terminal window
        SPEAK           = 0x1000,  //!< speak the message when the alarm is displayed
        COPY_KORGANIZER = 0x2000,  //!< KOrganizer should hold a copy of the event
        EXCL_HOLIDAYS   = 0x4000,  //!< don't trigger the alarm on holidays. Only valid
                                   //!< if a holiday region has been set by setHolidays().
        WORK_TIME_ONLY  = 0x8000,  //!< trigger the alarm only during working hours
        DISPLAY_COMMAND = 0x10000, //!< display command output in the alarm window
        REMINDER_ONCE   = 0x20000, //!< only trigger the reminder on the first recurrence
        DONT_SHOW_ERROR = 0x40000, //!< do not notify command alarm errors to user
        NOTIFY          = 0x80000, //!< use the standard notification system instead of alarm message window
        WAKE_SUSPEND    = 0x100000 //!< use kernel timer (not RTC) to wake-from-suspend when alarm due

                          // IMPORTANT: if any values are added to this list, ensure that the
                          //            additional enum values in KAEventPrivate are also adjusted.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /** The basic action type(s) for the event's main alarm.
     *  Values may be combined by OR'ing them together. */
    enum class Action
    {
        None           = 0,      //!< invalid
        Display        = 0x01,   //!< the alarm displays something
        Command        = 0x02,   //!< the alarm executes a command
        Email          = 0x04,   //!< the alarm sends an email
        Audio          = 0x08,   //!< the alarm plays an audio file (without any display)
        DisplayCommand = Display | Command,  //!< the alarm displays command output
        All            = Display | Command | Email | Audio   //!< all types mask
    };

    /** The sub-action type for the event's main alarm. */
    enum class SubAction
    {
        Message = static_cast<int>(KAAlarm::Action::Message),  //!< display a message text
        File    = static_cast<int>(KAAlarm::Action::File),     //!< display the contents of a file
        Command = static_cast<int>(KAAlarm::Action::Command),  //!< execute a command
        Email   = static_cast<int>(KAAlarm::Action::Email),    //!< send an email
        Audio   = static_cast<int>(KAAlarm::Action::Audio)     //!< play an audio file
    };

    /** What type of occurrence is due. */
    enum class OccurType
    {
        None                = 0,      //!< no occurrence is due
        FirstOrOnly         = 0x01,   //!< the first occurrence (takes precedence over LastRecur)
        RecurDate           = 0x02,   //!< a recurrence with only a date, not a time
        RecurDateTime       = 0x03,   //!< a recurrence with a date and time
        LastRecur           = 0x04,   //!< the last recurrence
        Repeat              = 0x10,   //!< (bitmask for a sub-repetition of an occurrence)
        FirstOrOnlyRepeat   = Repeat | FirstOrOnly,   //!< a sub-repetition of the first occurrence
        RecurDateRepeat     = Repeat | RecurDate,     //!< a sub-repetition of a date-only recurrence
        RecurDateTimeRepeat = Repeat | RecurDateTime, //!< a sub-repetition of a date/time recurrence
        LastRecurRepeat     = Repeat | LastRecur      //!< a sub-repetition of the last recurrence
    };

    /** How to treat sub-repetitions in nextOccurrence(). */
    enum class Repeats
    {
        Ignore = 0,    //!< check for recurrences only, ignore sub-repetitions
        Return,        //!< return a sub-repetition if it's the next occurrence
        RecurBefore    //!< if a sub-repetition is the next occurrence, return the previous recurrence, not the sub-repetition
    };

    /** What type of occurrence currently limits how long the alarm can be deferred. */
    enum class DeferLimit
    {
        None,        //!< there is no limit
        Main,        //!< the main alarm
        Recurrence,  //!< a recurrence
        Repetition,  //!< a sub-repetition
        Reminder     //!< a reminder
    };

    /** What to check for in nextDateTime() when evaluating the next event display or occurrence.
     *  If the event (not a reminder) has been deferred, the deferral time is returned regardless
     *  of NextType.
     *  Reminders are never returned if the recurrence to which they relate is excluded by working
     *  hours or holiday restrictions, regardless of whether or not NextWorkHoliday is specified.
     */
    enum NextType
    {
        NextRecur       = 0,      //!< (always done) check for only occurrence or next recurrence
        NextRepeat      = 0x01,   //!< check for sub-repetitions
        NextReminder    = 0x02,   //!< check for reminders
        NextWorkHoliday = 0x04,   //!< take account of any working hours or holiday restrictions
        NextDeferral    = 0x08    //!< return the event deferral time, or reminder deferral time if NextReminder set
    };
    Q_DECLARE_FLAGS(NextTypes, NextType)

    /** Next trigger type for an alarm. */
    enum class Trigger
    {

        /** Next trigger, including reminders. No account is taken of any
         *  working hours or holiday restrictions when evaluating this. */
        All,

        /** Next trigger of the main alarm, i.e. excluding reminders. No
         *  account is taken of any working hours or holiday restrictions when
         *  evaluating this. */
        Main,

        /** Next trigger of the main alarm, i.e. excluding reminders, taking
         *  account of any working hours or holiday restrictions. If the event
         *  has no working hours or holiday restrictions, this is equivalent to
         *  Main. */
        Work,

        /** Next trigger, including reminders, taking account of any working
         *  hours or holiday restrictions. If the event has no working hours or
         *  holiday restrictions, this is equivalent to All. */
        AllWork,

        /** Next trigger time for display purposes (i.e. excluding reminders). */
        Display
    };

    /** Command execution error type for last time the alarm was triggered. */
    enum class CmdErr
    {
        None    = 0,      //!< no error
        Fail    = 0x01,   //!< command alarm execution failed
        Pre     = 0x02,   //!< pre-alarm command execution failed
        Post    = 0x04,   //!< post-alarm command execution failed
        PrePost = Pre | Post
    };

    /** Options for pre- or post-alarm actions. These may be OR'ed together. */
    enum ExtraActionOption
    {
        CancelOnPreActError  = 0x01,   //!< cancel alarm on pre-alarm action error
        DontShowPreActError  = 0x02,   //!< do not notify pre-alarm action errors to user
        ExecPreActOnDeferral = 0x04    //!< execute pre-alarm action also for deferred alarms
    };
    Q_DECLARE_FLAGS(ExtraActionOptions, ExtraActionOption)

    /** How to deal with the event UID in updateKCalEvent(). */
    enum class UidAction
    {
        Ignore,        //!< leave KCal::Event UID unchanged
        Check,         //!< verify that the KCal::Event UID is already the same as the KAEvent ID, if the latter is non-empty
        Set            //!< set the KCal::Event UID to the KAEvent ID
    };

    /** Default constructor which creates an invalid event. */
    KAEvent();

    /** Construct an event and initialise with the specified parameters.
     *  @param dt    start date/time. If @p dt is date-only, or if #ANY_TIME flag
     *               is specified, the event will be date-only.
     *  @param name  name of the alarm.
     *  @param text  alarm message (@p action = #Message);
     *               file to display (@p action = #File);
     *               command to execute (@p action = #Command);
     *               email body (@p action = #Email);
     *               audio file (@p action = #Audio).
     *  @param bg    background color (for display alarms, ignored otherwise).
     *  @param fg    foreground color (for display alarms, ignored otherwise).
     *  @param font  font (for display alarms, ignored otherwise). Ignored if
     *               #DEFAULT_FONT flag is specified.
     *  @param action         alarm action type.
     *  @param lateCancel     late-cancellation period (minutes), else 0.
     *  @param flags          OR of #Flag enum values.
     *  @param changesPending true to inhibit automatic calculations and data
     *                        updates until further changes have been applied
     *                        to the instance; call endChanges() when changes
     *                        are complete.
     */
    KAEvent(const KADateTime& dt, const QString& name, const QString& text,
            const QColor& bg, const QColor& fg, const QFont& font,
            SubAction action, int lateCancel, Flags flags, bool changesPending = false);

    /** Construct an event and initialise it from a KCalendarCore::Event.
     *
     *  The initialisation is identical to that performed by set().
     */
    explicit KAEvent(const KCalendarCore::Event::Ptr&);

    KAEvent(const KAEvent& other);
    ~KAEvent();

    KAEvent& operator=(const KAEvent& other);

    /** Update an existing KCalendarCore::Event with the KAEvent data.
     *  @param event  Event to update.
     *  @param u      how to deal with the Event's UID.
     *  @param setCustomProperties if true, all the Event's existing custom
     *                             properties are cleared and replaced with the
     *                             KAEvent's custom properties. If false, the
     *                             KCal::Event's non-KAlarm custom properties
     *                             are left untouched.
     */
    bool updateKCalEvent(const KCalendarCore::Event::Ptr& event, UidAction u, bool setCustomProperties = true) const;

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
    void setEventId(const QString& id);

    /** Return the event's unique identifier. Note that the UID is guaranteed to be unique
     *  only within the calendar containing the event.
     */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString id() const;

    /** Increment the revision number of the event (SEQUENCE property in iCalendar). */
    void incrementRevision();

    /** Return the revision number of the event (SEQUENCE property in iCalendar). */
    int revision() const;

    /** Set the ID of the calendar resource which contains the event. */
    void setResourceId(ResourceId id);

    /** Return the ID of the calendar resource which contains the event. */
    ResourceId  resourceId() const;

    /** Note the event's storage format compatibility compared to the current KAlarm calendar format. */
    void setCompatibility(KACalendar::Compat c);

    /** Return the event's storage format compatibility compared to the current KAlarm calendar format. */
    KACalendar::Compat compatibility() const;

    /** Return the original KCalendarCore::Event's custom properties in the source calendar. */
                             //cppcheck-suppress[returnByReference]  QMap implicitly shared
    QMap<QByteArray, QString> customProperties() const;

    /** Return the action sub-type of the event's main alarm. For display alarms,
     *  this is Message or File, while other types of alarm simply return the
     *  basic action type (Command, Email, Audio).
     *  Note that for a display alarm whose text is generated by a command, the
     *  returned type is Command.
     */
    SubAction actionSubType() const;

    /** Return the OR of the basic action types of the event's main alarm (display,
     *  command, email, audio).
     *  Note that for a display alarm whose text is generated by a command, the
     *  returned type is @c Action::Display|Action::Command.
     */
    Action actionTypes() const;

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

    /** Enable the notification system to be used for a display alarm, instead
     *  of displaying a window.
     *  @see notify()
     */
    void setNotify(bool useNotify);

    /** Return whether the notification system is used instead of displaying
     *  a window, for a display alarm.
     *  @return @c true if it is a display alarm and it uses the notification
     *          system
     *  @see setNotify()
     */
    bool notify() const;

    /** Set the Akonadi item ID of the email which the alarm is related to.
     */
    void setEmailId(EmailId id);

    /** Return the ID of the email which the alarm is related to.
     */
    EmailId emailId() const;

    /** Set the alarm's name. */
    void setName(const QString& newName);

    /** Return the alarm's name. */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString name() const;

    /** Return the alarm's text. Its significance depends on the type of alarm;
     *  alternatively, use message(), displayMessage(), fileName() or command(),
     *  which incorporate checks on alarm type.
     */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
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
    static void setDefaultFont(const QFont& font);

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
    void setCommandError(CmdErr error) const;

    /** Return the command execution error for the last time the alarm triggered. */
    CmdErr commandError() const;

    /** Return whether execution errors for the command should not to be shown to the user. */
    bool commandHideError() const;

    /** Set the log file to write command alarm output to.
     *  @param logfile  log file path
     */
    void setLogFile(const QString& logfile);
    /** Return the log file which command alarm output should be written to.
     *  @return log file path, or empty if no log file. */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString logFile() const;

    /** Return whether alarm acknowledgement must be confirmed by the user, for a display alarm. */
    bool confirmAck() const;

    /** Return whether KOrganizer should hold a copy of the event. */
    bool copyToKOrganizer() const;

    /** Set the email related data for the event. */
    void setEmail(uint from, const KCalendarCore::Person::List&, const QString& subject,
                  const QStringList& attachments);

    /** Return the email message body, for an email alarm.
     *  @return email body, or empty if not an email alarm
     */
    QString emailMessage() const;

    /** Return the email identity to be used as the sender, for an email alarm.
     *  @return email UOID
     */
    uint emailFromId() const;

    /** Return the list of email addressees, including names, for an email alarm. */
    KCalendarCore::Person::List emailAddressees() const;

    /** Return a list of the email addresses, including names, for an email alarm. */
    QStringList emailAddresses() const;

    /** Return a string containing the email addressees, including names, for an email alarm.
     *  @param sep  separator string to insert between addresses.
     */
    QString emailAddresses(const QString& sep) const;

    /** Concatenate a list of email addresses into a string.
     *  @param addresses list of email addresses
     *  @param sep       separator string to insert between addresses
     */
    static QString joinEmailAddresses(const KCalendarCore::Person::List& addresses, const QString& sep);
    /** Return the list of email addressees, excluding names, for an email alarm. */
    QStringList emailPureAddresses() const;

    /** Return a string containing the email addressees, excluding names, for an email alarm.
     *  @param sep  separator string to insert between addresses.
     */
    QString emailPureAddresses(const QString& sep) const;

    /** Return the email subject line, for an email alarm. */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString emailSubject() const;

    /** Return the list of file paths of the attachments, for an email alarm. */
                             //cppcheck-suppress[returnByReference]  QStringList implicitly shared
    QStringList emailAttachments() const;

    /** Return the file paths of the attachments, as a string, for an email alarm.
     *  @param sep  string separator
     */
    QString emailAttachments(const QString& sep) const;

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
    void setAudioFile(const QString& filename, float volume, float fadeVolume,
                      int fadeSeconds, int repeatPause = -1, bool allowEmptyFile = false);

    /** Return the audio file path.
     *  @see setAudioFile()
     */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
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
     *  @see isTemplate(), name()
     */
    void setTemplate(const QString& name, int afterTime = -1);

    /** Return whether the event is an alarm template.
     *  @see setTemplate()
     */
    bool isTemplate() const;

    /** Return whether the alarm template does not specify a time.
     *  @return @c true if no time is specified, i.e. the normal default alarm time will
     *          be used, @c false if the template specifies a time.
     */
    bool usingDefaultTime() const;

    /** Return the number of minutes (>= 0) after the default alarm time which is
     *  specified in the alarm template. If this is specified, an alarm based on
     *  this template will have the "Time from now" radio button enabled in the alarm
     *  edit dialog.
     *  @return minutes after the default time, or -1 if the template specifies a time
     *          of day or a date-only alarm.
     */
    int templateAfterTime() const;

    /** Set the pre-alarm and post-alarm actions, and their options.
     *  @param pre  shell command to execute before the alarm is displayed
     *  @param post shell command to execute after the alarm is acknowledged
     *  @param preOptions options for pre-alarm actions
     *  @see preAction(), postAction(), extraActionOptions()
     */
    void setActions(const QString& pre, const QString& post, ExtraActionOptions preOptions);

    /** Return the shell command to execute before the alarm is displayed. */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString preAction() const;

    /** Return the shell command to execute after the display alarm is acknowledged.
     *  @see setActions()
     */
                             //cppcheck-suppress[returnByReference]  QString implicitly shared
    QString postAction() const;

    /** Return the pre-alarm action options.
     *  @see preAction(), setActions()
     */
    ExtraActionOptions extraActionOptions() const;

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
    void activateReminderAfter(const DateTime& mainAlarmTime);

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
    void defer(const DateTime& dt, bool reminder, bool adjustRecurrence = false);

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
    DateTime deferralLimit(DeferLimit* limitType = nullptr) const;

    /** Return the default deferral interval used in the deferral dialog.
     *  @see setDeferDefaultMinutes()
     */
    int deferDefaultMinutes() const;

    /** Return the default date-only setting used in the deferral dialog. */
    bool deferDefaultDateOnly() const;

    /** Return the start time for the event. If the event recurs, this is the
     *  time of the first recurrence. If the event is date-only, this returns a
     *  date-only value.
     *  @note No account is taken of any working hours or holiday restrictions.
     *        when determining the start date/time.
     *
     *  @see mainDateTime()
     */
    DateTime startDateTime() const;

    /** Set the next time to trigger the alarm (excluding sub-repetitions).
     *  Note that for a recurring event, this should match one of the
     *  recurrence times.
     */
    void setTime(const KADateTime& dt);

    /** Return the next time the event will trigger or be displayed. The evaluation
     *  depends on @p type:
     * - @p type == NextRecur:             returns the next recurrence or only occurrence.
     * - @p type contains NextRepeat:      returns the only occurrence or next
     *                                     recurrence/sub-repetition.
     * - @p type contains NextReminder:    as above but if a reminder occurs first, it will
     *                                     be returned. N.B. Reminders are not displayed for
     *                                     sub-repetitions.
     * - @p type contains NextWorkHoliday: as above but the search continues until a
     *                                     recurrence is found which occurs during working
     *                                     hours and not on a holiday.
     * - @p type contains NextDeferral:    if the event (not a reminder) has been deferred,
     *                                     returns the deferral time.
     *                                     If a reminder has been deferred AND @p type
     *                                     contains NextReminder, returns the deferral time.
     *
     *  @param type  what to check for when evaluating the next occurrence or display.
     *  @see startDateTime(), setTime()
     */
    DateTime nextDateTime(NextTypes type) const;

    /** Return the next time the main alarm will trigger.
     *  @note No account is taken of any working hours or holiday restrictions.
     *        when determining the next trigger date/time.
     *
     *  @param withRepeats  true to include sub-repetitions, false to exclude them.
     *  @see mainTime(), startDateTime(), setTime()
     */
    DateTime mainDateTime(bool withRepeats = false) const;

    /** Return the time at which the main alarm will next trigger.
     *  Sub-repetitions are ignored.
     *  @note No account is taken of any working hours or holiday restrictions.
     *        when determining the next trigger time.
     */
    QTime mainTime() const;

    /** Return the time at which the last sub-repetition of the current
     *  recurrence of the main alarm will occur.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining the last sub-repetition time.
     *
     *  @return last sub-repetition time, or main alarm time if no
     *          sub-repetitions are configured.
     */
    DateTime mainEndRepeatTime() const;

    /** Set the start-of-day time used by all date-only alarms.
     *  Note that adjustStartOfDay() should be called immediately after this,
     *  to adjust all events' internal data.
     */
    static void setStartOfDay(const QTime&);

    /** Call when the user changes the start-of-day time, to adjust the data
     *  for each date-only event in a list.
     *  @param events list of events. Any date-time events in the list are ignored.
     *  @see setStartOfDay()
     */
    static void adjustStartOfDay(const KAEvent::List& events);

    /** Return the next time the alarm will trigger.
     *  @param type specifies whether to ignore reminders, working time
     *              restrictions, etc.
     */
    DateTime nextTrigger(Trigger type) const;

    /** Set the date/time the event was created, or saved in the archive calendar.
     *  @see createdDateTime()
     */
    void setCreatedDateTime(const KADateTime& dt);

    /** Return the date/time the event was created, or saved in the archive calendar.
     *  @see setCreatedDateTime()
     */
    KADateTime createdDateTime() const;

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

    /** Enable or disable wake-from-suspend when the alarm is due.
     *  (Note that this option is used for kernel timer wake-from-suspend, not
     *   for RTC wake-from-suspend, since the latter can only be set for one
     *   alarm at any one time.)
     *  @param wake  true to wake-from-suspend, false to not wake-from-suspend.
     *  @see wakeFromSuspend()
     */
    void setWakeFromSuspend(bool wake);

    /** Return whether wake-from-suspend is enabled for the alarm.
     *  @see setWakeFromSuspend()
     */
    bool wakeFromSuspend() const;

    /** Enable or disable the alarm on holiday dates. The currently selected
     *  holiday region determines which dates are holidays.
     *  Note that this option only has any effect for recurring alarms.
     *  @param exclude  true to disable on holidays, false to enable
     *  @see holidaysExcluded(), setHolidays()
     */
    void setExcludeHolidays(bool exclude);

    /** Return whether the alarm is disabled on holiday dates.
     *  If no holiday region has been set by setHolidays(), this returns false;
     *  @see setExcludeHolidays()
     */
    bool holidaysExcluded() const;

    /** Set the holiday data to be used by all KAEvent instances, or notify that
     *  the holiday data has changed.
     *  Alarms which exclude holidays record the pointer to the holiday definition
     *  at the time their next trigger times were last calculated. The change in
     *  holiday definition will cause their next trigger times to be recalculated.
     *  @param holidays  the holiday data. The data object must persist for
     *                   the lifetime of the application, since this class just
     *                   stores a pointer to @p holidays.
     *  @see setExcludeHolidays()
     */
    static void setHolidays(const Holidays& holidays);

    /** Enable or disable the alarm on non-working days and outside working hours.
     *  Note that this option only has any effect for recurring alarms.
     *  @param wto  true to restrict to working time, false to enable any time
     *  @see workTimeOnly(), setWorkTime()
     */
    void setWorkTimeOnly(bool wto);

    /** Return whether the alarm is disabled on non-working days and outside working hours.
     *  @see setWorkTimeOnly()
     */
    bool workTimeOnly() const;

    /** Check whether a date/time conflicts with working hours and/or holiday
     *  restrictions for the alarm.
     *  @note If @p dt is date-only, only holidays and/or working days are
     *        taken account of; working hours are ignored.
     *  @param dt  the date/time to check.
     *  @return true if the alarm is disabled from occurring at time @p dt
     *               because @p dt is outside working hours (if the alarm is
     *               working time only) or is during a holiday (if the alarm is
     *               disabled on holidays);
     *         false if the alarm is permitted to occur at time @p dt. (But
     *               note that no check is made as to whether the alarm is
     *               actually scheduled to occur at time @p dt.)
     */
    bool excludedByWorkTimeOrHoliday(const KADateTime& dt) const;

    /** Set working days and times, to be used by all KAEvent instances.
     *  @param days      bits set to 1 for each working day. Array element 0 = Monday ... 6 = Sunday.
     *  @param start     start time in working day.
     *  @param end       end time in working day.
     *  @param timeSpec  time spec to be used for evaluating the start and end of the working day.
     *  @see setWorkTimeOnly(), excludedByWorkTimeOrHoliday()
     */
    static void setWorkTime(const QBitArray& days, const QTime& start, const QTime& end, const KADateTime::Spec& timeSpec);

    /** Clear the event's recurrence and sub-repetition data.
     *  @see setRecurrence(), recurs()
     */
    void setNoRecur();

    /** Initialise the event's recurrence from a KARecurrence.
     *  The event's start date/time is not changed.
     *  @see setRecurMinutely(), setRecurDaily(), setRecurWeekly(), setRecurMonthlyByDate(), setRecurMonthlyByPos(), setRecurAnnualByDate(), setRecurAnnualByPos(), setFirstRecurrence()
     */
    void setRecurrence(const KARecurrence& r);

    /** Set the recurrence to recur at a minutes interval.
     *  @param freq   how many minutes between recurrences.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date/time (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMinutely(int freq, int count, const KADateTime& end);

    /** Set the recurrence to recur daily.
     *  @param freq   how many days between recurrences.
     *  @param days   which days of the week alarms are allowed to occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurDaily(int freq, const QBitArray& days, int count, QDate end);

    /** Set the recurrence to recur weekly, on the specified weekdays.
     *  @param freq   how many weeks between recurrences.
     *  @param days   which days of the week alarms are allowed to occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurWeekly(int freq, const QBitArray& days, int count, QDate end);

    /** Set the recurrence to recur monthly, on the specified days within the month.
     *  @param freq   how many months between recurrences.
     *  @param days   which days of the month alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMonthlyByDate(int freq, const QList<int>& days, int count, QDate end);

    /** Holds days of the week combined with a week number in the month,
     *  used to specify some monthly or annual recurrences. */
    struct MonthPos
    {
        MonthPos() : days(7) {}
        int        weeknum;     //!< Week in month, or < 0 to count from end of month.
        QBitArray  days;        //!< Days in week, element 0 = Monday.
    };

    /** Set the recurrence to recur monthly, on the specified weekdays in the
     *  specified weeks of the month.
     *  @param freq   how many months between recurrences.
     *  @param pos    which days of the week/weeks of the month alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurMonthlyByPos(int freq, const QList<MonthPos>& pos, int count, QDate end);

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
    bool setRecurAnnualByDate(int freq, const QList<int>& months, int day, KARecurrence::Feb29Type, int count, QDate end);

    /** Set the recurrence to recur annually, on the specified weekdays in the
     *  specified weeks of the specified months.
     *  @param freq   how many years between recurrences.
     *  @param pos    which days of the week/weeks of the month alarms should occur on.
     *  @param months which months of the year alarms should occur on.
     *  @param count  number of occurrences, including first and last;
     *                = -1 to recur indefinitely;
     *                = 0 to use @p end instead.
     *  @param end   = end date (set invalid to use @p count instead).
     *  @return @c false if no recurrence was set up.
     */
    bool setRecurAnnualByPos(int freq, const QList<MonthPos>& pos, const QList<int>& months, int count, QDate end);

    /** Set dates to exclude from the recurrence.
     *  These dates replace any previous exception dates.
     */
    void setExceptionDates(const QList<QDate>& dates);

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
    const KARecurrence& recurrence() const;

    /** Return the recurrence interval in units of the recurrence period type
     *  (minutes, days, etc).
     *  @see longestRecurrenceInterval()
     */
    int recurInterval() const;

    /** Return the longest interval which can occur between consecutive recurrences.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when evaluating consecutive recurrence dates/times.
     *  @see recurInterval()
     */
    KCalendarCore::Duration longestRecurrenceInterval() const;

    /** Adjust the event date/time to the first recurrence of the event, on or after
     *  the event start date/time. The event start date may not be a recurrence date,
     *  in which case a later date will be set.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining the first recurrence of the event.
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
    bool setRepetition(const Repetition& r);

    /** Return the event's sub-repetition data.
     *  @see setRepetition(), repetitionText()
     */
    Repetition repetition() const;

    /** Return the count of the next sub-repetition which is due.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining the next event sub-repetition.
     *
     *  @return sub-repetition count (>=1), or 0 for the main recurrence.
     *  @see nextOccurrence()
     */
    int nextRepetition() const;

    /** Return the repetition interval as text suitable for display. */
    QString repetitionText(bool brief = false) const;

    /** Determine whether the event will occur strictly after the specified
     *  date/time. Reminders are ignored.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining event occurrences.
     *  @note If the event is date-only, its occurrences are considered to occur
     *        at the start-of-day time when comparing with @p preDateTime.
     *
     *  @param preDateTime        the specified date/time.
     *  @param includeRepetitions if true and the alarm has a sub-repetition, the
     *                            method will return true if any sub-repetitions
     *                            occur after @p preDateTime.
     *  @see nextOccurrence()
     */
    bool occursAfter(const KADateTime& preDateTime, bool includeRepetitions) const;

    /** Set the date/time of the event to the next scheduled occurrence after a
     *  specified date/time, provided that this is later than its current date/time.
     *  Any reminder alarm is adjusted accordingly.
     *  If the alarm has a sub-repetition, and a sub-repetition of a previous
     *  recurrence occurs after the specified date/time, that sub-repetition is
     *  set as the next occurrence.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining and setting the next occurrence date/time.
     *  @note If the event is date-only, its occurrences are considered to occur
     *        at the start-of-day time when comparing with @p preDateTime.
     *
     *  @see nextOccurrence()
     */
    OccurType setNextOccurrence(const KADateTime& preDateTime);

    /** Get the date/time of the next occurrence of the event, strictly after
     *  the specified date/time. Reminders are ignored.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining the next event occurrence.
     *  @note If the event is date-only, its occurrences are considered to occur
     *        at the start-of-day time when comparing with @p preDateTime.
     *
     *  @param preDateTime the specified date/time.
     *  @param result      date/time of next occurrence, or invalid date/time if none.
     *  @param option      how/whether to make allowance for sub-repetitions.
     *  @see nextRepetition(), setNextOccurrence(), previousOccurrence(), occursAfter()
     */
    OccurType nextOccurrence(const KADateTime& preDateTime, DateTime& result, Repeats option = Repeats::Ignore) const;

    /** Get the date/time of the last previous occurrence of the event,
     *  strictly before the specified date/time. Reminders are ignored.
     *  @note No account is taken of any working hours or holiday restrictions
     *        when determining the previous event occurrence.
     *  @note If the event is date-only, its occurrences are considered to occur
     *        at the start-of-day time when comparing with @p preDateTime.
     *
     *  @param afterDateTime       the specified date/time.
     *  @param result              date/time of previous occurrence, or invalid
     *                             date/time if none.
     *  @param includeRepetitions  if true and the alarm has a sub-repetition, the
     *                             last previous repetition is returned if
     *                             appropriate.
     *  @see nextOccurrence()
     */
    OccurType previousOccurrence(const KADateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const;

    /** Set the event to be a copy of the specified event, making the specified
     *  alarm the 'displaying' alarm.
     *  The purpose of setting up a 'displaying' alarm is to be able to reinstate
     *  the alarm message in case of a crash, or to reinstate it should the user
     *  choose to defer the alarm. Note that even repeat-at-login alarms need to be
     *  saved in case their end time expires before the next login.
     *  @param event      the event to copy
     *  @param type       the alarm type (main, reminder, deferred etc.)
     *  @param colId      the ID of the calendar resource which originally contained the event
     *  @param repeatAtLoginTime repeat-at-login time if @p type == AtLogin, else ignored
     *  @param showEdit   whether the Edit button was displayed
     *  @param showDefer  whether the Defer button was displayed
     *  @return @c true if successful, @c false if alarm was not copied.
     */
    bool setDisplaying(const KAEvent& event, KAAlarm::Type type, ResourceId colId, const KADateTime& repeatAtLoginTime, bool showEdit, bool showDefer);

    /** Reinstate the original event from the 'displaying' event.
     *  This instance is initialised from the supplied displaying @p event,
     *  and appropriate adjustments are made to convert it back to the
     *  original pre-displaying state.
     *  @param event      the displaying event
     *  @param colId      updated to the ID of the calendar resource which
     *                    originally contained the event
     *  @param showEdit   updated to true if Edit button was displayed, else false
     *  @param showDefer  updated to true if Defer button was displayed, else false
     */
    void reinstateFromDisplaying(const KCalendarCore::Event::Ptr& event, ResourceId& colId, bool& showEdit, bool& showDefer);

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
    KAAlarm nextAlarm(const KAAlarm& previousAlarm) const;

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

    /** Data categories for compare(). */
    enum class Compare
    {
        Id           = 0x01,   //!< the event ID
        ICalendar    = 0x02,   //!< extra event properties in the iCalendar file: custom properties, revision, creation timestamp
        UserSettable = 0x04,   //!< user settable data: resource ID, item ID
        CurrentState = 0x08    //!< changeable data which records the event's current state: next trigger time etc.
    };
    Q_DECLARE_FLAGS(Comparison, Compare)

    /** Compare this instance with another.
     *  All data which defines the characteristics of this event will be
     *  compared, plus optional data categories as specified.
     *  @param other       the other event to be compared.
     *  @param comparison  the optional data categories to compare, in addition
     *                     to the basic data. Other optional categories will be
     *                     ignored in the comparison.
     *  @return true if the instances match, false otherwise.
     */
    bool compare(const KAEvent& other, Comparison comparison) const;

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
     *  This is the KAlarm version which first used the current calendar/event
     *  format. It is NOT the KAlarmCal library .so version!
     *  @return version in the format returned by KAlarmCal::Version().
     *  @see currentCalendarVersionString()
     */
    static int currentCalendarVersion();

    /** Return the current KAlarm calendar storage format version.
     *  This is the KAlarm version which first used the current calendar/event
     *  format. It is NOT the KAlarmCal library .so version!
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
    static bool convertKCalEvents(const KCalendarCore::Calendar::Ptr&, int calendarVersion);

    /** Return a list of pointers to a list of KAEvent objects. */
    static List ptrList(QList<KAEvent>& events);

    /** Output the event's data as debug output. */
    void dumpDebug() const;

private:
    QSharedDataPointer<KAEventPrivate> d;
};

inline int operator&(KAEvent::Action a1, KAEvent::Action a2)
{ return static_cast<int>(a1) & static_cast<int>(a2); }

inline KAEvent::Action operator~(KAEvent::Action act)
{ return static_cast<KAEvent::Action>(~static_cast<int>(act)); }

inline QDebug operator<<(QDebug s, KAEvent::Action act)
{ s << static_cast<int>(act); return s; }

inline int operator&(KAEvent::OccurType t1, KAEvent::OccurType t2)
{ return static_cast<int>(t1) & static_cast<int>(t2); }

inline KAEvent::OccurType operator|(KAEvent::OccurType t1, KAEvent::OccurType t2)
{ return static_cast<KAEvent::OccurType>(static_cast<int>(t1) | static_cast<int>(t2)); }

inline KAEvent::OccurType operator~(KAEvent::OccurType type)
{ return static_cast<KAEvent::OccurType>(~static_cast<int>(type)); }

inline int operator&(KAEvent::CmdErr e1, KAEvent::CmdErr e2)
{ return static_cast<int>(e1) & static_cast<int>(e2); }

inline KAEvent::CmdErr operator~(KAEvent::CmdErr err)
{ return static_cast<KAEvent::CmdErr>(~static_cast<int>(err)); }

inline QDebug operator<<(QDebug s, KAEvent::CmdErr err)
{ s << static_cast<int>(err); return s; }

} // namespace KAlarmCal

Q_DECLARE_OPERATORS_FOR_FLAGS(KAlarmCal::KAEvent::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(KAlarmCal::KAEvent::Comparison)
Q_DECLARE_OPERATORS_FOR_FLAGS(KAlarmCal::KAEvent::NextTypes)
Q_DECLARE_METATYPE(KAlarmCal::KAEvent)

// vim: et sw=4:
