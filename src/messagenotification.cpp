/*
 *  messagenotification.cpp  -  displays an alarm message in a system notification
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagenotification.h"
#include "messagedisplayhelper.h"

#include "mainwindow.h"
#include "resourcescalendar.h"
#include "lib/file.h"
#include "kalarm_debug.h"

#include <KAboutData>
#include <KLocalizedString>
#ifdef RESTORE_NOTIFICATIONS
#include <KConfigGroup>
#include <KConfigGui>
#endif

#include <QSessionManager>

using namespace KAlarmCal;

//clazy:excludeall=non-pod-global-static

namespace
{

// Notification eventIds: these are the IDs contained in the '[Event/ID]'
// entries in kalarm.notifyrc.
const QString MessageId = QStringLiteral("Message");
const QString BeepId    = QStringLiteral("MessageBeep");
const QString SpeakId   = QStringLiteral("MessageSpeak");
const QString ErrorId   = QStringLiteral("MessageError");

// Flags for the notification
const KNotification::NotificationFlags NFLAGS = KNotification::RaiseWidgetOnActivation;

const QString NL = QStringLiteral("\n");
const QString SP = QStringLiteral(" ");

inline QString getNotifyEventId(const KAEvent& event)
{
    return event.beep() ? BeepId : event.speak() ? SpeakId : MessageId;
}

} // namespace


/*=============================================================================
* Helper class to save all message notifications' properties on session
* shutdown, to enable them to be recreated on the next startup.
*
* NOTE: When a notification has closed, there is currently no way to know
*       whether it has been closed by the user or has timed out. There is also
*       no way to know when a notification in the notification history is
*       closed by the user. So notifications are not restored on startup, since
*       that might re-raise notifications which the user has already closed.
*       If this changes in the future, notifications could be restored on
*       startup, in the same way as alarm windows are restored.
*/
class MNSessionManager : public QObject
{
    Q_OBJECT
public:
    MNSessionManager()
    {
#ifdef RESTORE_NOTIFICATIONS
        connect(qApp, &QGuiApplication::saveStateRequest, this, &MNSessionManager::saveState);
#endif
    }
    ~MNSessionManager() override {}

    static void create()
    {
        if (!mInstance)
            mInstance = new MNSessionManager;
    }

private Q_SLOTS:
#ifdef RESTORE_NOTIFICATIONS
    /******************************************************************************
    * Called by the session manager to request the application to save its state.
    */
    void saveState(QSessionManager& sm)
    {
        KConfigGui::setSessionConfig(sm.sessionId(), sm.sessionKey());
        KConfig* config = KConfigGui::sessionConfig();
        // Save each MessageNotification's data.
        int n = 1;
        for (MessageNotification* notif : std::as_const(MessageNotification::mNotificationList))
        {
            const QByteArray group = "Notification_" + QByteArray::number(++n);
            KConfigGroup cg(config, group.constData());
            notif->saveProperties(cg);
        }
        KConfigGroup cg(config, "Number");
        cg.writeEntry("NumberOfNotifications", MessageNotification::mNotificationList.count());
    }
#endif

private:
    static MNSessionManager* mInstance;
};

MNSessionManager* MNSessionManager::mInstance = nullptr;


QVector<MessageNotification*> MessageNotification::mNotificationList;

/******************************************************************************
* Restore MessageNotification instances saved at session shutdown.
*/
void MessageNotification::sessionRestore()
{
#ifdef RESTORE_NOTIFICATIONS
    KConfig* config = KConfigGui::sessionConfig();
    if (config)
    {
        KConfigGroup cg(config, "Number");
        const int count = cg.readEntry("NumberOfNotifications", 0);
        for (int n = 1;  n <= count;  ++n)
        {
            const QByteArray group = "Notification_" + QByteArray::number(n);
            cg = KConfigGroup(config, group.constData());
            // Have to initialise the MessageNotification instance with its
            // eventId already known. So first create a helper, then read
            // its properties, and finally create the MessageNotification.
            MessageDisplayHelper* helper = new MessageDisplayHelper(nullptr);
            if (!helper->readPropertyValues(cg))
                delete helper;
            else
            {
                const QString notifyId = cg.readEntry("NotifyId");
                new MessageNotification(notifyId, helper);
            }
        }
    }
#endif
}

/******************************************************************************
* Construct the message notification for the specified alarm.
* Other alarms in the supplied event may have been updated by the caller, so
* the whole event needs to be stored for updating the calendar file when it is
* displayed.
*/
MessageNotification::MessageNotification(const KAEvent& event, const KAAlarm& alarm, int flags)
    : KNotification(getNotifyEventId(event), NFLAGS)
    , MessageDisplay(event, alarm, flags)
{
    qCDebug(KALARM_LOG) << "MessageNotification():" << mEventId();
    MNSessionManager::create();
    setWidget(MainWindow::mainMainWindow());
    if (!(flags & NoInitView))
        MessageNotification::setUpDisplay();    // avoid calling virtual method from constructor

    connect(this, &KNotification::activated, this, &MessageNotification::buttonActivated);
    connect(this, &KNotification::closed, this, &MessageNotification::slotClosed);
    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageNotification::textsChanged);
    connect(mHelper, &MessageDisplayHelper::commandExited, this, &MessageNotification::commandCompleted);

    mNotificationList.append(this);
}

/******************************************************************************
* Construct the message notification for a specified error message.
* If 'dontShowAgain' is non-null, a "Don't show again" option is displayed. Note
* that the option is specific to 'event'.
*/
MessageNotification::MessageNotification(const KAEvent& event, const DateTime& alarmDateTime,
                       const QStringList& errmsgs, const QString& dontShowAgain)
    : KNotification(ErrorId, NFLAGS)
    , MessageDisplay(event, alarmDateTime, errmsgs, dontShowAgain)
{
    qCDebug(KALARM_LOG) << "MessageNotification(errmsg)";
    MNSessionManager::create();
    setWidget(MainWindow::mainMainWindow());
    MessageNotification::setUpDisplay();    // avoid calling virtual method from constructor

    connect(this, &KNotification::activated, this, &MessageNotification::buttonActivated);
    connect(this, &KNotification::closed, this, &MessageNotification::slotClosed);
    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageNotification::textsChanged);

    mNotificationList.append(this);
}

/******************************************************************************
* Construct the message notification from the properties contained in the
* supplied helper.
* Ownership of the helper is taken by the new instance.
*/
MessageNotification::MessageNotification(const QString& eventId, MessageDisplayHelper* helper)
    : KNotification(eventId, NFLAGS)
    , MessageDisplay(helper)
{
    qCDebug(KALARM_LOG) << "MessageNotification(helper):" << mEventId();
    MNSessionManager::create();
    setWidget(MainWindow::mainMainWindow());

    connect(this, &KNotification::activated, this, &MessageNotification::buttonActivated);
    connect(this, &KNotification::closed, this, &MessageNotification::slotClosed);
    connect(mHelper, &MessageDisplayHelper::textsChanged, this, &MessageNotification::textsChanged);
    connect(mHelper, &MessageDisplayHelper::commandExited, this, &MessageNotification::commandCompleted);

    mNotificationList.append(this);
    helper->processPropertyValues();
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageNotification::~MessageNotification()
{
    qCDebug(KALARM_LOG) << "~MessageNotification" << mEventId();
    close();
    mNotificationList.removeAll(this);
}

/******************************************************************************
* Construct the message notification.
*/
void MessageNotification::setUpDisplay()
{
    mHelper->initTexts();
    MessageDisplayHelper::DisplayTexts texts = mHelper->texts();

    setNotificationTitle(texts.title);

    // Show the alarm date/time. Any reminder indication is shown in the
    // notification title.
    // Alarm date/time: display time zone if not local time zone.
    mTimeText = texts.time;

    mMessageText.clear();
    if (!mErrorWindow())
    {
        // It's a normal alarm message notification
        switch (mAction())
        {
            case KAEvent::FILE:
                // Display the file name
                mMessageText = texts.fileName + NL;

                if (mErrorMsgs().isEmpty())
                {
                    // Display contents of file
                    switch (texts.fileType)
                    {
                        case File::Image:
                            break;   // can't display an image
                        case File::TextFormatted:
                        default:
                            mMessageText += texts.message;
                            break;
                    }
                }
                break;

            case KAEvent::MESSAGE:
                mMessageText = texts.message;
                break;

            case KAEvent::COMMAND:
                mMessageText = texts.message;
                mCommandInhibit = true;
                break;

            case KAEvent::EMAIL:
            default:
                break;
        }

        if (!texts.remainingTime.isEmpty())
        {
            // Advance reminder: show remaining time until the actual alarm
            mRemainingText = texts.remainingTime;
        }
    }
    else
    {
        // It's an error message
        switch (mAction())
        {
            case KAEvent::EMAIL:
            {
                // Display the email addresses and subject.
                mMessageText = texts.errorEmail[0] + SP + texts.errorEmail[1] + NL
                             + texts.errorEmail[2] + SP + texts.errorEmail[3] + NL;
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

    if (!mErrorMsgs().isEmpty())
    {
        setIconName(QStringLiteral("dialog-error"));
        mMessageText += mErrorMsgs().join(NL);
        mCommandInhibit = false;
    }

    setNotificationText();

    mEnableEdit = mShowEdit();
    if (!mNoDefer())
    {
        mEnableDefer = true;
        mHelper->setDeferralLimit(mEvent());  // ensure that button is disabled when alarm can't be deferred any more
    }
    setNotificationButtons();

    mInitialised = true;   // the notification's widgets have been created
}

/******************************************************************************
* Return the number of message notifications.
*/
int MessageNotification::notificationCount()
{
    return mNotificationList.count();
}

/******************************************************************************
* Returns the widget to act as parent for error messages, etc.
*/
QWidget* MessageNotification::displayParent()
{
    return widget();
}

void MessageNotification::closeDisplay()
{
    close();
}

/******************************************************************************
* Display the notification.
* Output any required audio notification, and reschedule or delete the event
* from the calendar file.
*/
void MessageNotification::showDisplay()
{
    if (mInitialised  &&  mHelper->activateAutoClose())
    {
        if (!mCommandInhibit  &&  !mShown)
        {
            qCDebug(KALARM_LOG) << "MessageNotification::showDisplay: sendEvent";
            sendEvent();
            mShown = true;
            // Ensure that the screen wakes from sleep, in case the window manager
            // doesn't do this when the notification is displayed.
            mHelper->wakeScreen();
        }
        if (!mDisplayComplete  &&  !mErrorWindow()  &&  mAlarmType() != KAAlarm::INVALID_ALARM)
            mHelper->displayComplete(false);   // reschedule
        mDisplayComplete = true;
    }
}

void MessageNotification::raiseDisplay()
{
}

/******************************************************************************
* Raise the alarm notification, re-output any required audio notification, and
* reschedule the alarm in the calendar file.
*/
void MessageNotification::repeat(const KAAlarm& alarm)
{
    if (!mInitialised)
        return;
    if (mEventId().isEmpty())
        return;
    KAEvent event = ResourcesCalendar::event(mEventId());
    if (event.isValid())
    {
        mAlarmType() = alarm.type();    // store new alarm type for use if it is later deferred
        if (mHelper->alarmShowing(event))
            ResourcesCalendar::updateEvent(event);
    }
}

bool MessageNotification::hasDefer() const
{
    return mEnableDefer;
}

/******************************************************************************
* Show the Defer button when it was previously hidden.
*/
void MessageNotification::showDefer()
{
    if (!mEnableDefer)
    {
        mNoDefer() = false;
        mEnableDefer = true;
        setNotificationButtons();
        mHelper->setDeferralLimit(mEvent());    // remove button when alarm can't be deferred any more
    }
}

/******************************************************************************
* Convert a reminder notification into a normal alarm notification.
*/
void MessageNotification::cancelReminder(const KAEvent& event, const KAAlarm& alarm)
{
    if (mHelper->cancelReminder(event, alarm))
    {
        const MessageDisplayHelper::DisplayTexts& texts = mHelper->texts();
        setNotificationTitle(texts.title);
        mTimeText = texts.time;
        mRemainingText.clear();
        setNotificationText();
        showDefer();
    }
}

/******************************************************************************
* Update and show the alarm's trigger time.
*/
void MessageNotification::showDateTime(const KAEvent& event, const KAAlarm& alarm)
{
    if (mHelper->updateDateTime(event, alarm))
    {
        mTimeText = mHelper->texts().time;
        setNotificationText();
    }
}

/******************************************************************************
* Called when the texts to display have changed.
*/
void MessageNotification::textsChanged(MessageDisplayHelper::DisplayTexts::TextIds ids, const QString& change)
{
    const MessageDisplayHelper::DisplayTexts& texts = mHelper->texts();

    if (ids & MessageDisplayHelper::DisplayTexts::Title)
        setNotificationTitle(texts.title);

    bool textChanged = false;
    if (ids & MessageDisplayHelper::DisplayTexts::Time)
    {
        mTimeText = texts.time;
        textChanged = true;
    }

    if (ids & MessageDisplayHelper::DisplayTexts::RemainingTime)
    {
        mRemainingText = texts.remainingTime;
        textChanged = true;
    }

    if (ids & MessageDisplayHelper::DisplayTexts::MessageAppend)
    {
        // More output is available from the command which is providing the text
        // for this notification. Add the output, but don't show the notification
        // until all output has been received. This is a workaround for
        // notification texts not being reliably updated by setText().
        mMessageText += change;
        return;
    }

    if (textChanged)
        setNotificationText();

    // Update the notification. Note that this does nothing if no changes have occurred.
    update();
}

/******************************************************************************
* Called when the command providing the alarm message text has exited.
* Because setText() doesn't reliably update the text in the notification,
* command output notifications are not displayed until all the text is
* available to display.
* 'success' is true if the command did not fail completely.
*/
void MessageNotification::commandCompleted(bool success)
{
    qCDebug(KALARM_LOG) << "MessageNotification::commandCompleted:" << success;
    if (!success)
    {
        // The command failed completely. KAlarmApp will output an error
        // message, so don't display the empty notification.
        deleteLater();
    }
    else
    {
        // The command may have produced some output, so display that, although
        // if an error occurred, KAlarmApp might display an error message as
        // well.
        setNotificationText();
        mCommandInhibit = false;
        showDisplay();
    }
}

/******************************************************************************
* Set the notification's title.
*/
void MessageNotification::setNotificationTitle(const QString& text)
{
    setTitle(mErrorMsgs().isEmpty() ? QString() : text);
}

/******************************************************************************
* Set the notification's text by combining the text portions.
*/
void MessageNotification::setNotificationText()
{
    setText(mMessageText + NL + mTimeText + NL + QStringLiteral("<i>") + mRemainingText + QStringLiteral("</i>"));
update();
}

/******************************************************************************
* Set the notification's action buttons.
*/
void MessageNotification::setNotificationButtons()
{
    mEditButtonIndex = -1;
    mDeferButtonIndex = -1;
    QStringList buttons;
    if (mEnableEdit)
    {
        mEditButtonIndex = 0;
        buttons += i18nc("@action:button", "Edit");
    }
    if (mEnableDefer)
    {
        mDeferButtonIndex = buttons.count();
        buttons += i18nc("@action:button", "Defer");
    }
    setActions(buttons);
    setDefaultAction(KAboutData::applicationData().displayName());
}

bool MessageNotification::isDeferButtonEnabled() const
{
    return mEnableDefer;
}

void MessageNotification::enableDeferButton(bool enable)
{
    mEnableDefer = enable;
    setNotificationButtons();
}

void MessageNotification::enableEditButton(bool enable)
{
    mEnableEdit = enable;
    setNotificationButtons();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageNotification::saveProperties(KConfigGroup& config)
{
    Q_UNUSED(config)
#ifdef RESTORE_NOTIFICATIONS
    if (mDisplayComplete  &&  mHelper->saveProperties(config))
        config.writeEntry("NotifyId", eventId());
#endif
}

/******************************************************************************
* Called when a button in the notification has been pressed.
* Button indexes start at 1.
*/
void MessageNotification::buttonActivated(unsigned int index)
{
    int i = static_cast<int>(index);
    if (i == 0)
    {
        displayMainWindow();
    }
    else if (i == mEditButtonIndex + 1)
    {
        if (mHelper->createEdit())
            mHelper->executeEdit();
    }
    else if (i == mDeferButtonIndex + 1)
    {
        DeferDlgData* data = createDeferDlg(this, true);
        executeDeferDlg(data);
    }
}

/******************************************************************************
* Called when the notification has closed, either by user action of by timeout.
* Note that when a notification has timed out, it shows in the notification
* history, but there is no way to know if the user closes it there.
* Only quits the application if there is no system tray icon displayed.
*/
void MessageNotification::slotClosed()
{
    qCDebug(KALARM_LOG) << "MessageNotification::slotClosed";
    mHelper->closeEvent();
}

#include "messagenotification.moc"

// vim: et sw=4:
