/*
 *  traywindow.h  -  the KDE system tray applet
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

#ifndef TRAYWINDOW_H
#define TRAYWINDOW_H

#include "editdlg.h"

#include <KAlarmCal/kaevent.h>

#include <kstatusnotifieritem.h>
#include <QIcon>

class QTimer;
class KToggleAction;
class MainWindow;
class NewAlarmAction;
#ifdef USE_AKONADI
class AlarmListModel;
#endif

using namespace KAlarmCal;

class TrayWindow : public KStatusNotifierItem
{
        Q_OBJECT
    public:
        explicit TrayWindow(MainWindow* parent);
        ~TrayWindow();
        void         removeWindow(MainWindow*);
        MainWindow*  assocMainWindow() const               { return mAssocMainWindow; }
        void         setAssocMainWindow(MainWindow* win)   { mAssocMainWindow = win; }

    signals:
        void         deleted();

    private slots:
        void         slotActivateRequested();
        void         slotSecondaryActivateRequested();
        void         slotNewAlarm(EditAlarmDlg::Type);
        void         slotNewFromTemplate(const KAEvent*);
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
        QIcon           mIconDisabled;
        KToggleAction*  mActionEnabled;
        NewAlarmAction* mActionNew;
#ifdef USE_AKONADI
        mutable AlarmListModel* mAlarmsModel; // active alarms sorted in time order
#endif
        QTimer*         mStatusUpdateTimer;
        QTimer*         mToolTipUpdateTimer;
        bool            mHaveDisabledAlarms;  // some individually disabled alarms exist
};

#endif // TRAYWINDOW_H

// vim: et sw=4:
