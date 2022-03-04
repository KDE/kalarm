/*
 *  rtcwakeaction.cpp  -  KAuth helper application to execute rtcwake commands
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rtcwakeaction.h"

#include "kalarm_debug.h"

#include <KLocalizedString>
#if KAUTH_VERSION >= QT_VERSION_CHECK(5, 92, 0)
#include <KAuth/ActionReply>
#else
#include <KAuthActionReply>
#endif

#include <QProcess>
#include <QDateTime>

#include <stdio.h>
#ifdef Q_OS_WIN
#define popen _popen
#define pclose _pclose
#endif

RtcWakeAction::RtcWakeAction()
{
    KLocalizedString::setApplicationDomain("kalarm");
}

ActionReply RtcWakeAction::settimer(const QVariantMap& args)
{
    qint64 t = args[QStringLiteral("time")].toLongLong();
    qCDebug(KALARM_LOG) << "RtcWakeAction::settimer(" << t << ")";

    // Find the rtcwake executable
    QString exe(QStringLiteral("/usr/sbin/rtcwake"));   // default location
    FILE* wh = popen("whereis -b rtcwake", "r");
    if (wh)
    {
        char buff[512] = { '\0' };
        bool ok = fgets(buff, sizeof(buff), wh);
        pclose(wh);
        if (ok)
        {
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
        qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
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
            errmsg = xi18nc("@text/plain", "Error setting wake from suspend.<nl/>Command was: <command>%1 %2</command><nl/>Error code: %3.", proc.program(), proc.arguments().join(QLatin1Char(' ')), result);
            break;
    }
    ActionReply reply = ActionReply::HelperErrorReply(result);
    reply.setErrorDescription(errmsg);
    qCDebug(KALARM_LOG) << "RtcWakeAction::settimer: Code=" << reply.errorCode() << reply.errorDescription();
    return reply;
}

KAUTH_HELPER_MAIN("org.kde.kalarm.rtcwake", RtcWakeAction)   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

// vim: et sw=4:
