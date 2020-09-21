/*
 *  messagedisplayhelper.h  -  helper class to display an alarm or error message
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MESSAGEDISPLAYHELPER_H
#define MESSAGEDISPLAYHELPER_H

#include "eventid.h"
#include "resources/resource.h"
#include "lib/file.h"
#include "lib/shellprocess.h"

#include <KAlarmCal/KAEvent>

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QDateTime>

class KConfigGroup;
class QTemporaryFile;
class AudioThread;
class PushButton;
class EditAlarmDlg;
class MessageDisplay;

using namespace KAlarmCal;

/**
 * Class for use by alarm message display classes, to handle common functions
 * for displaying an alarm.
 * In order for this class to use signals and slots, it needs to derive from
 * QObject. As a result, it has to be a separate class from MessageDisplay;
 * otherwise, MessageWindow would derive from two QObject classes, which is
 * prohibited.
 */
class MessageDisplayHelper : public QObject
{
    Q_OBJECT
public:
    /** Contains the texts to display in the alarm. */
    struct DisplayTexts
    {
        /** Identifiers for the fields in DisplayTexts. */
        enum TextId
        {
            Title         = 0x01,    //!< DisplayTexts::title
            Time          = 0x02,    //!< DisplayTexts::time
            TimeFull      = 0x04,    //!< DisplayTexts::timeFull
            FileName      = 0x08,    //!< DisplayTexts::fileName
            Message       = 0x10,    //!< DisplayTexts::message
            MessageAppend = 0x20,    //!< Text has been appended to DisplayTexts::message
            RemainingTime = 0x40     //!< DisplayTexts::remainingTime
        };
        Q_DECLARE_FLAGS(TextIds, TextId)

        QString title;            // window/notification title
        QString time;             // header showing alarm trigger time
        QString timeFull;         // header showing alarm trigger time and "Reminder" if appropriate
        QString fileName;         // if message is a file's contents, the file name
        QString message;          // the alarm message
        QString remainingTime;    // if advance reminder, the remaining time until the actual alarm
        QString errorEmail[4];    // if email alarm error message, the 'To' and 'Subject' contents
        File::FileType fileType;  // if message is a file's contents, the file type
        bool    newLine {false};  // 'message' has a newline stripped from the end
    };

    explicit MessageDisplayHelper(MessageDisplay* parent);     // for session management restoration only
    MessageDisplayHelper(MessageDisplay* parent, const KAEvent&, const KAAlarm&, int flags);
    MessageDisplayHelper(MessageDisplay* parent, const KAEvent&, const DateTime& alarmDateTime,
                         const QStringList& errmsgs, const QString& dontShowAgain);
    MessageDisplayHelper(const MessageDisplayHelper&) = delete;
    MessageDisplayHelper& operator=(const MessageDisplayHelper&) = delete;
    ~MessageDisplayHelper() override;
    void                setParent(MessageDisplay* parent)  { mParent = parent; }
    void                setSilenceButton(PushButton* b)    { mSilenceButton = b; }
    void                repeat(const KAAlarm&);
    const DateTime&     dateTime()             { return mDateTime; }
    KAAlarm::Type       alarmType() const      { return mAlarmType; }
    bool                cancelReminder(const KAEvent&, const KAAlarm&);
    bool                updateDateTime(const KAEvent&, const KAAlarm&);
    bool                isValid() const        { return !mInvalid; }
    bool                alwaysHidden() const   { return mAlwaysHide; }
    void                initTexts();
    const DisplayTexts& texts() const          { return mTexts; }
    bool                activateAutoClose();
    void                displayComplete(bool audio);
    bool                alarmShowing(KAEvent&);
    void                playAudio();
    EditAlarmDlg*       createEdit();
    void                executeEdit();
    void                setDeferralLimit(const KAEvent&);

    /** Called when a close request has been received.
     *  @return  true to close the alarm message, false to keep it open.
     */
    bool                closeEvent();

    bool                saveProperties(KConfigGroup&);
    bool                readProperties(const KConfigGroup&);
    bool                readPropertyValues(const KConfigGroup&);
    bool                processPropertyValues();

    static int          instanceCount(bool excludeAlwaysHidden = false);
    static bool         shouldShowError(const KAEvent& event, const QStringList& errmsgs, const QString& dontShowAgain = QString());
    static MessageDisplay* findEvent(const EventId& eventId, MessageDisplay* exclude = nullptr);
    static void         stopAudio(bool wait = false);
    static bool         isAudioPlaying();

Q_SIGNALS:
    /** Signal emitted when texts in the alarm message have changed.
     *  @param id      Which text has changed.
     *  @param change  If id == MessageAppend, the text which has been appended.
     */
    void textsChanged(DisplayTexts::TextIds, const QString& change = QString());

    /** Signal emitted on completion of the command providing the alarm message text. */
    void commandExited(bool success);

    /** Signal emitted when the alarm should close, after the auto-close time. */
    void autoCloseNow();

private Q_SLOTS:
    void    showRestoredAlarm();
    void    editCloseOk();
    void    editCloseCancel();
    void    checkDeferralLimit();
    void    slotSpeak();
    void    audioTerminating();
    void    startAudio();
    void    playReady();
    void    playFinished();
    void    slotSetRemainingTextDay()     { setRemainingTextDay(true); }
    void    slotSetRemainingTextMinute()  { setRemainingTextMinute(true); }
    void    readProcessOutput(ShellProcess*);
    void    commandCompleted(ShellProcess::Status);

private:
    QString dateTimeToDisplay() const;
    void    setRemainingTextDay(bool notify);
    void    setRemainingTextMinute(bool notify);
    bool    haveErrorMessage(unsigned msg) const;
    void    clearErrorMessage(unsigned msg) const;
    void    redisplayAlarm();

    static QVector<MessageDisplayHelper*> mInstanceList;  // list of existing message displays
    static QHash<EventId, unsigned> mErrorMessages; // error messages currently displayed, by event ID
    // Sound file playing
    static QPointer<AudioThread> mAudioThread;    // thread to play audio file

public:
    MessageDisplay*     mParent;
    // Properties needed by readProperties()
    QString             mMessage;
    QFont               mFont;
    QColor              mBgColour, mFgColour;
    DateTime            mDateTime;                // date/time displayed in the message window
    QDateTime           mCloseTime;               // UTC time at which window should be auto-closed
    EventId             mEventId;
    QString             mAudioFile;
    float               mVolume;
    float               mFadeVolume;
    int                 mFadeSeconds;
    int                 mDefaultDeferMinutes;
    KAAlarm::Type       mAlarmType;
    KAEvent::SubAction  mAction;
    Akonadi::Item::Id   mAkonadiItemId;           // if email text, message's Akonadi Item ID, else -1
    KAEvent::CmdErrType mCommandError;
    QStringList         mErrorMsgs;
    QString             mDontShowAgain;           // non-null for don't-show-again option with error message
    int                 mAudioRepeatPause;
    bool                mConfirmAck;
    bool                mShowEdit;                // display the Edit button
    bool                mNoDefer;                 // don't display a Defer option
    bool                mInvalid;                 // restored window is invalid
    // Miscellaneous
    KAEvent             mEvent;                   // the whole event, for updating the calendar file
    KAEvent             mOriginalEvent;           // the original event supplied to the constructor
    Resource            mResource;                // resource which the event comes/came from
    PushButton*         mSilenceButton {nullptr}; // button to stop audio, enabled when audio playing
    EditAlarmDlg*       mEditDlg {nullptr};       // alarm edit dialog invoked by Edit button
    QDateTime           mDeferLimit;              // last UTC time to which the message can currently be deferred
    bool                mDisableDeferral {false}; // true if past deferral limit, so don't enable Defer button
    bool                mNoCloseConfirm {false};  // the Defer or Edit button is closing the dialog
    bool                mAlwaysHide {false};      // the window should never be displayed
    bool                mErrorWindow {false};     // the window is simply an error message
    bool                mNoPostAction;            // don't execute any post-alarm action
    bool                mBeep;
    bool                mSpeak;                   // the message should be spoken via kttsd
private:
    DisplayTexts        mTexts;                   // texts to display in alarm message
    QTemporaryFile*     mTempFile {nullptr};      // temporary file used to display image/HTML
    QByteArray          mCommandOutput;           // cumulative output from command
    bool                mCommandHaveStdout {false}; // true if some stdout has been received from command
    bool                mNoRecordCmdError {false}; // don't record command alarm errors
    bool                mInitialised {false};     // initTexts() has been called to create the alarm's texts
    bool                mRescheduleEvent {false}; // true to delete event after message has been displayed

//friend class MessageDisplay;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MessageDisplayHelper::DisplayTexts::TextIds)

#endif // MESSAGEDISPLAYHELPER_H

// vim: et sw=4:
