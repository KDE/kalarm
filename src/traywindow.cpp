/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "traywindow.h"

#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagedisplay.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "resourcescalendar.h"
#include "resources/datamodel.h"
#include "resources/eventmodel.h"
#include "lib/synchtimer.h"
#include "kalarmcalendar/alarmtext.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KStandardAction>
#include <KAboutData>

#include <QList>
#include <QTimer>
#include <QLocale>
#include <QMenu>

#include <stdlib.h>
#include <limits.h>

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
    : KStatusNotifierItem(parent)
    , mAssocMainWindow(parent)
    , mStatusUpdateTimer(new QTimer(this))
{
    qCDebug(KALARM_LOG) << "TrayWindow:";
    setToolTipIconByName(QStringLiteral("kalarm"));
    setToolTipTitle(KAboutData::applicationData().displayName());
    setIconByName(QStringLiteral("kalarm"));
    setStatus(KStatusNotifierItem::Active);
    // Set up the context menu
    mActionEnabled = KAlarm::createAlarmEnableAction(this);
    addAction(QStringLiteral("tAlarmsEnable"), mActionEnabled);
    contextMenu()->addAction(mActionEnabled);
    connect(theApp(), &KAlarmApp::alarmEnabledToggled, this, &TrayWindow::setEnabledStatus);
    contextMenu()->addSeparator();

    mActionNew = new NewAlarmAction(false, i18nc("@action", "&New Alarm"), this);
    addAction(QStringLiteral("tNew"), mActionNew);
    contextMenu()->addAction(mActionNew);
    connect(mActionNew, &NewAlarmAction::selected, this, &TrayWindow::slotNewAlarm);
    connect(mActionNew, &NewAlarmAction::selectedTemplate, this, &TrayWindow::slotNewFromTemplate);
    contextMenu()->addSeparator();

    QAction* a = KAlarm::createStopPlayAction(this);
    addAction(QStringLiteral("tStopPlay"), a);
    contextMenu()->addAction(a);
    QObject::connect(theApp(), &KAlarmApp::audioPlaying, a, &QAction::setVisible);
    QObject::connect(theApp(), &KAlarmApp::audioPlaying, this, &TrayWindow::updateStatus);

    a = KAlarm::createSpreadWindowsAction(this);
    addAction(QStringLiteral("tSpread"), a);
    contextMenu()->addAction(a);
    contextMenu()->addSeparator();
    contextMenu()->addAction(KStandardAction::preferences(this, &TrayWindow::slotPreferences, this));

    // Disable standard quit behaviour. We have to intercept the quit event
    // (which triggers KStatusNotifierItem to quit unconditionally).
    QAction* act = action(QStringLiteral("quit"));
    if (act)
    {
        disconnect(act, &QAction::triggered, this, nullptr);
        connect(act, &QAction::triggered, this, &TrayWindow::slotQuit);
    }

    // Set icon to correspond with the alarms enabled menu status
    setEnabledStatus(theApp()->alarmsEnabled());

    connect(ResourcesCalendar::instance(), &ResourcesCalendar::haveDisabledAlarmsChanged, this, &TrayWindow::slotHaveDisabledAlarms);
    connect(this, &TrayWindow::activateRequested, this, &TrayWindow::slotActivateRequested);
    connect(this, &TrayWindow::secondaryActivateRequested, this, &TrayWindow::slotSecondaryActivateRequested);
    slotHaveDisabledAlarms(ResourcesCalendar::haveDisabledAlarms());

    // Hack: KSNI does not let us know when it is about to show the tooltip,
    // so we need to update it whenever something change in it.

    // This timer ensures that updateToolTip() is not called several times in a row
    mToolTipUpdateTimer = new QTimer(this);
    mToolTipUpdateTimer->setInterval(0);
    mToolTipUpdateTimer->setSingleShot(true);
    connect(mToolTipUpdateTimer, &QTimer::timeout, this, &TrayWindow::updateToolTip);

    // Update every minute to show accurate deadlines
    MinuteTimer::connect(mToolTipUpdateTimer, SLOT(start()));

    // Update when alarms are modified
    AlarmListModel* all = DataModel::allAlarmListModel();
    connect(all, &QAbstractItemModel::dataChanged,  mToolTipUpdateTimer, qOverload<>(&QTimer::start));
    connect(all, &QAbstractItemModel::rowsInserted, mToolTipUpdateTimer, qOverload<>(&QTimer::start));
    connect(all, &QAbstractItemModel::rowsMoved,    mToolTipUpdateTimer, qOverload<>(&QTimer::start));
    connect(all, &QAbstractItemModel::rowsRemoved,  mToolTipUpdateTimer, qOverload<>(&QTimer::start));
    connect(all, &QAbstractItemModel::modelReset,   mToolTipUpdateTimer, qOverload<>(&QTimer::start));

    // Set auto-hide status when next alarm or preferences change
    mStatusUpdateTimer->setSingleShot(true);
    connect(mStatusUpdateTimer, &QTimer::timeout, this, &TrayWindow::updateStatus);
    connect(ResourcesCalendar::instance(), &ResourcesCalendar::earliestAlarmChanged, this, &TrayWindow::updateStatus);
    Preferences::connect(&Preferences::autoHideSystemTrayChanged, this, &TrayWindow::updateStatus);
    updateStatus();

    // Update when tooltip preferences are modified
    Preferences::connect(&Preferences::tooltipPreferencesChanged, mToolTipUpdateTimer, qOverload<>(&QTimer::start));
}

TrayWindow::~TrayWindow()
{
    qCDebug(KALARM_LOG) << "~TrayWindow";
    theApp()->removeWindow(this);
    Q_EMIT deleted();
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
void TrayWindow::slotNewFromTemplate(const KAEvent& event)
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
    QTimer::singleShot(1, this, &TrayWindow::slotQuitAfter);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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
    qCDebug(KALARM_LOG) << "TrayWindow::setEnabledStatus:" << status;
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
    qCDebug(KALARM_LOG) << "TrayWindow::slotHaveDisabledAlarms:" << haveDisabled;
    mHaveDisabledAlarms = haveDisabled;
    updateIcon();
    updateToolTip();
}

/******************************************************************************
* Show the associated main window.
*/
void TrayWindow::showAssocMainWindow()
{
    if (mAssocMainWindow)
    {
        mAssocMainWindow->show();
        mAssocMainWindow->raise();
        mAssocMainWindow->activateWindow();
    }
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
    bool active = !period || MessageDisplay::isAudioPlaying();
    if (!active)
    {
        // Show the icon only if the next active alarm complies
        active = theApp()->alarmsEnabled();
        if (active)
        {
            KADateTime dt;
            const KAEvent& event = ResourcesCalendar::earliestAlarm(dt);
            active = event.isValid();
            if (active  &&  period > 0)
            {
                qint64 delay = KADateTime::currentLocalDateTime().secsTo(dt);
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
    setIconByName(!theApp()->alarmsEnabled() ? QStringLiteral("kalarm-disabled")
                  : mHaveDisabledAlarms ? QStringLiteral("kalarm-partdisabled")
                  : QStringLiteral("kalarm"));
}

/******************************************************************************
* Return the tooltip text showing alarms due in the next 24 hours.
* The limit of 24 hours is because only times, not dates, are displayed.
*/
QString TrayWindow::tooltipAlarmText() const
{
    const QString& prefix = Preferences::tooltipTimeToPrefix();
    int maxCount = Preferences::tooltipAlarmCount();
    const KADateTime now = KADateTime::currentLocalDateTime();
    const KADateTime tomorrow = now.addDays(1);

    // Get today's and tomorrow's alarms, sorted in time order
    int i, iend;
    QList<TipItem> items;    //clazy:exclude=inefficient-qlist,inefficient-qlist-soft   QList is better than QVector for insertions
    QVector<KAEvent> events = KAlarm::getSortedActiveEvents(const_cast<TrayWindow*>(this), &mAlarmsModel);
    for (i = 0, iend = events.count();  i < iend;  ++i)
    {
        KAEvent* event = &events[i];
        if (event->actionSubType() == KAEvent::SubAction::Message)
        {
            TipItem item;
            QDateTime dateTime = event->nextTrigger(KAEvent::Trigger::Display).effectiveKDateTime().toLocalZone().qDateTime();
            if (dateTime > tomorrow.qDateTime())
                break;   // ignore alarms after tomorrow at the current clock time
            item.dateTime = dateTime;

            // The alarm is due today, or early tomorrow
            if (Preferences::showTooltipAlarmTime())
            {
                item.text += QLocale().toString(item.dateTime.time(), QLocale::ShortFormat);
                item.text += QLatin1Char(' ');
            }
            if (Preferences::showTooltipTimeToAlarm())
            {
                int mins = (now.qDateTime().secsTo(item.dateTime) + 59) / 60;
                if (mins < 0)
                    mins = 0;
                char minutes[3] = "00";
                minutes[0] = static_cast<char>((mins%60) / 10 + '0');
                minutes[1] = static_cast<char>((mins%60) % 10 + '0');
                if (Preferences::showTooltipAlarmTime())
                    item.text += i18nc("@info prefix + hours:minutes", "(%1%2:%3)", prefix, mins/60, QLatin1String(minutes));
                else
                    item.text += i18nc("@info prefix + hours:minutes", "%1%2:%3", prefix, mins/60, QLatin1String(minutes));
                item.text += QLatin1Char(' ');
            }
            item.text += AlarmText::summary(*event);

            // Insert the item into the list in time-sorted order
            int it = 0;
            for (int itend = items.count();  it < itend;  ++it)
            {
                if (item.dateTime <= items.at(it).dateTime)
                    break;
            }
            items.insert(it, item);
        }
    }
    qCDebug(KALARM_LOG) << "TrayWindow::tooltipAlarmText";
    QString text;
    int count = 0;
    for (i = 0, iend = items.count();  i < iend;  ++i)
    {
        qCDebug(KALARM_LOG) << "TrayWindow::tooltipAlarmText: --" << (count+1) << ")" << items.at(i).text;
        if (i > 0)
            text += QLatin1String("<br />");
        text += items.at(i).text;
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
        mAssocMainWindow = nullptr;
}

// vim: et sw=4:
