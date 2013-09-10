/*
 *  rtcwakeaction.cpp  -  KAuth helper application to execute rtcwake commands
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

#include "rtcwakeaction.h"

#include <kglobal.h>
#include <klocale.h>
#include <kprocess.h>
#include <kdatetime.h>

#include <QDebug>

#include <stdio.h>

RtcWakeAction::RtcWakeAction()
{
    KGlobal::locale()->insertCatalog(QLatin1String("kalarm"));
}

ActionReply RtcWakeAction::settimer(const QVariantMap& args)
{
    unsigned t = args[QLatin1String("time")].toUInt();
    qDebug() << "RtcWakeAction::settimer(" << t << ")";

    // Find the rtcwake executable
    QString exe(QLatin1String("/usr/sbin/rtcwake"));   // default location
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
                qDebug() << "RtcWakeAction::settimer:" << exe;
            }
        }
    }

    // Set the wakeup by executing the rtcwake command
    int result = -2;   // default = command not found
    KProcess proc;
    if (!exe.isEmpty())
    {
        // The wakeup time is set using a time from now ("-s") in preference to
        // an absolute time ("-t") so that if the hardware clock is not in sync
        // with the system clock, the alarm will still occur at the correct time.
        // The "-m no" option sets the wakeup time without suspending the computer.

        // If 't' is zero, the current wakeup is cancelled by setting a new wakeup
        // time 2 seconds from now, which will then expire.
        unsigned now = KDateTime::currentUtcDateTime().toTime_t();
        proc << exe << QLatin1String("-m") << QLatin1String("no") << QLatin1String("-s") << QString::number(t ? t - now : 2);
        result = proc.execute(5000);   // allow a timeout of 5 seconds
    }
    QString errmsg;
    switch (result)
    {
        case 0:
            return ActionReply::SuccessReply;
        case -2:
            errmsg = i18nc("@text/plain", "Could not run <command>%1</command> to set wake from suspend", QLatin1String("rtcwake"));
            break;
        default:
            errmsg = i18nc("@text/plain", "Error setting wake from suspend.<nl/>Command was: <command>%1</command><nl/>Error code: %2.", proc.program().join(QLatin1String(" ")), result);
            break;
    }
    ActionReply reply(ActionReply::HelperError);
    reply.setErrorCode(result);
    reply.setErrorDescription(errmsg);
    qDebug() << "RtcWakeAction::settimer: Code=" << reply.errorCode() << reply.errorDescription();
    return reply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kalarmrtcwake", RtcWakeAction)

// vim: et sw=4:
