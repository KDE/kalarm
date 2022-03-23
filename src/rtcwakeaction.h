/*
 *  rtcwakeaction.h  -  KAuth helper application to execute rtcwake commands
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QObject>
#pragma once
#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>

using namespace KAuth;

class RtcWakeAction : public QObject
{
    Q_OBJECT
public:
    RtcWakeAction();

public Q_SLOTS:
    KAuth::ActionReply settimer(const QVariantMap& args);
};

// vim: et sw=4:
