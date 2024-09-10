/*
 *  wakedlg.cpp  -  dialog to configure wake-from-suspend alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wakedlg.h"
#include "ui_wakedlg.h"

#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "resourcescalendar.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/kaevent.h"

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QTimer>

using namespace KAlarmCal;

WakeFromSuspendDlg* WakeFromSuspendDlg::mInstance = nullptr;

WakeFromSuspendDlg* WakeFromSuspendDlg::create(QWidget* parent)
{
    if (!mInstance)
        mInstance = new WakeFromSuspendDlg(parent);
    return mInstance;
}

WakeFromSuspendDlg::WakeFromSuspendDlg(QWidget* parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18nc("@title:window", "Wake From Suspend"));


    mUi = new Ui_WakeFromSuspendDlgWidget;
    mUi->setupUi(this);
    mUi->advanceWakeTime->setValue(Preferences::wakeFromSuspendAdvance());

    mMainWindow = qobject_cast<MainWindow*>(parent);
    if (!mMainWindow)
        mMainWindow = MainWindow::mainMainWindow();

    // Check if there is any alarm selected in the main window, and enable/disable
    // the Show and Cancel buttons as necessary.
    enableDisableUseButton();

    // Update the Show and Cancel button status every 5 seconds
    mTimer = new QTimer(this);
    connect(mTimer, &QTimer::timeout, this, &WakeFromSuspendDlg::checkPendingAlarm);
    mTimer->start(5000);

    connect(mMainWindow, &MainWindow::selectionChanged, this, &WakeFromSuspendDlg::enableDisableUseButton);
    connect(mUi->showWakeButton, &QPushButton::clicked, this, &WakeFromSuspendDlg::showWakeClicked);
    connect(mUi->useWakeButton, &QPushButton::clicked, this, &WakeFromSuspendDlg::useWakeClicked);
    connect(mUi->cancelWakeButton, &QPushButton::clicked, this, &WakeFromSuspendDlg::cancelWakeClicked);
    connect(mUi->buttonBox, &QDialogButtonBox::rejected, this, &WakeFromSuspendDlg::close);

    connect(theApp(), &KAlarmApp::alarmEnabledToggled, this, &WakeFromSuspendDlg::enableDisableUseButton);
}

WakeFromSuspendDlg::~WakeFromSuspendDlg()
{
    if (mInstance == this)
        mInstance = nullptr;
    delete mUi;
}

/******************************************************************************
* Called when the alarm selection in the main window changes.
* Enable or disable the Use Highlighted Alarm button.
*/
void WakeFromSuspendDlg::enableDisableUseButton()
{
    bool enable = theApp()->alarmsEnabled();
    if (enable)
    {
        const QString wakeFromSuspendId = KAlarm::checkRtcWakeConfig().value(0);
        const KAEvent event = mMainWindow->selectedEvent();
        enable = event.isValid()
              && event.category() == CalEvent::ACTIVE
              && event.enabled()
              && !event.mainDateTime().isDateOnly()
              && event.id() != wakeFromSuspendId;
    }
    mUi->useWakeButton->setEnabled(enable);
    checkPendingAlarm();
}

/******************************************************************************
* Update the Show and Cancel buttons if the pending alarm status has changed.
* Reply = true if an alarm is still pending.
*/
bool WakeFromSuspendDlg::checkPendingAlarm()
{
    if (KAlarm::checkRtcWakeConfig(true).isEmpty())
    {
        mUi->showWakeButton->setEnabled(false);
        mUi->cancelWakeButton->setEnabled(false);
        return false;
    }
    return true;
}

/******************************************************************************
* Called when the user clicks the Show Current Alarm button.
* Highlight the currently scheduled wake-from-suspend alarm in the main window.
*/
void WakeFromSuspendDlg::showWakeClicked()
{
    if (checkPendingAlarm())
    {
        const QStringList params = KAlarm::checkRtcWakeConfig();
        if (!params.isEmpty())
        {
            const KAEvent event = ResourcesCalendar::event(EventId(params[0].toLongLong(), params[1]));
            if (event.isValid())
            {
                mMainWindow->selectEvent(event.id());
                return;
            }
        }
    }
    mMainWindow->clearSelection();
}

/******************************************************************************
* Called when the user clicks the Use Highlighted Alarm button.
* Schedules system wakeup for that alarm.
*/
void WakeFromSuspendDlg::useWakeClicked()
{
    const KAEvent event = mMainWindow->selectedEvent();
    if (!event.isValid())
        return;
    const KADateTime dt = event.mainDateTime().kDateTime();
    if (dt.isDateOnly())
    {
        KAMessageBox::error(this, i18nc("@info", "Cannot schedule wakeup time for a date-only alarm"));
        return;
    }
    if (KAMessageBox::warningContinueCancel(this,
                xi18nc("@info", "<para>This wakeup will cancel any existing wakeup which has been set by KAlarm "
                               "or any other application, because your computer can only schedule a single wakeup time.</para>"
                               "<para><note>Wake From Suspend is not supported at all on some computers, especially older ones, "
                               "and some computers only support setting a wakeup time up to 24 hours ahead. "
                               "You may wish to set up a test alarm to check your system's capability.</note></para>"),
                QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QStringLiteral("wakeupWarning"))
            != KMessageBox::Continue)
        return;
    const int advance = mUi->advanceWakeTime->value();
    const qint64 triggerTime = dt.addSecs(-advance * 60).toSecsSinceEpoch();
    if (KAlarm::setRtcWakeTime(triggerTime, this))
    {
        const QStringList param{QString::number(event.resourceId()), event.id(), QString::number(triggerTime)};
        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
        config.writeEntry("RtcWake", param);
        config.sync();
        Preferences::setWakeFromSuspendAdvance(static_cast<unsigned>(advance));
        close();
    }
}

/******************************************************************************
* Called when the user clicks the Cancel Wake From Suspend button.
* Cancels any currently scheduled system wakeup.
*/
void WakeFromSuspendDlg::cancelWakeClicked()
{
    KAlarm::setRtcWakeTime(0, this);
    KAlarm::deleteRtcWakeConfig();
    mUi->showWakeButton->setEnabled(false);
    mUi->cancelWakeButton->setEnabled(false);
    enableDisableUseButton();
}

#include "moc_wakedlg.cpp"

// vim: et sw=4:
