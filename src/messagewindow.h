/*
 *  messagewindow.h  -  displays an alarm message in a window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "mainwindowbase.h"
#include "messagedisplay.h"


class QShowEvent;
class QMoveEvent;
class QResizeEvent;
class QCloseEvent;
class PushButton;
class MessageText;
class QCheckBox;
class QLabel;

using namespace KAlarmCal;

/**
 * MessageWindow: A window to display an alarm or error message
 */
class MessageWindow : public MainWindowBase, public MessageDisplay
{
    Q_OBJECT
public:
    MessageWindow();     // for session management restoration only
    MessageWindow(const KAEvent&, const KAAlarm&, int flags);
    MessageWindow(const KAEvent&, const DateTime& alarmDateTime, const QStringList& errmsgs,
                  const QString& dontShowAgain);
    ~MessageWindow() override;

    QWidget* displayParent() override;
    void closeDisplay() override;
    void showDisplay() override;
    void raiseDisplay() override;

    void                repeat(const KAAlarm&) override;
    bool                hasDefer() const override;
    void                showDefer() override;
    void                showDateTime(const KAEvent&, const KAAlarm&) override;
    void                cancelReminder(const KAEvent&, const KAAlarm&) override;

    /** Use display() instead of show() to display a message window.
     *  This prevents windows which should be auto-closed from being shown.
     */
    void                display();

    QSize               sizeHint() const override;
    static void         redisplayAlarms();
    static int          windowCount(bool excludeAlwaysHidden = false);
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

    bool isDeferButtonEnabled() const override;
    void enableDeferButton(bool enable) override;
    void enableEditButton(bool enable) override;

    /** Called when the edit alarm dialog has been cancelled. */
    void editDlgCancelled() override;

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
    void                activeWindowChanged(WId);
    void                displayMainWindow();
    void                slotShowKMailMessage();
    void                enableButtons();
    void                commandCompleted(bool success);
    void                frameDrawn();

private:
    void                displayComplete();
    void                setButtonsReadOnly(bool);
    bool                getWorkAreaAndModal();
    static bool         isSpread(const QPoint& topLeft);
    void show();   // ensure that display() is called instead of show() on a MessageWindow object

    static QList<MessageWindow *>
        mWindowList; // list of message window instances
    // Properties needed by readProperties()
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
    DeferDlgData*       mDeferData {nullptr};     // defer dialog data
    int                 mButtonDelay;             // delay (ms) after window is shown before buttons are enabled
    int                 mScreenNumber;            // screen to display on, or -1 for default
    bool                mInitialised {false};     // setUpDisplay() has been called to create the window's widgets
    bool                mShown {false};           // true once the window has been displayed
    bool                mPositioning {false};     // true when the window is being positioned initially
};

// vim: et sw=4:
