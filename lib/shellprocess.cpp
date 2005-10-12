/*
 *  shellprocess.cpp  -  execute a shell process
 *  Program:  kalarm
 *  Copyright (c) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <kapplication.h>
#include <klocale.h>
#include <kdebug.h>
#include <kauthorized.h>

#include "shellprocess.moc"


QByteArray ShellProcess::mShellName;
QByteArray ShellProcess::mShellPath;
bool       ShellProcess::mInitialised = false;
bool       ShellProcess::mAuthorised  = false;


ShellProcess::ShellProcess(const QString& command)
	: KShellProcess(shellName()),
	  mCommand(command),
	  mStatus(INACTIVE),
	  mStdinExit(false)
{
}

/******************************************************************************
* Execute a command.
*/
bool ShellProcess::start(Communication comm)
{
	if (!authorised())
	{
		mStatus = UNAUTHORISED;
		return false;
	}
	KShellProcess::operator<<(mCommand);
	connect(this, SIGNAL(wroteStdin(KProcess*)), SLOT(writtenStdin(KProcess*)));
	connect(this, SIGNAL(processExited(KProcess*)), SLOT(slotExited(KProcess*)));
	if (!KShellProcess::start(KProcess::NotifyOnExit, comm))
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
void ShellProcess::slotExited(KProcess* proc)
{
	kdDebug(5950) << "ShellProcess::slotExited()\n";
	mStdinQueue.clear();
	mStatus = SUCCESS;
	if (!proc->normalExit())
	{
		kdWarning(5950) << "ShellProcess::slotExited(" << mCommand << ") " << mShellName << ": died/killed\n";
		mStatus = DIED;
	}
	else
	{
		// Some shells report if the command couldn't be found, or is not executable
		int status = proc->exitStatus();
		if (mShellName == "bash"  &&  (status == 126 || status == 127)
		||  mShellName == "ksh"  &&  status == 127)
		{
			kdWarning(5950) << "ShellProcess::slotExited(" << mCommand << ") " << mShellName << ": not found or not executable\n";
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
	bool write = !mStdinQueue.count();
	mStdinQueue.enqueue(scopy);
	if (write)
		KProcess::writeStdin(mStdinQueue.head(), mStdinQueue.head().length());
}

/******************************************************************************
* Called when output to STDIN completes.
* Send the next queued output, if any.
* Note that buffers written to STDIN must not be freed until the writtenStdin()
* signal has been processed.
*/
void ShellProcess::writtenStdin(KProcess* proc)
{
	if (mStdinQueue.count())
		mStdinQueue.dequeue();   // free the buffer which has now been written
	if (mStdinQueue.count())
		proc->writeStdin(mStdinQueue.head(), mStdinQueue.head().length());
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
			return i18n("Failed to execute command (shell access not authorized):");
		case START_FAIL:
		case NOT_FOUND:
			return i18n("Failed to execute command:");
		case DIED:
			return i18n("Command execution error:");
		case INACTIVE:
		case RUNNING:
		case SUCCESS:
		default:
			return QString::null;
	}
}

/******************************************************************************
* Determine which shell to use.
* This is a duplication of what KShellProcess does, but we need to know
* which shell is used in order to decide what its exit code means.
*/
const QByteArray& ShellProcess::shellPath()
{
	if (mShellPath.isEmpty())
	{
		// Get the path to the shell
		mShellPath = "/bin/sh";
		QByteArray envshell = QByteArray(getenv("SHELL")).trimmed();
		if (!envshell.isEmpty())
		{
			struct stat fileinfo;
			if (stat(envshell.data(), &fileinfo) != -1  // ensure file exists
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
		mAuthorised = KAuthorized::authorizeKAction("shell_access");
		mInitialised = true;
	}
	return mAuthorised;
}
