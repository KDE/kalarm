/*
 *  rtcwakeaction.cpp  -  KAuth helper application to execute rtcwake commands
 *  Program:  kalarm
 *  Copyright © 2011,2018 by David Jarvie <djarvie@kde.org>
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

#include "rtcwakeaction.h"

#include <KLocalizedString>
#include <kauthactionreply.h>
#include "kalarm_debug.h"

#include <QProcess>
#include <QDateTime>

#include <stdio.h>

RtcWakeAction::RtcWakeAction()
{
    KLocalizedString::setApplicationDomain("kalarm");
}

ActionReply RtcWakeAction::settimer(const QVariantMap& args)
{
    unsigned t = args[QStringLiteral("time")].toUInt();
    qCDebug(KALARM_LOG) << "RtcWakeAction::settimer(" << t << ")";

    // Find the rtcwake executable
    QString exe(QStringLiteral("/usr/sbin/rtcwake"));   // default location
    FILE* wh = popen("whereis -b rtcwake", "r");
    if (wh)
    {
        char buff[512] = { '\0' };
        fgets(buff, sizeof(buff), wh);
        pclose(wh);
        // The string should be in the form "rtcwake: /path/rtcwake"
        char* start = strchr(buff, ':');
        if (start)
        {
            if (*++start == ' ')
                ++start;
            char* end = strpbrk(start, " \r\n");
            if (end)
                *end = 0;
            if (*start)
            {
                exe = QString::fromLocal8Bit(start);
                qCDebug(KALARM_LOG) << "RtcWakeAction::settimer:" << exe;
            }
        }
    }

    // Set the wakeup by executing the rtcwake command
    int result = -2;   // default = command not found
    QProcess proc;
    if (!exe.isEmpty())
    {
        // The wakeup time is set using a time from now ("-s") in preference to
        // an absolute time ("-t") so that if the hardware clock is not in sync
        // with the system clock, the alarm will still occur at the correct time.
        // The "-m no" option sets the wakeup time without suspending the computer.

        // If 't' is zero, the current wakeup is cancelled by setting a new wakeup
        // time 2 seconds from now, which will then expire.
        unsigned now = QDateTime::currentDateTimeUtc().toTime_t();
        proc.setProgram(exe);
        proc.setArguments({ QStringLiteral("-m"), QStringLiteral("no"), QStringLiteral("-s"), QString::number(t ? t - now : 2) });
        proc.start();
        proc.waitForStarted(5000); // allow a timeout of 5 seconds
        result = proc.exitCode();
    }
    QString errmsg;
    switch (result)
    {
        case 0:
            return ActionReply::SuccessReply();
        case -2:
            errmsg = xi18nc("@text/plain", "Could not run <command>%1</command> to set wake from suspend", QStringLiteral("rtcwake"));
            break;
        default:
            errmsg = xi18nc("@text/plain", "Error setting wake from suspend.<nl/>Command was: <command>%1 %2</command><nl/>Error code: %3.", proc.program(), proc.arguments().join(QStringLiteral(" ")), result);
            break;
    }
    ActionReply reply = ActionReply::HelperErrorReply(result);
    reply.setErrorDescription(errmsg);
    qCDebug(KALARM_LOG) << "RtcWakeAction::settimer: Code=" << reply.errorCode() << reply.errorDescription();
    return reply;
}

KAUTH_HELPER_MAIN("org.kde.kalarm.rtcwake", RtcWakeAction)

// vim: et sw=4:
