/*
 *  prefsettings.cpp  -  program preference settings
 *  Program:  kalarm
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

#include <kglobal.h>
#include <kconfig.h>

#include "prefsettings.moc"


// Default config file settings
const bool   Settings::default_runInSystemTray         = true;
const bool   Settings::default_disableAlarmsIfStopped  = true;
const bool   Settings::default_autostartTrayIcon       = true;
const int    Settings::default_daemonTrayCheckInterval = 10;     // (seconds)
const QColor Settings::default_defaultBgColour(red);
const QFont  Settings::default_messageFont(QString::fromLatin1("Helvetica"), 16, QFont::Bold);
const QTime  Settings::default_startOfDay(0, 0);

// Config file entry names
static const QString GENERAL_SECTION      = QString::fromLatin1("General");
static const QString MESSAGE_BG_COLOUR    = QString::fromLatin1("MessageBackgroundColour");
static const QString MESSAGE_FONT         = QString::fromLatin1("MessageFont");
static const QString RUN_IN_SYSTEM_TRAY   = QString::fromLatin1("RunInSystemTray");
static const QString DISABLE_IF_STOPPED   = QString::fromLatin1("DisableAlarmsIfStopped");
static const QString AUTOSTART_TRAY       = QString::fromLatin1("AutostartTray");
static const QString DAEMON_TRAY_INTERVAL = QString::fromLatin1("DaemonTrayCheckInterval");
static const QString START_OF_DAY         = QString::fromLatin1("StartOfDay");
static const QString START_OF_DAY_CHECK   = QString::fromLatin1("Sod");

inline int Settings::startOfDayCheck() const
{
	return QTime().msecsTo(mStartOfDay) ^ 0x82451630;   // combine with a "random" constant to prevent clever people fiddling things
}


Settings::Settings(QWidget* parent)
	: QObject(parent)
{
	loadSettings();
}

void Settings::loadSettings()
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	mDefaultBgColour         = config->readColorEntry(MESSAGE_BG_COLOUR, &default_defaultBgColour);
	mMessageFont             = config->readFontEntry(MESSAGE_FONT, &default_messageFont);
	mRunInSystemTray         = config->readBoolEntry(RUN_IN_SYSTEM_TRAY, default_runInSystemTray);
	mDisableAlarmsIfStopped  = config->readBoolEntry(DISABLE_IF_STOPPED, default_disableAlarmsIfStopped);
	mAutostartTrayIcon       = config->readBoolEntry(AUTOSTART_TRAY, default_autostartTrayIcon);
	mDaemonTrayCheckInterval = config->readNumEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay              = config->readDateTimeEntry(START_OF_DAY, &defStartOfDay).time();
	mStartOfDayChanged       = (config->readNumEntry(START_OF_DAY_CHECK, 0) != startOfDayCheck());
	emit settingsChanged();
}

void Settings::saveSettings(bool syncToDisc)
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(MESSAGE_BG_COLOUR, mDefaultBgColour);
	config->writeEntry(MESSAGE_FONT, mMessageFont);
	config->writeEntry(RUN_IN_SYSTEM_TRAY, mRunInSystemTray);
	config->writeEntry(DISABLE_IF_STOPPED, mDisableAlarmsIfStopped);
	config->writeEntry(AUTOSTART_TRAY, mAutostartTrayIcon);
	config->writeEntry(DAEMON_TRAY_INTERVAL, mDaemonTrayCheckInterval);
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	if (syncToDisc)
		config->sync();
}

void Settings::updateStartOfDayCheck()
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(START_OF_DAY_CHECK, startOfDayCheck());
	config->sync();
	mStartOfDayChanged = false;
}

void Settings::emitSettingsChanged()
{
	emit settingsChanged();
}
