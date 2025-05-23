/*
 *  shellprocess.cpp  -  execute a shell process
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shellprocess.h"

#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KAuthorized>

#include <qplatformdefs.h>

#include <stdlib.h>


QByteArray ShellProcess::mShellName;
QByteArray ShellProcess::mShellPath;
bool       ShellProcess::mInitialised = false;
bool       ShellProcess::mAuthorised  = false;


ShellProcess::ShellProcess(const QString& command)
    : mCommand(command)
{
}

/******************************************************************************
* Execute a command.
*/
bool ShellProcess::start(OpenMode openMode)
{
    if (!authorised())
    {
        mStatus = Status::Unauthorised;
        return false;
    }
    connect(this, &QIODevice::bytesWritten, this, &ShellProcess::writtenStdin);
    connect(this, &QProcess::finished, this, &ShellProcess::slotExited);
    connect(this, &QProcess::readyReadStandardOutput, this, &ShellProcess::stdoutReady);
    connect(this, &QProcess::readyReadStandardError, this, &ShellProcess::stderrReady);
    const QStringList args{ QStringLiteral("-c"), mCommand };
    QProcess::start(QLatin1StringView(shellPath()), args, openMode);
    if (!waitForStarted())
    {
        mStatus = Status::StartFail;
        return false;
    }
    mStatus = Status::Running;
    return true;
}

/******************************************************************************
* Called when a shell process execution completes.
* Interprets the exit status according to which shell was called, and emits
* a shellExited() signal.
*/
void ShellProcess::slotExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KALARM_LOG) << "ShellProcess::slotExited:" << exitCode << "," << exitStatus;
    mStdinQueue.clear();
    mStatus = Status::Success;
    mExitCode = exitCode;
    if (exitStatus != NormalExit)
    {
        qCWarning(KALARM_LOG) << "ShellProcess::slotExited:" << mCommand << ":" << mShellName << ": crashed/killed";
        mStatus = Status::Died;
    }
    else
    {
        // Some shells report if the command couldn't be found, or is not executable
        if (((mShellName == "bash" || mShellName == "zsh")  &&  (exitCode == 126 || exitCode == 127))
        ||  (mShellName == "ksh"  &&  exitCode == 127))
        {
            qCWarning(KALARM_LOG) << "ShellProcess::slotExited:" << mCommand << ":" << mShellName << ": not found or not executable";
            mStatus = Status::NotFound;
        }
    }
    Q_EMIT shellExited(this);
}

/******************************************************************************
* Write a string to STDIN.
*/
void ShellProcess::writeStdin(const char* buffer, int bufflen)
{
    QByteArray scopy(buffer, bufflen);    // construct a deep copy
    bool doWrite = mStdinQueue.isEmpty();
    mStdinQueue.enqueue(scopy);
    if (doWrite)
    {
        mStdinBytes = std::as_const(mStdinQueue).head().length();
        write(std::as_const(mStdinQueue).head());
    }
}

/******************************************************************************
* Called when output to STDIN completes.
* Send the next queued output, if any.
* Note that buffers written to STDIN must not be freed until the bytesWritten()
* signal has been processed.
*/
void ShellProcess::writtenStdin(qint64 bytes)
{
    mStdinBytes -= bytes;
    if (mStdinBytes > 0)
        return;   // buffer has only been partially written so far
    if (!mStdinQueue.isEmpty())
        mStdinQueue.dequeue();   // free the buffer which has now been written
    if (!mStdinQueue.isEmpty())
    {
        mStdinBytes = std::as_const(mStdinQueue).head().length();
        write(std::as_const(mStdinQueue).head());
    }
    else if (mStdinExit)
        kill();
}

/******************************************************************************
* Tell the process to exit once all STDIN strings have been written.
*/
void ShellProcess::stdinExit()
{
    if (mStdinQueue.isEmpty())
        kill();
    else
        mStdinExit = true;
}

/******************************************************************************
* Return the error message corresponding to the command exit status.
* Reply = null string if not yet exited, or if command successful.
*/
QString ShellProcess::errorMessage() const
{
    switch (mStatus)
    {
        case Status::Unauthorised:
            return i18nc("@info", "Failed to execute command (shell access not authorized)");
        case Status::StartFail:
            return i18nc("@info", "Failed to execute command");
        case Status::NotFound:
            return i18nc("@info", "Failed to execute command (not found or not executable)");
        case Status::Died:
            return i18nc("@info", "Command execution error");
        case Status::Success:
            if (mExitCode)
                return i18nc("@info", "Command exit code: %1", mExitCode);
            // Fall through to Inactive
            [[fallthrough]];
        case Status::Inactive:
        case Status::Running:
        default:
            return {};
    }
}

/******************************************************************************
* Determine which shell to use.
* Don't use the KProcess default shell, since we need to know which shell is
* used in order to decide what its exit code means.
*/
const QByteArray& ShellProcess::shellPath()
{
    if (mShellPath.isEmpty())
    {
        // Get the path to the shell
        mShellPath = "/bin/sh";
        QByteArray envshell = qgetenv("SHELL").trimmed();
        if (!envshell.isEmpty())
        {
            // TODO Disable for windows. Perhaps all class must be disabled
#if !defined(Q_OS_WIN)
            QT_STATBUF fileinfo;
            if (QT_STAT(envshell.data(), &fileinfo) != -1  // ensure file exists
            &&  !S_ISDIR(fileinfo.st_mode)              // and it's not a directory
            &&  !S_ISCHR(fileinfo.st_mode)              // and it's not a character device
            &&  !S_ISBLK(fileinfo.st_mode)              // and it's not a block device
#ifdef S_ISSOCK
            &&  !S_ISSOCK(fileinfo.st_mode)             // and it's not a socket
#endif
            &&  !S_ISFIFO(fileinfo.st_mode)             // and it's not a fifo
            &&  !access(envshell.data(), X_OK))         // and it's executable
                mShellPath = envshell;
#endif
        }

        // Get the shell filename with the path stripped off
        int i = mShellPath.lastIndexOf('/');
        if (i >= 0)
            mShellName = mShellPath.mid(i + 1);
        else
            mShellName = mShellPath;
    }
    return mShellPath;
}

/******************************************************************************
* Check whether shell commands are allowed at all.
*/
bool ShellProcess::authorised()
{
    if (!mInitialised)
    {
        mAuthorised = KAuthorized::authorize(QStringLiteral("shell_access"));
        mInitialised = true;
    }
    return mAuthorised;
}

/******************************************************************************
* Splits a command line at the first non-escaped space character.
*/
QString ShellProcess::splitCommandLine(QString& cmdline)
{
    if (cmdline.isEmpty())
        return cmdline;
    QString cmd = cmdline;
    // Check for a leading quote
    const QChar quote = cmdline[0];
    const char q = quote.toLatin1();
    bool quoted = (q == '"' || q == '\'');
    // Split the command at the first non-escaped space
    for (int i = quoted ? 1 : 0, count = cmd.length();  i < count;  ++i)
    {
        switch (cmd.at(i).toLatin1())
        {
            case '\\':
                ++i;
                break;
            case '"':
            case '\'':
                if (quoted  &&  cmd.at(i) == quote)
                    quoted = false;
                break;
            case ' ':
                if (!quoted)
                {
                    cmdline = cmd.mid(i);  // command arguments
                    cmd.truncate(i);
                    return cmd;
                }
                break;
            default:
                break;
        }
    }
    cmdline.clear();
    return cmd;
}

#include "moc_shellprocess.cpp"

// vim: et sw=4:
