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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef SHELLPROCESS_H
#define SHELLPROCESS_H

#include <kprocess.h>


/*=============================================================================
=  Class ShellProcess
=  Runs a shell command and interprets the shell exit status as well as possible.
=============================================================================*/
class ShellProcess : public KShellProcess
{
		Q_OBJECT
	public:
		enum Status {
			INACTIVE,     // start() has not yet been called to run the command
			RUNNING,      // command is currently running
			SUCCESS,      // command appears to have exited successfully
			UNAUTHORISED, // shell commands are not authorised for this user
			DIED,         // command didn't exit cleanly, i.e. was killed or died
			NOT_FOUND,    // command either not found or not executable
			START_FAIL    // command couldn't be started for other reasons
		};
		ShellProcess(const QString& command);
		bool            start();
		Status          status() const       { return mStatus; }
		bool            normalExit() const   { return mStatus == SUCCESS; }
		const QString&  command() const      { return mCommand; }
		QString         errorMessage() const;
		static bool     authorised();
		static const QCString& shellName();

	signals:
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
