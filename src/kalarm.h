/*
 *  kalarm.h  -  global header file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KALARM_H
#define KALARM_H

#include "config-kalarm.h"
#if FILE_RESOURCES
#define VERSION_SUFFIX ""
#else
#define VERSION_SUFFIX "-A"
#endif
#define KALARM_VERSION       VERSION VERSION_SUFFIX
#define KALARM_FULL_VERSION  KALARM_VERSION " (KDE Apps " RELEASE_SERVICE_VERSION ")"

#define KALARM_NAME "KAlarm"
#define KALARM_DBUS_SERVICE  "org.kde.kalarm"  // D-Bus service name of KAlarm application

#endif // KALARM_H

