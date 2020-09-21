/*
 *  messagenotification.h  -  displays an alarm message in a system notification
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MESSAGENOTIFICATION_H
#define MESSAGENOTIFICATION_H

#include "messagedisplay.h"
#include <KNotification>

using namespace KAlarmCal;

/**
 * MessageNotification: A window to display an alarm or error message
 */
class MessageNotification : public KNotification, public MessageDisplay
{
    Q_OBJECT
public:
    MessageNotification(const KAEvent&, const KAAlarm&, int flags);
    MessageNotification(const KAEvent&, const DateTime& alarmDateTime, const QStringList& errmsgs,
                  const QString& dontShowAgain);
    ~MessageNotification() override;

    /** Restore MessageNotification instances saved at session shutdown. */
    static void sessionRestore();

    QWidget* displayParent() override;
    void closeDisplay() override;
    void showDisplay() override;
    void raiseDisplay() override;

    void                repeat(const KAAlarm&) override;
    bool                hasDefer() const override;
    void                showDefer() override;
    void                showDateTime(const KAEvent&, const KAAlarm&) override;
    void                cancelReminder(const KAEvent&, const KAAlarm&) override;
    static void         redisplayAlarms();
    static int          notificationCount();

protected Q_SLOTS:
    void textsChanged(MessageDisplayHelper::DisplayTexts::TextIds ids, const QString& change);

protected:
    void setUpDisplay() override;
    bool isDeferButtonEnabled() const override;
    void enableDeferButton(bool enable) override;
    void enableEditButton(bool enable) override;
    void saveProperties(KConfigGroup&);

private Q_SLOTS:
    void buttonActivated(unsigned int index);
    void commandCompleted(bool success);
    void slotClosed();

private:
    MessageNotification(const QString& eventId, MessageDisplayHelper* helper);
    void                setNotificationTitle(const QString&);
    void                setNotificationText();
    void                setNotificationButtons();

    static QVector<MessageNotification*> mNotificationList;   // list of notification instances
    // Miscellaneous
    QString             mTimeText;                // trigger time text
    QString             mMessageText;             // alarm message text
    QString             mRemainingText;           // remaining time text
    int                 mEditButtonIndex {-1};    // edit button's action index
    int                 mDeferButtonIndex {-1};   // defer button's action index
    bool                mEnableDefer {false};     // whether to show a Defer button
    bool                mEnableEdit {false};      // whether to show an Edit button
    bool                mInitialised {false};     // setUpDisplay() has been called to create the window's widgets
    bool                mDisplayComplete {false}; // true once displayComplete() has been called
    bool                mShown {false};           // true once the notification has been displayed
    bool                mCommandInhibit {false};  // true to prevent display until command exits

friend class MNSessionManager;
};

#endif // MESSAGENOTIFICATION_H

// vim: et sw=4:
