/*
 *  wakedlg.cpp  -  dialog to configure wake-from-suspend alarms
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "wakedlg.moc"
#include "ui_wakedlg.h"

#include "alarmcalendar.h"
#include "functions.h"
#include "kaevent.h"
#include "mainwindow.h"
#include "preferences.h"

#include <kglobal.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <QTimer>

WakeFromSuspendDlg* WakeFromSuspendDlg::mInstance = 0;

WakeFromSuspendDlg* WakeFromSuspendDlg::create(QWidget* parent)
{
    if (!mInstance)
        mInstance = new WakeFromSuspendDlg(parent);
    return mInstance;
}

WakeFromSuspendDlg::WakeFromSuspendDlg(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18nc("@title:window", "Wake From Suspend"));
    setButtons(Close);
    mUi = new Ui_WakeFromSuspendDlgWidget;
    mUi->setupUi(mainWidget());
    KConfigGroup config(KGlobal::config(), "General");
    mUi->advanceWakeTime->setValue(Preferences::wakeFromSuspendAdvance());

    mMainWindow = qobject_cast<MainWindow*>(parent);
    if (!mMainWindow)
        mMainWindow = MainWindow::mainMainWindow();

    // Check if there is any alarm selected in the main window, and enable/disable
    // the Show and Cancel buttons as necessary.
    slotSelectedEventChanged();

    // Update the Show and Cancel button status every 5 seconds
    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), SLOT(checkPendingAlarm()));
    mTimer->start(5000);

    connect(mMainWindow, SIGNAL(selectionChanged()), SLOT(slotSelectedEventChanged()));
    connect(mUi->showWakeButton, SIGNAL(clicked()), SLOT(showWakeClicked()));
    connect(mUi->useWakeButton, SIGNAL(clicked()), SLOT(useWakeClicked()));
    connect(mUi->cancelWakeButton, SIGNAL(clicked()), SLOT(cancelWakeClicked()));
}

/******************************************************************************
* Called when the the alarm selection in the main window changes.
* Enable or disable the Use Highlighted Alarm button.
*/
void WakeFromSuspendDlg::slotSelectedEventChanged()
{
    KAEvent event = mMainWindow->selectedEvent();
    mUi->useWakeButton->setEnabled(event.isValid() && !event.mainDateTime().isDateOnly());
    checkPendingAlarm();
}

/******************************************************************************
* Update the Show and Cancel buttons if the pending alarm status has changed.
* Reply = true if an alarm is still pending.
*/
bool WakeFromSuspendDlg::checkPendingAlarm()
{
    if (KAlarm::checkRtcWakeConfig().isEmpty())
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
        QStringList params = KAlarm::checkRtcWakeConfig();
        if (!params.isEmpty())
        {
#ifdef USE_AKONADI
            KAEvent* event = AlarmCalendar::resources()->event(params[0]);
            if (event)
            {
                mMainWindow->selectEvent(event->itemId());
                return;
            }
#else
            mMainWindow->selectEvent(params[0]);
            return;
#endif
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
    KAEvent event = mMainWindow->selectedEvent();
    if (event.isValid())
    {
        KDateTime dt = event.mainDateTime().kDateTime();
        if (dt.isDateOnly())
        {
            KMessageBox::sorry(this, i18nc("@info", "Cannot schedule wakeup time for a date-only alarm"));
            return;
        }
        if (KMessageBox::warningContinueCancel(this,
                    i18nc("@info", "<para>This wakeup will cancel any existing wakeup which has been set by KAlarm "
                                   "or any other application, because your computer can only schedule a single wakeup time.</para>"
                                   "<para><b>Note:</b> Wake From Suspend is not supported at all on some computers, especially older ones, "
                                   "and some computers only support setting a wakeup time up to 24 hours ahead. "
                                   "You may wish to set up a test alarm to check your system's capability.</para>"),
                    QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), QLatin1String("wakeupWarning"))
                != KMessageBox::Continue)
            return;
        int advance = mUi->advanceWakeTime->value();
        unsigned triggerTime = dt.addSecs(-advance * 60).toTime_t();
        if (KAlarm::setRtcWakeTime(triggerTime, this))
        {
            QStringList param;
            param << event.id() << QString::number(triggerTime);
            KConfigGroup config(KGlobal::config(), "General");
            config.writeEntry("RtcWake", param);
            Preferences::setWakeFromSuspendAdvance(advance);
            mUi->showWakeButton->setEnabled(true);
            mUi->cancelWakeButton->setEnabled(true);
        }
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
}

// vim: et sw=4:
