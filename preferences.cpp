/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include <kglobal.h>
#include <kconfig.h>
#include <kemailsettings.h>

#include "preferences.moc"


// Default config file settings
const bool       Preferences::default_runInSystemTray         = true;
const bool       Preferences::default_disableAlarmsIfStopped  = true;
const bool       Preferences::default_autostartTrayIcon       = true;
const bool       Preferences::default_confirmAlarmDeletion    = true;
const int        Preferences::default_daemonTrayCheckInterval = 10;     // (seconds)
const bool       Preferences::default_emailUseControlCentre   = true;
const QColor     Preferences::default_defaultBgColour(red);
const QFont      Preferences::default_messageFont(QString::fromLatin1("Helvetica"), 16, QFont::Bold);
const QTime      Preferences::default_startOfDay(0, 0);
const QColor     Preferences::default_expiredColour(darkRed);
const int        Preferences::default_expiredKeepDays         = 7;
const bool       Preferences::default_defaultLateCancel       = false;
const bool       Preferences::default_defaultConfirmAck       = false;
const bool       Preferences::default_defaultBeep             = false;
const bool       Preferences::default_defaultEmailBcc         = false;
const QString    Preferences::default_emailAddress            = "";
const Preferences::MailClient    Preferences::default_emailClient          = KMAIL;
const RecurrenceEdit::RepeatType Preferences::default_defaultRecurPeriod   = RecurrenceEdit::SUBDAILY;
const Reminder::Units            Preferences::default_defaultReminderUnits = Reminder::HOURS_MINUTES;

static const QString    defaultEmailClient = QString::fromLatin1("kmail");

// Config file entry names
static const QString GENERAL_SECTION          = QString::fromLatin1("General");
static const QString MESSAGE_BG_COLOUR        = QString::fromLatin1("MessageBackgroundColour");
static const QString MESSAGE_FONT             = QString::fromLatin1("MessageFont");
static const QString RUN_IN_SYSTEM_TRAY       = QString::fromLatin1("RunInSystemTray");
static const QString DISABLE_IF_STOPPED       = QString::fromLatin1("DisableAlarmsIfStopped");
static const QString AUTOSTART_TRAY           = QString::fromLatin1("AutostartTray");
static const QString CONFIRM_ALARM_DELETION   = QString::fromLatin1("ConfirmAlarmDeletion");
static const QString DAEMON_TRAY_INTERVAL     = QString::fromLatin1("DaemonTrayCheckInterval");
static const QString EMAIL_CLIENT             = QString::fromLatin1("EmailClient");
static const QString EMAIL_USE_CONTROL_CENTRE = QString::fromLatin1("EmailUseControlCenter");
static const QString EMAIL_ADDRESS            = QString::fromLatin1("EmailAddress");
static const QString START_OF_DAY             = QString::fromLatin1("StartOfDay");
static const QString START_OF_DAY_CHECK       = QString::fromLatin1("Sod");
static const QString EXPIRED_COLOUR           = QString::fromLatin1("ExpiredColour");
static const QString EXPIRED_KEEP_DAYS        = QString::fromLatin1("ExpiredKeepDays");
static const QString DEFAULTS_SECTION         = QString::fromLatin1("Defaults");
static const QString DEF_LATE_CANCEL          = QString::fromLatin1("DefLateCancel");
static const QString DEF_CONFIRM_ACK          = QString::fromLatin1("DefConfirmAck");
static const QString DEF_BEEP                 = QString::fromLatin1("DefBeep");
static const QString DEF_EMAIL_BCC            = QString::fromLatin1("DefEmailBcc");
static const QString DEF_RECUR_PERIOD         = QString::fromLatin1("DefRecurPeriod");
static const QString DEF_REMIND_UNITS         = QString::fromLatin1("DefRemindUnits");

inline int Preferences::startOfDayCheck() const
{
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	return QTime().msecsTo(mStartOfDay) ^ 0x82451630;
}


Preferences::Preferences(QWidget* parent)
	: QObject(parent)
{
	loadPreferences();
}

void Preferences::loadPreferences()
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
	QCString client = config->readEntry(EMAIL_CLIENT, defaultEmailClient).local8Bit();
	mEmailClient = (client == "sendmail" ? SENDMAIL : KMAIL);
	mEmailUseControlCentre   = config->readBoolEntry(EMAIL_USE_CONTROL_CENTRE, default_emailUseControlCentre);
	if (mEmailUseControlCentre)
	{
		KEMailSettings e;
		mEmailAddress = e.getSetting(KEMailSettings::EmailAddress);
	}
	else
		mEmailAddress = config->readEntry(EMAIL_ADDRESS);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay              = config->readDateTimeEntry(START_OF_DAY, &defStartOfDay).time();
	mStartOfDayChanged       = (config->readNumEntry(START_OF_DAY_CHECK, 0) != startOfDayCheck());
	mExpiredColour           = config->readColorEntry(EXPIRED_COLOUR, &default_expiredColour);
	mExpiredKeepDays         = config->readNumEntry(EXPIRED_KEEP_DAYS, default_expiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	mDefaultLateCancel       = config->readBoolEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	mDefaultConfirmAck       = config->readBoolEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultBeep             = config->readBoolEntry(DEF_BEEP, default_defaultBeep);
	mDefaultEmailBcc         = config->readBoolEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod          = config->readNumEntry(DEF_RECUR_PERIOD, default_defaultRecurPeriod);
	mDefaultRecurPeriod      = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                         ? default_defaultRecurPeriod : (RecurrenceEdit::RepeatType)recurPeriod;
	int reminderUnits        = config->readNumEntry(DEF_REMIND_UNITS, default_defaultReminderUnits);
	mDefaultReminderUnits    = (reminderUnits < Reminder::HOURS_MINUTES || reminderUnits > Reminder::WEEKS)
	                         ? default_defaultReminderUnits : (Reminder::Units)reminderUnits;
	emit preferencesChanged();
}

void Preferences::savePreferences(bool syncToDisc)
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
	config->writeEntry(EMAIL_CLIENT, (mEmailClient == SENDMAIL ? "sendmail" : "kmail"));
	config->writeEntry(EMAIL_USE_CONTROL_CENTRE, mEmailUseControlCentre);
	config->writeEntry(EMAIL_ADDRESS, (mEmailUseControlCentre ? QString() : mEmailAddress));
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->writeEntry(EXPIRED_COLOUR, mExpiredColour);
	config->writeEntry(EXPIRED_KEEP_DAYS, mExpiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_BEEP, mDefaultBeep);
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config->writeEntry(DEF_RECUR_PERIOD, mDefaultRecurPeriod);
	config->writeEntry(DEF_REMIND_UNITS, mDefaultReminderUnits);
	if (syncToDisc)
		config->sync();
}

void Preferences::updateStartOfDayCheck()
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(START_OF_DAY_CHECK, startOfDayCheck());
	config->sync();
	mStartOfDayChanged = false;
}

void Preferences::emitPreferencesChanged()
{
	emit preferencesChanged();
}

void Preferences::setEmailAddress(bool useControlCentre, const QString& address)
{
	mEmailUseControlCentre = useControlCentre;
	if (useControlCentre)
	{
		KEMailSettings e;
		mEmailAddress = e.getSetting(KEMailSettings::EmailAddress);
	}
	else
		mEmailAddress = address;
}
