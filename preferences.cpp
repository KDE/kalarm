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
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kglobalsettings.h>
#include <kmessagebox.h>
#include <kdatetime.h>
#include <ksystemtimezone.h>
#include <kdebug.h>

#include <libkpimidentities/identity.h>
#include <libkpimidentities/identitymanager.h>

#include "daemon.h"
#include "functions.h"
#include "kamail.h"
#include "messagebox.h"
#include "preferences.moc"


static QString translateXTermPath(KConfigGroup&, const QString& cmdline, bool write);

Preferences* Preferences::mInstance = 0;

// Default config file settings
QColor defaultMessageColours[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::white, Qt::lightGray, Qt::black, QColor() };
const ColourList                 Preferences::default_messageColours(defaultMessageColours);
const QColor                     Preferences::default_defaultBgColour(Qt::red);
const QColor                     Preferences::default_defaultFgColour(Qt::black);
QFont                            Preferences::mDefault_messageFont;    // initialised in constructor
const QTime                      Preferences::default_startOfDay(0, 0);
const bool                       Preferences::default_runInSystemTray         = true;
const bool                       Preferences::default_disableAlarmsIfStopped  = true;
const bool                       Preferences::default_quitWarn                = true;
const bool                       Preferences::default_autostartTrayIcon       = true;
const bool                       Preferences::default_confirmAlarmDeletion    = true;
const bool                       Preferences::default_askResource             = true;
const bool                       Preferences::default_modalMessages           = true;
const int                        Preferences::default_messageButtonDelay      = 0;     // (seconds)
const bool                       Preferences::default_showArchivedAlarms      = false;
const bool                       Preferences::default_showAlarmTime           = true;
const bool                       Preferences::default_showTimeToAlarm         = false;
const bool                       Preferences::default_showResources           = false;
const int                        Preferences::default_tooltipAlarmCount       = 5;
const bool                       Preferences::default_showTooltipAlarmTime    = true;
const bool                       Preferences::default_showTooltipTimeToAlarm  = true;
const QString                    Preferences::default_tooltipTimeToPrefix     = QLatin1String("+");
const int                        Preferences::default_daemonTrayCheckInterval = 10;     // (seconds)
const bool                       Preferences::default_emailCopyToKMail        = false;
const bool                       Preferences::default_emailQueuedNotify       = false;
const QColor                     Preferences::default_disabledColour(Qt::lightGray);
const QColor                     Preferences::default_archivedColour(Qt::darkRed);
const int                        Preferences::default_archivedKeepDays        = 7;
const QString                    Preferences::default_defaultSoundFile        = QString();
const float                      Preferences::default_defaultSoundVolume      = -1;
const int                        Preferences::default_defaultLateCancel       = 0;
const bool                       Preferences::default_defaultAutoClose        = false;
const bool                       Preferences::default_defaultCopyToKOrganizer = false;
const bool                       Preferences::default_defaultSoundRepeat      = false;
const SoundPicker::Type          Preferences::default_defaultSoundType        = SoundPicker::NONE;
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
const KTimeZone*           Preferences::mSystemTimeZone;
const KTimeZone*           Preferences::mTimeZone;
ColourList                 Preferences::mMessageColours;
QColor                     Preferences::mDefaultBgColour;
QFont                      Preferences::mMessageFont;
QTime                      Preferences::mStartOfDay;
bool                       Preferences::mRunInSystemTray;
bool                       Preferences::mDisableAlarmsIfStopped;
bool                       Preferences::mAutostartTrayIcon;
KARecurrence::Feb29Type    Preferences::mDefaultFeb29Type;
bool                       Preferences::mAskResource;
bool                       Preferences::mModalMessages;
int                        Preferences::mMessageButtonDelay;
bool                       Preferences::mShowArchivedAlarms;
bool                       Preferences::mShowAlarmTime;
bool                       Preferences::mShowTimeToAlarm;
bool                       Preferences::mShowResources;
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
QColor                     Preferences::mArchivedColour;
int                        Preferences::mArchivedKeepDays;
// Default settings for Edit Alarm dialog
QString                    Preferences::mDefaultSoundFile;
float                      Preferences::mDefaultSoundVolume;
int                        Preferences::mDefaultLateCancel;
bool                       Preferences::mDefaultAutoClose;
bool                       Preferences::mDefaultCopyToKOrganizer;
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


static const QString defaultFeb29RecurType    = QLatin1String("Mar1");
static const QString defaultEmailClient       = QLatin1String("kmail");

// Config file entry names
static const char* GENERAL_SECTION          = "General";
static const char* VERSION_NUM              = "Version";
static const char* TIMEZONE                 = "TimeZone";
static const char* MESSAGE_COLOURS          = "MessageColours";
static const char* MESSAGE_BG_COLOUR        = "MessageBackgroundColour";
static const char* MESSAGE_FONT             = "MessageFont";
static const char* RUN_IN_SYSTEM_TRAY       = "RunInSystemTray";
static const char* DISABLE_IF_STOPPED       = "DisableAlarmsIfStopped";
static const char* AUTOSTART_TRAY           = "AutostartTray";
static const char* FEB29_RECUR_TYPE         = "Feb29Recur";
static const char* ASK_RESOURCE             = "AskResource";
static const char* MODAL_MESSAGES           = "ModalMessages";
static const char* MESSAGE_BUTTON_DELAY     = "MessageButtonDelay";
static const char* SHOW_RESOURCES           = "ShowResources";
static const char* SHOW_ARCHIVED_ALARMS     = "ShowExpiredAlarms";
static const char* SHOW_ALARM_TIME          = "ShowAlarmTime";
static const char* SHOW_TIME_TO_ALARM       = "ShowTimeToAlarm";
static const char* TOOLTIP_ALARM_COUNT      = "TooltipAlarmCount";
static const char* TOOLTIP_ALARM_TIME       = "ShowTooltipAlarmTime";
static const char* TOOLTIP_TIME_TO_ALARM    = "ShowTooltipTimeToAlarm";
static const char* TOOLTIP_TIME_TO_PREFIX   = "TooltipTimeToPrefix";
static const char* DAEMON_TRAY_INTERVAL     = "DaemonTrayCheckInterval";
static const char* EMAIL_CLIENT             = "EmailClient";
static const char* EMAIL_COPY_TO_KMAIL      = "EmailCopyToKMail";
static const char* EMAIL_FROM               = "EmailFrom";
static const char* EMAIL_BCC_ADDRESS        = "EmailBccAddress";
static const char* CMD_XTERM_COMMAND        = "CmdXTerm";
static const char* START_OF_DAY             = "StartOfDay";
static const char* START_OF_DAY_CHECK       = "Sod";
static const char* DISABLED_COLOUR          = "DisabledColour";
static const char* ARCHIVED_COLOUR          = "ExpiredColour";
static const char* ARCHIVED_KEEP_DAYS       = "ExpiredKeepDays";
static const char* DEFAULTS_SECTION         = "Defaults";
static const char* DEF_LATE_CANCEL          = "DefLateCancel";
static const char* DEF_AUTO_CLOSE           = "DefAutoClose";
static const char* DEF_CONFIRM_ACK          = "DefConfirmAck";
static const char* DEF_COPY_TO_KORG         = "DefCopyKOrg";
static const char* DEF_SOUND_TYPE           = "DefSoundType";
static const char* DEF_SOUND_FILE           = "DefSoundFile";
static const char* DEF_SOUND_VOLUME         = "DefSoundVolume";
static const char* DEF_SOUND_REPEAT         = "DefSoundRepeat";
static const char* DEF_CMD_SCRIPT           = "DefCmdScript";
static const char* DEF_CMD_LOG_TYPE         = "DefCmdLogType";
static const char* DEF_LOG_FILE             = "DefLogFile";
static const char* DEF_EMAIL_BCC            = "DefEmailBcc";
static const char* DEF_RECUR_PERIOD         = "DefRecurPeriod";
static const char* DEF_REMIND_UNITS         = "DefRemindUnits";
static const char* DEF_PRE_ACTION           = "DefPreAction";
static const char* DEF_POST_ACTION          = "DefPostAction";

// Config file entry name for temporary use
static const char* TEMP                     = "Temp";

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

	KConfigGroup config(KGlobal::config(), GENERAL_SECTION);
	QString timeZone = config.readEntry(TIMEZONE);
	mTimeZone = 0;
	if (!timeZone.isEmpty())
		mTimeZone = KSystemTimeZones::zone(timeZone);
	if (!mTimeZone)
		mTimeZone = KSystemTimeZones::local();
	QStringList cols = config.readEntry(MESSAGE_COLOURS, QStringList() );
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
	mAskResource              = config->readEntry(ASK_RESOURCE, default_askResource);
	mModalMessages            = config->readEntry(MODAL_MESSAGES, default_modalMessages);
	mMessageButtonDelay       = config->readEntry(MESSAGE_BUTTON_DELAY, default_messageButtonDelay);
	if (mMessageButtonDelay > 10)
		mMessageButtonDelay = 10;    // prevent windows being unusable for a long time
	if (mMessageButtonDelay < -1)
		mMessageButtonDelay = -1;
	mShowResources            = config->readEntry(SHOW_RESOURCES, default_showResources);
	mShowArchivedAlarms       = config->readEntry(SHOW_ARCHIVED_ALARMS, default_showArchivedAlarms);
	mShowTimeToAlarm          = config->readEntry(SHOW_TIME_TO_ALARM, default_showTimeToAlarm);
	mShowAlarmTime            = !mShowTimeToAlarm ? true : config->readEntry(SHOW_ALARM_TIME, default_showAlarmTime);
	mTooltipAlarmCount        = config->readEntry(TOOLTIP_ALARM_COUNT, default_tooltipAlarmCount);
	if (mTooltipAlarmCount < 1)
		mTooltipAlarmCount = 1;
	mShowTooltipAlarmTime     = config.readEntry(TOOLTIP_ALARM_TIME, default_showTooltipAlarmTime);
	mShowTooltipTimeToAlarm   = config.readEntry(TOOLTIP_TIME_TO_ALARM, default_showTooltipTimeToAlarm);
	mTooltipTimeToPrefix      = config.readEntry(TOOLTIP_TIME_TO_PREFIX, default_tooltipTimeToPrefix);
	mDaemonTrayCheckInterval  = config.readEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	if (mDaemonTrayCheckInterval < 1)
		mDaemonTrayCheckInterval = 1;
	QByteArray client         = config.readEntry(EMAIL_CLIENT, defaultEmailClient).toLocal8Bit();  // don't use readPathEntry() here (values are hard-coded)
	mEmailClient              = (client == "sendmail" ? SENDMAIL : KMAIL);
	mEmailCopyToKMail         = config.readEntry(EMAIL_COPY_TO_KMAIL, default_emailCopyToKMail);
	QString from              = config.readEntry(EMAIL_FROM, emailFrom(default_emailFrom(), false, false));
	mEmailFrom                = emailFrom(from);
	QString bccFrom           = config.readEntry(EMAIL_BCC_ADDRESS, emailFrom(default_emailBccFrom, false, true));
	mEmailBccFrom             = emailFrom(bccFrom);
	if (mEmailFrom == MAIL_FROM_CONTROL_CENTRE  ||  mEmailBccFrom == MAIL_FROM_CONTROL_CENTRE)
		mEmailAddress = mEmailBccAddress = KAMail::controlCentreAddress();
	if (mEmailFrom == MAIL_FROM_ADDR)
		mEmailAddress     = from;
	if (mEmailBccFrom == MAIL_FROM_ADDR)
		mEmailBccAddress  = bccFrom;
	mCmdXTermCommand          = translateXTermPath(config, config.readEntry(CMD_XTERM_COMMAND, QString()), false);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay               = config.readEntry(START_OF_DAY, defStartOfDay).time();
	mOldStartOfDay.setHMS(0,0,0);
	int sod = config.readEntry(START_OF_DAY_CHECK, 0);
	if (sod)
		mOldStartOfDay    = mOldStartOfDay.addMSecs(sod ^ SODxor);
	mDisabledColour           = config.readEntry(DISABLED_COLOUR, default_disabledColour);
	mArchivedColour           = config.readEntry(ARCHIVED_COLOUR, default_archivedColour);
	mArchivedKeepDays         = config.readEntry(ARCHIVED_KEEP_DAYS, default_archivedKeepDays);

	config.changeGroup(DEFAULTS_SECTION);
	mDefaultLateCancel        = config.readEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	if (mDefaultLateCancel < 0)
		mDefaultLateCancel = 0;
	mDefaultAutoClose         = config.readEntry(DEF_AUTO_CLOSE, default_defaultAutoClose);
	mDefaultConfirmAck        = config.readEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultCopyToKOrganizer  = config.readEntry(DEF_COPY_TO_KORG, default_defaultCopyToKOrganizer);
	int soundType             = config.readEntry(DEF_SOUND_TYPE, static_cast<int>(default_defaultSoundType));
	mDefaultSoundType         = (soundType < 0 || soundType > SoundPicker::SPEAK)
	                          ? default_defaultSoundType : (SoundPicker::Type)soundType;
	mDefaultSoundVolume       = static_cast<float>(config.readEntry(DEF_SOUND_VOLUME, static_cast<double>(default_defaultSoundVolume)));
	mDefaultSoundRepeat       = config.readEntry(DEF_SOUND_REPEAT, default_defaultSoundRepeat);
	mDefaultSoundFile         = config.readPathEntry(DEF_SOUND_FILE);
	mDefaultCmdScript         = config.readEntry(DEF_CMD_SCRIPT, default_defaultCmdScript);
	int logType               = config.readEntry(DEF_CMD_LOG_TYPE, static_cast<int>(default_defaultCmdLogType));
	mDefaultCmdLogType        = (logType < DISCARD_OUTPUT || logType > EXEC_IN_TERMINAL)
	                          ? default_defaultCmdLogType : (CmdLogType)logType;
	mDefaultCmdLogFile        = config.readPathEntry(DEF_LOG_FILE);
	mDefaultEmailBcc          = config.readEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod           = config.readEntry(DEF_RECUR_PERIOD, static_cast<int>(default_defaultRecurPeriod));
	mDefaultRecurPeriod       = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                          ? default_defaultRecurPeriod : static_cast<RecurrenceEdit::RepeatType>(recurPeriod);
	QByteArray feb29          = config.readEntry(FEB29_RECUR_TYPE, defaultFeb29RecurType).toLocal8Bit();
	mDefaultFeb29Type         = (feb29 == "Mar1") ? KARecurrence::FEB29_MAR1 : (feb29 == "Feb28") ? KARecurrence::FEB29_FEB28 : KARecurrence::FEB29_FEB29;
	int reminderUnits         = config.readEntry(DEF_REMIND_UNITS, static_cast<int>(default_defaultReminderUnits));
	mDefaultReminderUnits     = (reminderUnits < TimePeriod::HOURS_MINUTES || reminderUnits > TimePeriod::WEEKS)
	                          ? default_defaultReminderUnits : static_cast<TimePeriod::Units>(reminderUnits);
	mDefaultPreAction         = config.readEntry(DEF_PRE_ACTION, default_defaultPreAction);
	mDefaultPostAction        = config.readEntry(DEF_POST_ACTION, default_defaultPostAction);
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
	KConfigGroup config(KGlobal::config(), GENERAL_SECTION);
	config.writeEntry(VERSION_NUM, KALARM_VERSION);
	config.writeEntry(TIMEZONE, (mTimeZone ? mTimeZone->name() : QString()));
	QStringList colours;
	for (int i = 0, end = mMessageColours.count();  i < end;  ++i)
		colours.append(QColor(mMessageColours[i]).name());
	config.writeEntry(MESSAGE_COLOURS, colours);
	config.writeEntry(MESSAGE_BG_COLOUR, mDefaultBgColour);
	config.writeEntry(MESSAGE_FONT, mMessageFont);
	config.writeEntry(RUN_IN_SYSTEM_TRAY, mRunInSystemTray);
	config.writeEntry(DISABLE_IF_STOPPED, mDisableAlarmsIfStopped);
	config.writeEntry(AUTOSTART_TRAY, mAutostartTrayIcon);
	config.writeEntry(ASK_RESOURCE, mAskResource);
	config.writeEntry(MODAL_MESSAGES, mModalMessages);
	config.writeEntry(MESSAGE_BUTTON_DELAY, mMessageButtonDelay);
	config.writeEntry(SHOW_RESOURCES, mShowResources);
	config.writeEntry(SHOW_ARCHIVED_ALARMS, mShowArchivedAlarms);
	config.writeEntry(SHOW_ALARM_TIME, mShowAlarmTime);
	config.writeEntry(SHOW_TIME_TO_ALARM, mShowTimeToAlarm);
	config.writeEntry(TOOLTIP_ALARM_COUNT, mTooltipAlarmCount);
	config.writeEntry(TOOLTIP_ALARM_TIME, mShowTooltipAlarmTime);
	config.writeEntry(TOOLTIP_TIME_TO_ALARM, mShowTooltipTimeToAlarm);
	config.writeEntry(TOOLTIP_TIME_TO_PREFIX, mTooltipTimeToPrefix);
	config.writeEntry(DAEMON_TRAY_INTERVAL, mDaemonTrayCheckInterval);
	config.writeEntry(EMAIL_CLIENT, (mEmailClient == SENDMAIL ? "sendmail" : "kmail"));
	config.writeEntry(EMAIL_COPY_TO_KMAIL, mEmailCopyToKMail);
	config.writeEntry(EMAIL_FROM, emailFrom(mEmailFrom, true, false));
	config.writeEntry(EMAIL_BCC_ADDRESS, emailFrom(mEmailBccFrom, true, true));
	config.writeEntry(CMD_XTERM_COMMAND, translateXTermPath(config, mCmdXTermCommand, true));
	config.writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config.writeEntry(DISABLED_COLOUR, mDisabledColour);
	config.writeEntry(ARCHIVED_COLOUR, mArchivedColour);
	config.writeEntry(ARCHIVED_KEEP_DAYS, mArchivedKeepDays);

	config.changeGroup(DEFAULTS_SECTION);
	config.writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config.writeEntry(DEF_AUTO_CLOSE, mDefaultAutoClose);
	config.writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config.writeEntry(DEF_COPY_TO_KORG, mDefaultCopyToKOrganizer);
	config.writeEntry(DEF_SOUND_TYPE, static_cast<int>(mDefaultSoundType));
	config.writePathEntry(DEF_SOUND_FILE, mDefaultSoundFile);
	config.writeEntry(DEF_SOUND_VOLUME, static_cast<double>(mDefaultSoundVolume));
	config.writeEntry(DEF_SOUND_REPEAT, mDefaultSoundRepeat);
	config.writeEntry(DEF_CMD_SCRIPT, mDefaultCmdScript);
	config.writeEntry(DEF_CMD_LOG_TYPE, static_cast<int>(mDefaultCmdLogType));
	config.writePathEntry(DEF_LOG_FILE, mDefaultCmdLogFile);
	config.writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config.writeEntry(DEF_RECUR_PERIOD, static_cast<int>(mDefaultRecurPeriod));
	config.writeEntry(FEB29_RECUR_TYPE, (mDefaultFeb29Type == KARecurrence::FEB29_MAR1 ? "Mar1" : mDefaultFeb29Type == KARecurrence::FEB29_FEB28 ? "Feb28" : "None"));
	config.writeEntry(DEF_REMIND_UNITS, static_cast<int>(mDefaultReminderUnits));
	config.writeEntry(DEF_PRE_ACTION, mDefaultPreAction);
	config.writeEntry(DEF_POST_ACTION, mDefaultPostAction);

	if (syncToDisc)
		config.sync();
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
	KConfigGroup config(KGlobal::config(), GENERAL_SECTION);
	config.writeEntry(START_OF_DAY_CHECK, startOfDayCheck());
	config.sync();
	mStartOfDayChanged = false;
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
	if (mTimeZone)
		return mTimeZone;
	return default_timeZone();
}

const KTimeZone* Preferences::default_timeZone()
{
	if (!mSystemTimeZone)
		mSystemTimeZone = KSystemTimeZones::local();
	return mSystemTimeZone;
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
	KConfigGroup config(KGlobal::config(), GENERAL_SECTION);
	int version = KAlarm::getVersionNumber(config.readEntry(VERSION_NUM, QString()));
	if (version >= KAlarm::Version(1,4,5))
		return;     // config format is up to date

	// Convert KAlarm pre-1.4.5 preferences
	static const char* DEF_SOUND = "DefSound";
	config.changeGroup(DEFAULTS_SECTION);
	bool sound = config.readEntry(DEF_SOUND, false);
	if (!sound)
		config.writeEntry(DEF_SOUND_TYPE, static_cast<int>(SoundPicker::NONE));
	config.deleteEntry(DEF_SOUND);

	if (version < KAlarm::Version(1,3,0))
	{
                config.changeGroup(GENERAL_SECTION);
		// Convert KAlarm pre-1.3 preferences
		static const char* EMAIL_ADDRESS             = "EmailAddress";
		static const char* EMAIL_USE_CTRL_CENTRE     = "EmailUseControlCenter";
		static const char* EMAIL_BCC_USE_CTRL_CENTRE = "EmailBccUseControlCenter";
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
			config.writeEntry(EMAIL_FROM, (useCC ? FROM_CONTROL_CENTRE : config.readEntry(EMAIL_ADDRESS, QString())));
			config.writeEntry(EMAIL_BCC_ADDRESS, (bccUseCC ? FROM_CONTROL_CENTRE : config.readEntry(EMAIL_BCC_ADDRESS, QString())));
			config.deleteEntry(EMAIL_ADDRESS);
			config.deleteEntry(EMAIL_BCC_USE_CTRL_CENTRE);
			config.deleteEntry(EMAIL_USE_CTRL_CENTRE);
		}
		// Convert KAlarm 1.2 preferences
		static const char* DEF_CMD_XTERM = "DefCmdXterm";
		config.changeGroup(DEFAULTS_SECTION);
		if (config.hasKey(DEF_CMD_XTERM))
		{
			config.writeEntry(DEF_CMD_LOG_TYPE,
				static_cast<int>(config.readEntry(DEF_CMD_XTERM, false) ? EXEC_IN_TERMINAL : DISCARD_OUTPUT));
			config.deleteEntry(DEF_CMD_XTERM);
		}
	}

	config.changeGroup(GENERAL_SECTION);
	config.writeEntry(VERSION_NUM, KALARM_VERSION);
	config.sync();
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
QString translateXTermPath(KConfigGroup& config, const QString& cmdline, bool write)
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
	if (write)
	{
		config.writePathEntry(TEMP, cmd);
		cmd = config.readEntry(TEMP, QString());
	}
	else
	{
		config.writeEntry(TEMP, cmd);
		cmd = config.readPathEntry(TEMP);
	}
	config.deleteEntry(TEMP);
	if (quoted)
		return quote + cmd + params;
	else
		return cmd + params;
}
