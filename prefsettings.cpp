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
const bool   Settings::default_confirmAlarmDeletion    = true;
const int    Settings::default_daemonTrayCheckInterval = 10;     // (seconds)
const QColor Settings::default_defaultBgColour(red);
const QFont  Settings::default_messageFont(QString::fromLatin1("Helvetica"), 16, QFont::Bold);
const QTime  Settings::default_startOfDay(0, 0);
const bool   Settings::default_defaultLateCancel       = false;
const bool   Settings::default_defaultConfirmAck       = false;
const bool   Settings::default_defaultBeep             = false;
const bool   Settings::default_defaultEmailBcc         = false;
const RecurrenceEdit::RepeatType Settings::default_defaultRecurPeriod = RecurrenceEdit::SUBDAILY;

// Config file entry names
static const QString GENERAL_SECTION        = QString::fromLatin1("General");
static const QString MESSAGE_BG_COLOUR      = QString::fromLatin1("MessageBackgroundColour");
static const QString MESSAGE_FONT           = QString::fromLatin1("MessageFont");
static const QString RUN_IN_SYSTEM_TRAY     = QString::fromLatin1("RunInSystemTray");
static const QString DISABLE_IF_STOPPED     = QString::fromLatin1("DisableAlarmsIfStopped");
static const QString AUTOSTART_TRAY         = QString::fromLatin1("AutostartTray");
static const QString CONFIRM_ALARM_DELETION = QString::fromLatin1("ConfirmAlarmDeletion");
static const QString DAEMON_TRAY_INTERVAL   = QString::fromLatin1("DaemonTrayCheckInterval");
static const QString START_OF_DAY           = QString::fromLatin1("StartOfDay");
static const QString START_OF_DAY_CHECK     = QString::fromLatin1("Sod");
static const QString DEFAULTS_SECTION       = QString::fromLatin1("Defaults");
static const QString DEF_LATE_CANCEL        = QString::fromLatin1("DefLateCancel");
static const QString DEF_CONFIRM_ACK        = QString::fromLatin1("DefConfirmAck");
static const QString DEF_BEEP               = QString::fromLatin1("DefBeep");
static const QString DEF_EMAIL_BCC          = QString::fromLatin1("DefEmailBcc");
static const QString DEF_RECUR_PERIOD       = QString::fromLatin1("DefRecurPeriod");

inline int Settings::startOfDayCheck() const
{
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	return QTime().msecsTo(mStartOfDay) ^ 0x82451630;
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
	mConfirmAlarmDeletion    = config->readBoolEntry(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);
	mDaemonTrayCheckInterval = config->readNumEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay              = config->readDateTimeEntry(START_OF_DAY, &defStartOfDay).time();
	mStartOfDayChanged       = (config->readNumEntry(START_OF_DAY_CHECK, 0) != startOfDayCheck());
	config->setGroup(DEFAULTS_SECTION);
	mDefaultLateCancel       = config->readBoolEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	mDefaultConfirmAck       = config->readBoolEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultBeep             = config->readBoolEntry(DEF_BEEP, default_defaultBeep);
	mDefaultEmailBcc         = config->readBoolEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod          = config->readNumEntry(DEF_RECUR_PERIOD, default_defaultRecurPeriod);
	mDefaultRecurPeriod      = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                         ? default_defaultRecurPeriod : (RecurrenceEdit::RepeatType)recurPeriod;
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
	config->writeEntry(CONFIRM_ALARM_DELETION, mConfirmAlarmDeletion);
	config->writeEntry(DAEMON_TRAY_INTERVAL, mDaemonTrayCheckInterval);
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_BEEP, mDefaultBeep);
#ifdef KALARM_EMAIL
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
#endif
	config->writeEntry(DEF_RECUR_PERIOD, mDefaultRecurPeriod);
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
