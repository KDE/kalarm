/*
 *  messagedisplayhelper.cpp  -  helper class to display an alarm or error message
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagedisplayhelper.h"
#include "messagedisplayhelper_p.h"
#include "messagedisplay.h"

#include "displaycalendar.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "resourcescalendar.h"
#include "resources/resources.h"
#include "lib/messagebox.h"
#include "lib/pushbutton.h"
#include "lib/synchtimer.h"
#include "kalarm_debug.h"

#include <KPIMTextEdit/TextToSpeech>

#include <KLocalizedString>
#include <KConfig>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <KNotification>
#include <phonon/mediaobject.h>
#include <phonon/audiooutput.h>
#include <phonon/volumefadereffect.h>

#include <QLocale>
#include <QTimer>
#include <QByteArray>
#include <QMimeDatabase>
#include <QUrl>
#include <QTextBrowser>
#include <QTemporaryFile>

using namespace KAlarmCal;

// Error message bit masks
enum
{
    ErrMsg_Speak     = 0x01,
    ErrMsg_AudioFile = 0x02
};


QVector<MessageDisplayHelper*> MessageDisplayHelper::mInstanceList;
QHash<EventId, unsigned>       MessageDisplayHelper::mErrorMessages;
// There can only be one audio thread at a time: trying to play multiple
// sound files simultaneously would result in a cacophony, and besides
// that, Phonon currently crashes...
QPointer<AudioThread> MessageDisplayHelper::mAudioThread;
MessageDisplayHelper* AudioThread::mAudioOwner = nullptr;

/******************************************************************************
* Construct the message display handler for the specified alarm.
* Other alarms in the supplied event may have been updated by the caller, so
* the whole event needs to be stored for updating the calendar file when it is
* displayed.
*/
MessageDisplayHelper::MessageDisplayHelper(MessageDisplay* parent, const KAEvent& event, const KAAlarm& alarm, int flags)
    : mParent(parent)
    , mMessage(event.cleanText())
    , mFont(event.font())
    , mBgColour(event.bgColour())
    , mFgColour(event.fgColour())
    , mEventId(event)
    , mAudioFile(event.audioFile())
    , mVolume(event.soundVolume())
    , mFadeVolume(event.fadeVolume())
    , mFadeSeconds(qMin(event.fadeSeconds(), 86400))
    , mDefaultDeferMinutes(event.deferDefaultMinutes())
    , mAlarmType(alarm.type())
    , mAction(event.actionSubType())
    , mAkonadiItemId(event.akonadiItemId())
    , mCommandError(event.commandError())
    , mAudioRepeatPause(event.repeatSoundPause())
    , mConfirmAck(event.confirmAck())
    , mNoDefer(true)
    , mInvalid(false)
    , mEvent(event)
    , mOriginalEvent(event)
    , mResource(Resources::resourceForEvent(mEventId.eventId()))
    , mAlwaysHide(flags & MessageDisplay::AlwaysHide)
    , mNoPostAction(alarm.type() & KAAlarm::REMINDER_ALARM)
    , mBeep(event.beep())
    , mSpeak(event.speak())
    , mNoRecordCmdError(flags & MessageDisplay::NoRecordCmdError)
    , mRescheduleEvent(!(flags & MessageDisplay::NoReschedule))
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper():" << mEventId;
    if (alarm.type() & KAAlarm::REMINDER_ALARM)
    {
        if (event.reminderMinutes() < 0)
        {
            event.previousOccurrence(alarm.dateTime(false).effectiveKDateTime(), mDateTime, false);
            if (!mDateTime.isValid()  &&  event.repeatAtLogin())
                mDateTime = alarm.dateTime().addSecs(event.reminderMinutes() * 60);
        }
        else
            mDateTime = event.mainDateTime(true);
    }
    else
        mDateTime = alarm.dateTime(true);
    if (!(flags & (MessageDisplay::NoInitView | MessageDisplay::AlwaysHide)))
    {
        const bool readonly = KAlarm::eventReadOnly(mEventId.eventId());
        mShowEdit = !mEventId.isEmpty()  &&  !readonly;
        mNoDefer = readonly || (flags & MessageDisplay::NoDefer) || alarm.repeatAtLogin();
    }

    mInstanceList.append(this);
    if (event.autoClose())
        mCloseTime = alarm.dateTime().effectiveKDateTime().toUtc().qDateTime().addSecs(event.lateCancel() * 60);
}

/******************************************************************************
* Construct the message display handler for a specified error message.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
MessageDisplayHelper::MessageDisplayHelper(MessageDisplay* parent, const KAEvent& event, const DateTime& alarmDateTime,
                                           const QStringList& errmsgs, const QString& dontShowAgain)
    : mParent(parent)
    , mMessage(event.cleanText())
    , mDateTime(alarmDateTime)
    , mEventId(event)
    , mAlarmType(KAAlarm::MAIN_ALARM)
    , mAction(event.actionSubType())
    , mAkonadiItemId(-1)
    , mCommandError(KAEvent::CMD_NO_ERROR)
    , mErrorMsgs(errmsgs)
    , mDontShowAgain(dontShowAgain)
    , mConfirmAck(false)
    , mShowEdit(false)
    , mNoDefer(true)
    , mInvalid(false)
    , mEvent(event)
    , mOriginalEvent(event)
    , mErrorWindow(true)
    , mNoPostAction(true)
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper(errmsg)";
    mInstanceList.append(this);
}

/******************************************************************************
* Construct the message display handler for restoration by session management.
* The handler is initialised by readProperties().
*/
MessageDisplayHelper::MessageDisplayHelper(MessageDisplay* parent)
    : mParent(parent)
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper(): restore";
    mInstanceList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageDisplayHelper::~MessageDisplayHelper()
{
    qCDebug(KALARM_LOG) << "~MessageDisplayHelper()" << mEventId;
    if (AudioThread::mAudioOwner == this  &&  !mAudioThread.isNull())
        mAudioThread->quit();
    mErrorMessages.remove(mEventId);
    mInstanceList.removeAll(this);
    delete mTempFile;
    if (!mNoPostAction  &&  !mEvent.postAction().isEmpty())
        theApp()->alarmCompleted(mEvent);
}

/******************************************************************************
* Obtain the texts to show in the displayed alarm.
*/
void MessageDisplayHelper::initTexts()
{
    DisplayTexts texts;

    const bool reminder = (!mErrorWindow  &&  (mAlarmType & KAAlarm::REMINDER_ALARM));
    mTexts.title = (mAlarmType & KAAlarm::REMINDER_ALARM) ? i18nc("@title:window", "Reminder")
                                                          : i18nc("@title:window", "Message");

    // Show the alarm date/time, together with a reminder text where appropriate.
    if (mDateTime.isValid())
    {
        // Alarm date/time: display time zone if not local time zone.
        mTexts.time = mTexts.timeFull = dateTimeToDisplay();
        if (reminder)
        {
            // Reminder.
            // Create a label "time\nReminder" by inserting the time at the
            // start of the translated string, allowing for possible HTML tags
            // enclosing "Reminder".
            QString s = i18nc("@info", "Reminder");
            QRegExp re(QStringLiteral("^(<[^>]+>)*"));  // search for HTML tag "<...>"
            re.indexIn(s);
            // Prefix the time, plus a newline, to "Reminder", inside any HTML tags.
            s.insert(re.matchedLength(), mTexts.time + QLatin1String("<br/>"));
            mTexts.timeFull = s;
        }
    }

    if (!mErrorWindow)
    {
        // It's a normal alarm message display
        switch (mAction)
        {
            case KAEvent::FILE:
            {
                // Display the file name
                mTexts.fileName = mMessage;

                // Display contents of file
                const QUrl url = QUrl::fromUserInput(mMessage, QString(), QUrl::AssumeLocalFile);

                auto statJob = KIO::statDetails(url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
                const bool exists = statJob->exec();
                const bool isDir = statJob->statResult().isDir();

                bool opened = false;
                if (exists && !isDir)
                {
                    auto job = KIO::storedGet(url);
                    KJobWidgets::setWindow(job, MainWindow::mainMainWindow());
                    if (job->exec())
                    {
                        opened = true;
                        const QByteArray data = job->data();

                        QMimeDatabase db;
                        QMimeType mime = db.mimeTypeForUrl(url);
                        if (mime.name() == QLatin1String("application/octet-stream"))
                            mime = db.mimeTypeForData(mTempFile);
                        mTexts.fileType = File::fileType(mime);
                        switch (mTexts.fileType)
                        {
                            case File::Image:
                            case File::TextFormatted:
                                delete mTempFile;
                                mTempFile = new QTemporaryFile;
                                mTempFile->open();
                                mTempFile->write(data);
                                break;
                            default:
                                break;
                        }

                        switch (mTexts.fileType)
                        {
                            case File::Image:
                                mTexts.message = QLatin1String(R"(<div align="center"><img src=")") + mTempFile->fileName() + QLatin1String(R"("></div>)");
                                mTempFile->close();   // keep the file available to be displayed
                                break;
                            case File::TextFormatted:
                            {
                                QTextBrowser browser;
                                browser.setSource(QUrl::fromLocalFile(mTempFile->fileName()));   //krazy:exclude=qclasses
                                mTexts.message = browser.toHtml();
                                delete mTempFile;
                                mTempFile = nullptr;
                                break;
                            }
                            default:
                                mTexts.message = QString::fromUtf8(data);
                                break;
                        }
                    }
                }

                if (!exists || isDir || !opened)
                {
                    mErrorMsgs += isDir ? i18nc("@info", "File is a folder") : exists ? i18nc("@info", "Failed to open file") : i18nc("@info", "File not found");
                }
                break;
            }
            case KAEvent::MESSAGE:
                mTexts.message = mMessage;
                break;

            case KAEvent::COMMAND:
                theApp()->execCommandAlarm(mEvent, mEvent.alarm(mAlarmType), mNoRecordCmdError,
                                           this, SLOT(readProcessOutput(ShellProcess*)), "commandCompleted");
                break;

            case KAEvent::EMAIL:
            default:
                break;
        }

        if (reminder  &&  mEvent.reminderMinutes() > 0)
        {
            // Advance reminder: show remaining time until the actual alarm
            if (mDateTime.isDateOnly()  ||  KADateTime::currentLocalDate().daysTo(mDateTime.date()) > 0)
            {
                setRemainingTextDay(false);
                MidnightTimer::connect(this, SLOT(slotSetRemainingTextDay()));    // update every day
            }
            else
            {
                setRemainingTextMinute(false);
                MinuteTimer::connect(this, SLOT(slotSetRemainingTextMinute()));   // update every minute
            }
        }
    }
    else
    {
        // It's an error message
        switch (mAction)
        {
            case KAEvent::EMAIL:
            {
                // Display the email addresses and subject.
                mTexts.errorEmail[0] = i18nc("@info Email addressee", "To:");
                mTexts.errorEmail[1] = mEvent.emailAddresses(QStringLiteral("\n"));
                mTexts.errorEmail[2] = i18nc("@info Email subject", "Subject:");
                mTexts.errorEmail[3] = mEvent.emailSubject();
                break;
            }
            case KAEvent::COMMAND:
            case KAEvent::FILE:
            case KAEvent::MESSAGE:
            default:
                // Just display the error message strings
                break;
        }
    }

    if (!mErrorMsgs.isEmpty())
        mTexts.title = i18nc("@title:window", "Error");

    mInitialised = true;   // the alarm's texts have been created
}

/******************************************************************************
* Return the number of message displays, optionally excluding always-hidden ones.
*/
int MessageDisplayHelper::instanceCount(bool excludeAlwaysHidden)
{
    int count = mInstanceList.count();
    if (excludeAlwaysHidden)
    {
        for (const MessageDisplayHelper* h : std::as_const(mInstanceList))
        {
            if (h->mAlwaysHide)
                --count;
        }
    }
    return count;
}

/******************************************************************************
* Check whether to display an error message.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
bool MessageDisplayHelper::shouldShowError(const KAEvent& event, const QStringList& errmsgs, const QString& dontShowAgain)
{
    if (!dontShowAgain.isEmpty()
    &&  KAlarm::dontShowErrors(EventId(event), dontShowAgain))
        return false;

    // Don't pile up duplicate error messages for the same alarm
    EventId eid(event);
    for (const MessageDisplayHelper* h : std::as_const(mInstanceList))
    {
        if (h->mErrorWindow  &&  h->mEventId == eid
        &&  h->mErrorMsgs == errmsgs  &&  h->mDontShowAgain == dontShowAgain)
            return false;
    }
    return true;
}

/******************************************************************************
* Convert a reminder display into a normal alarm display.
*/
bool MessageDisplayHelper::cancelReminder(const KAEvent& event, const KAAlarm& alarm)
{
    if (!mInitialised)
        return false;
    mDateTime = alarm.dateTime(true);
    mNoPostAction = false;
    mAlarmType = alarm.type();
    if (event.autoClose())
        mCloseTime = alarm.dateTime().effectiveKDateTime().toUtc().qDateTime().addSecs(event.lateCancel() * 60);
    mTexts.title = i18nc("@title:window", "Message");
    mTexts.time = mTexts.timeFull = dateTimeToDisplay();
    mTexts.remainingTime.clear();
    MidnightTimer::disconnect(this, SLOT(slotSetRemainingTextDay()));
    MinuteTimer::disconnect(this, SLOT(slotSetRemainingTextMinute()));
    Q_EMIT textsChanged(DisplayTexts::Title | DisplayTexts::Time | DisplayTexts::TimeFull | DisplayTexts::RemainingTime);
    return true;
}

/******************************************************************************
* Update the alarm's trigger time. No textsChanged() signal is emitted.
*/
bool MessageDisplayHelper::updateDateTime(const KAEvent& event, const KAAlarm& alarm)
{
    mDateTime = (alarm.type() & KAAlarm::REMINDER_ALARM) ? event.mainDateTime(true) : alarm.dateTime(true);
    if (!mDateTime.isValid())
        return false;
    mTexts.time = mTexts.timeFull = dateTimeToDisplay();
    return true;
}

/******************************************************************************
* Get the trigger time to display.
*/
QString MessageDisplayHelper::dateTimeToDisplay() const
{
    QString tm;
    if (mDateTime.isValid())
    {
        if (mDateTime.isDateOnly())
            tm = QLocale().toString(mDateTime.date(), QLocale::ShortFormat);
        else
        {
            bool showZone = false;
            if (mDateTime.timeType() == KADateTime::UTC
            ||  (mDateTime.timeType() == KADateTime::TimeZone && !mDateTime.isLocalZone()))
            {
                // Display time zone abbreviation if it's different from the local
                // zone. Note that the iCalendar time zone might represent the local
                // time zone in a slightly different way from the system time zone,
                // so the zone comparison above might not produce the desired result.
                const QString tz = mDateTime.kDateTime().toString(QStringLiteral("%Z"));
                KADateTime local = mDateTime.kDateTime();
                local.setTimeSpec(KADateTime::Spec::LocalZone());
                showZone = (local.toString(QStringLiteral("%Z")) != tz);
            }
            const QDateTime dt = mDateTime.qDateTime();
            tm = QLocale().toString(dt, QLocale::ShortFormat);
            if (showZone)
                tm += QLatin1Char(' ') + mDateTime.timeZone().displayName(dt, QTimeZone::ShortName, QLocale());
        }
    }
    return tm;
}

/******************************************************************************
* Set the remaining time text in a reminder display.
* Called at the start of every day (at the user-defined start-of-day time).
*/
void MessageDisplayHelper::setRemainingTextDay(bool notify)
{
    const int days = KADateTime::currentLocalDate().daysTo(mDateTime.date());
    if (days <= 0  &&  !mDateTime.isDateOnly())
    {
        // The alarm is due today, so start refreshing every minute
        MidnightTimer::disconnect(this, SLOT(slotSetRemainingTextDay()));
        setRemainingTextMinute(notify);
        MinuteTimer::connect(this, SLOT(slotSetRemainingTextMinute()));   // update every minute
    }
    else
    {
        if (days <= 0)
            mTexts.remainingTime = i18nc("@info", "Today");
        else if (days % 7)
            mTexts.remainingTime = i18ncp("@info", "Tomorrow", "in %1 days' time", days);
        else
            mTexts.remainingTime = i18ncp("@info", "in 1 week's time", "in %1 weeks' time", days/7);
        if (notify)
            Q_EMIT textsChanged(DisplayTexts::RemainingTime);
    }
}

/******************************************************************************
* Set the remaining time text in a reminder display.
* Called on every minute boundary.
*/
void MessageDisplayHelper::setRemainingTextMinute(bool notify)
{
    const int mins = (KADateTime::currentUtcDateTime().secsTo(mDateTime.effectiveKDateTime()) + 59) / 60;
    if (mins < 60)
        mTexts.remainingTime = i18ncp("@info", "in 1 minute's time", "in %1 minutes' time", (mins > 0 ? mins : 0));
    else if (mins % 60 == 0)
        mTexts.remainingTime = i18ncp("@info", "in 1 hour's time", "in %1 hours' time", mins/60);
    else
    {
        QString hourText = i18ncp("@item:intext inserted into 'in ... %1 minute's time' below", "1 hour", "%1 hours", mins/60);
        mTexts.remainingTime = i18ncp("@info '%2' is the previous message '1 hour'/'%1 hours'", "in %2 1 minute's time", "in %2 %1 minutes' time", mins%60, hourText);
    }
    if (notify)
        Q_EMIT textsChanged(DisplayTexts::RemainingTime);
}

/******************************************************************************
* Called when output is available from the command which is providing the text
* for this display. Add the output.
*/
void MessageDisplayHelper::readProcessOutput(ShellProcess* proc)
{
    const QByteArray data = proc->readAll();
    if (!data.isEmpty())
    {
        mCommandOutput.clear();

        // Strip any trailing newline, to avoid showing trailing blank line
        // in message display.
        QString newText;
        if (mTexts.newLine)
            newText = QStringLiteral("\n");
        mTexts.newLine = data.endsWith('\n');
        newText += QString::fromLocal8Bit(data.data(), data.length() - (mTexts.newLine ? 1 : 0));
        mTexts.message += newText;
        Q_EMIT textsChanged(DisplayTexts::MessageAppend, newText);
    }
}

/******************************************************************************
* Called when the command which is providing the text for this display has
* completed. Check whether the command succeeded, even partially.
*/
void MessageDisplayHelper::commandCompleted(ShellProcess::Status status)
{
    bool failed;
    switch (status)
    {
        case ShellProcess::SUCCESS:
        case ShellProcess::DIED:
            failed = false;
            break;

        case ShellProcess::UNAUTHORISED:
        case ShellProcess::NOT_FOUND:
        case ShellProcess::START_FAIL:
        case ShellProcess::INACTIVE:
        default:
            failed = true;
            break;
    }
    Q_EMIT commandExited(!failed);
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
bool MessageDisplayHelper::saveProperties(KConfigGroup& config)
{
    if (!mErrorWindow  &&  !mAlwaysHide)
    {
        config.writeEntry("EventID", mEventId.eventId());
        config.writeEntry("CollectionID", mResource.id());
        config.writeEntry("AlarmType", static_cast<int>(mAlarmType));
        if (mAlarmType == KAAlarm::INVALID_ALARM)
            qCCritical(KALARM_LOG) << "MessageDisplayHelper::saveProperties: Invalid alarm: id=" << mEventId << ", alarm count=" << mEvent.alarmCount();
        config.writeEntry("Message", mMessage);
        config.writeEntry("Type", static_cast<int>(mAction));
        config.writeEntry("Font", mFont);
        config.writeEntry("BgColour", mBgColour);
        config.writeEntry("FgColour", mFgColour);
        config.writeEntry("ConfirmAck", mConfirmAck);
        if (mDateTime.isValid())
        {
            config.writeEntry("Time", mDateTime.effectiveDateTime());
            config.writeEntry("DateOnly", mDateTime.isDateOnly());
            QByteArray zone;
            if (mDateTime.isUtc())
                zone = "UTC";
            else if (mDateTime.isOffsetFromUtc())
            {
                int offset = mDateTime.utcOffset();
                if (offset >= 0)
                    zone = '+' + QByteArray::number(offset);
                else
                    zone = QByteArray::number(offset);
            }
            else if (mDateTime.timeType() == KADateTime::TimeZone)
            {
                const QTimeZone tz = mDateTime.timeZone();
                if (tz.isValid())
                    zone = tz.id();
            }
            config.writeEntry("TimeZone", zone);
        }
        if (mCloseTime.isValid())
            config.writeEntry("Expiry", mCloseTime);
        if (mAudioRepeatPause >= 0  &&  mSilenceButton  &&  mSilenceButton->isEnabled())
        {
            // Only need to restart sound file playing if it's being repeated
            config.writePathEntry("AudioFile", mAudioFile);
            config.writeEntry("Volume", static_cast<int>(mVolume * 100));
            config.writeEntry("AudioPause", mAudioRepeatPause);
        }
        config.writeEntry("Speak", mSpeak);
        config.writeEntry("DeferMins", mDefaultDeferMinutes);
        config.writeEntry("NoDefer", mNoDefer);
        config.writeEntry("NoPostAction", mNoPostAction);
        config.writeEntry("AkonadiItemId", mAkonadiItemId);
        config.writeEntry("CmdErr", static_cast<int>(mCommandError));
        config.writeEntry("DontShowAgain", mDontShowAgain);
        return true;
    }
    else
    {
        config.writeEntry("Invalid", true);
        return false;
    }
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
* Reply = true if the parent display needs to initialise its display.
*/
bool MessageDisplayHelper::readProperties(const KConfigGroup& config)
{
    return readPropertyValues(config)
       &&  processPropertyValues();
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
* Reply = true if the parent display needs to initialise its display.
*/
bool MessageDisplayHelper::readPropertyValues(const KConfigGroup& config)
{
    const QString eventId = config.readEntry("EventID");
    const ResourceId resourceId = config.readEntry("CollectionID", ResourceId(-1));
    mInvalid             = config.readEntry("Invalid", false);
    mAlarmType           = static_cast<KAAlarm::Type>(config.readEntry("AlarmType", 0));
    if (mAlarmType == KAAlarm::INVALID_ALARM)
    {
        mInvalid = true;
        qCCritical(KALARM_LOG) << "MessageDisplayHelper::readProperties: Invalid alarm: id=" << eventId;
    }
    mMessage             = config.readEntry("Message");
    mAction              = static_cast<KAEvent::SubAction>(config.readEntry("Type", 0));
    mFont                = config.readEntry("Font", QFont());
    mBgColour            = config.readEntry("BgColour", QColor(Qt::white));
    mFgColour            = config.readEntry("FgColour", QColor(Qt::black));
    mConfirmAck          = config.readEntry("ConfirmAck", false);
    QDateTime invalidDateTime;
    QDateTime dt         = config.readEntry("Time", invalidDateTime);
    const QByteArray zoneId = config.readEntry("TimeZone").toLatin1();
    KADateTime::Spec timeSpec;
    if (zoneId.isEmpty())
        timeSpec = KADateTime::LocalZone;
    else if (zoneId == "UTC")
        timeSpec = KADateTime::UTC;
    else if (zoneId.startsWith('+')  ||  zoneId.startsWith('-'))
        timeSpec.setType(KADateTime::OffsetFromUTC, zoneId.toInt());
    else
        timeSpec = QTimeZone(zoneId);
    mDateTime = KADateTime(dt.date(), dt.time(), timeSpec);
    const bool dateOnly  = config.readEntry("DateOnly", false);
    if (dateOnly)
        mDateTime.setDateOnly(true);
    mCloseTime           = config.readEntry("Expiry", invalidDateTime);
    mCloseTime.setTimeSpec(Qt::UTC);
    mAudioFile           = config.readPathEntry("AudioFile", QString());
    mVolume              = static_cast<float>(config.readEntry("Volume", 0)) / 100;
    mFadeVolume          = -1;
    mFadeSeconds         = 0;
    if (!mAudioFile.isEmpty())   // audio file URL was only saved if it repeats
        mAudioRepeatPause = config.readEntry("AudioPause", 0);
    mBeep                = false;   // don't beep after restart (similar to not playing non-repeated sound file)
    mSpeak               = config.readEntry("Speak", false);
    mDefaultDeferMinutes = config.readEntry("DeferMins", 0);
    mNoDefer             = config.readEntry("NoDefer", false);
    mNoPostAction        = config.readEntry("NoPostAction", true);
    mAkonadiItemId       = config.readEntry("AkonadiItemId", QVariant(QVariant::LongLong)).toLongLong();
    mCommandError        = KAEvent::CmdErrType(config.readEntry("CmdErr", static_cast<int>(KAEvent::CMD_NO_ERROR)));
    mDontShowAgain       = config.readEntry("DontShowAgain", QString());
    mShowEdit            = false;
    // Temporarily initialise mResource and mEventId - they will be set by redisplayAlarm()
    mResource            = Resources::resource(resourceId);
    mEventId             = EventId(resourceId, eventId);
    if (mAlarmType == KAAlarm::INVALID_ALARM)
        return false;
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::readProperties:" << eventId;
    return true;
}

bool MessageDisplayHelper::processPropertyValues()
{
    // Recreate the event from the calendar file (if possible)
    if (!mEventId.eventId().isEmpty())
    {
        // Close any other display for this alarm which has already been restored by redisplayAlarms()
        if (!Resources::allCreated())
        {
            connect(Resources::instance(), &Resources::resourcesCreated,
                                     this, &MessageDisplayHelper::showRestoredAlarm);
            return false;
        }
        redisplayAlarm();
    }
    return true;
}

/******************************************************************************
* Fetch the restored alarm from the calendar and redisplay it in this display.
*/
void MessageDisplayHelper::showRestoredAlarm()
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::showRestoredAlarm:" << mEventId;
    redisplayAlarm();
    mParent->setUpDisplay();
    mParent->showDisplay();
}

/******************************************************************************
* Fetch the restored alarm from the calendar and redisplay it in this display.
*/
void MessageDisplayHelper::redisplayAlarm()
{
    mResource = Resources::resourceForEvent(mEventId.eventId());
    mEventId.setResourceId(mResource.id());
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::redisplayAlarm:" << mEventId;
    // Delete any already existing display for the same event
    MessageDisplay* duplicate = findEvent(mEventId, mParent);
    if (duplicate)
    {
        qCDebug(KALARM_LOG) << "MessageDisplayHelper::redisplayAlarm: Deleting duplicate display:" << mEventId;
        delete duplicate;
    }

    const KAEvent event = ResourcesCalendar::event(mEventId);
    if (event.isValid())
    {
        mEvent = event;
        mShowEdit = true;
    }
    else
    {
        // It's not in the active calendar, so try the displaying or archive calendars
        mParent->retrieveEvent(mEventId, mEvent, mResource, mShowEdit, mNoDefer);
        mNoDefer = !mNoDefer;
    }
}

/******************************************************************************
* Called when an alarm is currently being displayed, to store a copy of the
* alarm in the displaying calendar, and to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
bool MessageDisplayHelper::alarmShowing(KAEvent& event)
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::alarmShowing:" << event.id() << "," << KAAlarm::debugType(mAlarmType);
    const KAAlarm alarm = event.alarm(mAlarmType);
    if (!alarm.isValid())
    {
        qCCritical(KALARM_LOG) << "MessageDisplayHelper::alarmShowing: Alarm type not found:" << event.id() << ":" << mAlarmType;
        return false;
    }
    if (!mAlwaysHide)
    {
        // Copy the alarm to the displaying calendar in case of a crash, etc.
        KAEvent dispEvent;
        const ResourceId id = Resources::resourceForEvent(event.id()).id();
        dispEvent.setDisplaying(event, mAlarmType, id,
                                mDateTime.effectiveKDateTime(), mShowEdit, !mNoDefer);
        if (DisplayCalendar::open())
        {
            DisplayCalendar::deleteEvent(dispEvent.id());   // in case it already exists
            DisplayCalendar::addEvent(dispEvent);
            DisplayCalendar::save();
        }
    }
    theApp()->rescheduleAlarm(event, alarm);
    return true;
}

/******************************************************************************
* Returns the existing message display (if any) which is showing the event with
* the specified ID.
*/
MessageDisplay* MessageDisplayHelper::findEvent(const EventId& eventId, MessageDisplay* exclude)
{
    if (!eventId.isEmpty())
    {
        for (const MessageDisplayHelper* h : std::as_const(mInstanceList))
        {
            if (h->mParent != exclude  &&  h->mEventId == eventId  &&  !h->mErrorWindow)
                return h->mParent;
        }
    }
    return nullptr;
}

/******************************************************************************
* Beep and play the audio file, as appropriate.
*/
void MessageDisplayHelper::playAudio()
{
    if (mBeep)
    {
        // Beep using two methods, in case the sound card/speakers are switched off or not working
        QApplication::beep();      // beep through the internal speaker
        KNotification::beep();     // beep through the sound card & speakers
    }
    if (!mAudioFile.isEmpty())
    {
        if (!mVolume  &&  mFadeVolume <= 0)
            return;    // ensure zero volume doesn't play anything
        startAudio();    // play the audio file
    }
    else if (mSpeak)
    {
        // The message is to be spoken. In case of error messges,
        // call it on a timer to allow the display to be shown first.
        QTimer::singleShot(0, this, &MessageDisplayHelper::slotSpeak);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* Speak the message.
* Called asynchronously to avoid delaying the display of the message.
*/
void MessageDisplayHelper::slotSpeak()
{
    KPIMTextEdit::TextToSpeech* tts = KPIMTextEdit::TextToSpeech::self();
    if (!tts->isReady())
    {
        KAMessageBox::detailedError(MainWindow::mainMainWindow(), i18nc("@info", "Unable to speak message"), i18nc("@info", "Text-to-speech subsystem is not available"));
        clearErrorMessage(ErrMsg_Speak);
        return;
    }

    tts->say(mMessage);
}

/******************************************************************************
* Called when another display's audio thread has been destructed.
* Start playing this display's audio file. Because initialising the sound system
* and loading the file may take some time, it is called in a separate thread to
* allow the display to show first.
*/
void MessageDisplayHelper::startAudio()
{
    if (mAudioThread)
    {
        // An audio file is already playing for another message display, so
        // wait until it has finished.
        connect(mAudioThread.data(), &QObject::destroyed, this, &MessageDisplayHelper::audioTerminating);
    }
    else
    {
        qCDebug(KALARM_LOG) << "MessageDisplayHelper::startAudio:" << QThread::currentThread();
        mAudioThread = new AudioThread(this, mAudioFile, mVolume, mFadeVolume, mFadeSeconds, mAudioRepeatPause);
        connect(mAudioThread.data(), &AudioThread::readyToPlay, this, &MessageDisplayHelper::playReady);
        connect(mAudioThread.data(), &QThread::finished, this, &MessageDisplayHelper::playFinished);
        if (mSilenceButton)
            connect(mSilenceButton, &QAbstractButton::clicked, mAudioThread.data(), &QThread::quit);
        // Notify after creating mAudioThread, so that isAudioPlaying() will
        // return the correct value.
        theApp()->notifyAudioPlaying(true);
        mAudioThread->start();
    }
}

/******************************************************************************
* Return whether audio playback is currently active.
*/
bool MessageDisplayHelper::isAudioPlaying()
{
    return mAudioThread;
}

/******************************************************************************
* Stop audio playback.
*/
void MessageDisplayHelper::stopAudio(bool wait)
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::stopAudio";
    if (mAudioThread)
        mAudioThread->stop(wait);
}

/******************************************************************************
* Called when the audio file is ready to start playing.
*/
void MessageDisplayHelper::playReady()
{
    if (mSilenceButton)
        mSilenceButton->setEnabled(true);
}

/******************************************************************************
* Called when another display's audio thread is being destructed.
* Wait until the destructor has finished.
*/
void MessageDisplayHelper::audioTerminating()
{
    QTimer::singleShot(0, this, &MessageDisplayHelper::startAudio);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Called when the audio file thread finishes.
*/
void MessageDisplayHelper::playFinished()
{
    if (mSilenceButton)
        mSilenceButton->setEnabled(false);
    if (mAudioThread)   // mAudioThread can actually be null here!
    {
        const QString errmsg = mAudioThread->error();
        if (!errmsg.isEmpty()  &&  !haveErrorMessage(ErrMsg_AudioFile))
        {
            KAMessageBox::error(mParent->displayParent(), errmsg);
            clearErrorMessage(ErrMsg_AudioFile);
        }
    }
    delete mAudioThread.data();
    if (mAlwaysHide)
        mParent->closeDisplay();
}

/******************************************************************************
* Constructor for audio thread.
*/
AudioThread::AudioThread(MessageDisplayHelper* parent, const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause)
    : QThread(parent),
      mFile(audioFile),
      mVolume(volume),
      mFadeVolume(fadeVolume),
      mFadeSeconds(fadeSeconds),
      mRepeatPause(repeatPause),
      mAudioObject(nullptr)
{
    if (mAudioOwner)
        qCCritical(KALARM_LOG) << "MessageDisplayHelper::AudioThread: mAudioOwner already set";
    mAudioOwner = parent;
}

/******************************************************************************
* Destructor for audio thread. Waits for thread completion and tidies up.
* Note that this destructor is executed in the parent thread.
*/
AudioThread::~AudioThread()
{
    qCDebug(KALARM_LOG) << "~MessageDisplayHelper::AudioThread";
    stop(true);   // stop playing and tidy up (timeout 3 seconds)
    delete mAudioObject;
    mAudioObject = nullptr;
    if (mAudioOwner == parent())
        mAudioOwner = nullptr;
    // Notify after deleting mAudioThread, so that isAudioPlaying() will
    // return the correct value.
    QTimer::singleShot(0, theApp(), &KAlarmApp::notifyAudioStopped);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Quits the thread and waits for thread completion and tidies up.
*/
void AudioThread::stop(bool waiT)
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::AudioThread::stop";
    quit();       // stop playing and tidy up
    wait(3000);   // wait for run() to exit (timeout 3 seconds)
    if (!isFinished())
    {
        // Something has gone wrong - forcibly kill the thread
        terminate();
        if (waiT)
            wait();
    }
}

/******************************************************************************
* Kick off the thread to play the audio file.
*/
void AudioThread::run()
{
    mMutex.lock();
    if (mAudioObject)
    {
        mMutex.unlock();
        return;
    }
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::AudioThread::run:" << QThread::currentThread() << mFile;
    const QString audioFile = mFile;
    const QUrl url = QUrl::fromUserInput(mFile);
    mFile = url.isLocalFile() ? url.toLocalFile() : url.toString();
    Phonon::MediaSource source(url);
    if (source.type() == Phonon::MediaSource::Invalid)
    {
        mError = xi18nc("@info", "Cannot open audio file: <filename>%1</filename>", audioFile);
        mMutex.unlock();
        qCCritical(KALARM_LOG) << "MessageDisplayHelper::AudioThread::run: Open failure:" << audioFile;
        return;
    }
    mAudioObject = new Phonon::MediaObject();
    mAudioObject->setCurrentSource(source);
    mAudioObject->setTransitionTime(100);   // workaround to prevent clipping of end of files in Xine backend
    auto output = new Phonon::AudioOutput(Phonon::NotificationCategory, mAudioObject);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    mPath = Phonon::createPath(mAudioObject, output);
    if (mVolume >= 0  ||  mFadeVolume >= 0)
    {
        const float vol = (mVolume >= 0) ? mVolume : output->volume();
        const float maxvol = qMax(vol, mFadeVolume);
        output->setVolume(maxvol);
        if (mFadeVolume >= 0  &&  mFadeSeconds > 0)
        {
            auto fader = new Phonon::VolumeFaderEffect(mAudioObject);
            fader->setVolume(mFadeVolume / maxvol);
            fader->fadeTo(mVolume / maxvol, mFadeSeconds * 1000);
            mPath.insertEffect(fader);
        }
    }
    connect(mAudioObject, &Phonon::MediaObject::stateChanged, this, &AudioThread::playStateChanged, Qt::DirectConnection);
    connect(mAudioObject, &Phonon::MediaObject::finished, this, &AudioThread::checkAudioPlay, Qt::DirectConnection);
    mPlayedOnce = false;
    mPausing    = false;
    mMutex.unlock();
    Q_EMIT readyToPlay();
    checkAudioPlay();

    // Start an event loop.
    // The function will exit once exit() or quit() is called.
    // First, ensure that the thread object is deleted once it has completed.
    connect(this, &QThread::finished, this, &QObject::deleteLater);
    exec();
    stopPlay();
}

/******************************************************************************
* Called when the audio file has loaded and is ready to play, or when play
* has completed.
* If it is ready to play, start playing it (for the first time or repeated).
* If play has not yet completed, wait a bit longer.
*/
void AudioThread::checkAudioPlay()
{
    mMutex.lock();
    if (!mAudioObject)
    {
        mMutex.unlock();
        return;
    }
    if (mPausing)
        mPausing = false;
    else
    {
        // The file has loaded and is ready to play, or play has completed
        if (mPlayedOnce)
        {
            if (mRepeatPause < 0)
            {
                // Play has completed
                mMutex.unlock();
                stopPlay();
                return;
            }
            if (mRepeatPause > 0)
            {
                // Pause before playing the file again
                mPausing = true;
                QTimer::singleShot(mRepeatPause * 1000, this, &AudioThread::checkAudioPlay);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
                mMutex.unlock();
                return;
            }
        }
        mPlayedOnce = true;
    }

    // Start playing the file, either for the first time or again
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::AudioThread::checkAudioPlay: start";
    mAudioObject->play();
    mMutex.unlock();
}

/******************************************************************************
* Called when the playback object changes state.
* If an error has occurred, quit and return the error to the caller.
*/
void AudioThread::playStateChanged(Phonon::State newState)
{
    if (newState == Phonon::ErrorState)
    {
        QMutexLocker locker(&mMutex);
        const QString err = mAudioObject->errorString();
        if (!err.isEmpty())
        {
            qCCritical(KALARM_LOG) << "MessageDisplayHelper::AudioThread::playStateChanged: Play failure:" << mFile << ":" << err;
            mError = xi18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para><para>%2</para>", mFile, err);
            exit(1);
        }
    }
}

/******************************************************************************
* Called when play completes, the Silence button is clicked, or the display is
* closed, to terminate audio access.
*/
void AudioThread::stopPlay()
{
    mMutex.lock();
    if (mAudioObject)
    {
        mAudioObject->stop();
        const QList<Phonon::Effect*> effects = mPath.effects();
        for (int i = 0;  i < effects.count();  ++i)
        {
            mPath.removeEffect(effects[i]);
            delete effects[i];
        }
        delete mAudioObject;
        mAudioObject = nullptr;
    }
    mMutex.unlock();
    quit();   // exit the event loop, if it's still running
}

QString AudioThread::error() const
{
    QMutexLocker locker(&mMutex);
    return mError;
}

/******************************************************************************
* Display the alarm.
* Reply = true if the alarm should be shown, false if not.
*/
bool MessageDisplayHelper::activateAutoClose()
{
    if (mCloseTime.isValid())
    {
        // Set a timer to auto-close the display.
        int delay = QDateTime::currentDateTimeUtc().secsTo(mCloseTime);
        if (delay < 0)
            delay = 0;
        QTimer::singleShot(delay * 1000, this, &MessageDisplayHelper::autoCloseNow);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        if (!delay)
            return false;    // don't show the alarm if auto-closing is already due
    }
    return true;
}

/******************************************************************************
* Called when the display has been shown properly (in its correct position),
* to play sounds and reschedule the event.
*/
void MessageDisplayHelper::displayComplete(bool audio)
{
    delete mTempFile;
    mTempFile = nullptr;
    if (audio)
        playAudio();
    if (mRescheduleEvent)
        alarmShowing(mEvent);
}

/******************************************************************************
* To be called when a close event is received.
* Only quits the application if there is no system tray icon displayed.
*/
bool MessageDisplayHelper::closeEvent()
{
    // Don't prompt or delete the alarm from the display calendar if the session is closing
    if (!mErrorWindow  &&  !qApp->isSavingSession())
    {
        if (mConfirmAck  &&  !mParent->confirmAcknowledgement())
            return false;

        if (!mEventId.isEmpty())
        {
            // Delete from the display calendar
            KAlarm::deleteDisplayEvent(CalEvent::uid(mEventId.eventId(), CalEvent::DISPLAYING));
        }
    }
    return true;
}

/******************************************************************************
* Create an alarm edit dialog.
*
* NOTE: The alarm edit dialog is made a child of the main window, not of
*       displayParent(), so that if displayParent() closes before the dialog
*       (e.g. on auto-close), KAlarm doesn't crash. The dialog is set non-modal
*       so that the main window is unaffected, but modal mode is simulated so
*       that displayParent() is inactive while the dialog is open.
*/
EditAlarmDlg* MessageDisplayHelper::createEdit()
{
    qCDebug(KALARM_LOG) << "MessageDisplayHelper::createEdit";
    mEditDlg = EditAlarmDlg::create(false, mOriginalEvent, false, MainWindow::mainMainWindow(), EditAlarmDlg::RES_IGNORE);
    if (mEditDlg)
    {
        mEditDlg->setAttribute(Qt::WA_NativeWindow, true);
        connect(mEditDlg, &QDialog::accepted, this, &MessageDisplayHelper::editCloseOk);
        connect(mEditDlg, &QDialog::rejected, this, &MessageDisplayHelper::editCloseCancel);
        connect(mEditDlg, &QObject::destroyed, this, &MessageDisplayHelper::editCloseCancel);
    }
    return mEditDlg;
}

/******************************************************************************
* Execute the alarm edit dialog.
*/
void MessageDisplayHelper::executeEdit()
{
    MainWindow::mainMainWindow()->editAlarm(mEditDlg, mOriginalEvent);
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog invoked by the Edit button.
* Closes the display.
*/
void MessageDisplayHelper::editCloseOk()
{
    mEditDlg = nullptr;
    mNoCloseConfirm = true;   // allow window to close without confirmation prompt
    mParent->closeDisplay();
}

/******************************************************************************
* Called when Cancel is clicked in the alarm edit dialog invoked by the Edit
* button, or when the dialog is deleted.
*/
void MessageDisplayHelper::editCloseCancel()
{
    mEditDlg = nullptr;
    mParent->editDlgCancelled();
}

/******************************************************************************
* Set up to disable the defer button when the deferral limit is reached.
*/
void MessageDisplayHelper::setDeferralLimit(const KAEvent& event)
{
    mDeferLimit = event.deferralLimit().effectiveKDateTime().toUtc().qDateTime();
    MidnightTimer::connect(this, SLOT(checkDeferralLimit()));   // check every day
    mDisableDeferral = false;
    checkDeferralLimit();
}

/******************************************************************************
* Check whether the deferral limit has been reached.
* If so, disable the Defer button.
* N.B. Ideally, just a single QTimer::singleShot() call would be made to disable
*      the defer button at the corret time. But for a 32-bit integer, the
*      milliseconds parameter overflows in about 25 days, so instead a daily
*      check is done until the day when the deferral limit is reached, followed
*      by a non-overflowing QTimer::singleShot() call.
*/
void MessageDisplayHelper::checkDeferralLimit()
{
    if (!mParent->isDeferButtonEnabled()  ||  !mDeferLimit.isValid())
        return;
    int n = KADateTime::currentLocalDate().daysTo(KADateTime(mDeferLimit, KADateTime::LocalZone).date());
    if (n > 0)
        return;
    MidnightTimer::disconnect(this, SLOT(checkDeferralLimit()));
    if (n == 0)
    {
        // The deferral limit will be reached today
        n = QDateTime::currentDateTimeUtc().secsTo(mDeferLimit);
        if (n > 0)
        {
            QTimer::singleShot(n * 1000, this, &MessageDisplayHelper::checkDeferralLimit);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
            return;
        }
    }
    mParent->enableDeferButton(false);
    mDisableDeferral = true;
}

/******************************************************************************
* Check whether the specified error message is already displayed for this
* alarm, and note that it will now be displayed.
* Reply = true if message is already displayed.
*/
bool MessageDisplayHelper::haveErrorMessage(unsigned msg) const
{
    if (!mErrorMessages.contains(mEventId))
        mErrorMessages.insert(mEventId, 0);
    unsigned& message = mErrorMessages[mEventId];
    const bool result = (message & msg);
    message |= msg;
    return result;
}

void MessageDisplayHelper::clearErrorMessage(unsigned msg) const
{
    if (mErrorMessages.contains(mEventId))
    {
        unsigned& message = mErrorMessages[mEventId];
        if (message == msg)
            mErrorMessages.remove(mEventId);
        else
            message &= ~msg;
    }
}

#include "moc_messagedisplayhelper_p.cpp"

// vim: et sw=4:
