/*
 *  shellprocess.cpp  -  execute a shell process
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <kapplication.h>
#include <klocale.h>
#include <kdebug.h>

#include "shellprocess.moc"


QCString  ShellProcess::mShellName;
bool      ShellProcess::mInitialised = false;
bool      ShellProcess::mAuthorised  = false;


ShellProcess::ShellProcess(const QString& command)
	: KShellProcess(shellName()),
	  mCommand(command),
	  mStatus(INACTIVE)
{
}

/******************************************************************************
* Execute a command.
*/
bool ShellProcess::start()
{
	if (!authorised())
	{
		mStatus = UNAUTHORISED;
		return false;
	}
	KShellProcess::operator<<(mCommand);
	connect(this, SIGNAL(processExited(KProcess*)), SLOT(slotExited(KProcess*)));
	if (!KShellProcess::start(KProcess::NotifyOnExit))
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
* Find which shell to use.
* This is a duplication of what KShellProcess does, but we need to know
* which shell is used in order to decide what its exit code means.
*/
const QCString& ShellProcess::shellName()
{
	if (mShellName.isEmpty())
	{
		QCString shell = "/bin/sh";
		QCString envshell = QCString(getenv("SHELL")).stripWhiteSpace();
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
				shell = envshell;
		}
		// Get the shell filename with the path stripped
		mShellName = shell;
		int i = mShellName.findRev('/');
		if (i >= 0)
			mShellName = mShellName.mid(i + 1);
	}
	return mShellName;
}

/******************************************************************************
* Check whether shell commands are allowed at all.
*/
bool ShellProcess::authorised()
{
	if (!mInitialised)
	{
#if KDE_VERSION >= 290
		mAuthorised = kapp->authorize("shell_access");
#else
		mAuthorised = true;
#endif
		mInitialised = true;
	}
	return mAuthorised;
}
