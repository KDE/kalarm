/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  Copyright (c) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <QByteArray>

#include <kglobal.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kglobalsettings.h>
#include <kmessagebox.h>

#include <libkpimidentities/identity.h>
#include <libkpimidentities/identitymanager.h>

#include "daemon.h"
#include "functions.h"
#include "kamail.h"
#include "messagebox.h"
#include "preferences.moc"


Preferences* Preferences::mInstance = 0;

// Default config file settings
QColor defaultMessageColours[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::white, Qt::lightGray, Qt::black, QColor() };
const ColourList                 Preferences::default_messageColours(defaultMessageColours);
const QColor                     Preferences::default_defaultBgColour(Qt::red);
const QColor                     Preferences::default_defaultFgColour(Qt::black);
QFont                            Preferences::mDefault_messageFont;    // initialised in constructor
const QTime                      Preferences::default_startOfDay(0, 0);
const bool                       Preferences::default_autostartDaemon         = true;
const bool                       Preferences::default_runInSystemTray         = true;
const bool                       Preferences::default_disableAlarmsIfStopped  = true;
const bool                       Preferences::default_quitWarn                = true;
const bool                       Preferences::default_autostartTrayIcon       = true;
const bool                       Preferences::default_confirmAlarmDeletion    = true;
const bool                       Preferences::default_modalMessages           = true;
const int                        Preferences::default_messageButtonDelay      = 0;     // (seconds)
const bool                       Preferences::default_showExpiredAlarms       = false;
const bool                       Preferences::default_showAlarmTime           = true;
const bool                       Preferences::default_showTimeToAlarm         = false;
const int                        Preferences::default_tooltipAlarmCount       = 5;
const bool                       Preferences::default_showTooltipAlarmTime    = true;
const bool                       Preferences::default_showTooltipTimeToAlarm  = true;
const QString                    Preferences::default_tooltipTimeToPrefix     = QLatin1String("+");
const int                        Preferences::default_daemonTrayCheckInterval = 10;     // (seconds)
const bool                       Preferences::default_emailCopyToKMail        = false;
const bool                       Preferences::default_emailQueuedNotify       = false;
const QColor                     Preferences::default_disabledColour(Qt::lightGray);
const QColor                     Preferences::default_expiredColour(Qt::darkRed);
const int                        Preferences::default_expiredKeepDays         = 7;
const QString                    Preferences::default_defaultSoundFile        = QString();
const float                      Preferences::default_defaultSoundVolume      = -1;
const int                        Preferences::default_defaultLateCancel       = 0;
const bool                       Preferences::default_defaultAutoClose        = false;
const bool                       Preferences::default_defaultCopyToKOrganizer = false;
const bool                       Preferences::default_defaultSound            = false;
const bool                       Preferences::default_defaultSoundRepeat      = false;
const SoundPicker::Type          Preferences::default_defaultSoundType        = SoundPicker::BEEP;
const bool                       Preferences::default_defaultConfirmAck       = false;
const bool                       Preferences::default_defaultCmdScript        = false;
const Preferences::CmdLogType    Preferences::default_defaultCmdLogType       = Preferences::DISCARD_OUTPUT;
const bool                       Preferences::default_defaultEmailBcc         = false;
const QString                    Preferences::default_emailAddress            = QString();
const QString                    Preferences::default_emailBccAddress         = QString();
const Preferences::MailClient    Preferences::default_emailClient             = KMAIL;
const Preferences::MailFrom      Preferences::default_emailBccFrom            = MAIL_FROM_CONTROL_CENTRE;
const RecurrenceEdit::RepeatType Preferences::default_defaultRecurPeriod      = RecurrenceEdit::NO_RECUR;
const KARecurrence::Feb29Type    Preferences::default_defaultFeb29Type        = KARecurrence::FEB29_MAR1;
const TimePeriod::Units          Preferences::default_defaultReminderUnits    = TimePeriod::HOURS_MINUTES;
const QString                    Preferences::default_defaultPreAction;
const QString                    Preferences::default_defaultPostAction;

Preferences::MailFrom Preferences::default_emailFrom()
{
	return KAMail::identitiesExist() ? MAIL_FROM_KMAIL : MAIL_FROM_CONTROL_CENTRE;
}

// Active config file settings
ColourList                 Preferences::mMessageColours;
QColor                     Preferences::mDefaultBgColour;
QFont                      Preferences::mMessageFont;
QTime                      Preferences::mStartOfDay;
bool                       Preferences::mAutostartDaemon;
bool                       Preferences::mRunInSystemTray;
bool                       Preferences::mDisableAlarmsIfStopped;
bool                       Preferences::mAutostartTrayIcon;
KARecurrence::Feb29Type    Preferences::mDefaultFeb29Type;
bool                       Preferences::mModalMessages;
int                        Preferences::mMessageButtonDelay;
bool                       Preferences::mShowExpiredAlarms;
bool                       Preferences::mShowAlarmTime;
bool                       Preferences::mShowTimeToAlarm;
int                        Preferences::mTooltipAlarmCount;
bool                       Preferences::mShowTooltipAlarmTime;
bool                       Preferences::mShowTooltipTimeToAlarm;
QString                    Preferences::mTooltipTimeToPrefix;
int                        Preferences::mDaemonTrayCheckInterval;
QString                    Preferences::mEmailAddress;
QString                    Preferences::mEmailBccAddress;
Preferences::MailClient    Preferences::mEmailClient;
Preferences::MailFrom      Preferences::mEmailFrom;
Preferences::MailFrom      Preferences::mEmailBccFrom;
bool                       Preferences::mEmailCopyToKMail;
QString                    Preferences::mCmdXTermCommand;
QColor                     Preferences::mDisabledColour;
QColor                     Preferences::mExpiredColour;
int                        Preferences::mExpiredKeepDays;
// Default settings for Edit Alarm dialog
QString                    Preferences::mDefaultSoundFile;
float                      Preferences::mDefaultSoundVolume;
int                        Preferences::mDefaultLateCancel;
bool                       Preferences::mDefaultAutoClose;
bool                       Preferences::mDefaultCopyToKOrganizer;
bool                       Preferences::mDefaultSound;
SoundPicker::Type          Preferences::mDefaultSoundType;
bool                       Preferences::mDefaultSoundRepeat;
bool                       Preferences::mDefaultConfirmAck;
bool                       Preferences::mDefaultEmailBcc;
bool                       Preferences::mDefaultCmdScript;
Preferences::CmdLogType    Preferences::mDefaultCmdLogType;
QString                    Preferences::mDefaultCmdLogFile;
RecurrenceEdit::RepeatType Preferences::mDefaultRecurPeriod;
TimePeriod::Units          Preferences::mDefaultReminderUnits;
QString                    Preferences::mDefaultPreAction;
QString                    Preferences::mDefaultPostAction;
// Change tracking
QTime                      Preferences::mOldStartOfDay;
bool                       Preferences::mStartOfDayChanged;
bool                       Preferences::mOldAutostartDaemon;


static const QString defaultFeb29RecurType    = QLatin1String("Mar1");
static const QString defaultEmailClient       = QLatin1String("kmail");

// Config file entry names
static const QString GENERAL_SECTION          = "General";
static const QString VERSION_NUM              = "Version";
static const QString MESSAGE_COLOURS          = "MessageColours";
static const QString MESSAGE_BG_COLOUR        = "MessageBackgroundColour";
static const QString MESSAGE_FONT             = "MessageFont";
static const QString RUN_IN_SYSTEM_TRAY       = "RunInSystemTray";
static const QString DISABLE_IF_STOPPED       = "DisableAlarmsIfStopped";
static const QString AUTOSTART_TRAY           = "AutostartTray";
static const QString FEB29_RECUR_TYPE         = "Feb29Recur";
static const QString MODAL_MESSAGES           = "ModalMessages";
static const QString MESSAGE_BUTTON_DELAY     = "MessageButtonDelay";
static const QString SHOW_EXPIRED_ALARMS      = "ShowExpiredAlarms";
static const QString SHOW_ALARM_TIME          = "ShowAlarmTime";
static const QString SHOW_TIME_TO_ALARM       = "ShowTimeToAlarm";
static const QString TOOLTIP_ALARM_COUNT      = "TooltipAlarmCount";
static const QString TOOLTIP_ALARM_TIME       = "ShowTooltipAlarmTime";
static const QString TOOLTIP_TIME_TO_ALARM    = "ShowTooltipTimeToAlarm";
static const QString TOOLTIP_TIME_TO_PREFIX   = "TooltipTimeToPrefix";
static const QString DAEMON_TRAY_INTERVAL     = "DaemonTrayCheckInterval";
static const QString EMAIL_CLIENT             = "EmailClient";
static const QString EMAIL_COPY_TO_KMAIL      = "EmailCopyToKMail";
static const QString EMAIL_FROM               = "EmailFrom";
static const QString EMAIL_BCC_ADDRESS        = "EmailBccAddress";
static const QString CMD_XTERM_COMMAND        = "CmdXTerm";
static const QString START_OF_DAY             = "StartOfDay";
static const QString START_OF_DAY_CHECK       = "Sod";
static const QString DISABLED_COLOUR          = "DisabledColour";
static const QString EXPIRED_COLOUR           = "ExpiredColour";
static const QString EXPIRED_KEEP_DAYS        = "ExpiredKeepDays";
static const QString DEFAULTS_SECTION         = "Defaults";
static const QString DEF_LATE_CANCEL          = "DefLateCancel";
static const QString DEF_AUTO_CLOSE           = "DefAutoClose";
static const QString DEF_CONFIRM_ACK          = "DefConfirmAck";
static const QString DEF_COPY_TO_KORG         = "DefCopyKOrg";
static const QString DEF_SOUND                = "DefSound";
static const QString DEF_SOUND_TYPE           = "DefSoundType";
static const QString DEF_SOUND_FILE           = "DefSoundFile";
static const QString DEF_SOUND_VOLUME         = "DefSoundVolume";
static const QString DEF_SOUND_REPEAT         = "DefSoundRepeat";
static const QString DEF_CMD_SCRIPT           = "DefCmdScript";
static const QString DEF_CMD_LOG_TYPE         = "DefCmdLogType";
static const QString DEF_LOG_FILE             = "DefLogFile";
static const QString DEF_EMAIL_BCC            = "DefEmailBcc";
static const QString DEF_RECUR_PERIOD         = "DefRecurPeriod";
static const QString DEF_REMIND_UNITS         = "DefRemindUnits";
static const QString DEF_PRE_ACTION           = "DefPreAction";
static const QString DEF_POST_ACTION          = "DefPostAction";
// Obsolete - compatibility with pre-1.2.1
static const QString EMAIL_ADDRESS            = "EmailAddress";
static const QString EMAIL_USE_CONTROL_CENTRE = "EmailUseControlCenter";
static const QString EMAIL_BCC_USE_CONTROL_CENTRE = "EmailBccUseControlCenter";

// Values for EmailFrom entry
static const QString FROM_CONTROL_CENTRE      = QLatin1String("@ControlCenter");
static const QString FROM_KMAIL               = QLatin1String("@KMail");

// Config file entry names for notification messages
const QString Preferences::QUIT_WARN              = QLatin1String("QuitWarn");
const QString Preferences::CONFIRM_ALARM_DELETION = QLatin1String("ConfirmAlarmDeletion");
const QString Preferences::EMAIL_QUEUED_NOTIFY    = QLatin1String("EmailQueuedNotify");

static const int SODxor = 0x82451630;
inline int Preferences::startOfDayCheck()
{
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	return QTime().msecsTo(mStartOfDay) ^ SODxor;
}


void Preferences::initialise()
{
	if (!mInstance)
	{
		// Initialise static variables here to avoid static initialisation
		// sequencing errors.
		mDefault_messageFont = QFont(KGlobalSettings::generalFont().family(), 16, QFont::Bold);

		mInstance = new Preferences;

		convertOldPrefs();    // convert preferences written by previous KAlarm versions
		read();

		// Set the default button for the Quit warning message box to Cancel
		MessageBox::setContinueDefault(QUIT_WARN, KMessageBox::Cancel);
		MessageBox::setDefaultShouldBeShownContinue(QUIT_WARN, default_quitWarn);
		MessageBox::setDefaultShouldBeShownContinue(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
		MessageBox::setDefaultShouldBeShownContinue(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);
	}
}

void Preferences::connect(const char* signal, const QObject* receiver, const char* member)
{
	initialise();
	QObject::connect(mInstance, signal, receiver, member);
}

void Preferences::emitStartOfDayChanged()
{
	emit startOfDayChanged(mOldStartOfDay);
}

void Preferences::emitPreferencesChanged()
{
	emit preferencesChanged();
}

/******************************************************************************
* Read preference values from the config file.
*/
void Preferences::read()
{
	initialise();

	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	QStringList cols = config->readEntry(MESSAGE_COLOURS, QStringList() );
	if (!cols.count())
		mMessageColours = default_messageColours;
	else
	{
		mMessageColours.clear();
		for (QStringList::Iterator it = cols.begin();  it != cols.end();  ++it)
		{
			QColor c((*it));
			if (c.isValid())
				mMessageColours.insert(c);
		}
	}
	mDefaultBgColour          = config->readEntry(MESSAGE_BG_COLOUR, default_defaultBgColour);
	mMessageFont              = config->readEntry(MESSAGE_FONT, mDefault_messageFont);
	mRunInSystemTray          = config->readEntry(RUN_IN_SYSTEM_TRAY, default_runInSystemTray);
	mDisableAlarmsIfStopped   = config->readEntry(DISABLE_IF_STOPPED, default_disableAlarmsIfStopped);
	mAutostartTrayIcon        = config->readEntry(AUTOSTART_TRAY, default_autostartTrayIcon);
	mModalMessages            = config->readEntry(MODAL_MESSAGES, default_modalMessages);
	mMessageButtonDelay       = config->readEntry(MESSAGE_BUTTON_DELAY, default_messageButtonDelay);
	mShowExpiredAlarms        = config->readEntry(SHOW_EXPIRED_ALARMS, default_showExpiredAlarms);
	mShowTimeToAlarm          = config->readEntry(SHOW_TIME_TO_ALARM, default_showTimeToAlarm);
	mShowAlarmTime            = !mShowTimeToAlarm ? true : config->readEntry(SHOW_ALARM_TIME, default_showAlarmTime);
	mTooltipAlarmCount        = config->readEntry(TOOLTIP_ALARM_COUNT, default_tooltipAlarmCount);
	if (mTooltipAlarmCount < 1)
		mTooltipAlarmCount = 1;
	mShowTooltipAlarmTime     = config->readEntry(TOOLTIP_ALARM_TIME, default_showTooltipAlarmTime);
	mShowTooltipTimeToAlarm   = config->readEntry(TOOLTIP_TIME_TO_ALARM, default_showTooltipTimeToAlarm);
	mTooltipTimeToPrefix      = config->readEntry(TOOLTIP_TIME_TO_PREFIX, default_tooltipTimeToPrefix);
	mDaemonTrayCheckInterval  = config->readEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	if (mDaemonTrayCheckInterval < 1)
		mDaemonTrayCheckInterval = 1;
	QByteArray client         = config->readEntry(EMAIL_CLIENT, defaultEmailClient).toLocal8Bit();  // don't use readPathEntry() here (values are hard-coded)
	mEmailClient              = (client == "sendmail" ? SENDMAIL : KMAIL);
	mEmailCopyToKMail         = config->readEntry(EMAIL_COPY_TO_KMAIL, default_emailCopyToKMail);
	QString from              = config->readEntry(EMAIL_FROM, emailFrom(default_emailFrom(), false, false));
	mEmailFrom                = emailFrom(from);
	QString bccFrom           = config->readEntry(EMAIL_BCC_ADDRESS, emailFrom(default_emailBccFrom, false, true));
	mEmailBccFrom             = emailFrom(bccFrom);
	if (mEmailFrom == MAIL_FROM_CONTROL_CENTRE  ||  mEmailBccFrom == MAIL_FROM_CONTROL_CENTRE)
		mEmailAddress = mEmailBccAddress = KAMail::controlCentreAddress();
	if (mEmailFrom == MAIL_FROM_ADDR)
		mEmailAddress     = from;
	if (mEmailBccFrom == MAIL_FROM_ADDR)
		mEmailBccAddress  = bccFrom;
	mCmdXTermCommand          = config->readPathEntry(CMD_XTERM_COMMAND);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay               = config->readEntry(START_OF_DAY, defStartOfDay).time();
	mOldStartOfDay.setHMS(0,0,0);
	int sod = config->readEntry(START_OF_DAY_CHECK, 0);
	if (sod)
		mOldStartOfDay    = mOldStartOfDay.addMSecs(sod ^ SODxor);
	mDisabledColour           = config->readEntry(DISABLED_COLOUR, default_disabledColour);
	mExpiredColour            = config->readEntry(EXPIRED_COLOUR, default_expiredColour);
	mExpiredKeepDays          = config->readEntry(EXPIRED_KEEP_DAYS, default_expiredKeepDays);

	config->setGroup(DEFAULTS_SECTION);
	mDefaultLateCancel        = config->readEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	if (mDefaultLateCancel < 0)
		mDefaultLateCancel = 0;
	mDefaultAutoClose         = config->readEntry(DEF_AUTO_CLOSE, default_defaultAutoClose);
	mDefaultConfirmAck        = config->readEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultCopyToKOrganizer  = config->readEntry(DEF_COPY_TO_KORG, default_defaultCopyToKOrganizer);
	mDefaultSound             = config->readEntry(DEF_SOUND, default_defaultSound);
	int soundType             = config->readEntry(DEF_SOUND_TYPE, static_cast<int>(default_defaultSoundType));
	mDefaultSoundType         = (soundType < SoundPicker::BEEP || soundType > SoundPicker::PLAY_FILE)
	                          ? default_defaultSoundType : (SoundPicker::Type)soundType;
	mDefaultSoundVolume       = static_cast<float>(config->readEntry(DEF_SOUND_VOLUME, static_cast<double>(default_defaultSoundVolume)));
#ifdef WITHOUT_ARTS
	mDefaultSoundRepeat       = false;
#else
	mDefaultSoundRepeat       = config->readEntry(DEF_SOUND_REPEAT, default_defaultSoundRepeat);
#endif
	mDefaultSoundFile         = config->readPathEntry(DEF_SOUND_FILE);
	mDefaultCmdScript         = config->readEntry(DEF_CMD_SCRIPT, default_defaultCmdScript);
	int logType               = config->readEntry(DEF_CMD_LOG_TYPE, static_cast<int>(default_defaultCmdLogType));
	mDefaultCmdLogType        = (logType < DISCARD_OUTPUT || logType > EXEC_IN_TERMINAL)
	                          ? default_defaultCmdLogType : (CmdLogType)logType;
	mDefaultCmdLogFile        = config->readPathEntry(DEF_LOG_FILE);
	mDefaultEmailBcc          = config->readEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod           = config->readEntry(DEF_RECUR_PERIOD, static_cast<int>(default_defaultRecurPeriod));
	mDefaultRecurPeriod       = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                          ? default_defaultRecurPeriod : static_cast<RecurrenceEdit::RepeatType>(recurPeriod);
	QByteArray feb29          = config->readEntry(FEB29_RECUR_TYPE, defaultFeb29RecurType).toLocal8Bit();
	mDefaultFeb29Type         = (feb29 == "Mar1") ? KARecurrence::FEB29_MAR1 : (feb29 == "Feb28") ? KARecurrence::FEB29_FEB28 : KARecurrence::FEB29_FEB29;
	int reminderUnits         = config->readEntry(DEF_REMIND_UNITS, static_cast<int>(default_defaultReminderUnits));
	mDefaultReminderUnits     = (reminderUnits < TimePeriod::HOURS_MINUTES || reminderUnits > TimePeriod::WEEKS)
	                          ? default_defaultReminderUnits : static_cast<TimePeriod::Units>(reminderUnits);
	mDefaultPreAction         = config->readEntry(DEF_PRE_ACTION, default_defaultPreAction);
	mDefaultPostAction        = config->readEntry(DEF_POST_ACTION, default_defaultPostAction);
	mAutostartDaemon          = Daemon::autoStart(default_autostartDaemon);
	mOldAutostartDaemon       = mAutostartDaemon;
	mInstance->emitPreferencesChanged();
	mStartOfDayChanged = (mStartOfDay != mOldStartOfDay);
	if (mStartOfDayChanged)
	{
		mInstance->emitStartOfDayChanged();
		mOldStartOfDay = mStartOfDay;
	}
}

/******************************************************************************
* Save preference values to the config file.
*/
void Preferences::save(bool syncToDisc)
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(VERSION_NUM, KALARM_VERSION);
	QStringList colours;
	for (int i = 0, end = mMessageColours.count();  i < end;  ++i)
		colours.append(QColor(mMessageColours[i]).name());
	config->writeEntry(MESSAGE_COLOURS, colours);
	config->writeEntry(MESSAGE_BG_COLOUR, mDefaultBgColour);
	config->writeEntry(MESSAGE_FONT, mMessageFont);
	config->writeEntry(RUN_IN_SYSTEM_TRAY, mRunInSystemTray);
	config->writeEntry(DISABLE_IF_STOPPED, mDisableAlarmsIfStopped);
	config->writeEntry(AUTOSTART_TRAY, mAutostartTrayIcon);
	config->writeEntry(MODAL_MESSAGES, mModalMessages);
	config->writeEntry(MESSAGE_BUTTON_DELAY, mMessageButtonDelay);
	config->writeEntry(SHOW_EXPIRED_ALARMS, mShowExpiredAlarms);
	config->writeEntry(SHOW_ALARM_TIME, mShowAlarmTime);
	config->writeEntry(SHOW_TIME_TO_ALARM, mShowTimeToAlarm);
	config->writeEntry(TOOLTIP_ALARM_COUNT, mTooltipAlarmCount);
	config->writeEntry(TOOLTIP_ALARM_TIME, mShowTooltipAlarmTime);
	config->writeEntry(TOOLTIP_TIME_TO_ALARM, mShowTooltipTimeToAlarm);
	config->writeEntry(TOOLTIP_TIME_TO_PREFIX, mTooltipTimeToPrefix);
	config->writeEntry(DAEMON_TRAY_INTERVAL, mDaemonTrayCheckInterval);
	config->writeEntry(EMAIL_CLIENT, (mEmailClient == SENDMAIL ? "sendmail" : "kmail"));
	config->writeEntry(EMAIL_COPY_TO_KMAIL, mEmailCopyToKMail);
	config->writeEntry(EMAIL_FROM, emailFrom(mEmailFrom, true, false));
	config->writeEntry(EMAIL_BCC_ADDRESS, emailFrom(mEmailBccFrom, true, true));
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	config->writePathEntry(CMD_XTERM_COMMAND, mCmdXTermCommand);
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->writeEntry(DISABLED_COLOUR, mDisabledColour);
	config->writeEntry(EXPIRED_COLOUR, mExpiredColour);
	config->writeEntry(EXPIRED_KEEP_DAYS, mExpiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_AUTO_CLOSE, mDefaultAutoClose);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_COPY_TO_KORG, mDefaultCopyToKOrganizer);
	config->writeEntry(DEF_SOUND, mDefaultSound);
	config->writeEntry(DEF_SOUND_TYPE, static_cast<int>(mDefaultSoundType));
	config->writePathEntry(DEF_SOUND_FILE, mDefaultSoundFile);
	config->writeEntry(DEF_SOUND_VOLUME, static_cast<double>(mDefaultSoundVolume));
	config->writeEntry(DEF_SOUND_REPEAT, mDefaultSoundRepeat);
	config->writeEntry(DEF_CMD_SCRIPT, mDefaultCmdScript);
	config->writeEntry(DEF_CMD_LOG_TYPE, static_cast<int>(mDefaultCmdLogType));
	config->writePathEntry(DEF_LOG_FILE, mDefaultCmdLogFile);
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config->writeEntry(DEF_RECUR_PERIOD, static_cast<int>(mDefaultRecurPeriod));
	config->writeEntry(FEB29_RECUR_TYPE, (mDefaultFeb29Type == KARecurrence::FEB29_MAR1 ? "Mar1" : mDefaultFeb29Type == KARecurrence::FEB29_FEB28 ? "Feb28" : "None"));
	config->writeEntry(DEF_REMIND_UNITS, static_cast<int>(mDefaultReminderUnits));
	config->writeEntry(DEF_PRE_ACTION, mDefaultPreAction);
	config->writeEntry(DEF_POST_ACTION, mDefaultPostAction);
	if (syncToDisc)
		config->sync();
	if (mAutostartDaemon != mOldAutostartDaemon)
	{
		// The alarm daemon autostart setting has changed.
		Daemon::enableAutoStart(mAutostartDaemon);
		mOldAutostartDaemon = mAutostartDaemon;
	}
	mInstance->emitPreferencesChanged();
	if (mStartOfDay != mOldStartOfDay)
	{
		mInstance->emitStartOfDayChanged();
		mOldStartOfDay = mStartOfDay;
	}
}

void Preferences::syncToDisc()
{
	KGlobal::config()->sync();
}

void Preferences::updateStartOfDayCheck()
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(START_OF_DAY_CHECK, startOfDayCheck());
	config->sync();
	mStartOfDayChanged = false;
}

QString Preferences::emailFrom(Preferences::MailFrom from, bool useAddress, bool bcc)
{
	switch (from)
	{
		case MAIL_FROM_KMAIL:
			return FROM_KMAIL;
		case MAIL_FROM_CONTROL_CENTRE:
			return FROM_CONTROL_CENTRE;
		case MAIL_FROM_ADDR:
			return useAddress ? (bcc ? mEmailBccAddress : mEmailAddress) : QString();
		default:
			return QString();
	}
}

Preferences::MailFrom Preferences::emailFrom(const QString& str)
{
	if (str == FROM_KMAIL)
		return MAIL_FROM_KMAIL;
	if (str == FROM_CONTROL_CENTRE)
		return MAIL_FROM_CONTROL_CENTRE;
	return MAIL_FROM_ADDR;
}

/******************************************************************************
* Get user's default 'From' email address.
*/
QString Preferences::emailAddress()
{
	switch (mEmailFrom)
	{
		case MAIL_FROM_KMAIL:
			return KAMail::identityManager()->defaultIdentity().fullEmailAddr();
		case MAIL_FROM_CONTROL_CENTRE:
			return KAMail::controlCentreAddress();
		case MAIL_FROM_ADDR:
			return mEmailAddress;
		default:
			return QString();
	}
}

QString Preferences::emailBccAddress()
{
	switch (mEmailBccFrom)
	{
		case MAIL_FROM_CONTROL_CENTRE:
			return KAMail::controlCentreAddress();
		case MAIL_FROM_ADDR:
			return mEmailBccAddress;
		default:
			return QString();
	}
}

void Preferences::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
	switch (from)
	{
		case MAIL_FROM_KMAIL:
			break;
		case MAIL_FROM_CONTROL_CENTRE:
			mEmailAddress = KAMail::controlCentreAddress();
			break;
		case MAIL_FROM_ADDR:
			mEmailAddress = address;
			break;
		default:
			return;
	}
	mEmailFrom = from;
}

void Preferences::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	if (useControlCentre)
		mEmailBccAddress = KAMail::controlCentreAddress();
	else
		mEmailBccAddress = address;
	mEmailBccFrom = useControlCentre ? MAIL_FROM_CONTROL_CENTRE : MAIL_FROM_ADDR;
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
void Preferences::convertOldPrefs()
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	int version = KAlarm::getVersionNumber(config->readEntry(VERSION_NUM, QString()));
	if (version >= KAlarm::Version(1,3,0))
		return;     // config format is up to date

	bool sync = false;
	QMap<QString, QString> entries = config->entryMap(GENERAL_SECTION);
	if (!entries.contains(EMAIL_FROM)  &&  entries.contains(EMAIL_USE_CONTROL_CENTRE))
	{
		// Preferences were written by KAlarm pre-1.2.1
		config->setGroup(GENERAL_SECTION);
		bool useCC = false;
		bool bccUseCC = false;
		const bool default_emailUseControlCentre    = true;
		const bool default_emailBccUseControlCentre = true;
		useCC = config->readEntry(EMAIL_USE_CONTROL_CENTRE, default_emailUseControlCentre);
		// EmailBccUseControlCenter was missing in preferences written by KAlarm pre-0.9.5
		bccUseCC = config->hasKey(EMAIL_BCC_USE_CONTROL_CENTRE)
		         ? config->readEntry(EMAIL_BCC_USE_CONTROL_CENTRE, default_emailBccUseControlCentre)
			 : useCC;
		config->writeEntry(EMAIL_FROM, (useCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_ADDRESS, QString())));
		config->writeEntry(EMAIL_BCC_ADDRESS, (bccUseCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_BCC_ADDRESS, QString())));
		config->deleteEntry(EMAIL_ADDRESS);
		config->deleteEntry(EMAIL_BCC_USE_CONTROL_CENTRE);
		config->deleteEntry(EMAIL_USE_CONTROL_CENTRE);
		sync = true;
	}
	// Convert KAlarm 1.2 preferences
	static const QString DEF_CMD_XTERM = QLatin1String("DefCmdXterm");
	config->setGroup(DEFAULTS_SECTION);
	if (config->hasKey(DEF_CMD_XTERM))
	{
		config->writeEntry(DEF_CMD_LOG_TYPE,
			static_cast<int>(config->readEntry(DEF_CMD_XTERM, false) ? EXEC_IN_TERMINAL : DISCARD_OUTPUT));
		config->deleteEntry(DEF_CMD_XTERM);
		sync = true;
	}
	if (sync)
		config->sync();
}
