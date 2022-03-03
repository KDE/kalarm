/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "editdlg.h"
#include "kalarmcalendar/kaevent.h"

#include <KStatusNotifierItem>

class QTimer;
class KToggleAction;
class MainWindow;
class NewAlarmAction;
class AlarmListModel;

using namespace KAlarmCal;

class TrayWindow : public KStatusNotifierItem
{
    Q_OBJECT
public:
    explicit TrayWindow(MainWindow* parent);
    ~TrayWindow() override;
    void         removeWindow(MainWindow*);
    MainWindow*  assocMainWindow() const               { return mAssocMainWindow; }
    void         setAssocMainWindow(MainWindow* win)   { mAssocMainWindow = win; }
    void         showAssocMainWindow();

Q_SIGNALS:
    void         deleted();

private Q_SLOTS:
    void         slotActivateRequested();
    void         slotSecondaryActivateRequested();
    void         slotNewAlarm(EditAlarmDlg::Type);
    void         slotNewFromTemplate(const KAEvent&);
    void         slotPreferences();
    void         setEnabledStatus(bool status);
    void         slotHaveDisabledAlarms(bool disabled);
    void         slotQuit();
    void         slotQuitAfter();
    void         updateStatus();
    void         updateToolTip();

private:
    QString      tooltipAlarmText() const;
    void         updateIcon();

    MainWindow*     mAssocMainWindow;     // main window associated with this, or null
    KToggleAction*  mActionEnabled;
    NewAlarmAction* mActionNew;
    mutable AlarmListModel* mAlarmsModel {nullptr}; // active alarms sorted in time order
    QTimer*         mStatusUpdateTimer;
    QTimer*         mToolTipUpdateTimer;
    bool            mHaveDisabledAlarms {false};  // some individually disabled alarms exist
};

// vim: et sw=4:
