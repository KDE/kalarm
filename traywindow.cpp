/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2012 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "traywindow.h"

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "synchtimer.h"
#include "templatemenuaction.h"

#include <kalarmcal/alarmtext.h>

#include <kactioncollection.h>
#include <ktoggleaction.h>
#include <kapplication.h>
#include <klocale.h>
#include <K4AboutData>
#include <QMenu>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstandardaction.h>
#include <kstandardguiitem.h>
#include <kiconeffect.h>
#include <kconfig.h>
#include <qdebug.h>
#include <KGlobal>
#include <KIconLoader>

#include <QList>
#include <QTimer>

#include <stdlib.h>
#include <limits.h>
#include <KLocale>

using namespace KAlarmCal;

struct TipItem
{
    QDateTime  dateTime;
    QString    text;
};


/*=============================================================================
= Class: TrayWindow
= The KDE system tray window.
=============================================================================*/

TrayWindow::TrayWindow(MainWindow* parent)
    : KStatusNotifierItem(parent),
      mAssocMainWindow(parent),
      mAlarmsModel(0),
      mStatusUpdateTimer(new QTimer(this)),
      mHaveDisabledAlarms(false)
{
    qDebug();
    setToolTipIconByName(QLatin1String("kalarm"));
    setToolTipTitle(KGlobal::mainComponent().aboutData()->programName());
    setIconByName(QLatin1String("kalarm"));
    // Load the disabled icon for use by setIconByPixmap()
    // - setIconByName() doesn't work for this one!
    mIconDisabled.addPixmap(KIconLoader::global()->loadIcon(QLatin1String("kalarm-disabled"), KIconLoader::Panel));
    setStatus(KStatusNotifierItem::Active);
#if 0 //QT5
    // Set up the context menu
    QList<QAction *> actions = actionCollection();
    mActionEnabled = KAlarm::createAlarmEnableAction(this);
    actions.addAction(QLatin1String("tAlarmsEnable"), mActionEnabled);
    contextMenu()->addAction(mActionEnabled);
    connect(theApp(), SIGNAL(alarmEnabledToggled(bool)), SLOT(setEnabledStatus(bool)));
    contextMenu()->addSeparator();

    mActionNew = new NewAlarmAction(false, i18nc("@action", "&New Alarm"), this);
    actions->addAction(QLatin1String("tNew"), mActionNew);
    contextMenu()->addAction(mActionNew);
    connect(mActionNew, SIGNAL(selected(EditAlarmDlg::Type)), SLOT(slotNewAlarm(EditAlarmDlg::Type)));
    connect(mActionNew->fromTemplateAlarmAction(), SIGNAL(selected(const KAEvent*)), SLOT(slotNewFromTemplate(const KAEvent*)));
    contextMenu()->addSeparator();

    QAction * a = KAlarm::createStopPlayAction(this);
    actions->addAction(QLatin1String("tStopPlay"), a);
    contextMenu()->addAction(a);
    QObject::connect(theApp(), SIGNAL(audioPlaying(bool)), a, SLOT(setVisible(bool)));
    QObject::connect(theApp(), SIGNAL(audioPlaying(bool)), SLOT(updateStatus()));

    a = KAlarm::createSpreadWindowsAction(this);
    actions->addAction(QLatin1String("tSpread"), a);
    contextMenu()->addAction(a);
    contextMenu()->addSeparator();
    contextMenu()->addAction(KStandardAction::preferences(this, SLOT(slotPreferences()), actions));

    // Replace the default handler for the Quit context menu item
    const char* quitName = KStandardAction::name(KStandardAction::Quit);
    QAction* qa = actions->action(QLatin1String(quitName));
    disconnect(qa, SIGNAL(triggered(bool)), 0, 0);
    connect(qa, SIGNAL(triggered(bool)), SLOT(slotQuit()));

    // Set icon to correspond with the alarms enabled menu status
    setEnabledStatus(theApp()->alarmsEnabled());

    connect(AlarmCalendar::resources(), SIGNAL(haveDisabledAlarmsChanged(bool)), SLOT(slotHaveDisabledAlarms(bool)));
    connect(this, SIGNAL(activateRequested(bool,QPoint)), SLOT(slotActivateRequested()));
    connect(this, SIGNAL(secondaryActivateRequested(QPoint)), SLOT(slotSecondaryActivateRequested()));
    slotHaveDisabledAlarms(AlarmCalendar::resources()->haveDisabledAlarms());

    // Hack: KSNI does not let us know when it is about to show the tooltip,
    // so we need to update it whenever something change in it.

    // This timer ensures that updateToolTip() is not called several times in a row
    mToolTipUpdateTimer = new QTimer(this);
    mToolTipUpdateTimer->setInterval(0);
    mToolTipUpdateTimer->setSingleShot(true);
    connect(mToolTipUpdateTimer, SIGNAL(timeout()), SLOT(updateToolTip()));

    // Update every minute to show accurate deadlines
    MinuteTimer::connect(mToolTipUpdateTimer, SLOT(start()));

    // Update when alarms are modified
    connect(AlarmListModel::all(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            mToolTipUpdateTimer, SLOT(start()));
    connect(AlarmListModel::all(), SIGNAL(rowsInserted(QModelIndex,int,int)),
            mToolTipUpdateTimer, SLOT(start()));
    connect(AlarmListModel::all(), SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            mToolTipUpdateTimer, SLOT(start()));
    connect(AlarmListModel::all(), SIGNAL(rowsRemoved(QModelIndex,int,int)),
            mToolTipUpdateTimer, SLOT(start()));
    connect(AlarmListModel::all(), SIGNAL(modelReset()),
            mToolTipUpdateTimer, SLOT(start()));

    // Set auto-hide status when next alarm or preferences change
    mStatusUpdateTimer->setSingleShot(true);
    connect(mStatusUpdateTimer, SIGNAL(timeout()), SLOT(updateStatus()));
    connect(AlarmCalendar::resources(), SIGNAL(earliestAlarmChanged()), SLOT(updateStatus()));
    Preferences::connect(SIGNAL(autoHideSystemTrayChanged(int)), this, SLOT(updateStatus()));
    updateStatus();

    // Update when tooltip preferences are modified
    Preferences::connect(SIGNAL(tooltipPreferencesChanged()), mToolTipUpdateTimer, SLOT(start()));
#endif
}

TrayWindow::~TrayWindow()
{
    qDebug();
    theApp()->removeWindow(this);
    emit deleted();
}

/******************************************************************************
* Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void TrayWindow::slotNewAlarm(EditAlarmDlg::Type type)
{
    KAlarm::editNewAlarm(type);
}

/******************************************************************************
* Called when the "New Alarm" menu item is selected to edit a new alarm from a
* template.
*/
void TrayWindow::slotNewFromTemplate(const KAEvent* event)
{
    KAlarm::editNewAlarm(event);
}

/******************************************************************************
* Called when the "Configure KAlarm" menu item is selected.
*/
void TrayWindow::slotPreferences()
{
    KAlarmPrefDlg::display();
}

/******************************************************************************
* Called when the Quit context menu item is selected.
* Note that KAlarmApp::doQuit()  must be called by the event loop, not directly
* from the menu item, since otherwise the tray icon will be deleted while still
* processing the menu, resulting in a crash.
* Ideally, the connect() call setting up this slot in the constructor would use
* Qt::QueuedConnection, but the slot is never called in that case.
*/
void TrayWindow::slotQuit()
{
    // Note: QTimer::singleShot(0, ...) never calls the slot.
    QTimer::singleShot(1, this, SLOT(slotQuitAfter()));
}
void TrayWindow::slotQuitAfter()
{
    theApp()->doQuit(static_cast<QWidget*>(parent()));
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
    qDebug() << (int)status;
    updateIcon();
    updateStatus();
    updateToolTip();
}

/******************************************************************************
* Called when individual alarms are enabled or disabled.
* Set the enabled icon to show or hide a disabled indication.
*/
void TrayWindow::slotHaveDisabledAlarms(bool haveDisabled)
{
    qDebug() << haveDisabled;
    mHaveDisabledAlarms = haveDisabled;
    updateIcon();
    updateToolTip();
}

/******************************************************************************
* A left click displays the KAlarm main window.
*/
void TrayWindow::slotActivateRequested()
{
    // Left click: display/hide the first main window
    if (mAssocMainWindow  &&  mAssocMainWindow->isVisible())
    {
        mAssocMainWindow->raise();
        mAssocMainWindow->activateWindow();
    }
}

/******************************************************************************
* A middle button click displays the New Alarm window.
*/
void TrayWindow::slotSecondaryActivateRequested()
{
    if (mActionNew->isEnabled())
        mActionNew->trigger();    // display a New Alarm dialog
}

/******************************************************************************
* Adjust icon auto-hide status according to when the next alarm is due.
* The icon is always shown if audio is playing, to give access to the 'stop'
* menu option.
*/
void TrayWindow::updateStatus()
{
    mStatusUpdateTimer->stop();
    int period =  Preferences::autoHideSystemTray();
    // If the icon is always to be shown (AutoHideSystemTray = 0),
    // or audio is playing, show the icon.
    bool active = !period || MessageWin::isAudioPlaying();
    if (!active)
    {
        // Show the icon only if the next active alarm complies
        active = theApp()->alarmsEnabled();
        if (active)
        {
            KAEvent* event = AlarmCalendar::resources()->earliestAlarm();
            active = static_cast<bool>(event);
            if (event  &&  period > 0)
            {
                KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
                qint64 delay = KDateTime::currentLocalDateTime().secsTo_long(dt);
                delay -= static_cast<qint64>(period) * 60;   // delay until icon to be shown
                active = (delay <= 0);
                if (!active)
                {
                    // First alarm trigger is too far in future, so tray icon is to
                    // be auto-hidden. Set timer for when it should be shown again.
                    delay *= 1000;   // convert to msec
                    int delay_int = static_cast<int>(delay);
                    if (delay_int != delay)
                        delay_int = INT_MAX;
                    mStatusUpdateTimer->setInterval(delay_int);
                    mStatusUpdateTimer->start();
                }
            }
        }
    }
    setStatus(active ? Active : Passive);
}

/******************************************************************************
* Adjust tooltip according to the app state.
* The tooltip text shows alarms due in the next 24 hours. The limit of 24
* hours is because only times, not dates, are displayed.
*/
void TrayWindow::updateToolTip()
{
    bool enabled = theApp()->alarmsEnabled();
    QString subTitle;
    if (enabled && Preferences::tooltipAlarmCount())
        subTitle = tooltipAlarmText();

    if (!enabled)
        subTitle = i18n("Disabled");
    else if (mHaveDisabledAlarms)
    {
        if (!subTitle.isEmpty())
            subTitle += QLatin1String("<br/>");
        subTitle += i18nc("@info:tooltip Brief: some alarms are disabled", "(Some alarms disabled)");
    }
    setToolTipSubTitle(subTitle);
}

/******************************************************************************
* Adjust icon according to the app state.
*/
void TrayWindow::updateIcon()
{
    if (theApp()->alarmsEnabled())
        setIconByName(mHaveDisabledAlarms ? QLatin1String("kalarm-partdisabled") : QLatin1String("kalarm"));
    else
        setIconByPixmap(mIconDisabled);
}

/******************************************************************************
* Return the tooltip text showing alarms due in the next 24 hours.
* The limit of 24 hours is because only times, not dates, are displayed.
*/
QString TrayWindow::tooltipAlarmText() const
{
    KAEvent event;
    const QString& prefix = Preferences::tooltipTimeToPrefix();
    int maxCount = Preferences::tooltipAlarmCount();
    KDateTime now = KDateTime::currentLocalDateTime();
    KDateTime tomorrow = now.addDays(1);

    // Get today's and tomorrow's alarms, sorted in time order
    int i, iend;
    QList<TipItem> items;
    QVector<KAEvent> events = KAlarm::getSortedActiveEvents(const_cast<TrayWindow*>(this), &mAlarmsModel);
    for (i = 0, iend = events.count();  i < iend;  ++i)
    {
        KAEvent* event = &events[i];
        if (event->actionSubType() == KAEvent::MESSAGE)
        {
            TipItem item;
            QDateTime dateTime = event->nextTrigger(KAEvent::DISPLAY_TRIGGER).effectiveKDateTime().toLocalZone().dateTime();
            if (dateTime > tomorrow.dateTime())
                break;   // ignore alarms after tomorrow at the current clock time
            item.dateTime = dateTime;

            // The alarm is due today, or early tomorrow
            if (Preferences::showTooltipAlarmTime())
            {
                item.text += KLocale::global()->formatTime(item.dateTime.time());
                item.text += QLatin1Char(' ');
            }
            if (Preferences::showTooltipTimeToAlarm())
            {
                int mins = (now.dateTime().secsTo(item.dateTime) + 59) / 60;
                if (mins < 0)
                    mins = 0;
                char minutes[3] = "00";
                minutes[0] = static_cast<char>((mins%60) / 10 + '0');
                minutes[1] = static_cast<char>((mins%60) % 10 + '0');
                if (Preferences::showTooltipAlarmTime())
                    item.text += i18nc("@info/plain prefix + hours:minutes", "(%1%2:%3)", prefix, mins/60, QLatin1String(minutes));
                else
                    item.text += i18nc("@info/plain prefix + hours:minutes", "%1%2:%3", prefix, mins/60, QLatin1String(minutes));
                item.text += QLatin1Char(' ');
            }
            item.text += AlarmText::summary(*event);

            // Insert the item into the list in time-sorted order
            int it = 0;
            for (int itend = items.count();  it < itend;  ++it)
            {
                if (item.dateTime <= items[it].dateTime)
                    break;
            }
            items.insert(it, item);
        }
    }
    qDebug();
    QString text;
    int count = 0;
    for (i = 0, iend = items.count();  i < iend;  ++i)
    {
        qDebug() << "--" << (count+1) << ")" << items[i].text;
        if (i > 0)
            text += QLatin1String("<br />");
        text += items[i].text;
        if (++count == maxCount)
            break;
    }
    return text;
}

/******************************************************************************
* Called when the associated main window is closed.
*/
void TrayWindow::removeWindow(MainWindow* win)
{
    if (win == mAssocMainWindow)
        mAssocMainWindow = 0;
}
#include "moc_traywindow.cpp"
// vim: et sw=4:
