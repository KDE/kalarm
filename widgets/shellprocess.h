/*
 *  shellprocess.h  -  execute a shell process
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

#ifndef SHELLPROCESS_H
#define SHELLPROCESS_H

#include <kprocess.h>


/**
 *  The ShellProcess class runs a shell command and interprets the shell exit status
 *  as far as possible.
 *  Derived from KShellProcess, this class additionally tries to interpret the shell
 *  exit status. Different shells use different exit codes. Currently, if bash or ksh
 *  report that the command could not be found or could not be executed, the NOT_FOUND
 *  status is returned.
 *
 *  @short KShellProcess with interpretation of shell exit status.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ShellProcess : public KShellProcess
{
		Q_OBJECT
	public:
		/** Current status of the shell process.
		 *  @li INACTIVE - start() has not yet been called to run the command.
		 *  @li RUNNING - the command is currently running.
		 *  @li SUCCESS - the command appears to have exited successfully.
		 *  @li UNAUTHORISED - shell commands are not authorised for this user.
		 *  @li DIED - the command didn't exit cleanly, i.e. was killed or died.
		 *  @li NOT_FOUND - the command was either not found or not executable.
		 *  @li START_FAIL - the command couldn't be started for other reasons.
		 */
		enum Status {
			INACTIVE,     // start() has not yet been called to run the command
			RUNNING,      // command is currently running
			SUCCESS,      // command appears to have exited successfully
			UNAUTHORISED, // shell commands are not authorised for this user
			DIED,         // command didn't exit cleanly, i.e. was killed or died
			NOT_FOUND,    // command either not found or not executable
			START_FAIL    // command couldn't be started for other reasons
		};
		/** Constructor.
		 *  @param command The command line to be run when start() is called.
		 */
		ShellProcess(const QString& command);
		/** Executes the configured command. */
		bool            start();
		/** Returns the current status of the shell process. */
		Status          status() const       { return mStatus; }
		/** Returns whether the command was run successfully.
		 *  @return True if the command has been run and appears to have exited successfully.
		 */
		bool            normalExit() const   { return mStatus == SUCCESS; }
		/** Returns the command configured to be run. */
		const QString&  command() const      { return mCommand; }
		/** Returns the error message corresponding to the command exit status.
		 *  @return Error message if an error occurred. Null string if the command has not yet
		 *          exited, or if the command ran successfully.
		 */
		QString         errorMessage() const;
		/** Returns whether the user is authorised to run shell commands. Shell commands may
		 *  be prohibited in kiosk mode, for example.
		 */
		static bool     authorised();
		/** Finds which shell to use. */
		static const QCString& shellName();

	signals:
		/** Signal emitted when the shell process execution completes. It is not emitted
		 *  if start() did not attempt to start the command execution, e.g. in kiosk mode.
		 */
		void  shellExited(ShellProcess*);

	private slots:
		void  slotExited(KProcess*);

	private:
		// Prohibit the following inherited methods
		ShellProcess&  operator<<(const QString&);
		ShellProcess&  operator<<(const QCString&);
		ShellProcess&  operator<<(const QStringList&);
		ShellProcess&  operator<<(const char*);

		static QCString   mShellName;      // name of shell to be used
		static bool       mInitialised;    // true once static data has been initialised
		static bool       mAuthorised;     // true if shell commands are authorised
		QString           mCommand;        // copy of command to be executed
		Status            mStatus;         // current execution status
};

#endif // SHELLPROCESS_H
