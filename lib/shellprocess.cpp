/*
 *  shellprocess.cpp  -  execute a shell process
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007,2008 by David Jarvie <djarvie@kde.org>
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

#include <stdlib.h>
#include <qglobal.h>
#include <kde_file.h>
#include <kapplication.h>
#include <klocale.h>
#include <qdebug.h>
#include <kauthorized.h>

#include "shellprocess.h"


QByteArray ShellProcess::mShellName;
QByteArray ShellProcess::mShellPath;
bool       ShellProcess::mInitialised = false;
bool       ShellProcess::mAuthorised  = false;


ShellProcess::ShellProcess(const QString& command)
    : mCommand(command),
      mStdinBytes(0),
      mStatus(INACTIVE),
      mStdinExit(false)
{
}

/******************************************************************************
* Execute a command.
*/
bool ShellProcess::start(OpenMode openMode)
{
    if (!authorised())
    {
        mStatus = UNAUTHORISED;
        return false;
    }
    connect(this, SIGNAL(bytesWritten(qint64)), SLOT(writtenStdin(qint64)));
    connect(this, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(slotExited(int,QProcess::ExitStatus)));
    connect(this, SIGNAL(readyReadStandardOutput()), SLOT(stdoutReady()));
    connect(this, SIGNAL(readyReadStandardError()), SLOT(stderrReady()));
    QStringList args;
    args << QLatin1String("-c") << mCommand;
    QProcess::start(QLatin1String(shellName()), args, openMode);
    if (!waitForStarted())
    {
        mStatus = START_FAIL;
        return false;
    }
    mStatus = RUNNING;
    return true;
}

/******************************************************************************
* Called when a shell process execution completes.
* Interprets the exit status according to which shell was called, and emits
* a shellExited() signal.
*/
void ShellProcess::slotExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << exitCode << "," << exitStatus;
    mStdinQueue.clear();
    mStatus = SUCCESS;
    mExitCode = exitCode;
    if (exitStatus != NormalExit)
    {
        qWarning() << mCommand << ":" << mShellName << ": crashed/killed";
        mStatus = DIED;
    }
    else
    {
        // Some shells report if the command couldn't be found, or is not executable
        if ((mShellName == "bash"  &&  (exitCode == 126 || exitCode == 127))
        ||  (mShellName == "ksh"  &&  exitCode == 127))
        {
            qWarning() << mCommand << ":" << mShellName << ": not found or not executable";
            mStatus = NOT_FOUND;
        }
    }
    emit shellExited(this);
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
        mStdinBytes = mStdinQueue.head().length();
        write(mStdinQueue.head());
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
        mStdinBytes = mStdinQueue.head().length();
        write(mStdinQueue.head());
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
        case UNAUTHORISED:
            return i18nc("@info", "Failed to execute command (shell access not authorized)");
        case START_FAIL:
        case NOT_FOUND:
            return i18nc("@info", "Failed to execute command");
        case DIED:
            return i18nc("@info", "Command execution error");
        case SUCCESS:
            if (mExitCode)
                return i18nc("@info", "Command exit code: %1", mExitCode);
            // Fall through to INACTIVE
        case INACTIVE:
        case RUNNING:
        default:
            return QString();
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
            KDE_struct_stat fileinfo;
            if (KDE_stat(envshell.data(), &fileinfo) != -1  // ensure file exists
            &&  !S_ISDIR(fileinfo.st_mode)              // and it's not a directory
            &&  !S_ISCHR(fileinfo.st_mode)              // and it's not a character device
            &&  !S_ISBLK(fileinfo.st_mode)              // and it's not a block device
#ifdef S_ISSOCK
            &&  !S_ISSOCK(fileinfo.st_mode)             // and it's not a socket
#endif
            &&  !S_ISFIFO(fileinfo.st_mode)             // and it's not a fifo
            &&  !access(envshell.data(), X_OK))         // and it's executable
                mShellPath = envshell;
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
        mAuthorised = KAuthorized::authorizeKAction(QLatin1String("shell_access"));
        mInitialised = true;
    }
    return mAuthorised;
}
#include "moc_shellprocess.cpp"
// vim: et sw=4:
