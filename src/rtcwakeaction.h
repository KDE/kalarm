/*
 *  rtcwakeaction.h  -  KAuth helper application to execute rtcwake commands
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RTCWAKEACTION_H
#define RTCWAKEACTION_H

#include <KAuth>

using namespace KAuth;

class RtcWakeAction : public QObject
{
        Q_OBJECT
    public:
        RtcWakeAction();

    public Q_SLOTS:
        ActionReply settimer(const QVariantMap& args);
};

#endif // RTCWAKEACTION_H

// vim: et sw=4:
