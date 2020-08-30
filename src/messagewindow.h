/*
 *  messagewindow.h  -  displays an alarm message in a window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include "mainwindowbase.h"
#include "messagedisplay.h"

#include <KAlarmCal/KAEvent>

#include <QDateTime>

class QShowEvent;
class QMoveEvent;
class QResizeEvent;
class QCloseEvent;
class PushButton;
class MessageText;
class QCheckBox;
class QLabel;
class DeferAlarmDlg;
class EditAlarmDlg;

using namespace KAlarmCal;

/**
 * MessageWindow: A window to display an alarm or error message
 */
class MessageWindow : public MainWindowBase, public MessageDisplay
{
    Q_OBJECT
public:
    enum {                // flags for constructor
        NO_RESCHEDULE = 0x01,    // don't reschedule the event once it has displayed
        NO_DEFER      = 0x02,    // don't display the Defer button
        ALWAYS_HIDE   = 0x04,    // never show the window (e.g. for audio-only alarms)
        NO_INIT_VIEW  = 0x08     // for internal MessageWindow use only
    };

    MessageWindow();     // for session management restoration only
    MessageWindow(const KAEvent*, const KAAlarm&, int flags);
    ~MessageWindow() override;

    QWidget* displayParent() override;
    void closeDisplay() override;
    void raiseDisplay() override;
    void hideDisplay() override;

    void                repeat(const KAAlarm&) override;
    bool                hasDefer() const override;
    void                showDefer() override;
    void                showDateTime(const KAEvent&, const KAAlarm&) override;
    void                cancelReminder(const KAEvent&, const KAAlarm&) override;
    virtual void        show();
    QSize               sizeHint() const override;
    static void         redisplayAlarms();
    static int          windowCount(bool excludeAlwaysHidden = false);
    static void         showError(const KAEvent&, const DateTime& alarmDateTime, const QStringList& errmsgs,
                                  const QString& dontShowAgain = QString());
    static bool         spread(bool scatter);

protected Q_SLOTS:
    void textsChanged(MessageDisplayHelper::DisplayTexts::TextIds ids, const QString& change);

protected:
    /** Called by MessageDisplayHelper to confirm that the alarm message should
     *  be acknowledged (closed).
     *  @return  true to close the alarm message, false to keep it open.
     */
    bool confirmAcknowledgement() override;

    void setUpDisplay() override;
    void showDisplay() override;

    void                showEvent(QShowEvent*) override;
    void                moveEvent(QMoveEvent*) override;
    void                resizeEvent(QResizeEvent*) override;
    void                closeEvent(QCloseEvent*) override;
    void                saveProperties(KConfigGroup&) override;
    void                readProperties(const KConfigGroup&) override;

private Q_SLOTS:
    void                slotOk();
    void                slotEdit();
    void                slotDefer();
    void                editCloseOk();
    void                editCloseCancel();
    void                activeWindowChanged(WId);
    void                checkDeferralLimit();
    void                displayMainWindow();
    void                slotShowKMailMessage();
    void                enableButtons();
    void                frameDrawn();

private:
    MessageWindow(const KAEvent*, const DateTime& alarmDateTime, const QStringList& errmsgs,
                  const QString& dontShowAgain);
    void                displayComplete();
    void                setButtonsReadOnly(bool);
    bool                getWorkAreaAndModal();
    void                setDeferralLimit(const KAEvent&);
    static bool         isSpread(const QPoint& topLeft);

    static QVector<MessageWindow*> mWindowList;   // list of existing message windows
    static bool         mRedisplayed;             // redisplayAlarms() was called
    // Properties needed by readProperties()
    QDateTime           mCloseTime;               // UTC time at which window should be auto-closed
    int                 mRestoreHeight;
    // Miscellaneous
    QLabel*             mTimeLabel {nullptr};     // trigger time label
    QLabel*             mRemainingText {nullptr}; // the remaining time (for a reminder window)
    PushButton*         mOkButton;
    PushButton*         mEditButton {nullptr};
    PushButton*         mDeferButton {nullptr};
    PushButton*         mSilenceButton {nullptr};
    PushButton*         mKAlarmButton;
    PushButton*         mKMailButton {nullptr};
    MessageText*        mCommandText {nullptr};   // shows output from command
    QCheckBox*          mDontShowAgainCheck {nullptr};
    EditAlarmDlg*       mEditDlg {nullptr};       // alarm edit dialog invoked by Edit button
    DeferAlarmDlg*      mDeferDlg {nullptr};
    QDateTime           mDeferLimit;              // last UTC time to which the message can currently be deferred
    int                 mButtonDelay;             // delay (ms) after window is shown before buttons are enabled
    int                 mScreenNumber;            // screen to display on, or -1 for default
    bool                mInitialised {false};     // setUpDisplay() has been called to create the window's widgets
    bool                mShown {false};           // true once the window has been displayed
    bool                mPositioning {false};     // true when the window is being positioned initially
    bool                mNoCloseConfirm {false};  // the Defer or Edit button is closing the dialog
    bool                mDisableDeferral {false}; // true if past deferral limit, so don't enable Defer button
};

#endif // MESSAGEWINDOW_H

// vim: et sw=4:
