/*
 *  commandoptions.cpp  -  extract command line options
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "commandoptions.h"

#include "functions.h"
#include "kalarm_debug.h"
#include "kalarmcalendar/identities.h"
#include "kamail.h"
#include "preferences.h"

#include <kpimtextedit/kpimtextedit-texttospeech.h>
#include <KLocalizedString>

#include <QCommandLineParser>

#include <iostream>

namespace
{
enum Option
{
    ACK_CONFIRM,
    ATTACH,
    AUTO_CLOSE,
    BCC,
    BEEP,
    COLOUR,
    COLOURFG,
    OptCANCEL_EVENT,
    DISABLE,
    DISABLE_ALL,
    EXEC,
    EXEC_DISPLAY,
    OptEDIT,
    EDIT_NEW_DISPLAY,
    EDIT_NEW_COMMAND,
    EDIT_NEW_EMAIL,
    EDIT_NEW_AUDIO,
    OptEDIT_NEW_PRESET,
    OptFILE,
    FROM_ID,
    INTERVAL,
    KORGANIZER,
    LATE_CANCEL,
    OptLIST,
    LOGIN,
    MAIL,
    NAME,
    NOTIFY,
    PLAY,
    PLAY_REPEAT,
    RECURRENCE,
    REMINDER,
    REMINDER_ONCE,
    REPEAT,
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
    SPEAK,
#endif
    SUBJECT,
#ifndef NDEBUG
    TEST_SET_TIME,
#endif
    TIME,
    OptTRAY,
    OptTRIGGER_EVENT,
    UNTIL,
    VOLUME,
    Num_Options,       // number of Option values
    Opt_Message        // special value representing "message"
};

bool convInterval(const QString& timeParam, KARecurrence::Type&, int& timeInterval, bool allowMonthYear = false);
}

class CommandOptions::Private
{
public:
    explicit Private(CommandOptions* parent)
        : p(parent)
    {}
    bool    checkCommand(Option, Command, EditAlarmDlg::Type = EditAlarmDlg::NO_TYPE);
    void    setError(const QString& error);
    void    setErrorRequires(Option opt, Option opt2, Option opt3 = Num_Options);
    void    setErrorParameter(Option);
    void    setErrorIncompatible(Option opt1, Option opt2);
    void    checkEditType(EditAlarmDlg::Type type, Option opt)
                          { checkEditType(type, EditAlarmDlg::NO_TYPE, opt); }
    void    checkEditType(EditAlarmDlg::Type, EditAlarmDlg::Type, Option);
    QString optionName(Option, bool shortName = false) const;

    CommandOptions* const p;
    Option mCommandOpt;             // option for the selected command
};

/*===========================================================================*/

CommandOptions* CommandOptions::mFirstInstance = nullptr;

CommandOptions::CommandOptions()
    : d(new Private(this))
    , mOptions(Num_Options, nullptr)
    , mBgColour(Preferences::defaultBgColour())
    , mFgColour(Preferences::defaultFgColour())
    , mFlags(KAEvent::DEFAULT_FONT)
{
    if (!mFirstInstance)
        mFirstInstance = this;
}

/******************************************************************************
* Set the command line options for the parser, and remove any arguments
* following --exec or --exec-display (since the parser can't handle this).
* Reply = command line arguments for parser.
*/
QStringList CommandOptions::setOptions(QCommandLineParser* parser, const QStringList& args)
{
    mParser = parser;

    mOptions[ACK_CONFIRM]
              = new QCommandLineOption(QStringList{QStringLiteral("a"), QStringLiteral("ack-confirm")},
                                       i18n("Prompt for confirmation when alarm is acknowledged"));
    mOptions[ATTACH]
              = new QCommandLineOption(QStringList{QStringLiteral("A"), QStringLiteral("attach")},
                                       i18n("Attach file to email (repeat as needed)"),
                                       QStringLiteral("url"));
    mOptions[AUTO_CLOSE]
              = new QCommandLineOption(QStringLiteral("auto-close"),
                                       i18n("Auto-close alarm window after --late-cancel period"));
    mOptions[BCC]
              = new QCommandLineOption(QStringLiteral("bcc"),
                                       i18n("Blind copy email to self"));
    mOptions[BEEP]
              = new QCommandLineOption(QStringList{QStringLiteral("b"), QStringLiteral("beep")},
                                       i18n("Beep when message is displayed"));
    mOptions[COLOUR]
              = new QCommandLineOption(QStringList{QStringLiteral("c"), QStringLiteral("colour"), QStringLiteral("color")},
                                       i18n("Message background color (name or hex 0xRRGGBB)"),
                                       QStringLiteral("color"));
    mOptions[COLOURFG]
              = new QCommandLineOption(QStringList{QStringLiteral("C"), QStringLiteral("colourfg"), QStringLiteral("colorfg")},
                                       i18n("Message foreground color (name or hex 0xRRGGBB)"),
                                       QStringLiteral("color"));
    mOptions[OptCANCEL_EVENT]
              = new QCommandLineOption(QStringLiteral("cancelEvent"),
                                       i18n("Cancel alarm with the specified event ID"),
                                       QStringLiteral("eventID"));
    mOptions[DISABLE]
              = new QCommandLineOption(QStringList{QStringLiteral("d"), QStringLiteral("disable")},
                                       i18n("Disable the alarm"));
    mOptions[DISABLE_ALL]
              = new QCommandLineOption(QStringLiteral("disable-all"),
                                       i18n("Disable monitoring of all alarms"));
    mOptions[EXEC]
              = new QCommandLineOption(QStringList{QStringLiteral("e"), QStringLiteral("exec")},
                                       i18n("Execute a shell command line"),
                                       QStringLiteral("commandLine"));
    mOptions[EXEC_DISPLAY]
              = new QCommandLineOption(QStringList{QStringLiteral("E"), QStringLiteral("exec-display")},
                                       i18n("Command line to generate alarm message text"),
                                       QStringLiteral("commandLine"));
    mOptions[OptEDIT]
              = new QCommandLineOption(QStringLiteral("edit"),
                                       i18n("Display the alarm edit dialog to edit the specified alarm"),
                                       QStringLiteral("eventID"));
    mOptions[EDIT_NEW_DISPLAY]
              = new QCommandLineOption(QStringLiteral("edit-new-display"),
                                       i18n("Display the alarm edit dialog to edit a new display alarm"));
    mOptions[EDIT_NEW_COMMAND]
              = new QCommandLineOption(QStringLiteral("edit-new-command"),
                                       i18n("Display the alarm edit dialog to edit a new command alarm"));
    mOptions[EDIT_NEW_EMAIL]
              = new QCommandLineOption(QStringLiteral("edit-new-email"),
                                       i18n("Display the alarm edit dialog to edit a new email alarm"));
    mOptions[EDIT_NEW_AUDIO]
              = new QCommandLineOption(QStringLiteral("edit-new-audio"),
                                       i18n("Display the alarm edit dialog to edit a new audio alarm"));
    mOptions[OptEDIT_NEW_PRESET]
              = new QCommandLineOption(QStringLiteral("edit-new-preset"),
                                       i18n("Display the alarm edit dialog, preset with a template"),
                                       QStringLiteral("templateName"));
    mOptions[OptFILE]
              = new QCommandLineOption(QStringList{QStringLiteral("f"), QStringLiteral("file")},
                                       i18n("File to display"),
                                       QStringLiteral("url"));
    mOptions[FROM_ID]
              = new QCommandLineOption(QStringList{QStringLiteral("F"), QStringLiteral("from-id")},
                                       i18n("KMail identity to use as sender of email"),
                                       QStringLiteral("ID"));
    mOptions[INTERVAL]
              = new QCommandLineOption(QStringList{QStringLiteral("i"), QStringLiteral("interval")},
                                       i18n("Interval between alarm repetitions"),
                                       QStringLiteral("period"));
    mOptions[KORGANIZER]
              = new QCommandLineOption(QStringList{QStringLiteral("k"), QStringLiteral("korganizer")},
                                       i18n("Show alarm as an event in KOrganizer"));
    mOptions[LATE_CANCEL]
              = new QCommandLineOption(QStringList{QStringLiteral("l"), QStringLiteral("late-cancel")},
                                       i18n("Cancel alarm if more than 'period' late when triggered"),
                                       QStringLiteral("period"),
                                       QStringLiteral("1"));
    mOptions[OptLIST]
              = new QCommandLineOption(QStringLiteral("list"),
                                       i18n("Output list of scheduled alarms to stdout"));
    mOptions[LOGIN]
              = new QCommandLineOption(QStringList{QStringLiteral("L"), QStringLiteral("login")},
                                       i18n("Repeat alarm at every login"));
    mOptions[MAIL]
              = new QCommandLineOption(QStringList{QStringLiteral("m"), QStringLiteral("mail")},
                                       i18n("Send an email to the given address (repeat as needed)"),
                                       QStringLiteral("address"));
    mOptions[NAME]
              = new QCommandLineOption(QStringList{QStringLiteral("n"), QStringLiteral("name")},
                                       i18n("Name of alarm"));
    mOptions[NOTIFY]
              = new QCommandLineOption(QStringList{QStringLiteral("N"), QStringLiteral("notify")},
                                       i18n("Display alarm message as a notification"));
    mOptions[PLAY]
              = new QCommandLineOption(QStringList{QStringLiteral("p"), QStringLiteral("play")},
                                       i18n("Audio file to play once"),
                                       QStringLiteral("url"));
    mOptions[PLAY_REPEAT]
              = new QCommandLineOption(QStringList{QStringLiteral("P"), QStringLiteral("play-repeat")},
                                       i18n("Audio file to play repeatedly"),
                                       QStringLiteral("url"));
    mOptions[RECURRENCE]
              = new QCommandLineOption(QStringLiteral("recurrence"),
                                       i18n("Specify alarm recurrence using iCalendar syntax"),
                                       QStringLiteral("spec"));
    mOptions[REMINDER]
              = new QCommandLineOption(QStringList{QStringLiteral("R"), QStringLiteral("reminder")},
                                       i18n("Display reminder before or after alarm"),
                                       QStringLiteral("period"));
    mOptions[REMINDER_ONCE]
              = new QCommandLineOption(QStringLiteral("reminder-once"),
                                       i18n("Display reminder once, before or after first alarm recurrence"),
                                       QStringLiteral("period"));
    mOptions[REPEAT]
              = new QCommandLineOption(QStringList{QStringLiteral("r"), QStringLiteral("repeat")},
                                       i18n("Number of times to repeat alarm (including initial occasion)"),
                                       QStringLiteral("count"));
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
    mOptions[SPEAK]
              = new QCommandLineOption(QStringList{QStringLiteral("s"), QStringLiteral("speak")},
                                       i18n("Speak the message when it is displayed"));
#endif
    mOptions[SUBJECT]
              = new QCommandLineOption(QStringList{QStringLiteral("S"), QStringLiteral("subject")},
                                       i18n("Email subject line"),
                                       QStringLiteral("text"));
#ifndef NDEBUG
    mOptions[TEST_SET_TIME]
              = new QCommandLineOption(QStringLiteral("test-set-time"),
                                       i18n("Simulate system time [[[yyyy-]mm-]dd-]hh:mm [TZ] (debug mode)"),
                                       QStringLiteral("time"));
#endif
    mOptions[TIME]
              = new QCommandLineOption(QStringList{QStringLiteral("t"), QStringLiteral("time")},
                                       i18n("Trigger alarm at time [[[yyyy-]mm-]dd-]hh:mm [TZ], or date yyyy-mm-dd [TZ]"),
                                       QStringLiteral("time"));
    mOptions[OptTRAY]
              = new QCommandLineOption(QStringLiteral("tray"),
                                       i18n("Display system tray icon"));
    mOptions[OptTRIGGER_EVENT]
              = new QCommandLineOption(QStringLiteral("triggerEvent"),
                                       i18n("Trigger alarm with the specified event ID"),
                                       QStringLiteral("eventID"));
    mOptions[UNTIL]
              = new QCommandLineOption(QStringList{QStringLiteral("u"), QStringLiteral("until")},
                                       i18n("Repeat until time [[[yyyy-]mm-]dd-]hh:mm [TZ], or date yyyy-mm-dd [TZ]"),
                                       QStringLiteral("time"));
    mOptions[VOLUME]
              = new QCommandLineOption(QStringList{QStringLiteral("V"), QStringLiteral("volume")},
                                       i18n("Volume to play audio file"),
                                       QStringLiteral("percent"));

    for (int i = 0; i < mOptions.size(); ++i)
    {
        if (!mOptions.at(i))
            qCCritical(KALARM_LOG) << "CommandOptions::setOptions: Command option" << i << "not initialised";
        else
            mParser->addOption(*(mOptions.at(i)));
    }
    mParser->addPositionalArgument(QStringLiteral("message"),
                                   i18n("Message text to display"),
                                   QStringLiteral("[message]"));

    // Check for any options which eat up all following arguments.
    mNonExecArguments.clear();
    for (int i = 0;  i < args.size();  ++i)
    {
        const QString arg = args[i];
        if (arg == QLatin1String("--nofork"))
            continue;     // Ignore debugging option
        mNonExecArguments << arg;
        if (arg == d->optionName(EXEC)  ||  arg == d->optionName(EXEC, true)
        ||  arg == d->optionName(EXEC_DISPLAY)  ||  arg == d->optionName(EXEC_DISPLAY, true))
        {
            // All following arguments (including ones beginning with '-')
            // belong to this option. QCommandLineParser can't handle this, so
            // remove them from the command line.
            ++i;   // leave the first argument, which is the command to be executed
            while (++i < args.size())
                mExecArguments << args[i];
        }
    }
    return mNonExecArguments;
}

void CommandOptions::parse()
{
    // First check for parse errors (unknown option, missing argument).
    // Simply calling mParser->process() would exit the program if an error is found,
    // which isn't the correct action if another KAlarm instance is running.
    if (!mParser->parse(mNonExecArguments))
    {
        qCWarning(KALARM_LOG) << "CommandOptions::parse:" << mParser->errorText();
        mError.clear();
        d->setError(mParser->errorText());
    }
    else
        mParser->process(mNonExecArguments);
}

void CommandOptions::process()
{
    if (mCommand == CMD_ERROR)
        return;

#ifndef NDEBUG
    if (mParser->isSet(*mOptions.at(TEST_SET_TIME)))
    {
        const QString time = mParser->value(*mOptions.at(TEST_SET_TIME));
        if (!KAlarm::convertTimeString(time.toLatin1(), mSimulationTime, KADateTime::realCurrentLocalDateTime(), true))
            d->setErrorParameter(TEST_SET_TIME);
    }
#endif
    if (d->checkCommand(OptTRAY, TRAY))
    {
    }
    if (d->checkCommand(OptLIST, LIST))
    {
        if (!mParser->positionalArguments().empty())
            d->setErrorParameter(OptLIST);
    }
    if (d->checkCommand(OptTRIGGER_EVENT, TRIGGER_EVENT)
    ||  d->checkCommand(OptCANCEL_EVENT, CANCEL_EVENT)
    ||  d->checkCommand(OptEDIT, EDIT))
    {
        // Fetch the resource and event IDs. The supplied ID is the event ID,
        // optionally prefixed by the resource ID followed by a colon delimiter.
        mResourceId = EventId::extractIDs(mParser->value(*mOptions.at(d->mCommandOpt)), mEventId);
    }
    if (d->checkCommand(OptEDIT_NEW_PRESET, EDIT_NEW_PRESET))
    {
        mName = mParser->value(*mOptions.at(d->mCommandOpt));
    }
    if (d->checkCommand(OptFILE, NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::FILE;
        mEditActionSet = true;
        mText          = mParser->value(*mOptions.at(d->mCommandOpt));
    }
    if (d->checkCommand(EXEC_DISPLAY, NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mFlags        |= KAEvent::DISPLAY_COMMAND;
        mText          = mParser->value(*mOptions.at(d->mCommandOpt)) + QLatin1String(" ") + mExecArguments.join(QLatin1Char(' '));
    }
    if (d->checkCommand(EXEC, NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mText          = mParser->value(*mOptions.at(d->mCommandOpt)) + QLatin1String(" ") + mExecArguments.join(QLatin1Char(' '));
    }
    if (d->checkCommand(MAIL, NEW))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (d->checkCommand(EDIT_NEW_DISPLAY, EDIT_NEW, EditAlarmDlg::DISPLAY))
    {
        mEditType = EditAlarmDlg::DISPLAY;
        if (!mEditActionSet  ||  (mEditAction != KAEvent::COMMAND && mEditAction != KAEvent::FILE))
        {
            mEditAction    = KAEvent::MESSAGE;
            mEditActionSet = true;
        }
        const QStringList args = mParser->positionalArguments();
        if (!args.empty())
            mText = args[0];
    }
    if (d->checkCommand(EDIT_NEW_COMMAND, EDIT_NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
    }
    if (d->checkCommand(EDIT_NEW_EMAIL, EDIT_NEW, EditAlarmDlg::EMAIL))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (d->checkCommand(EDIT_NEW_AUDIO, EDIT_NEW, EditAlarmDlg::AUDIO))
    {
        mEditType      = EditAlarmDlg::AUDIO;
        mEditAction    = KAEvent::AUDIO;
        mEditActionSet = true;
    }
    if (mError.isEmpty()  &&  mCommand == NONE)
    {
        if (mParser->positionalArguments().empty())
        {
            if (d->checkCommand(PLAY, NEW) || d->checkCommand(PLAY_REPEAT, NEW))
            {
                mEditType      = EditAlarmDlg::AUDIO;
                mEditAction    = KAEvent::AUDIO;
                mEditActionSet = true;
            }
        }
        else
        {
            qCDebug(KALARM_LOG) << "CommandOptions::process: Message";
            mCommand       = NEW;
            d->mCommandOpt = Opt_Message;
            mEditType      = EditAlarmDlg::DISPLAY;
            mEditAction    = KAEvent::MESSAGE;
            mEditActionSet = true;
            mText          = arg(0);
        }
    }
    if (mEditActionSet  &&  mEditAction == KAEvent::EMAIL)
    {
        if (mParser->isSet(*mOptions.at(SUBJECT)))
            mSubject = mParser->value(*mOptions.at(SUBJECT));
        if (mParser->isSet(*mOptions.at(FROM_ID)))
            mFromID = Identities::identityUoid(mParser->value(*mOptions.at(FROM_ID)));
        const QStringList mailParams = mParser->values(*mOptions.at(MAIL));
        for (const QString& addr : mailParams)
        {
            QString a(addr);
            if (!KAMail::checkAddress(a))
                d->setError(xi18nc("@info:shell", "<icode>%1</icode>: invalid email address", d->optionName(MAIL)));
            KCalendarCore::Person person(QString(), addr);
            mAddressees += person;
        }
        const QStringList attParams = mParser->values(*mOptions.at(ATTACH));
        for (const QString& att : attParams)
            mAttachments += att;
        const QStringList args = mParser->positionalArguments();
        if (!args.empty())
            mText = args[0];
    }
    if (mParser->isSet(*mOptions.at(DISABLE_ALL)))
    {
        if (mCommand == TRIGGER_EVENT  ||  mCommand == LIST)
            d->setErrorIncompatible(DISABLE_ALL, d->mCommandOpt);
        mDisableAll = true;
    }

    // Check that other options are only specified for the
    // correct main command options.
    d->checkEditType(EditAlarmDlg::DISPLAY, COLOUR);
    d->checkEditType(EditAlarmDlg::DISPLAY, COLOURFG);
    d->checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, PLAY);
    d->checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, PLAY_REPEAT);
    d->checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, VOLUME);
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
    d->checkEditType(EditAlarmDlg::DISPLAY, SPEAK);
#endif
    d->checkEditType(EditAlarmDlg::DISPLAY, BEEP);
    d->checkEditType(EditAlarmDlg::DISPLAY, REMINDER);
    d->checkEditType(EditAlarmDlg::DISPLAY, REMINDER_ONCE);
    d->checkEditType(EditAlarmDlg::DISPLAY, ACK_CONFIRM);
    d->checkEditType(EditAlarmDlg::DISPLAY, AUTO_CLOSE);
    d->checkEditType(EditAlarmDlg::DISPLAY, NOTIFY);
    d->checkEditType(EditAlarmDlg::EMAIL, SUBJECT);
    d->checkEditType(EditAlarmDlg::EMAIL, FROM_ID);
    d->checkEditType(EditAlarmDlg::EMAIL, ATTACH);
    d->checkEditType(EditAlarmDlg::EMAIL, BCC);

    switch (mCommand)
    {
        case EDIT_NEW:
            if (mParser->isSet(*mOptions.at(DISABLE)))
                d->setErrorIncompatible(DISABLE, d->mCommandOpt);
            // Fall through to NEW
            Q_FALLTHROUGH();
        case NEW:
        {
            // Display a message or file, execute a command, or send an email
            if (mParser->isSet(*mOptions.at(NAME)))
            {
                // Alarm name is specified
                mName = mParser->value(*mOptions.at(NAME));
            }
            if (mParser->isSet(*mOptions.at(COLOUR)))
            {
                // Background colour is specified
                QString colourText = mParser->value(*mOptions.at(COLOUR));
                if (colourText.at(0) == QLatin1Char('0')
                &&  colourText.at(1).toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QStringLiteral("#"));
                mBgColour.setNamedColor(colourText);
                if (!mBgColour.isValid())
                    d->setErrorParameter(COLOUR);
            }
            if (mParser->isSet(*mOptions.at(COLOURFG)))
            {
                // Foreground colour is specified
                QString colourText = mParser->value(*mOptions.at(COLOURFG));
                if (colourText.at(0) == QLatin1Char('0')
                &&  colourText.at(1).toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QStringLiteral("#"));
                mFgColour.setNamedColor(colourText);
                if (!mFgColour.isValid())
                    d->setErrorParameter(COLOURFG);
            }

            if (mParser->isSet(*mOptions.at(TIME)))
            {
                const QByteArray dateTime = mParser->value(*mOptions.at(TIME)).toLocal8Bit();
                if (!KAlarm::convertTimeString(dateTime, mAlarmTime))
                    d->setErrorParameter(TIME);
            }
            else
                mAlarmTime = KADateTime::currentLocalDateTime();

            const bool haveRecurrence = mParser->isSet(*mOptions.at(RECURRENCE));
            if (haveRecurrence)
            {
                if (mParser->isSet(*mOptions.at(LOGIN)))
                    d->setErrorIncompatible(LOGIN, RECURRENCE);
                else if (mParser->isSet(*mOptions.at(UNTIL)))
                    d->setErrorIncompatible(UNTIL, RECURRENCE);
                const QString rule = mParser->value(*mOptions.at(RECURRENCE));
                mRecurrence = new KARecurrence;
                mRecurrence->set(rule);
            }
            if (mParser->isSet(*mOptions.at(INTERVAL)))
            {
                // Repeat count is specified
                int count = 0;
                KADateTime endTime;
                if (mParser->isSet(*mOptions.at(LOGIN)))
                    d->setErrorIncompatible(LOGIN, INTERVAL);
                bool ok;
                if (mParser->isSet(*mOptions.at(REPEAT)))
                {
                    count = mParser->value(*mOptions.at(REPEAT)).toInt(&ok);
                    if (!ok || !count || count < -1 || (count < 0 && haveRecurrence))
                        d->setErrorParameter(REPEAT);
                }
                else if (haveRecurrence)
                    d->setErrorRequires(INTERVAL, REPEAT);
                else if (mParser->isSet(*mOptions.at(UNTIL)))
                {
                    count = 0;
                    const QByteArray dateTime = mParser->value(*mOptions.at(UNTIL)).toLocal8Bit();
                    if (mParser->isSet(*mOptions.at(TIME)))
                        ok = KAlarm::convertTimeString(dateTime, endTime, mAlarmTime);
                    else
                        ok = KAlarm::convertTimeString(dateTime, endTime);
                    if (!ok)
                        d->setErrorParameter(UNTIL);
                    else if (mAlarmTime.isDateOnly()  &&  !endTime.isDateOnly())
                        d->setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", d->optionName(UNTIL)));
                    if (!mAlarmTime.isDateOnly()  &&  endTime.isDateOnly())
                        endTime.setTime(QTime(23,59,59));
                    if (endTime < mAlarmTime)
                        d->setError(xi18nc("@info:shell", "<icode>%1</icode> earlier than <icode>%2</icode>", d->optionName(UNTIL), d->optionName(TIME)));
                }
                else
                    count = -1;

                // Get the recurrence interval
                int intervalOfType;
                KARecurrence::Type recurType;
                if (!convInterval(mParser->value(*mOptions.at(INTERVAL)), recurType, intervalOfType, !haveRecurrence))
                    d->setErrorParameter(INTERVAL);
                else if (mAlarmTime.isDateOnly()  &&  recurType == KARecurrence::MINUTELY)
                    d->setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", d->optionName(INTERVAL)));

                if (haveRecurrence)
                {
                    if (mRecurrence)
                    {
                        // There is a also a recurrence specified, so set up a sub-repetition.
                        // In this case, 'intervalOfType' is in minutes.
                        mRepeatInterval = KCalendarCore::Duration(intervalOfType * 60);
                        const KCalendarCore::Duration longestInterval = mRecurrence->longestInterval();
                        if (mRepeatInterval * count > longestInterval)
                            d->setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> and <icode>%2</icode> parameters: repetition is longer than <icode>%3</icode> interval",
                                           d->optionName(INTERVAL), d->optionName(REPEAT), d->optionName(RECURRENCE)));
                        mRepeatCount = count;
                    }
                }
                else
                {
                    // There is no other recurrence specified, so convert the repetition
                    // parameters into a KCal::Recurrence
                    mRecurrence = new KARecurrence;
                    mRecurrence->set(recurType, intervalOfType, count, mAlarmTime, endTime);
                }
            }
            else
            {
                if (mParser->isSet(*mOptions.at(REPEAT)))
                    d->setErrorRequires(REPEAT, INTERVAL);
                else if (mParser->isSet(*mOptions.at(UNTIL)))
                    d->setErrorRequires(UNTIL, INTERVAL);
            }

            const bool audioRepeat = mParser->isSet(*mOptions.at(PLAY_REPEAT));
            if (audioRepeat  ||  mParser->isSet(*mOptions.at(PLAY)))
            {
                // Play a sound with the alarm
                const Option opt = audioRepeat ? PLAY_REPEAT : PLAY;
                if (audioRepeat  &&  mParser->isSet(*mOptions.at(PLAY)))
                    d->setErrorIncompatible(PLAY, PLAY_REPEAT);
                if (mParser->isSet(*mOptions.at(BEEP)))
                    d->setErrorIncompatible(BEEP, opt);
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
                else if (mParser->isSet(*mOptions.at(SPEAK)))
                    d->setErrorIncompatible(SPEAK, opt);
#endif
                mAudioFile = mParser->value(*mOptions.at(audioRepeat ? PLAY_REPEAT : PLAY));
                if (mParser->isSet(*mOptions.at(VOLUME)))
                {
                    bool ok;
                    const int volumepc = mParser->value(*mOptions.at(VOLUME)).toInt(&ok);
                    if (!ok  ||  volumepc < 0  ||  volumepc > 100)
                        d->setErrorParameter(VOLUME);
                    mAudioVolume = static_cast<float>(volumepc) / 100;
                }
            }
            else if (mParser->isSet(*mOptions.at(VOLUME)))
                d->setErrorRequires(VOLUME, PLAY, PLAY_REPEAT);
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
            if (mParser->isSet(*mOptions.at(SPEAK)))
            {
                if (mParser->isSet(*mOptions.at(BEEP)))
                    d->setErrorIncompatible(BEEP, SPEAK);
            }
#endif
            const bool onceOnly = mParser->isSet(*mOptions.at(REMINDER_ONCE));
            if (mParser->isSet(*mOptions.at(REMINDER))  ||  onceOnly)
            {
                // Issue a reminder alarm in advance of or after the main alarm
                if (onceOnly  &&  mParser->isSet(*mOptions.at(REMINDER)))
                    d->setErrorIncompatible(REMINDER, REMINDER_ONCE);
                const Option opt = onceOnly ? REMINDER_ONCE : REMINDER;
                KARecurrence::Type recurType;
                QString optval = mParser->value(*mOptions.at(onceOnly ? REMINDER_ONCE : REMINDER));
                const bool after = (optval.at(0) == QLatin1Char('+'));
                if (after)
                    optval.remove(0, 1);   // it's a reminder after the main alarm
                if (!convInterval(optval, recurType, mReminderMinutes))
                    d->setErrorParameter(opt);
                else if (recurType == KARecurrence::MINUTELY  &&  mAlarmTime.isDateOnly())
                    d->setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", d->optionName(opt)));
                if (after)
                    mReminderMinutes = -mReminderMinutes;
                if (onceOnly)
                    mFlags |= KAEvent::REMINDER_ONCE;
            }

            if (mParser->isSet(*mOptions.at(LATE_CANCEL)))
            {
                KARecurrence::Type recurType;
                const bool ok = convInterval(mParser->value(*mOptions.at(LATE_CANCEL)), recurType, mLateCancel);
                if (!ok)
                    d->setErrorParameter(LATE_CANCEL);
            }
            else if (mParser->isSet(*mOptions.at(AUTO_CLOSE)))
                d->setErrorRequires(AUTO_CLOSE, LATE_CANCEL);

            if (mParser->isSet(*mOptions.at(NOTIFY)))
            {
                if (mParser->isSet(*mOptions.at(COLOUR)))
                    d->setErrorIncompatible(NOTIFY, COLOUR);
                if (mParser->isSet(*mOptions.at(COLOURFG)))
                    d->setErrorIncompatible(NOTIFY, COLOURFG);
                if (mParser->isSet(*mOptions.at(ACK_CONFIRM)))
                    d->setErrorIncompatible(NOTIFY, ACK_CONFIRM);
                if (mParser->isSet(*mOptions.at(PLAY)))
                    d->setErrorIncompatible(NOTIFY, PLAY);
                if (mParser->isSet(*mOptions.at(AUTO_CLOSE)))
                    d->setErrorIncompatible(NOTIFY, AUTO_CLOSE);
            }

            if (mParser->isSet(*mOptions.at(ACK_CONFIRM)))
                mFlags |= KAEvent::CONFIRM_ACK;
            if (mParser->isSet(*mOptions.at(AUTO_CLOSE)))
                mFlags |= KAEvent::AUTO_CLOSE;
            if (mParser->isSet(*mOptions.at(BEEP)))
                mFlags |= KAEvent::BEEP;
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
            if (mParser->isSet(*mOptions.at(SPEAK)))
                mFlags |= KAEvent::SPEAK;
#endif
            if (mParser->isSet(*mOptions.at(NOTIFY)))
                mFlags |= KAEvent::NOTIFY;
            if (mParser->isSet(*mOptions.at(KORGANIZER)))
                mFlags |= KAEvent::COPY_KORGANIZER;
            if (mParser->isSet(*mOptions.at(DISABLE)))
                mFlags |= KAEvent::DISABLED;
            if (audioRepeat)
                mFlags |= KAEvent::REPEAT_SOUND;
            if (mParser->isSet(*mOptions.at(LOGIN)))
                mFlags |= KAEvent::REPEAT_AT_LOGIN;
            if (mParser->isSet(*mOptions.at(BCC)))
                mFlags |= KAEvent::EMAIL_BCC;
            if (mAlarmTime.isDateOnly())
                mFlags |= KAEvent::ANY_TIME;
            break;
        }
        case NONE:
        {
            // No arguments - run interactively & display the main window
            if (!mError.isEmpty())
                break;
            qCDebug(KALARM_LOG) << "CommandOptions::process: Interactive";
            QStringList errors;
            if (mParser->isSet(*mOptions.at(ACK_CONFIRM)))
                errors << d->optionName(ACK_CONFIRM);
            if (mParser->isSet(*mOptions.at(ATTACH)))
                errors << d->optionName(ATTACH);
            if (mParser->isSet(*mOptions.at(AUTO_CLOSE)))
                errors << d->optionName(AUTO_CLOSE);
            if (mParser->isSet(*mOptions.at(BCC)))
                errors << d->optionName(BCC);
            if (mParser->isSet(*mOptions.at(BEEP)))
                errors << d->optionName(BEEP);
            if (mParser->isSet(*mOptions.at(COLOUR)))
                errors << d->optionName(COLOUR);
            if (mParser->isSet(*mOptions.at(COLOURFG)))
                errors << d->optionName(COLOURFG);
            if (mParser->isSet(*mOptions.at(DISABLE)))
                errors << d->optionName(DISABLE);
            if (mParser->isSet(*mOptions.at(FROM_ID)))
                errors << d->optionName(FROM_ID);
            if (mParser->isSet(*mOptions.at(KORGANIZER)))
                errors << d->optionName(KORGANIZER);
            if (mParser->isSet(*mOptions.at(LATE_CANCEL)))
                errors << d->optionName(LATE_CANCEL);
            if (mParser->isSet(*mOptions.at(LOGIN)))
                errors << d->optionName(LOGIN);
            if (mParser->isSet(*mOptions.at(NAME)))
                errors << d->optionName(NAME);
            if (mParser->isSet(*mOptions.at(NOTIFY)))
                errors << d->optionName(NOTIFY);
            if (mParser->isSet(*mOptions.at(PLAY)))
                errors << d->optionName(PLAY);
            if (mParser->isSet(*mOptions.at(PLAY_REPEAT)))
                errors << d->optionName(PLAY_REPEAT);
            if (mParser->isSet(*mOptions.at(REMINDER)))
                errors << d->optionName(REMINDER);
            if (mParser->isSet(*mOptions.at(REMINDER_ONCE)))
                errors << d->optionName(REMINDER_ONCE);
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
            if (mParser->isSet(*mOptions.at(SPEAK)))
                errors << d->optionName(SPEAK);
#endif
            if (mParser->isSet(*mOptions.at(NOTIFY)))
                errors << d->optionName(NOTIFY);
            if (mParser->isSet(*mOptions.at(SUBJECT)))
                errors << d->optionName(SUBJECT);
            if (mParser->isSet(*mOptions.at(TIME)))
                errors << d->optionName(TIME);
            if (mParser->isSet(*mOptions.at(VOLUME)))
                errors << d->optionName(VOLUME);
            if (!errors.isEmpty())
                mError = errors.join(QLatin1Char(' ')) + i18nc("@info:shell", ": option(s) only valid with an appropriate action option or message");
            break;
        }
        default:
            break;
    }

    if (!mError.isEmpty())
        d->setError(mError);
}

QString CommandOptions::commandName() const
{
    return d->optionName(d->mCommandOpt);
}

void CommandOptions::printError(const QString& errmsg)
{
    // Note: we can't use mArgs->usage() since that also quits any other
    // running 'instances' of the program.
    std::cerr << errmsg.toLocal8Bit().data()
              << i18nc("@info:shell", "\nUse --help to get a list of available command line options.\n").toLocal8Bit().data();
}

// Fetch one of the arguments (i.e. not belonging to any option).
QString CommandOptions::arg(int n)
{
    const QStringList args = mParser->positionalArguments();
    return (n < args.size()) ? args[n] : QString();
}

/*===========================================================================*/

void CommandOptions::Private::setError(const QString& errmsg)
{
    qCWarning(KALARM_LOG) << "CommandOptions::setError:" << errmsg;
    p->mCommand = CMD_ERROR;
    if (p->mError.isEmpty())
        p->mError = errmsg + i18nc("@info:shell", "\nUse --help to get a list of available command line options.\n");
}

/******************************************************************************
* Check if the given command option is specified, and if so set mCommand etc.
* If another command option has also been detected, issue an error.
* If 'allowedEditType' is set, supersede any previous specification of that
* edit type with the given command option - this allows, e.g., --mail to be
* used along with --edit-new-email so the user can specify addressees.
*/
bool CommandOptions::Private::checkCommand(Option command, Command code, EditAlarmDlg::Type allowedEditType)
{
    if (!p->mError.isEmpty()
    ||  !p->mParser->isSet(*p->mOptions.at(command)))
        return false;
    if (p->mCommand != NONE
    &&  (allowedEditType == EditAlarmDlg::NO_TYPE  ||  (p->mCommand != NEW || p->mEditType != allowedEditType)))
        setErrorIncompatible(mCommandOpt, command);
    qCDebug(KALARM_LOG).nospace() << "CommandOptions::checkCommand: " << optionName(command);
    p->mCommand = code;
    mCommandOpt = command;
    return true;
}

// Set the error message to "--opt requires --opt2" or "--opt requires --opt2 or --opt3".
void CommandOptions::Private::setErrorRequires(Option opt, Option opt2, Option opt3)
{
    if (opt3 == Num_Options)
        setError(xi18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", optionName(opt), optionName(opt2)));
    else
        setError(xi18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode> or <icode>%3</icode>", optionName(opt), optionName(opt2), optionName(opt3)));
}

void CommandOptions::Private::setErrorParameter(Option opt)
{
    setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter", optionName(opt)));
}

void CommandOptions::Private::setErrorIncompatible(Option opt1, Option opt2)
{
    setError(xi18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", optionName(opt1), optionName(opt2)));
}

void CommandOptions::Private::checkEditType(EditAlarmDlg::Type type1, EditAlarmDlg::Type type2, Option opt)
{
    if (p->mParser->isSet(*p->mOptions.at(opt))  &&  p->mCommand != NONE
    &&  ((p->mCommand != NEW && p->mCommand != EDIT_NEW)  ||  (p->mEditType != type1 && (type2 == EditAlarmDlg::NO_TYPE || p->mEditType != type2))))
        setErrorIncompatible(opt, mCommandOpt);
}

QString CommandOptions::Private::optionName(Option opt, bool shortName) const
{
    if (opt == Opt_Message)
        return QStringLiteral("message");
    const QStringList names = p->mOptions.at(opt)->names();
    if (names.empty())
        return {};
    for (const QString& name : names)
    {
        if (shortName  &&  name.size() == 1)
            return QStringLiteral("-") + name;
        else if (!shortName  &&  name.size() > 1)
            return QStringLiteral("--") + name;
    }
    return QStringLiteral("-") + names[0];
}

namespace
{

/******************************************************************************
* Convert a non-zero positive time interval command line parameter.
* 'timeInterval' receives the count for the recurType. If 'allowMonthYear' is
* false, weeks and days are converted to minutes.
* Reply = true if successful.
*/
bool convInterval(const QString& timeParam, KARecurrence::Type& recurType, int& timeInterval, bool allowMonthYear)
{
    QByteArray timeString = timeParam.toLocal8Bit();
    // Get the recurrence interval
    bool ok = true;
    uint interval = 0;
    uint length = timeString.length();
    switch (timeString.at(length - 1))
    {
        case 'Y':
            if (!allowMonthYear)
                ok = false;
            recurType = KARecurrence::ANNUAL_DATE;
            timeString = timeString.left(length - 1);
            break;
        case 'W':
            recurType = KARecurrence::WEEKLY;
            timeString = timeString.left(length - 1);
            break;
        case 'D':
            recurType = KARecurrence::DAILY;
            timeString = timeString.left(length - 1);
            break;
        case 'M':
        {
            int i = timeString.indexOf('H');
            if (i < 0)
            {
                if (!allowMonthYear)
                    ok = false;
                recurType = KARecurrence::MONTHLY_DAY;
                timeString = timeString.left(length - 1);
            }
            else
            {
                recurType = KARecurrence::MINUTELY;
                interval = timeString.left(i).toUInt(&ok) * 60;
                timeString = timeString.mid(i + 1, length - i - 2);
            }
            break;
        }
        default:       // should be a digit
            recurType = KARecurrence::MINUTELY;
            break;
    }
    if (ok)
        interval += timeString.toUInt(&ok);
    if (!allowMonthYear)
    {
        // Convert time interval to minutes
        switch (recurType)
        {
            case KARecurrence::WEEKLY:
                interval *= 7;
                // fall through to DAILY
                Q_FALLTHROUGH();
            case KARecurrence::DAILY:
                interval *= 24*60;
                recurType = KARecurrence::MINUTELY;
                break;
            default:
                break;
        }
    }
    timeInterval = static_cast<int>(interval);
    return ok && interval;
}

}

// vim: et sw=4:
