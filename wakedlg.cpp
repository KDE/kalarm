/*
 *  wakedlg.cpp  -  dialog to configure wake-from-suspend alarms
 *  Program:  kalarm
 *  Copyright © 2011-2012 by David Jarvie <djarvie@kde.org>
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
#include "wakedlg.h"
#include "ui_wakedlg.h"

#include "alarmcalendar.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"

#include <kalarmcal/kaevent.h>

#include <kglobal.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include <kdebug.h>

#include <QTimer>

using namespace KAlarmCal;

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
    setAttribute(Qt::WA_DeleteOnClose);
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
    enableDisableUseButton();

    // Update the Show and Cancel button status every 5 seconds
    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), SLOT(checkPendingAlarm()));
    mTimer->start(5000);

    connect(mMainWindow, SIGNAL(selectionChanged()), SLOT(enableDisableUseButton()));
    connect(mUi->showWakeButton, SIGNAL(clicked()), SLOT(showWakeClicked()));
    connect(mUi->useWakeButton, SIGNAL(clicked()), SLOT(useWakeClicked()));
    connect(mUi->cancelWakeButton, SIGNAL(clicked()), SLOT(cancelWakeClicked()));

    connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), SLOT(enableDisableUseButton()));
}

WakeFromSuspendDlg::~WakeFromSuspendDlg()
{
    if (mInstance == this)
        mInstance = 0;
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
        QString wakeFromSuspendId = KAlarm::checkRtcWakeConfig().value(0);
#ifdef USE_AKONADI
        const KAEvent event = mMainWindow->selectedEvent();
        enable = event.isValid()
              && event.category() == CalEvent::ACTIVE
              && event.enabled()
              && !event.mainDateTime().isDateOnly()
              && event.id() != wakeFromSuspendId;
#else
        const KAEvent* event = mMainWindow->selectedEvent();
        enable = event && event->isValid()
              && event->category() == CalEvent::ACTIVE
              && event->enabled()
              && !event->mainDateTime().isDateOnly()
              && event->id() != wakeFromSuspendId;
#endif
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
        QStringList params = KAlarm::checkRtcWakeConfig();
        if (!params.isEmpty())
        {
#ifdef USE_AKONADI
            KAEvent* event = AlarmCalendar::resources()->event(EventId(params[0].toLongLong(), params[1]));
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
#ifdef USE_AKONADI
    KAEvent event = mMainWindow->selectedEvent();
    if (!event.isValid())
        return;
    KDateTime dt = event.mainDateTime().kDateTime();
#else
    KAEvent* event = mMainWindow->selectedEvent();
    if (!event)
        return;
    KDateTime dt = event->mainDateTime().kDateTime();
#endif
    if (dt.isDateOnly())
    {
        KAMessageBox::sorry(this, i18nc("@info", "Cannot schedule wakeup time for a date-only alarm"));
        return;
    }
    if (KAMessageBox::warningContinueCancel(this,
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
#ifdef USE_AKONADI
        param << QString::number(event.collectionId()) << event.id() << QString::number(triggerTime);
#else
        param << event->id() << QString::number(triggerTime);
#endif
        KConfigGroup config(KGlobal::config(), "General");
        config.writeEntry("RtcWake", param);
        config.sync();
        Preferences::setWakeFromSuspendAdvance(advance);
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
#include "moc_wakedlg.h"
// vim: et sw=4:
