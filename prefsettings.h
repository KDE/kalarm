/*
 *  prefsettings.h  -  program preference settings
 *  Program:  kalarm
 *
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef PREFSETTINGS_H
#define PREFSETTINGS_H

#include <qobject.h>
#include <qcolor.h>
#include <qfont.h>
class QWidget;


// Settings configured in the Preferences dialog
class Settings : public QObject
{
		Q_OBJECT
	public:
		Settings(QWidget* parent);

		QColor       defaultBgColour() const          { return mDefaultBgColour; }
		const QFont& messageFont() const              { return mMessageFont; }
		bool         autostartTrayIcon() const        { return mAutostartTrayIcon; }
		int          daemonTrayCheckInterval() const  { return mDaemonTrayCheckInterval; }
		void         loadSettings();
		void         saveSettings();
		void         emitSettingsChanged();

		static const QColor default_defaultBgColour;
		static const QFont  default_messageFont;
		static const bool   default_autostartTrayIcon;
		static const int    default_daemonTrayCheckInterval;
		bool                mAutostartTrayIcon;
		int                 mDaemonTrayCheckInterval;
		QColor              mDefaultBgColour;
		QFont               mMessageFont;

	signals:
		void settingsChanged();
};

#endif // PREFSETTINGS_H
