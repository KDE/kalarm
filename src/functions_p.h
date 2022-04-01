/*
 *  functions_p.h  -  private declarations for miscellaneous functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "config-kalarm.h"

#include <QObject>

class EditAlarmDlg;

namespace KAlarm
{

// Private class which exists solely to allow signals/slots to work.
class Private : public QObject
{
    Q_OBJECT
public:
    explicit Private(QObject* parent = nullptr) : QObject(parent), mMsgParent(nullptr) {}
    static Private* instance()
    {
        if (!mInstance)
            mInstance = new Private;
        return mInstance;
    }

    QWidget* mMsgParent;

#if ENABLE_WAKE_FROM_SUSPEND
public Q_SLOTS:
    void cancelRtcWake();
#endif

private:
    static Private* mInstance;
};

// Private class to handle Edit New Alarm dialog OK button.
class PrivateNewAlarmDlg : public QObject
{
    Q_OBJECT
public:
    PrivateNewAlarmDlg() = default;
    explicit PrivateNewAlarmDlg(EditAlarmDlg*);
    void accept(EditAlarmDlg*);

private Q_SLOTS:
    void okClicked();
    void cancelClicked();
};

} // namespace KAlarm

// vim: et sw=4:
