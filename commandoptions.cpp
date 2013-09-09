/*
 *  commandoptions.cpp  -  extract command line options
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"      //krazy:exclude=includes (kalarm.h must be first)
#include "commandoptions.h"
#include "alarmtime.h"
#include "kalarmapp.h"
#include "kamail.h"

#include <kalarmcal/identities.h>

#include <kcmdlineargs.h>

#include <iostream>

static bool convInterval(const QByteArray& timeParam, KARecurrence::Type&, int& timeInterval, bool allowMonthYear = false);

void CommandOptions::setError(const QString& error)
{
    if (mError.isEmpty())
        mError = error;
}

CommandOptions::CommandOptions()
    : mCommand(NONE),
      mEditActionSet(false),
      mRecurrence(0),
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
    mArgs = KCmdLineArgs::parsedArgs();
#ifndef NDEBUG
    if (mArgs->isSet("test-set-time"))
    {
        QString time = mArgs->getOption("test-set-time");
        if (!AlarmTime::convertTimeString(time.toLatin1(), mSimulationTime, KDateTime::realCurrentLocalDateTime(), true))
            setErrorParameter("--test-set-time");
    }
#endif
    if (checkCommand("tray", TRAY))
    {
    }
    if (checkCommand("list", LIST))
    {
        if (mArgs->count())
            setErrorParameter("--list");
    }
    if (checkCommand("triggerEvent", TRIGGER_EVENT)
    ||  checkCommand("cancelEvent", CANCEL_EVENT)
    ||  checkCommand("edit", EDIT))
    {
#ifdef USE_AKONADI
        // Fetch the event ID. This can optionally include a prefix of the
        // resource ID followed by a colon delimiter.
        mEventId = EventId(mArgs->getOption(mCommandName));
#else
        mEventId = mArgs->getOption(mCommandName);
#endif
    }
    if (checkCommand("edit-new-preset", EDIT_NEW_PRESET))
    {
        mTemplateName = mArgs->getOption(mCommandName);
    }
    if (checkCommand("file", NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::FILE;
        mEditActionSet = true;
        mText          = mArgs->getOption(mCommandName);
    }
    if (checkCommand("exec-display", NEW))
    {
        mEditType      = EditAlarmDlg::DISPLAY;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mFlags        |= KAEvent::DISPLAY_COMMAND;
        mText          = mArgs->getOption(mCommandName);
        int n = mArgs->count();
        for (int i = 0;  i < n;  ++i)
        {
            mText += QLatin1Char(' ');
            mText += mArgs->arg(i);
        }
    }
    if (checkCommand("exec", NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
        mText          = mArgs->getOption(mCommandName);
        int n = mArgs->count();
        for (int i = 0;  i < n;  ++i)
        {
            mText += QLatin1Char(' ');
            mText += mArgs->arg(i);
        }
    }
    if (checkCommand("mail", NEW))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (checkCommand("edit-new-display", EDIT_NEW, EditAlarmDlg::DISPLAY))
    {
        mEditType = EditAlarmDlg::DISPLAY;
        if (!mEditActionSet  ||  (mEditAction != KAEvent::COMMAND && mEditAction != KAEvent::FILE))
        {
            mEditAction    = KAEvent::MESSAGE;
            mEditActionSet = true;
        }
        if (mArgs->count())
            mText = mArgs->arg(0);
    }
    if (checkCommand("edit-new-command", EDIT_NEW))
    {
        mEditType      = EditAlarmDlg::COMMAND;
        mEditAction    = KAEvent::COMMAND;
        mEditActionSet = true;
    }
    if (checkCommand("edit-new-email", EDIT_NEW, EditAlarmDlg::EMAIL))
    {
        mEditType      = EditAlarmDlg::EMAIL;
        mEditAction    = KAEvent::EMAIL;
        mEditActionSet = true;
    }
    if (checkCommand("edit-new-audio", EDIT_NEW, EditAlarmDlg::AUDIO))
    {
        mEditType      = EditAlarmDlg::AUDIO;
        mEditAction    = KAEvent::AUDIO;
        mEditActionSet = true;
    }
    if (mError.isEmpty()  &&  mCommand == NONE)
    {
        if (!mArgs->count())
        {
            if (checkCommand("play", NEW) || checkCommand("play-repeat", NEW))
            {
                mEditType      = EditAlarmDlg::AUDIO;
                mEditAction    = KAEvent::AUDIO;
                mEditActionSet = true;
            }
        }
        else
        {
            kDebug() << "Message";
            mCommand       = NEW;
            mCommandName   = "message";
            mEditType      = EditAlarmDlg::DISPLAY;
            mEditAction    = KAEvent::MESSAGE;
            mEditActionSet = true;
            mText          = mArgs->arg(0);
        }
    }
    if (mEditActionSet  &&  mEditAction == KAEvent::EMAIL)
    {
        if (mArgs->isSet("subject"))
            mSubject = mArgs->getOption("subject");
        if (mArgs->isSet("from-id"))
            mFromID = Identities::identityUoid(mArgs->getOption("from-id"));
        QStringList params = mArgs->getOptionList("mail");
        for (int i = 0, count = params.count();  i < count;  ++i)
        {
            QString addr = params[i];
            if (!KAMail::checkAddress(addr))
                setError(i18nc("@info:shell", "<icode>%1</icode>: invalid email address", QLatin1String("--mail")));
#ifdef USE_AKONADI
            KCalCore::Person::Ptr person(new KCalCore::Person(QString(), addr));
            mAddressees += person;
#else
            mAddressees += KCal::Person(QString(), addr);
#endif
        }
        params = mArgs->getOptionList("attach");
        for (int i = 0, count = params.count();  i < count;  ++i)
            mAttachments += params[i];
        if (mArgs->count())
            mText = mArgs->arg(0);
    }
    if (mArgs->isSet("disable-all"))
    {
        if (mCommand == TRIGGER_EVENT  ||  mCommand == LIST)
            setErrorIncompatible("--disable-all", mCommandName);
        mDisableAll = true;
    }

    // Check that other options are only specified for the
    // correct main command options.
    checkEditType(EditAlarmDlg::DISPLAY, "color");
    checkEditType(EditAlarmDlg::DISPLAY, "colorfg");
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, "play");
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, "play-repeat");
    checkEditType(EditAlarmDlg::DISPLAY, EditAlarmDlg::AUDIO, "volume");
    checkEditType(EditAlarmDlg::DISPLAY, "speak");
    checkEditType(EditAlarmDlg::DISPLAY, "beep");
    checkEditType(EditAlarmDlg::DISPLAY, "reminder");
    checkEditType(EditAlarmDlg::DISPLAY, "reminder-once");
    checkEditType(EditAlarmDlg::DISPLAY, "ack-confirm");
    checkEditType(EditAlarmDlg::DISPLAY, "auto-close");
    checkEditType(EditAlarmDlg::EMAIL, "subject");
    checkEditType(EditAlarmDlg::EMAIL, "from-id");
    checkEditType(EditAlarmDlg::EMAIL, "attach");
    checkEditType(EditAlarmDlg::EMAIL, "bcc");

    switch (mCommand)
    {
        case EDIT_NEW:
            if (mArgs->isSet("disable"))
                setErrorIncompatible("--disable", mCommandName);
            // Fall through to NEW
        case NEW:
        {
            // Display a message or file, execute a command, or send an email
            if (mArgs->isSet("color"))
            {
                // Background colour is specified
                QString colourText = mArgs->getOption("color");
                if (colourText[0] == QLatin1Char('0') && colourText[1].toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QLatin1String("#"));
                mBgColour.setNamedColor(colourText);
                if (!mBgColour.isValid())
                    setErrorParameter("--color");
            }
            if (mArgs->isSet("colorfg"))
            {
                // Foreground colour is specified
                QString colourText = mArgs->getOption("colorfg");
                if (colourText[0] == QLatin1Char('0') && colourText[1].toLower() == QLatin1Char('x'))
                    colourText.replace(0, 2, QLatin1String("#"));
                mFgColour.setNamedColor(colourText);
                if (!mFgColour.isValid())
                    setErrorParameter("--colorfg");
            }

            if (mArgs->isSet("time"))
            {
                QByteArray dateTime = mArgs->getOption("time").toLocal8Bit();
                if (!AlarmTime::convertTimeString(dateTime, mAlarmTime))
                    setErrorParameter("--time");
            }
            else
                mAlarmTime = KDateTime::currentLocalDateTime();

            bool haveRecurrence = mArgs->isSet("recurrence");
            if (haveRecurrence)
            {
                if (mArgs->isSet("login"))
                    setErrorIncompatible("--login", "--recurrence");
                else if (mArgs->isSet("until"))
                    setErrorIncompatible("--until", "--recurrence");
                QString rule = mArgs->getOption("recurrence");
                mRecurrence = new KARecurrence;
                mRecurrence->set(rule);
            }
            if (mArgs->isSet("interval"))
            {
                // Repeat count is specified
                int count = 0;
                KDateTime endTime;
                if (mArgs->isSet("login"))
                    setErrorIncompatible("--login", "--interval");
                bool ok;
                if (mArgs->isSet("repeat"))
                {
                    count = mArgs->getOption("repeat").toInt(&ok);
                    if (!ok || !count || count < -1 || (count < 0 && haveRecurrence))
                        setErrorParameter("--repeat");
                }
                else if (haveRecurrence)
                    setErrorRequires("--interval", "--repeat");
                else if (mArgs->isSet("until"))
                {
                    count = 0;
                    QByteArray dateTime = mArgs->getOption("until").toLocal8Bit();
                    bool ok;
                    if (mArgs->isSet("time"))
                        ok = AlarmTime::convertTimeString(dateTime, endTime, mAlarmTime);
                    else
                        ok = AlarmTime::convertTimeString(dateTime, endTime);
                    if (!ok)
                        setErrorParameter("--until");
                    else if (mAlarmTime.isDateOnly()  &&  !endTime.isDateOnly())
                        setError(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", QLatin1String("--until")));
                    if (!mAlarmTime.isDateOnly()  &&  endTime.isDateOnly())
                        endTime.setTime(QTime(23,59,59));
                    if (endTime < mAlarmTime)
                        setError(i18nc("@info:shell", "<icode>%1</icode> earlier than <icode>%2</icode>", QLatin1String("--until"), QLatin1String("--time")));
                }
                else
                    count = -1;

                // Get the recurrence interval
                int interval;
                KARecurrence::Type recurType;
                if (!convInterval(mArgs->getOption("interval").toLocal8Bit(), recurType, interval, !haveRecurrence))
                    setErrorParameter("--interval");
                else if (mAlarmTime.isDateOnly()  &&  recurType == KARecurrence::MINUTELY)
                    setError(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", QLatin1String("--interval")));

                if (haveRecurrence)
                {
                    if (mRecurrence)
                    {
                        // There is a also a recurrence specified, so set up a sub-repetition
                        int longestInterval = mRecurrence->longestInterval();
                        if (count * interval > longestInterval)
                            setError(i18nc("@info:shell", "Invalid <icode>%1</icode> and <icode>%2</icode> parameters: repetition is longer than <icode>%3</icode> interval",
                                           QLatin1String("--interval"), QLatin1String("--repeat"), QLatin1String("--recurrence")));
                        mRepeatCount    = count;
                        mRepeatInterval = interval;
                    }
                }
                else
                {
                    // There is no other recurrence specified, so convert the repetition
                    // parameters into a KCal::Recurrence
                    mRecurrence = new KARecurrence;
                    mRecurrence->set(recurType, interval, count, mAlarmTime, endTime);
                }
            }
            else
            {
                if (mArgs->isSet("repeat"))
                    setErrorRequires("--repeat", "--interval");
                else if (mArgs->isSet("until"))
                    setErrorRequires("--until", "--interval");
            }

            bool audioRepeat = mArgs->isSet("play-repeat");
            if (audioRepeat  ||  mArgs->isSet("play"))
            {
                // Play a sound with the alarm
                const char* opt = audioRepeat ? "--play-repeat" : "--play";
                if (audioRepeat  &&  mArgs->isSet("play"))
                    setErrorIncompatible("--play", "--play-repeat");
                if (mArgs->isSet("beep"))
                    setErrorIncompatible("--beep", opt);
                else if (mArgs->isSet("speak"))
                    setErrorIncompatible("--speak", opt);
                mAudioFile = mArgs->getOption(audioRepeat ? "play-repeat" : "play");
                if (mArgs->isSet("volume"))
                {
                    bool ok;
                    int volumepc = mArgs->getOption("volume").toInt(&ok);
                    if (!ok  ||  volumepc < 0  ||  volumepc > 100)
                        setErrorParameter("--volume");
                    mAudioVolume = static_cast<float>(volumepc) / 100;
                }
            }
            else if (mArgs->isSet("volume"))
                setErrorRequires("--volume", "--play", "--play-repeat");
            if (mArgs->isSet("speak"))
            {
                if (mArgs->isSet("beep"))
                    setErrorIncompatible("--beep", "--speak");
                else if (!theApp()->speechEnabled())
                    setError(i18nc("@info:shell", "<icode>%1</icode> requires speech synthesis to be configured using Jovie", QLatin1String("--speak")));
            }
            bool onceOnly = mArgs->isSet("reminder-once");
            if (mArgs->isSet("reminder")  ||  onceOnly)
            {
                // Issue a reminder alarm in advance of or after the main alarm
                if (onceOnly  &&  mArgs->isSet("reminder"))
                    setErrorIncompatible("--reminder", "--reminder-once");
                const char* opt = onceOnly ? "--reminder-once" : "--reminder";
                KARecurrence::Type recurType;
                QByteArray optval = mArgs->getOption(onceOnly ? "reminder-once" : "reminder").toLocal8Bit();
                bool after = (optval[0] == '+');
                if (after)
                    optval = optval.right(1);
                if (!convInterval(optval, recurType, mReminderMinutes))
                    setErrorParameter(opt);
                else if (recurType == KARecurrence::MINUTELY  &&  mAlarmTime.isDateOnly())
                    setError(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", QLatin1String(opt)));
                if (after)
                    mReminderMinutes = -mReminderMinutes;
                if (onceOnly)
                    mFlags |= KAEvent::REMINDER_ONCE;
            }

            if (mArgs->isSet("late-cancel"))
            {
                KARecurrence::Type recurType;
                bool ok = convInterval(mArgs->getOption("late-cancel").toLocal8Bit(), recurType, mLateCancel);
                if (!ok)
                    setErrorParameter("--late-cancel");
            }
            else if (mArgs->isSet("auto-close"))
                setErrorRequires("--auto-close", "--late-cancel");

            if (mArgs->isSet("ack-confirm"))
                mFlags |= KAEvent::CONFIRM_ACK;
            if (mArgs->isSet("auto-close"))
                mFlags |= KAEvent::AUTO_CLOSE;
            if (mArgs->isSet("beep"))
                mFlags |= KAEvent::BEEP;
            if (mArgs->isSet("speak"))
                mFlags |= KAEvent::SPEAK;
            if (mArgs->isSet("korganizer"))
                mFlags |= KAEvent::COPY_KORGANIZER;
            if (mArgs->isSet("disable"))
                mFlags |= KAEvent::DISABLED;
            if (audioRepeat)
                mFlags |= KAEvent::REPEAT_SOUND;
            if (mArgs->isSet("login"))
                mFlags |= KAEvent::REPEAT_AT_LOGIN;
            if (mArgs->isSet("bcc"))
                mFlags |= KAEvent::EMAIL_BCC;
            if (mAlarmTime.isDateOnly())
                mFlags |= KAEvent::ANY_TIME;
            break;
        }
        case NONE:
            // No arguments - run interactively & display the main window
            if (!mError.isEmpty())
                break;
            kDebug() << "Interactive";
            if (mArgs->isSet("ack-confirm"))
                mError += QLatin1String("--ack-confirm ");
            if (mArgs->isSet("attach"))
                mError += QLatin1String("--attach ");
            if (mArgs->isSet("auto-close"))
                mError += QLatin1String("--auto-close ");
            if (mArgs->isSet("bcc"))
                mError += QLatin1String("--bcc ");
            if (mArgs->isSet("beep"))
                mError += QLatin1String("--beep ");
            if (mArgs->isSet("color"))
                mError += QLatin1String("--color ");
            if (mArgs->isSet("colorfg"))
                mError += QLatin1String("--colorfg ");
            if (mArgs->isSet("disable"))
                mError += QLatin1String("--disable ");
            if (mArgs->isSet("from-id"))
                mError += QLatin1String("--from-id ");
            if (mArgs->isSet("korganizer"))
                mError += QLatin1String("--korganizer ");
            if (mArgs->isSet("late-cancel"))
                mError += QLatin1String("--late-cancel ");
            if (mArgs->isSet("login"))
                mError += QLatin1String("--login ");
            if (mArgs->isSet("play"))
                mError += QLatin1String("--play ");
            if (mArgs->isSet("play-repeat"))
                mError += QLatin1String("--play-repeat ");
            if (mArgs->isSet("reminder"))
                mError += QLatin1String("--reminder ");
            if (mArgs->isSet("reminder-once"))
                mError += QLatin1String("--reminder-once ");
            if (mArgs->isSet("speak"))
                mError += QLatin1String("--speak ");
            if (mArgs->isSet("subject"))
                mError += QLatin1String("--subject ");
            if (mArgs->isSet("time"))
                mError += QLatin1String("--time ");
            if (mArgs->isSet("volume"))
                mError += QLatin1String("--volume ");
            if (!mError.isEmpty())
                mError += i18nc("@info:shell", ": option(s) only valid with an appropriate action option or message");
            break;
        default:
            break;
    }

    mArgs->clear();      // free up memory

    if (!mError.isEmpty())
    {
        printError(mError);
        mCommand = CMD_ERROR;
    }
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
bool CommandOptions::checkCommand(const QByteArray& command, Command code, EditAlarmDlg::Type allowedEditType)
{
    if (!mError.isEmpty()
    ||  !mArgs->isSet(command))
        return false;
    if (mCommand != NONE
    &&  (allowedEditType == EditAlarmDlg::NO_TYPE
      || (allowedEditType != EditAlarmDlg::NO_TYPE  &&  (mCommand != NEW || mEditType != allowedEditType))))
        setErrorIncompatible(mCommandName, "--" + command);
    kDebug().nospace() << " --" << command;
    mCommand = code;
    mCommandName = command;
    return true;
}

// Set the error message to "--opt requires --opt2" or "--opt requires --opt2 or --opt3".
void CommandOptions::setErrorRequires(const char* opt, const char* opt2, const char* opt3)
{
    if (!opt3)
        setError(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String(opt), QLatin1String(opt2)));
    else
        setError(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode> or <icode>%3</icode>", QLatin1String(opt), QLatin1String(opt2), QLatin1String(opt3)));
}

void CommandOptions::setErrorParameter(const char* opt)
{
    setError(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String(opt)));
}

void CommandOptions::setErrorIncompatible(const QByteArray& opt1, const QByteArray& opt2)
{
    QByteArray o1 = opt1;
    if (!opt1.startsWith("--")  &&  opt1 != "message")    //krazy:exclude=strings (it's a QByteArray)
        o1.prepend("--");
    QByteArray o2 = opt2;
    if (!opt2.startsWith("--")  &&  opt2 != "message")    //krazy:exclude=strings (it's a QByteArray)
        o2.prepend("--");
    setError(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QString::fromLatin1(o1), QString::fromLatin1(o2)));
}

void CommandOptions::checkEditType(EditAlarmDlg::Type type1, EditAlarmDlg::Type type2, const QByteArray& optName)
{
    if (mArgs->isSet(optName)  &&  mCommand != NONE
    &&  ((mCommand != NEW && mCommand != EDIT_NEW)  ||  (mEditType != type1 && (type2 == EditAlarmDlg::NO_TYPE || mEditType != type2))))
        setErrorIncompatible(optName, mCommandName);
}

/******************************************************************************
* Convert a non-zero positive time interval command line parameter.
* 'timeInterval' receives the count for the recurType. If 'allowMonthYear' is
* false, weeks are converted to days in 'timeInterval'.
* Reply = true if successful.
*/
static bool convInterval(const QByteArray& timeParam, KARecurrence::Type& recurType, int& timeInterval, bool allowMonthYear)
{
    QByteArray timeString = timeParam;
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
                break;
            default:
                break;
        }
    }
    timeInterval = static_cast<int>(interval);
    return ok && interval;
}

// vim: et sw=4:
