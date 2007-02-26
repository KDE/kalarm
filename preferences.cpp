/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <time.h>
#include <unistd.h>

#include <QByteArray>

#include <kglobal.h>
#include <kconfiggroup.h>
#include <kmessagebox.h>
#include <ksystemtimezone.h>
#include <kdebug.h>

#include <libkpimidentities/identity.h>
#include <libkpimidentities/identitymanager.h>

#include "functions.h"
#include "kamail.h"
#include "messagebox.h"
#include "preferences.moc"


// Config file entry names
static const char* GENERAL_SECTION  = "General";
static const char* DEFAULTS_SECTION = "Defaults";
static const char* VERSION_NUM      = "Version";

// Config file entry name for temporary use
static const char* TEMP = "Temp";

// Values for EmailFrom entry
static const QString FROM_CONTROL_CENTRE = QLatin1String("@ControlCenter");
static const QString FROM_KMAIL          = QLatin1String("@KMail");

// Config file entry names for notification messages
const char* Preferences::QUIT_WARN              = "QuitWarn";
const char* Preferences::CONFIRM_ALARM_DELETION = "ConfirmAlarmDeletion";
const char* Preferences::EMAIL_QUEUED_NOTIFY    = "EmailQueuedNotify";
const bool  default_quitWarn             = true;
const bool  default_emailQueuedNotify    = false;
const bool  default_confirmAlarmDeletion = true;

static QString translateXTermPath(const QString& cmdline, bool write);


Preferences*     Preferences::mInstance = 0;
const KTimeZone* Preferences::mSystemTimeZone = 0;
QTime            Preferences::mOldStartOfDay(0, 0, 0);
bool             Preferences::mStartOfDayChanged = false;


Preferences* Preferences::self()
{
	if (!mInstance)
	{
		// Set the default button for the Quit warning message box to Cancel
		MessageBox::setContinueDefault(QUIT_WARN, KMessageBox::Cancel);
		MessageBox::setDefaultShouldBeShownContinue(QUIT_WARN, default_quitWarn);
		MessageBox::setDefaultShouldBeShownContinue(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
		MessageBox::setDefaultShouldBeShownContinue(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);

		mInstance = new Preferences;
		mInstance->readConfig();
	}
	return mInstance;
}

Preferences::Preferences()
	: mConverted(false),
	  mConverting(false)
{
	QObject::connect(this, SIGNAL(base_StartOfDayChanged(const QDateTime&)), SLOT(startDayChange(const QDateTime&)));
}

/******************************************************************************
* Override the base class's readConfig().
* Convert obsolete config values first.
*/
void Preferences::usrReadConfig()
{
	if (mConverting)
		return;   // prevent recursion
	PreferencesBase::usrReadConfig();
	if (!mConverted)
	{
		mConverting = true;
		if (convertOldPrefs())    // convert preferences written by previous KAlarm versions
			writeConfig();
		mConverting = false;
		mConverted = true;
	}
}

/******************************************************************************
* Get the user's time zone, or if none has been chosen, the system time zone.
* The system time zone is cached, and the cached value will be returned unless
* 'reload' is true, in which case the value is re-read from the system.
*/
const KTimeZone* Preferences::timeZone(bool reload)
{
	if (reload)
		mSystemTimeZone = 0;
	QString timeZone = self()->mBase_TimeZone;
	const KTimeZone* tz = 0;
	if (!timeZone.isEmpty())
		tz = KSystemTimeZones::zone(timeZone);
	if (!tz)
	{
		if (!mSystemTimeZone)
			mSystemTimeZone = KSystemTimeZones::local();
		tz = mSystemTimeZone;
	}
	return tz;
}

void Preferences::setTimeZone(const KTimeZone* tz)
{
	self()->setBase_TimeZone(tz ? tz->name() : QString());
}

ColourList Preferences::messageColours()
{
	Preferences* prefs = self();
	ColourList colours;
	for (int i = 0, end = prefs->mBase_MessageColours.count();  i < end;  ++i)
	{
		QColor c = prefs->mBase_MessageColours[i];
		if (c.isValid())
			colours << c;
	}
	return colours;
}

void Preferences::setMessageColours(const ColourList& colours)
{
	QStringList out;
	for (int i = 0, end = colours.count();  i < end;  ++i)
		out << QColor(colours[i]).name();
	self()->setBase_MessageColours(out);
}

static const int SODxor = 0x82451630;
inline int Preferences::startOfDayCheck(const QTime& t)
{
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	return QTime().msecsTo(t) ^ SODxor;
}

void Preferences::setStartOfDay(const QTime& t)
{
	self()->setBase_StartOfDay(QDateTime(QDate(1900,1,1), t));
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	updateStartOfDayCheck(t);
	if (t != mOldStartOfDay)
	{
		emit mInstance->startOfDayChanged(t, mOldStartOfDay);
		mOldStartOfDay = t;
	}
}

// Called when the start of day value has changed in the config file
void Preferences::startDayChange(const QDateTime& dt)
{
	int SOD = sod();
	if (SOD)
		mOldStartOfDay = QTime(0,0).addMSecs(SOD ^ SODxor);
	QTime t = dt.time();
	mStartOfDayChanged = (t != mOldStartOfDay);
	if (mStartOfDayChanged)
	{
		emit mInstance->startOfDayChanged(t, mOldStartOfDay);
		mOldStartOfDay = t;
	}
}

void Preferences::updateStartOfDayCheck(const QTime& t)
{
	self()->setSod(startOfDayCheck(t));
	writeConfig();
	mStartOfDayChanged = false;
}

Preferences::MailFrom Preferences::emailFrom()
{
	QString from = self()->mBase_EmailFrom;
	if (from == FROM_KMAIL)
		return MAIL_FROM_KMAIL;
	if (from == FROM_CONTROL_CENTRE)
		return MAIL_FROM_CONTROL_CENTRE;
	return MAIL_FROM_ADDR;
}

/******************************************************************************
* Get user's default 'From' email address.
*/
QString Preferences::emailAddress()
{
	QString from = self()->mBase_EmailFrom;
	if (from == FROM_KMAIL)
		return KAMail::identityManager()->defaultIdentity().fullEmailAddr();
	if (from == FROM_CONTROL_CENTRE)
		return KAMail::controlCentreAddress();
	return from;
}

void Preferences::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
	QString out;
	switch (from)
	{
		case MAIL_FROM_KMAIL:          out = FROM_KMAIL; break;
		case MAIL_FROM_CONTROL_CENTRE: out = FROM_CONTROL_CENTRE; break;
		case MAIL_FROM_ADDR:           out = address; break;
		default:  return;
	}
	self()->setBase_EmailFrom(out);
}

Preferences::MailFrom Preferences::emailBccFrom()
{
	QString from = self()->mBase_EmailBccAddress;
	if (from == FROM_CONTROL_CENTRE)
		return MAIL_FROM_CONTROL_CENTRE;
	return MAIL_FROM_ADDR;
}

QString Preferences::emailBccAddress()
{
	QString from = self()->mBase_EmailBccAddress;
	if (from == FROM_CONTROL_CENTRE)
		return KAMail::controlCentreAddress();
	return from;
}

bool Preferences::emailBccUseControlCentre()
{
	return self()->mBase_EmailBccAddress == FROM_CONTROL_CENTRE;
}

void Preferences::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	QString out;
	if (useControlCentre)
		out = FROM_CONTROL_CENTRE;
	else
		out = address;
	self()->setBase_EmailBccAddress(out);
}

QString Preferences::cmdXTermCommand()
{
	return translateXTermPath(self()->mBase_CmdXTermCommand, false);
}

void Preferences::setCmdXTermCommand(const QString& cmd)
{
	self()->setBase_CmdXTermCommand(translateXTermPath(cmd, true));
}


void Preferences::connect(const char* signal, const QObject* receiver, const char* member)
{
	QObject::connect(self(), signal, receiver, member);
}

/******************************************************************************
* Called to allow or suppress output of the specified message dialog, where the
* dialog has a checkbox to turn notification off.
*/
void Preferences::setNotify(const QString& messageID, bool notify)
{
	MessageBox::saveDontShowAgainContinue(messageID, !notify);
}

/******************************************************************************
* Return whether the specified message dialog is output, where the dialog has
* a checkbox to turn notification off.
* Reply = false if message has been suppressed (by preferences or by selecting
*               "don't ask again")
*       = true in all other cases.
*/
bool Preferences::notifying(const QString& messageID)
{
	return MessageBox::shouldBeShownContinue(messageID);
}

/******************************************************************************
* If the preferences were written by a previous version of KAlarm, do any
* necessary conversions.
*/
bool Preferences::convertOldPrefs()
{
	mConvertDefSoundType  = false;
	mConvertDefCmdLogType = false;
	mConvertRecurPeriod   = false;
	mConvertReminderUnits = false;
	mConvertedEmailFrom   = QString();   // set to null (not empty)
	mConvertedBccAddress  = QString();   // set to null (not empty)

	KConfigGroup config(KGlobal::config(), GENERAL_SECTION);
	int version = KAlarm::getVersionNumber(config.readEntry(VERSION_NUM, QString()));
	if (version >= KAlarm::Version(1,9,5))
		return false;     // config format is up to date

	// Config file entry names for entries which need to be converted
	static const char* XDEF_LATE_CANCEL  = "DefLateCancel";
	static const char* XDEF_AUTO_CLOSE   = "DefAutoClose";
	static const char* XDEF_CONFIRM_ACK  = "DefConfirmAck";
	static const char* XDEF_COPY_TO_KORG = "DefCopyKOrg";
	static const char* XDEF_SOUND_TYPE   = "DefSoundType";
	static const char* XDEF_SOUND_FILE   = "DefSoundFile";
	static const char* XDEF_SOUND_VOLUME = "DefSoundVolume";
	static const char* XDEF_SOUND_REPEAT = "DefSoundRepeat";
	static const char* XDEF_CMD_SCRIPT   = "DefCmdScript";
	static const char* XDEF_CMD_LOG_TYPE = "DefCmdLogType";
	static const char* XDEF_LOG_FILE     = "DefLogFile";
	static const char* XDEF_EMAIL_BCC    = "DefEmailBcc";
	static const char* XDEF_RECUR_PERIOD = "DefRecurPeriod";
	static const char* XDEF_REMIND_UNITS = "DefRemindUnits";
	static const char* XDEF_PRE_ACTION   = "DefPreAction";
	static const char* XDEF_POST_ACTION  = "DefPostAction";
	static const char* EMAIL_FROM        = "EmailFrom";
	static const char* EMAIL_BCC_ADDRESS = "EmailBccAddress";
	config.changeGroup(DEFAULTS_SECTION);
	if (config.hasKey(XDEF_CMD_LOG_TYPE))
	{
		CmdLogType type;
		switch (config.readEntry(XDEF_CMD_LOG_TYPE, (int)0))
		{
			default:
			case 0:  type = Preferences::Log_Discard;  break;
			case 1:  type = Preferences::Log_File;     break;
			case 2:  type = Preferences::Log_Terminal; break;
		}
		setDefaultCmdLogType(type);
		config.deleteEntry(XDEF_CMD_LOG_TYPE);
	}
	if (config.hasKey(XDEF_RECUR_PERIOD))
	{
		RecurType type;
		switch (config.readEntry(XDEF_RECUR_PERIOD, (int)0))
		{
			default:
			case 0:  type = Preferences::Recur_None;     break;
			case 1:  type = Preferences::Recur_Login;    break;
			case 2:  type = Preferences::Recur_SubDaily; break;
			case 3:  type = Preferences::Recur_Daily;    break;
			case 4:  type = Preferences::Recur_Weekly;   break;
			case 5:  type = Preferences::Recur_Monthly;  break;
			case 6:  type = Preferences::Recur_Yearly;   break;
		}
		setDefaultRecurPeriod(type);
		config.deleteEntry(XDEF_RECUR_PERIOD);
	}
	if (config.hasKey(XDEF_REMIND_UNITS))
	{
		TimePeriod::Units type;
		switch (config.readEntry(XDEF_REMIND_UNITS, (int)0))
		{
			default:
			case 0:  type = TimePeriod::HoursMinutes; break;
			case 1:  type = TimePeriod::Days;         break;
			case 2:  type = TimePeriod::Weeks;        break;
		}
		setDefaultReminderUnits(type);
		config.deleteEntry(XDEF_REMIND_UNITS);
	}
	if (config.hasKey(XDEF_LATE_CANCEL))
	{
		setDefaultLateCancel(config.readEntry(XDEF_LATE_CANCEL, (unsigned)0));
		config.deleteEntry(XDEF_LATE_CANCEL);
	}
	if (config.hasKey(XDEF_AUTO_CLOSE))
	{
		setDefaultAutoClose(config.readEntry(XDEF_AUTO_CLOSE, false));
		config.deleteEntry(XDEF_AUTO_CLOSE);
	}
	if (config.hasKey(XDEF_CONFIRM_ACK))
	{
		setDefaultConfirmAck(config.readEntry(XDEF_CONFIRM_ACK, false));
		config.deleteEntry(XDEF_CONFIRM_ACK);
	}
	if (config.hasKey(XDEF_COPY_TO_KORG))
	{
		setDefaultCopyToKOrganizer(config.readEntry(XDEF_COPY_TO_KORG, false));
		config.deleteEntry(XDEF_COPY_TO_KORG);
	}
	if (config.hasKey(XDEF_SOUND_FILE))
	{
		setDefaultSoundFile(config.readPathEntry(XDEF_SOUND_FILE));
		config.deleteEntry(XDEF_SOUND_FILE);
	}
	if (config.hasKey(XDEF_SOUND_VOLUME))
	{
		int vol = static_cast<int>(config.readEntry(XDEF_SOUND_VOLUME, (double)0) * 100);
		setBase_DefaultSoundVolume(vol < 0 ? -1 : vol > 100 ? 100 : vol);
		config.deleteEntry(XDEF_SOUND_VOLUME);
	}
	if (config.hasKey(XDEF_SOUND_REPEAT))
	{
		setDefaultSoundRepeat(config.readEntry(XDEF_SOUND_REPEAT, false));
		config.deleteEntry(XDEF_SOUND_REPEAT);
	}
	if (config.hasKey(XDEF_CMD_SCRIPT))
	{
		setDefaultCmdScript(config.readEntry(XDEF_CMD_SCRIPT, false));
		config.deleteEntry(XDEF_CMD_SCRIPT);
	}
	if (config.hasKey(XDEF_LOG_FILE))
	{
		setDefaultCmdLogFile(config.readPathEntry(XDEF_LOG_FILE));
		config.deleteEntry(XDEF_LOG_FILE);
	}
	if (config.hasKey(XDEF_EMAIL_BCC))
	{
		setDefaultEmailBcc(config.readEntry(XDEF_EMAIL_BCC, false));
		config.deleteEntry(XDEF_EMAIL_BCC);
	}
	if (config.hasKey(XDEF_PRE_ACTION))
	{
		setDefaultPreAction(config.readEntry(XDEF_PRE_ACTION));
		config.deleteEntry(XDEF_PRE_ACTION);
	}
	if (config.hasKey(XDEF_POST_ACTION))
	{
		setDefaultPostAction(config.readEntry(XDEF_POST_ACTION));
		config.deleteEntry(XDEF_POST_ACTION);
	}
	if (version < KAlarm::Version(1,4,6))
	{
		// Convert KAlarm pre-1.4.5 preferences
		static const char* XDEF_SOUND = "DefSound";
		if (config.hasKey(XDEF_SOUND))
		{
			bool sound = config.readEntry(XDEF_SOUND, false);
			if (!sound)
			{
				setDefaultSoundType(Preferences::Sound_None);
				config.deleteEntry(XDEF_SOUND_TYPE);
			}
			config.deleteEntry(XDEF_SOUND);
		}
	}
	if (config.hasKey(XDEF_SOUND_TYPE))
	{
		// Convert KAlarm 1.9.4 preferences
		SoundType type;
		switch (config.readEntry(XDEF_SOUND_TYPE, (int)0))
		{
			default:
			case 0:  type = Preferences::Sound_None;  break;
			case 1:  type = Preferences::Sound_Beep;  break;
			case 2:  type = Preferences::Sound_File;  break;
			case 3:  type = Preferences::Sound_Speak; break;
		}
		setDefaultSoundType(type);
		config.deleteEntry(XDEF_SOUND_TYPE);
	}

	if (version < KAlarm::Version(1,3,0))
	{
		// Convert KAlarm pre-1.3 preferences
		static const char* EMAIL_ADDRESS             = "EmailAddress";
		static const char* EMAIL_USE_CTRL_CENTRE     = "EmailUseControlCenter";
		static const char* EMAIL_BCC_USE_CTRL_CENTRE = "EmailBccUseControlCenter";
		config.changeGroup(GENERAL_SECTION);
		QMap<QString, QString> entries = config.entryMap();
		if (!entries.contains(EMAIL_FROM)  &&  entries.contains(EMAIL_USE_CTRL_CENTRE))
		{
			// Preferences were written by KAlarm pre-1.2.1
			bool useCC = false;
			bool bccUseCC = false;
			const bool default_emailUseControlCentre    = true;
			const bool default_emailBccUseControlCentre = true;
			useCC = config.readEntry(EMAIL_USE_CTRL_CENTRE, default_emailUseControlCentre);
			// EmailBccUseControlCenter was missing in preferences written by KAlarm pre-0.9.5
			bccUseCC = config.hasKey(EMAIL_BCC_USE_CTRL_CENTRE)
			         ? config.readEntry(EMAIL_BCC_USE_CTRL_CENTRE, default_emailBccUseControlCentre)
				 : useCC;
			setBase_EmailFrom(useCC ? FROM_CONTROL_CENTRE : config.readEntry(EMAIL_ADDRESS, QString()));
			setBase_EmailBccAddress(bccUseCC ? FROM_CONTROL_CENTRE : config.readEntry(EMAIL_BCC_ADDRESS, QString()));
			config.deleteEntry(EMAIL_ADDRESS);
			config.deleteEntry(EMAIL_BCC_USE_CTRL_CENTRE);
			config.deleteEntry(EMAIL_USE_CTRL_CENTRE);
		}
		// Convert KAlarm 1.2 preferences
		static const char* DEF_CMD_XTERM = "DefCmdXterm";
		config.changeGroup(DEFAULTS_SECTION);
		if (config.hasKey(DEF_CMD_XTERM))
		{
			setDefaultCmdLogType(config.readEntry(DEF_CMD_XTERM, false) ? Preferences::Log_Terminal : Preferences::Log_Discard);
			config.deleteEntry(DEF_CMD_XTERM);
		}
	}
	return true;
}

/******************************************************************************
* Translate an X terminal command path to/from config file format.
* Note that only a home directory specification at the start of the path is
* translated, so there's no need to worry about missing out some of the
* executable's path due to quotes etc.
* N.B. Calling KConfig::read/writePathEntry() on the entire command line
*      causes a crash on some systems, so it's necessary to extract the
*      executable path first before processing.
*/
QString translateXTermPath(const QString& cmdline, bool write)
{
	QString params;
	QString cmd = cmdline;
	if (cmdline.isEmpty())
		return cmdline;
	// Strip any leading quote
	QChar quote = cmdline[0];
	char q = quote.toLatin1();
	bool quoted = (q == '"' || q == '\'');
	if (quoted)
		cmd = cmdline.mid(1);
	// Split the command at the first non-escaped space
	for (int i = 0, count = cmd.length();  i < count;  ++i)
	{
		switch (cmd[i].toLatin1())
		{
			case '\\':
				++i;
				continue;
			case '"':
			case '\'':
				if (cmd[i] != quote)
					continue;
				// fall through to ' '
			case ' ':
				params = cmd.mid(i);
				cmd = cmd.left(i);
				break;
			default:
				continue;
		}
		break;
	}
	// Translate any home directory specification at the start of the
	// executable's path.
	KConfigGroup group(KGlobal::config(), GENERAL_SECTION);
	if (write)
	{
		group.writePathEntry(TEMP, cmd);
		cmd = group.readEntry(TEMP, QString());
	}
	else
	{
		group.writeEntry(TEMP, cmd);
		cmd = group.readPathEntry(TEMP);
	}
	group.deleteEntry(TEMP);
	if (quoted)
		return quote + cmd + params;
	else
		return cmd + params;
}
