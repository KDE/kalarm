/*
 *  commandoptions.cpp  -  extract command line options
 *  Program:  kalarm
 *  Copyright © 2001-2018 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"      //krazy:exclude=includes (kalarm.h must be first)
#include "commandoptions.h"
#include "alarmtime.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "kalarm_debug.h"

#include <kalarmcal/identities.h>
#include <kpimtextedit/texttospeech.h>
#include <KLocalizedString>

#include <QCommandLineParser>

#include <iostream>

namespace
{
bool convInterval(const QString& timeParam, KARecurrence::Type&, int& timeInterval, bool allowMonthYear = false);
}

CommandOptions* CommandOptions::mFirstInstance = nullptr;

CommandOptions::CommandOptions()
  : mParser(nullptr),
    mOptions(Num_Options, nullptr),
    mCommand(NONE),
    mEditActionSet(false),
    mRecurrence(nullptr),
    mRepeatCount(0),
    mRepeatInterval(0),
    mLateCancel(0),
    mBgColour(Preferences::defaultBgColour()),
    mFgColour(Preferences::defaultFgColour()),
    mReminderMinutes(0),
    mAudioVolume(-1),
    mFromID(0),
    mFlags(KAEvent::DEFAULT_FONT),
    mDisableAll(false)
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
              = new QCommandLineOption(QStringList() << QStringLiteral("a") << QStringLiteral("ack-confirm"),
                                       i18n("Prompt for confirmation when alarm is acknowledged"));
    mOptions[ATTACH]
              = new QCommandLineOption(QStringList() << QStringLiteral("A") << QStringLiteral("attach"),
                                       i18n("Attach file to email (repeat as needed)"),
                                       QStringLiteral("url"));
    mOptions[AUTO_CLOSE]
              = new QCommandLineOption(QStringLiteral("auto-close"),
                                       i18n("Auto-close alarm window after --late-cancel period"));
    mOptions[BCC]
              = new QCommandLineOption(QStringLiteral("bcc"),
                                       i18n("Blind copy email to self"));
    mOptions[BEEP]
              = new QCommandLineOption(QStringList() << QStringLiteral("b") << QStringLiteral("beep"),
                                       i18n("Beep when message is displayed"));
    mOptions[COLOUR]
              = new QCommandLineOption(QStringList() << QStringLiteral("c") << QStringLiteral("colour") << QStringLiteral("color"),
                                       i18n("Message background color (name or hex 0xRRGGBB)"),
                                       QStringLiteral("color"));
    mOptions[COLOURFG]
              = new QCommandLineOption(QStringList() << QStringLiteral("C") << QStringLiteral("colourfg") << QStringLiteral("colorfg"),
                                       i18n("Message foreground color (name or hex 0xRRGGBB)"),
                                       QStringLiteral("color"));
    mOptions[OptCANCEL_EVENT]
              = new QCommandLineOption(QStringLiteral("cancelEvent"),
                                       i18n("Cancel alarm with the specified event ID"),
                                       QStringLiteral("eventID"));
    mOptions[DISABLE]
              = new QCommandLineOption(QStringList() << QStringLiteral("d") << QStringLiteral("disable"),
                                       i18n("Disable the alarm"));
    mOptions[DISABLE_ALL]
              = new QCommandLineOption(QStringLiteral("disable-all"),
                                       i18n("Disable monitoring of all alarms"));
    mOptions[EXEC]
              = new QCommandLineOption(QStringList() << QStringLiteral("e") << QStringLiteral("exec"),
                                       i18n("Execute a shell command line"),
                                       QStringLiteral("commandLine"));
    mOptions[EXEC_DISPLAY]
              = new QCommandLineOption(QStringList() << QStringLiteral("E") << QStringLiteral("exec-display"),
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
    mOptions[FILE]
              = new QCommandLineOption(QStringList() << QStringLiteral("f") << QStringLiteral("file"),
                                       i18n("File to display"),
                                       QStringLiteral("url"));
    mOptions[FROM_ID]
              = new QCommandLineOption(QStringList() << QStringLiteral("F") << QStringLiteral("from-id"),
                                       i18n("KMail identity to use as sender of email"),
                                       QStringLiteral("ID"));
    mOptions[INTERVAL]
              = new QCommandLineOption(QStringList() << QStringLiteral("i") << QStringLiteral("interval"),
                                       i18n("Interval between alarm repetitions"),
                                       QStringLiteral("period"));
    mOptions[KORGANIZER]
              = new QCommandLineOption(QStringList() << QStringLiteral("k") << QStringLiteral("korganizer"),
                                       i18n("Show alarm as an event in KOrganizer"));
    mOptions[LATE_CANCEL]
              = new QCommandLineOption(QStringList() << QStringLiteral("l") << QStringLiteral("late-cancel"),
                                       i18n("Cancel alarm if more than 'period' late when triggered"),
                                       QStringLiteral("period"),
                                       QStringLiteral("1"));
    mOptions[OptLIST]
              = new QCommandLineOption(QStringLiteral("list"),
                                       i18n("Output list of scheduled alarms to stdout"));
    mOptions[LOGIN]
              = new QCommandLineOption(QStringList() << QStringLiteral("L") << QStringLiteral("login"),
                                       i18n("Repeat alarm at every login"));
    mOptions[MAIL]
              = new QCommandLineOption(QStringList() << QStringLiteral("m") << QStringLiteral("mail"),
                                       i18n("Send an email to the given address (repeat as needed)"),
                                       QStringLiteral("address"));
    mOptions[PLAY]
              = new QCommandLineOption(QStringList() << QStringLiteral("p") << QStringLiteral("play"),
                                       i18n("Audio file to play once"),
                                       QStringLiteral("url"));
    mOptions[PLAY_REPEAT]
              = new QCommandLineOption(QStringList() << QStringLiteral("P") << QStringLiteral("play-repeat"),
                                       i18n("Audio file to play repeatedly"),
                                       QStringLiteral("url"));
    mOptions[RECURRENCE]
              = new QCommandLineOption(QStringLiteral("recurrence"),
                                       i18n("Specify alarm recurrence using iCalendar syntax"),
                                       QStringLiteral("spec"));
    mOptions[REMINDER]
              = new QCommandLineOption(QStringList() << QStringLiteral("R") << QStringLiteral("reminder"),
                                       i18n("Display reminder before or after alarm"),
                                       QStringLiteral("period"));
    mOptions[REMINDER_ONCE]
              = new QCommandLineOption(QStringLiteral("reminder-once"),
                                       i18n("Display reminder once, before or after first alarm recurrence"),
                                       QStringLiteral("period"));
    mOptions[REPEAT]
              = new QCommandLineOption(QStringList() << QStringLiteral("r") << QStringLiteral("repeat"),
                                       i18n("Number of times to repeat alarm (including initial occasion)"),
                                       QStringLiteral("count"));
    mOptions[SPEAK]
              = new QCommandLineOption(QStringList() << QStringLiteral("s") << QStringLiteral("speak"),
                                       i18n("Speak the message when it is displayed"));
    mOptions[SUBJECT]
              = new QCommandLineOption(QStringList() << QStringLiteral("S") << QStringLiteral("subject"),
                                       i18n("Email subject line"),
                                       QStringLiteral("text"));
#ifndef NDEBUG
    mOptions[TEST_SET_TIME]
              = new QCommandLineOption(QStringLiteral("test-set-time"),
                                       i18n("Simulate system time [[[yyyy-]mm-]dd-]hh:mm [TZ] (debug mode)"),
                                       QStringLiteral("time"));
#endif
    mOptions[TIME]
              = new QCommandLineOption(QStringList() << QStringLiteral("t") << QStringLiteral("time"),
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
              = new QCommandLineOption(QStringList() << QStringLiteral("u") << QStringLiteral("until"),
                                       i18n("Repeat until time [[[yyyy-]mm-]dd-]hh:mm [TZ], or date yyyy-mm-dd [TZ]"),
                                       QStringLiteral("time"));
    mOptions[VOLUME]
              = new QCommandLineOption(QStringList() << QStringLiteral("V") << QStringLiteral("volume"),
                                       i18n("Volume to play audio file"),
                                       QStringLiteral("percent"));

    for (int i = 0; i < mOptions.size(); ++i)
    {
        if (!mOptions[i])
            qCCritical(KALARM_LOG) << "Command option" << i << "not initialised";
        else
            mParser->addOption(*(mOptions[i]));
    }
    mParser->addVersionOption();
    mParser->addHelpOption();
    mParser->addPositionalArgument(QStringLiteral("message"),
                                   i18n("Message text to display"),
                                   QStringLiteral("[message]"));

    // Check for any options which eat up all following arguments.
    mNonExecArguments.clear();
    for (int i = 0;  i < args.size();  ++i)
    {
        const QString arg = args[i];
        if (arg == QStringLiteral("--nofork"))
            continue;     // Ignore debugging option
        mNonExecArguments << arg;
        if (arg == optionName(EXEC)  ||  arg == optionName(EXEC, true)
        ||  arg == optionName(EXEC_DISPLAY)  ||  arg == optionName(EXEC_DISPLAY, true))
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
    if (!mParser->parse(mNonExecArguments))
    {
        setError(mParser->errorText());
        return;
    }
    if (mParser->isSet(QStringLiteral("help")))
    {
        mCommand = EXIT;
        mError = mParser->helpText();
        return;
    }
    if (mParser->isSet(QStringLiteral("version")))
    {
        mCommand = EXIT;
        mError = QCoreApplication::applicationName() + QStringLiteral(" ") + QCoreApplication::applicationVersion();
        return;
    }
}

void CommandOptions::process()
{
    if (mCommand == CMD_ERROR  ||  mCommand == EXIT)
        return;

#ifndef NDEBUG
    if (mParser->isSet(*mOptions[TEST_SET_TIME]))
    {
        const QString time = mParser->value(*mOptions[TEST_SET_TIME]);
        if (!AlarmTime::convertTimeString(time.toLatin1(), mSimulationTime, KADateTime::realCurrentLocalDateTime(), true))
            setErrorParameter(TEST_SET_TIME);
    }
#endif
    if (checkCommand(OptTRAY, TRAY))
    {
    }
    if (checkCommand(OptLIST, LIST))
    {
        if (!mParser->positionalArguments().empty())
            setErrorParameter(OptLIST);
    }
    if (checkCommand(OptTRIGGER_EVENT, TRIGGER_EVENT)
    ||  checkCommand(OptCANCEL_EVENT, CANCEL_EVENT)
    ||  checkCommand(OptEDIT, EDIT))
    {
        // Fetch the event ID. This can optionally include a prefix of the
        // resource ID followed by a colon delimiter.
        mEventId = EventId(mParser->value(*mOptions[mCommandOpt]));
    }
    if (checkCommand(OptEDIT_NEW_PRESET, EDIT_NEW_PRESET))
    {
        mTemplateName = mParser->value(*mOptions[mCommandOpt]);
    }
    if (checkCommand(FILE, NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::FILE;
        mEditActionSet = true;
        mText          = mParser->value(*mOptions[mCommandOpt]);
    }
    if (checkCommand(EXEC_DISPLAY, NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mFlags        |= KAEvent::DISPLAY_COMMAND;
        mText          = mParser->value(*mOptions[mCommandOpt]) + QStringLiteral(" ") + mExecArguments.join(QLatin1Char(' '));
    }
    if (checkCommand(EXEC, NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mText          = mParser->value(*mOptions[mCommandOpt]) + QStringLiteral(" ") + mExecArguments.join(QLatin1Char(' '));
    }
    if (checkCommand(MAIL, NEW))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (checkCommand(EDIT_NEW_DISPLAY, EDIT_NEW, EditAlarmDlg::DISPLAY))
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
    if (checkCommand(EDIT_NEW_COMMAND, EDIT_NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
    }
    if (checkCommand(EDIT_NEW_EMAIL, EDIT_NEW, EditAlarmDlg::EMAIL))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (checkCommand(EDIT_NEW_AUDIO, EDIT_NEW, EditAlarmDlg::AUDIO))
    {
        mEditType      = EditAlarmDlg::AUDIO;
        mEditAction    = KAEvent::AUDIO;
        mEditActionSet = true;
    }
    if (mError.isEmpty()  &&  mCommand == NONE)
    {
        if (mParser->positionalArguments().empty())
        {
            if (checkCommand(PLAY, NEW) || checkCommand(PLAY_REPEAT, NEW))
            {
                mEditType      = EditAlarmDlg::AUDIO;
                mEditAction    = KAEvent::AUDIO;
                mEditActionSet = true;
            }
        }
        else
        {
            qCDebug(KALARM_LOG) << "Message";
            mCommand       = NEW;
            mCommandOpt    = Opt_Message;
            mEditType      = EditAlarmDlg::DISPLAY;
            mEditAction    = KAEvent::MESSAGE;
            mEditActionSet = true;
            mText          = arg(0);
        }
    }
    if (mEditActionSet  &&  mEditAction == KAEvent::EMAIL)
    {
        if (mParser->isSet(*mOptions[SUBJECT]))
            mSubject = mParser->value(*mOptions[SUBJECT]);
        if (mParser->isSet(*mOptions[FROM_ID]))
            mFromID = Identities::identityUoid(mParser->value(*mOptions[FROM_ID]));
        QStringList params = mParser->values(*mOptions[MAIL]);
        for (int i = 0, count = params.count();  i < count;  ++i)
        {
            QString addr = params[i];
            if (!KAMail::checkAddress(addr))
                setError(xi18nc("@info:shell", "<icode>%1</icode>: invalid email address", optionName(MAIL)));
            KCalCore::Person::Ptr person(new KCalCore::Person(QString(), addr));
            mAddressees += person;
        }
        params = mParser->values(*mOptions[ATTACH]);
        for (int i = 0, count = params.count();  i < count;  ++i)
            mAttachments += params[i];
        const QStringList args = mParser->positionalArguments();
        if (!args.empty())
            mText = args[0];
    }
    if (mParser->isSet(*mOptions[DISABLE_ALL]))
    {
        if (mCommand == TRIGGER_EVENT  ||  mCommand == LIST)
            setErrorIncompatible(DISABLE_ALL, mCommandOpt);
        mDisableAll = true;
    }

    // Check that other options are only specified for the
    // correct main command options.
    checkEditType(EditAlarmDlg::DISPLAY, COLOUR);
    checkEditType(EditAlarmDlg::DISPLAY, COLOURFG);
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, PLAY);
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, PLAY_REPEAT);
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, VOLUME);
    checkEditType(EditAlarmDlg::DISPLAY, SPEAK);
    checkEditType(EditAlarmDlg::DISPLAY, BEEP);
    checkEditType(EditAlarmDlg::DISPLAY, REMINDER);
    checkEditType(EditAlarmDlg::DISPLAY, REMINDER_ONCE);
    checkEditType(EditAlarmDlg::DISPLAY, ACK_CONFIRM);
    checkEditType(EditAlarmDlg::DISPLAY, AUTO_CLOSE);
    checkEditType(EditAlarmDlg::EMAIL, SUBJECT);
    checkEditType(EditAlarmDlg::EMAIL, FROM_ID);
    checkEditType(EditAlarmDlg::EMAIL, ATTACH);
    checkEditType(EditAlarmDlg::EMAIL, BCC);

    switch (mCommand)
    {
        case EDIT_NEW:
            if (mParser->isSet(*mOptions[DISABLE]))
                setErrorIncompatible(DISABLE, mCommandOpt);
            // Fall through to NEW
        case NEW:
        {
            // Display a message or file, execute a command, or send an email
            if (mParser->isSet(*mOptions[COLOUR]))
            {
                // Background colour is specified
                QString colourText = mParser->value(*mOptions[COLOUR]);
                if (colourText[0] == QLatin1Char('0') && colourText[1].toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QStringLiteral("#"));
                mBgColour.setNamedColor(colourText);
                if (!mBgColour.isValid())
                    setErrorParameter(COLOUR);
            }
            if (mParser->isSet(*mOptions[COLOURFG]))
            {
                // Foreground colour is specified
                QString colourText = mParser->value(*mOptions[COLOURFG]);
                if (colourText[0] == QLatin1Char('0') && colourText[1].toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QStringLiteral("#"));
                mFgColour.setNamedColor(colourText);
                if (!mFgColour.isValid())
                    setErrorParameter(COLOURFG);
            }

            if (mParser->isSet(*mOptions[TIME]))
            {
                QByteArray dateTime = mParser->value(*mOptions[TIME]).toLocal8Bit();
                if (!AlarmTime::convertTimeString(dateTime, mAlarmTime))
                    setErrorParameter(TIME);
            }
            else
                mAlarmTime = KADateTime::currentLocalDateTime();

            bool haveRecurrence = mParser->isSet(*mOptions[RECURRENCE]);
            if (haveRecurrence)
            {
                if (mParser->isSet(*mOptions[LOGIN]))
                    setErrorIncompatible(LOGIN, RECURRENCE);
                else if (mParser->isSet(*mOptions[UNTIL]))
                    setErrorIncompatible(UNTIL, RECURRENCE);
                QString rule = mParser->value(*mOptions[RECURRENCE]);
                mRecurrence = new KARecurrence;
                mRecurrence->set(rule);
            }
            if (mParser->isSet(*mOptions[INTERVAL]))
            {
                // Repeat count is specified
                int count = 0;
                KADateTime endTime;
                if (mParser->isSet(*mOptions[LOGIN]))
                    setErrorIncompatible(LOGIN, INTERVAL);
                bool ok;
                if (mParser->isSet(*mOptions[REPEAT]))
                {
                    count = mParser->value(*mOptions[REPEAT]).toInt(&ok);
                    if (!ok || !count || count < -1 || (count < 0 && haveRecurrence))
                        setErrorParameter(REPEAT);
                }
                else if (haveRecurrence)
                    setErrorRequires(INTERVAL, REPEAT);
                else if (mParser->isSet(*mOptions[UNTIL]))
                {
                    count = 0;
                    QByteArray dateTime = mParser->value(*mOptions[UNTIL]).toLocal8Bit();
                    bool ok;
                    if (mParser->isSet(*mOptions[TIME]))
                        ok = AlarmTime::convertTimeString(dateTime, endTime, mAlarmTime);
                    else
                        ok = AlarmTime::convertTimeString(dateTime, endTime);
                    if (!ok)
                        setErrorParameter(UNTIL);
                    else if (mAlarmTime.isDateOnly()  &&  !endTime.isDateOnly())
                        setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", optionName(UNTIL)));
                    if (!mAlarmTime.isDateOnly()  &&  endTime.isDateOnly())
                        endTime.setTime(QTime(23,59,59));
                    if (endTime < mAlarmTime)
                        setError(xi18nc("@info:shell", "<icode>%1</icode> earlier than <icode>%2</icode>", optionName(UNTIL), optionName(TIME)));
                }
                else
                    count = -1;

                // Get the recurrence interval
                int intervalOfType;
                KARecurrence::Type recurType;
                if (!convInterval(mParser->value(*mOptions[INTERVAL]), recurType, intervalOfType, !haveRecurrence))
                    setErrorParameter(INTERVAL);
                else if (mAlarmTime.isDateOnly()  &&  recurType == KARecurrence::MINUTELY)
                    setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", optionName(INTERVAL)));

                if (haveRecurrence)
                {
                    if (mRecurrence)
                    {
                        // There is a also a recurrence specified, so set up a sub-repetition.
                        // In this case, 'intervalOfType' is in minutes.
                        mRepeatInterval = KCalCore::Duration(intervalOfType * 60);
                        KCalCore::Duration longestInterval = mRecurrence->longestInterval();
                        if (mRepeatInterval * count > longestInterval)
                            setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> and <icode>%2</icode> parameters: repetition is longer than <icode>%3</icode> interval",
                                           optionName(INTERVAL), optionName(REPEAT), optionName(RECURRENCE)));
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
                if (mParser->isSet(*mOptions[REPEAT]))
                    setErrorRequires(REPEAT, INTERVAL);
                else if (mParser->isSet(*mOptions[UNTIL]))
                    setErrorRequires(UNTIL, INTERVAL);
            }

            bool audioRepeat = mParser->isSet(*mOptions[PLAY_REPEAT]);
            if (audioRepeat  ||  mParser->isSet(*mOptions[PLAY]))
            {
                // Play a sound with the alarm
                Option opt = audioRepeat ? PLAY_REPEAT : PLAY;
                if (audioRepeat  &&  mParser->isSet(*mOptions[PLAY]))
                    setErrorIncompatible(PLAY, PLAY_REPEAT);
                if (mParser->isSet(*mOptions[BEEP]))
                    setErrorIncompatible(BEEP, opt);
                else if (mParser->isSet(*mOptions[SPEAK]))
                    setErrorIncompatible(SPEAK, opt);
                mAudioFile = mParser->value(*mOptions[audioRepeat ? PLAY_REPEAT : PLAY]);
                if (mParser->isSet(*mOptions[VOLUME]))
                {
                    bool ok;
                    int volumepc = mParser->value(*mOptions[VOLUME]).toInt(&ok);
                    if (!ok  ||  volumepc < 0  ||  volumepc > 100)
                        setErrorParameter(VOLUME);
                    mAudioVolume = static_cast<float>(volumepc) / 100;
                }
            }
            else if (mParser->isSet(*mOptions[VOLUME]))
                setErrorRequires(VOLUME, PLAY, PLAY_REPEAT);
            if (mParser->isSet(*mOptions[SPEAK]))
            {
                if (mParser->isSet(*mOptions[BEEP]))
                    setErrorIncompatible(BEEP, SPEAK);
                else if (!KPIMTextEdit::TextToSpeech::self()->isReady())
                    setError(xi18nc("@info:shell", "<icode>%1</icode> requires KAlarm to be compiled with QTextToSpeech support", optionName(SPEAK)));
            }
            bool onceOnly = mParser->isSet(*mOptions[REMINDER_ONCE]);
            if (mParser->isSet(*mOptions[REMINDER])  ||  onceOnly)
            {
                // Issue a reminder alarm in advance of or after the main alarm
                if (onceOnly  &&  mParser->isSet(*mOptions[REMINDER]))
                    setErrorIncompatible(REMINDER, REMINDER_ONCE);
                Option opt = onceOnly ? REMINDER_ONCE : REMINDER;
                KARecurrence::Type recurType;
                QString optval = mParser->value(*mOptions[onceOnly ? REMINDER_ONCE : REMINDER]);
                bool after = (optval[0] == QLatin1Char('+'));
                if (after)
                    optval.remove(0, 1);   // it's a reminder after the main alarm
                if (!convInterval(optval, recurType, mReminderMinutes))
                    setErrorParameter(opt);
                else if (recurType == KARecurrence::MINUTELY  &&  mAlarmTime.isDateOnly())
                    setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", optionName(opt)));
                if (after)
                    mReminderMinutes = -mReminderMinutes;
                if (onceOnly)
                    mFlags |= KAEvent::REMINDER_ONCE;
            }

            if (mParser->isSet(*mOptions[LATE_CANCEL]))
            {
                KARecurrence::Type recurType;
                bool ok = convInterval(mParser->value(*mOptions[LATE_CANCEL]), recurType, mLateCancel);
                if (!ok)
                    setErrorParameter(LATE_CANCEL);
            }
            else if (mParser->isSet(*mOptions[AUTO_CLOSE]))
                setErrorRequires(AUTO_CLOSE, LATE_CANCEL);

            if (mParser->isSet(*mOptions[ACK_CONFIRM]))
                mFlags |= KAEvent::CONFIRM_ACK;
            if (mParser->isSet(*mOptions[AUTO_CLOSE]))
                mFlags |= KAEvent::AUTO_CLOSE;
            if (mParser->isSet(*mOptions[BEEP]))
                mFlags |= KAEvent::BEEP;
            if (mParser->isSet(*mOptions[SPEAK]))
                mFlags |= KAEvent::SPEAK;
            if (mParser->isSet(*mOptions[KORGANIZER]))
                mFlags |= KAEvent::COPY_KORGANIZER;
            if (mParser->isSet(*mOptions[DISABLE]))
                mFlags |= KAEvent::DISABLED;
            if (audioRepeat)
                mFlags |= KAEvent::REPEAT_SOUND;
            if (mParser->isSet(*mOptions[LOGIN]))
                mFlags |= KAEvent::REPEAT_AT_LOGIN;
            if (mParser->isSet(*mOptions[BCC]))
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
            qCDebug(KALARM_LOG) << "Interactive";
            QStringList errors;
            if (mParser->isSet(*mOptions[ACK_CONFIRM]))
                errors << optionName(ACK_CONFIRM);
            if (mParser->isSet(*mOptions[ATTACH]))
                errors << optionName(ATTACH);
            if (mParser->isSet(*mOptions[AUTO_CLOSE]))
                errors << optionName(AUTO_CLOSE);
            if (mParser->isSet(*mOptions[BCC]))
                errors << optionName(BCC);
            if (mParser->isSet(*mOptions[BEEP]))
                errors << optionName(BEEP);
            if (mParser->isSet(*mOptions[COLOUR]))
                errors << optionName(COLOUR);
            if (mParser->isSet(*mOptions[COLOURFG]))
                errors << optionName(COLOURFG);
            if (mParser->isSet(*mOptions[DISABLE]))
                errors << optionName(DISABLE);
            if (mParser->isSet(*mOptions[FROM_ID]))
                errors << optionName(FROM_ID);
            if (mParser->isSet(*mOptions[KORGANIZER]))
                errors << optionName(KORGANIZER);
            if (mParser->isSet(*mOptions[LATE_CANCEL]))
                errors << optionName(LATE_CANCEL);
            if (mParser->isSet(*mOptions[LOGIN]))
                errors << optionName(LOGIN);
            if (mParser->isSet(*mOptions[PLAY]))
                errors << optionName(PLAY);
            if (mParser->isSet(*mOptions[PLAY_REPEAT]))
                errors << optionName(PLAY_REPEAT);
            if (mParser->isSet(*mOptions[REMINDER]))
                errors << optionName(REMINDER);
            if (mParser->isSet(*mOptions[REMINDER_ONCE]))
                errors << optionName(REMINDER_ONCE);
            if (mParser->isSet(*mOptions[SPEAK]))
                errors << optionName(SPEAK);
            if (mParser->isSet(*mOptions[SUBJECT]))
                errors << optionName(SUBJECT);
            if (mParser->isSet(*mOptions[TIME]))
                errors << optionName(TIME);
            if (mParser->isSet(*mOptions[VOLUME]))
                errors << optionName(VOLUME);
            if (!errors.isEmpty())
                mError = errors.join(QLatin1Char(' ')) + i18nc("@info:shell", ": option(s) only valid with an appropriate action option or message");
            break;
        }
        default:
            break;
    }

    if (!mError.isEmpty())
        setError(mError);
}

void CommandOptions::setError(const QString& errmsg)
{
    qCWarning(KALARM_LOG) << errmsg;
    mCommand = CMD_ERROR;
    if (mError.isEmpty())
        mError = errmsg + i18nc("@info:shell", "\nUse --help to get a list of available command line options.\n");
}

void CommandOptions::printError(const QString& errmsg)
{
    // Note: we can't use mArgs->usage() since that also quits any other
    // running 'instances' of the program.
    std::cerr << errmsg.toLocal8Bit().data()
          << i18nc("@info:shell", "\nUse --help to get a list of available command line options.\n").toLocal8Bit().data();
}

/******************************************************************************
* Check if the given command option is specified, and if so set mCommand etc.
* If another command option has also been detected, issue an error.
* If 'allowedEditType' is set, supersede any previous specification of that
* edit type with the given command option - this allows, e.g., --mail to be
* used along with --edit-new-email so the user can specify addressees.
*/
bool CommandOptions::checkCommand(Option command, Command code, EditAlarmDlg::Type allowedEditType)
{
    if (!mError.isEmpty()
    ||  !mParser->isSet(*mOptions[command]))
        return false;
    if (mCommand != NONE
    &&  (allowedEditType == EditAlarmDlg::NO_TYPE
      || (allowedEditType != EditAlarmDlg::NO_TYPE  &&  (mCommand != NEW || mEditType != allowedEditType))))
        setErrorIncompatible(mCommandOpt, command);
    qCDebug(KALARM_LOG).nospace() << optionName(command);
    mCommand = code;
    mCommandOpt = command;
    return true;
}

// Set the error message to "--opt requires --opt2" or "--opt requires --opt2 or --opt3".
void CommandOptions::setErrorRequires(Option opt, Option opt2, Option opt3)
{
    if (opt3 == Num_Options)
        setError(xi18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", optionName(opt), optionName(opt2)));
    else
        setError(xi18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode> or <icode>%3</icode>", optionName(opt), optionName(opt2), optionName(opt3)));
}

void CommandOptions::setErrorParameter(Option opt)
{
    setError(xi18nc("@info:shell", "Invalid <icode>%1</icode> parameter", optionName(opt)));
}

void CommandOptions::setErrorIncompatible(Option opt1, Option opt2)
{
    setError(xi18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", optionName(opt1), optionName(opt2)));
}

void CommandOptions::checkEditType(EditAlarmDlg::Type type1, EditAlarmDlg::Type type2, Option opt)
{
    if (mParser->isSet(*mOptions[opt])  &&  mCommand != NONE
    &&  ((mCommand != NEW && mCommand != EDIT_NEW)  ||  (mEditType != type1 && (type2 == EditAlarmDlg::NO_TYPE || mEditType != type2))))
        setErrorIncompatible(opt, mCommandOpt);
}

// Fetch one of the arguments (i.e. not belonging to any option).
QString CommandOptions::arg(int n)
{
    const QStringList args = mParser->positionalArguments();
    return (n < args.size()) ? args[n] : QString();
}

QString CommandOptions::optionName(Option opt, bool shortName) const
{
    if (opt == Opt_Message)
        return QStringLiteral("message");
    const QStringList names = mOptions[opt]->names();
    if (names.empty())
        return QString();
    for (int i = 0;  i < names.size();  ++i)
    {
        if (shortName  &&  names[i].size() == 1)
            return QStringLiteral("-") + names[i];
        else if (!shortName  &&  names[i].size() > 1)
            return QStringLiteral("--") + names[i];
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
    switch (timeString[length - 1])
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
