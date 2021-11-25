/*
 *  wakedlg.h  -  dialog to configure wake-from-suspend alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>

class QTimer;
class MainWindow;
class Ui_WakeFromSuspendDlgWidget;

class WakeFromSuspendDlg : public QDialog
{
    Q_OBJECT
public:
    static WakeFromSuspendDlg* create(QWidget* parent);
    ~WakeFromSuspendDlg() override;

private Q_SLOTS:
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

// vim: et sw=4:
