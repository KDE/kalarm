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

#include <klocale.h>
#include <kprocess.h>
#include <kdatetime.h>

#include <QDebug>

ActionReply RtcWakeAction::settimer(const QVariantMap& args)
{
    unsigned t = args["time"].toUInt();
    qDebug() << "RtcWakeAction::settimer(" << t << ")";
    if (!t)
    {
        // Cancel the current wakeup. This is done by setting a new wakeup
        // time 5 seconds from now, which will then expire.
        t = KDateTime::currentUtcDateTime().toTime_t() + 5;
    }
    int result = -2;   // default = command not found
    KProcess proc;
    QString exe("/usr/sbin/rtcwake");
    if (!exe.isEmpty())
    {
        // The "-m no" option sets the wakeup time without suspending the computer.
        proc << exe << "-m" << "no" << "-t" << QString::number(t);
        result = proc.execute(5000);   // allow a timeout of 5 seconds
    }
    QString errmsg;
    switch (result)
    {
        case 0:
            return ActionReply::SuccessReply;
        case -2:
            errmsg = i18nc("@text/plain", "Could not run <command>%1</command> to set wake from suspend", "rtcwake");
            break;
        default:
            errmsg = i18nc("@text/plain", "Error setting wake from suspend.<nl/>Command was: <command>%1</command><nl/>Error code: %2.", proc.program().join(" "), result);
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
