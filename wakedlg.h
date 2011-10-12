/*
 *  wakedlg.h  -  dialog to configure wake-from-suspend alarms
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

#ifndef WAKEDLG_H
#define WAKEDLG_H

#include <kdialog.h>

class QTimer;
class MainWindow;
class Ui_WakeFromSuspendDlgWidget;
namespace KAlarm { class KAEvent; }

class WakeFromSuspendDlg : public KDialog
{
        Q_OBJECT
    public:
        static WakeFromSuspendDlg* create(QWidget* parent);
        ~WakeFromSuspendDlg();

    private slots:
        void enableDisableUseButton();
        void showWakeClicked();
        void useWakeClicked();
        void cancelWakeClicked();
        bool checkPendingAlarm();

    private:
        explicit WakeFromSuspendDlg(QWidget* parent);

        static WakeFromSuspendDlg* mInstance;   // the one and only instance of the dialog
        Ui_WakeFromSuspendDlgWidget* mUi;
        MainWindow* mMainWindow;
        QTimer*     mTimer;
};

#endif // WAKEDLG_H

// vim: et sw=4:
