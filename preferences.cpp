/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2008 by David Jarvie <djarvie@kde.org>
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


static QString translateXTermPath(KConfig*, const QString& cmdline, bool write);

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
const bool                       Preferences::default_modalMessages           = true;
const int                        Preferences::default_messageButtonDelay      = 0;     // (seconds)
const int                        Preferences::default_tooltipAlarmCount       = 5;
const bool                       Preferences::default_showTooltipAlarmTime    = true;
const bool                       Preferences::default_showTooltipTimeToAlarm  = true;
const QString                    Preferences::default_tooltipTimeToPrefix     = QString::fromLatin1("+");
const int                        Preferences::default_daemonTrayCheckInterval = 10;     // (seconds)
const bool                       Preferences::default_emailCopyToKMail        = false;
const bool                       Preferences::default_emailQueuedNotify       = false;
const QColor                     Preferences::default_disabledColour(Qt::lightGray);
const QColor                     Preferences::default_expiredColour(Qt::darkRed);
const int                        Preferences::default_expiredKeepDays         = 7;
const QString                    Preferences::default_defaultSoundFile        = QString::null;
const float                      Preferences::default_defaultSoundVolume      = -1;
const int                        Preferences::default_defaultLateCancel       = 0;
const bool                       Preferences::default_defaultAutoClose        = false;
const bool                       Preferences::default_defaultCopyToKOrganizer = false;
const bool                       Preferences::default_defaultSoundRepeat      = false;
const SoundPicker::Type          Preferences::default_defaultSoundType        = SoundPicker::NONE;
const bool                       Preferences::default_defaultConfirmAck       = false;
const bool                       Preferences::default_defaultCmdScript        = false;
const EditAlarmDlg::CmdLogType   Preferences::default_defaultCmdLogType       = EditAlarmDlg::DISCARD_OUTPUT;
const bool                       Preferences::default_defaultEmailBcc         = false;
const QString                    Preferences::default_emailAddress            = QString::null;
const QString                    Preferences::default_emailBccAddress         = QString::null;
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
bool                       Preferences::mRunInSystemTray;
bool                       Preferences::mDisableAlarmsIfStopped;
bool                       Preferences::mAutostartTrayIcon;
KARecurrence::Feb29Type    Preferences::mDefaultFeb29Type;
bool                       Preferences::mModalMessages;
int                        Preferences::mMessageButtonDelay;
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
SoundPicker::Type          Preferences::mDefaultSoundType;
bool                       Preferences::mDefaultSoundRepeat;
bool                       Preferences::mDefaultConfirmAck;
bool                       Preferences::mDefaultEmailBcc;
bool                       Preferences::mDefaultCmdScript;
EditAlarmDlg::CmdLogType   Preferences::mDefaultCmdLogType;
QString                    Preferences::mDefaultCmdLogFile;
RecurrenceEdit::RepeatType Preferences::mDefaultRecurPeriod;
TimePeriod::Units          Preferences::mDefaultReminderUnits;
QString                    Preferences::mDefaultPreAction;
QString                    Preferences::mDefaultPostAction;
// Change tracking
QTime                      Preferences::mOldStartOfDay;
bool                       Preferences::mStartOfDayChanged;


static const QString defaultFeb29RecurType    = QString::fromLatin1("Mar1");
static const QString defaultEmailClient       = QString::fromLatin1("kmail");

// Config file entry names
static const QString GENERAL_SECTION          = QString::fromLatin1("General");
static const QString VERSION_NUM              = QString::fromLatin1("Version");
static const QString MESSAGE_COLOURS          = QString::fromLatin1("MessageColours");
static const QString MESSAGE_BG_COLOUR        = QString::fromLatin1("MessageBackgroundColour");
static const QString MESSAGE_FONT             = QString::fromLatin1("MessageFont");
static const QString RUN_IN_SYSTEM_TRAY       = QString::fromLatin1("RunInSystemTray");
static const QString DISABLE_IF_STOPPED       = QString::fromLatin1("DisableAlarmsIfStopped");
static const QString AUTOSTART_TRAY           = QString::fromLatin1("AutostartTray");
static const QString FEB29_RECUR_TYPE         = QString::fromLatin1("Feb29Recur");
static const QString MODAL_MESSAGES           = QString::fromLatin1("ModalMessages");
static const QString MESSAGE_BUTTON_DELAY     = QString::fromLatin1("MessageButtonDelay");
static const QString TOOLTIP_ALARM_COUNT      = QString::fromLatin1("TooltipAlarmCount");
static const QString TOOLTIP_ALARM_TIME       = QString::fromLatin1("ShowTooltipAlarmTime");
static const QString TOOLTIP_TIME_TO_ALARM    = QString::fromLatin1("ShowTooltipTimeToAlarm");
static const QString TOOLTIP_TIME_TO_PREFIX   = QString::fromLatin1("TooltipTimeToPrefix");
static const QString DAEMON_TRAY_INTERVAL     = QString::fromLatin1("DaemonTrayCheckInterval");
static const QString EMAIL_CLIENT             = QString::fromLatin1("EmailClient");
static const QString EMAIL_COPY_TO_KMAIL      = QString::fromLatin1("EmailCopyToKMail");
static const QString EMAIL_FROM               = QString::fromLatin1("EmailFrom");
static const QString EMAIL_BCC_ADDRESS        = QString::fromLatin1("EmailBccAddress");
static const QString CMD_XTERM_COMMAND        = QString::fromLatin1("CmdXTerm");
static const QString START_OF_DAY             = QString::fromLatin1("StartOfDay");
static const QString START_OF_DAY_CHECK       = QString::fromLatin1("Sod");
static const QString DISABLED_COLOUR          = QString::fromLatin1("DisabledColour");
static const QString EXPIRED_COLOUR           = QString::fromLatin1("ExpiredColour");
static const QString EXPIRED_KEEP_DAYS        = QString::fromLatin1("ExpiredKeepDays");
static const QString DEFAULTS_SECTION         = QString::fromLatin1("Defaults");
static const QString DEF_LATE_CANCEL          = QString::fromLatin1("DefLateCancel");
static const QString DEF_AUTO_CLOSE           = QString::fromLatin1("DefAutoClose");
static const QString DEF_CONFIRM_ACK          = QString::fromLatin1("DefConfirmAck");
static const QString DEF_COPY_TO_KORG         = QString::fromLatin1("DefCopyKOrg");
static const QString DEF_SOUND_TYPE           = QString::fromLatin1("DefSoundType");
static const QString DEF_SOUND_FILE           = QString::fromLatin1("DefSoundFile");
static const QString DEF_SOUND_VOLUME         = QString::fromLatin1("DefSoundVolume");
static const QString DEF_SOUND_REPEAT         = QString::fromLatin1("DefSoundRepeat");
static const QString DEF_CMD_SCRIPT           = QString::fromLatin1("DefCmdScript");
static const QString DEF_CMD_LOG_TYPE         = QString::fromLatin1("DefCmdLogType");
static const QString DEF_LOG_FILE             = QString::fromLatin1("DefLogFile");
static const QString DEF_EMAIL_BCC            = QString::fromLatin1("DefEmailBcc");
static const QString DEF_RECUR_PERIOD         = QString::fromLatin1("DefRecurPeriod");
static const QString DEF_REMIND_UNITS         = QString::fromLatin1("RemindUnits");
static const QString DEF_PRE_ACTION           = QString::fromLatin1("DefPreAction");
static const QString DEF_POST_ACTION          = QString::fromLatin1("DefPostAction");

// Config file entry name for temporary use
static const QString TEMP                     = QString::fromLatin1("Temp");

// Values for EmailFrom entry
static const QString FROM_CONTROL_CENTRE      = QString::fromLatin1("@ControlCenter");
static const QString FROM_KMAIL               = QString::fromLatin1("@KMail");

// Config file entry names for notification messages
const QString Preferences::QUIT_WARN              = QString::fromLatin1("QuitWarn");
const QString Preferences::CONFIRM_ALARM_DELETION = QString::fromLatin1("ConfirmAlarmDeletion");
const QString Preferences::EMAIL_QUEUED_NOTIFY    = QString::fromLatin1("EmailQueuedNotify");

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
	QStringList cols = config->readListEntry(MESSAGE_COLOURS);
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
	mDefaultBgColour          = config->readColorEntry(MESSAGE_BG_COLOUR, &default_defaultBgColour);
	mMessageFont              = config->readFontEntry(MESSAGE_FONT, &mDefault_messageFont);
	mRunInSystemTray          = config->readBoolEntry(RUN_IN_SYSTEM_TRAY, default_runInSystemTray);
	mDisableAlarmsIfStopped   = config->readBoolEntry(DISABLE_IF_STOPPED, default_disableAlarmsIfStopped);
	mAutostartTrayIcon        = config->readBoolEntry(AUTOSTART_TRAY, default_autostartTrayIcon);
	mModalMessages            = config->readBoolEntry(MODAL_MESSAGES, default_modalMessages);
	mMessageButtonDelay       = config->readNumEntry(MESSAGE_BUTTON_DELAY, default_messageButtonDelay);
	if (mMessageButtonDelay > 10)
		mMessageButtonDelay = 10;    // prevent windows being unusable for a long time
	if (mMessageButtonDelay < -1)
		mMessageButtonDelay = -1;
	mTooltipAlarmCount        = static_cast<int>(config->readUnsignedNumEntry(TOOLTIP_ALARM_COUNT, default_tooltipAlarmCount));
	if (mTooltipAlarmCount < 1)
		mTooltipAlarmCount = 1;
	mShowTooltipAlarmTime     = config->readBoolEntry(TOOLTIP_ALARM_TIME, default_showTooltipAlarmTime);
	mShowTooltipTimeToAlarm   = config->readBoolEntry(TOOLTIP_TIME_TO_ALARM, default_showTooltipTimeToAlarm);
	mTooltipTimeToPrefix      = config->readEntry(TOOLTIP_TIME_TO_PREFIX, default_tooltipTimeToPrefix);
	mDaemonTrayCheckInterval  = static_cast<int>(config->readUnsignedNumEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval));
	if (mDaemonTrayCheckInterval < 1)
		mDaemonTrayCheckInterval = 1;
	QCString client           = config->readEntry(EMAIL_CLIENT, defaultEmailClient).local8Bit();  // don't use readPathEntry() here (values are hard-coded)
	mEmailClient              = (client == "sendmail" ? SENDMAIL : KMAIL);
	mEmailCopyToKMail         = config->readBoolEntry(EMAIL_COPY_TO_KMAIL, default_emailCopyToKMail);
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
	mCmdXTermCommand          = translateXTermPath(config, config->readEntry(CMD_XTERM_COMMAND), false);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay               = config->readDateTimeEntry(START_OF_DAY, &defStartOfDay).time();
	mOldStartOfDay.setHMS(0,0,0);
	int sod = config->readNumEntry(START_OF_DAY_CHECK, 0);
	if (sod)
		mOldStartOfDay    = mOldStartOfDay.addMSecs(sod ^ SODxor);
	mDisabledColour           = config->readColorEntry(DISABLED_COLOUR, &default_disabledColour);
	mExpiredColour            = config->readColorEntry(EXPIRED_COLOUR, &default_expiredColour);
	mExpiredKeepDays          = config->readNumEntry(EXPIRED_KEEP_DAYS, default_expiredKeepDays);

	config->setGroup(DEFAULTS_SECTION);
	mDefaultLateCancel        = static_cast<int>(config->readUnsignedNumEntry(DEF_LATE_CANCEL, default_defaultLateCancel));
	mDefaultAutoClose         = config->readBoolEntry(DEF_AUTO_CLOSE, default_defaultAutoClose);
	mDefaultConfirmAck        = config->readBoolEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultCopyToKOrganizer  = config->readBoolEntry(DEF_COPY_TO_KORG, default_defaultCopyToKOrganizer);
	int soundType             = config->readNumEntry(DEF_SOUND_TYPE, default_defaultSoundType);
	mDefaultSoundType         = (soundType < 0 || soundType > SoundPicker::SPEAK)
	                          ? default_defaultSoundType : (SoundPicker::Type)soundType;
	mDefaultSoundVolume       = static_cast<float>(config->readDoubleNumEntry(DEF_SOUND_VOLUME, default_defaultSoundVolume));
#ifdef WITHOUT_ARTS
	mDefaultSoundRepeat       = false;
#else
	mDefaultSoundRepeat       = config->readBoolEntry(DEF_SOUND_REPEAT, default_defaultSoundRepeat);
#endif
	mDefaultSoundFile         = config->readPathEntry(DEF_SOUND_FILE);
	mDefaultCmdScript         = config->readBoolEntry(DEF_CMD_SCRIPT, default_defaultCmdScript);
	int logType               = config->readNumEntry(DEF_CMD_LOG_TYPE, default_defaultCmdLogType);
	mDefaultCmdLogType        = (logType < EditAlarmDlg::DISCARD_OUTPUT || logType > EditAlarmDlg::EXEC_IN_TERMINAL)
	                          ? default_defaultCmdLogType : (EditAlarmDlg::CmdLogType)logType;
	mDefaultCmdLogFile        = config->readPathEntry(DEF_LOG_FILE);
	mDefaultEmailBcc          = config->readBoolEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod           = config->readNumEntry(DEF_RECUR_PERIOD, default_defaultRecurPeriod);
	mDefaultRecurPeriod       = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                          ? default_defaultRecurPeriod : (RecurrenceEdit::RepeatType)recurPeriod;
	QCString feb29            = config->readEntry(FEB29_RECUR_TYPE, defaultFeb29RecurType).local8Bit();
	mDefaultFeb29Type         = (feb29 == "Mar1") ? KARecurrence::FEB29_MAR1 : (feb29 == "Feb28") ? KARecurrence::FEB29_FEB28 : KARecurrence::FEB29_FEB29;
	QString remindUnits       = config->readEntry(DEF_REMIND_UNITS);
	mDefaultReminderUnits     = (remindUnits == QString::fromLatin1("Minutes"))      ? TimePeriod::MINUTES
	                          : (remindUnits == QString::fromLatin1("HoursMinutes")) ? TimePeriod::HOURS_MINUTES
	                          : (remindUnits == QString::fromLatin1("Days"))         ? TimePeriod::DAYS
	                          : (remindUnits == QString::fromLatin1("Weeks"))        ? TimePeriod::WEEKS : default_defaultReminderUnits;
	mDefaultPreAction         = config->readEntry(DEF_PRE_ACTION, default_defaultPreAction);
	mDefaultPostAction        = config->readEntry(DEF_POST_ACTION, default_defaultPostAction);
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
	for (ColourList::const_iterator it = mMessageColours.begin();  it != mMessageColours.end();  ++it)
		colours.append(QColor(*it).name());
	config->writeEntry(MESSAGE_COLOURS, colours);
	config->writeEntry(MESSAGE_BG_COLOUR, mDefaultBgColour);
	config->writeEntry(MESSAGE_FONT, mMessageFont);
	config->writeEntry(RUN_IN_SYSTEM_TRAY, mRunInSystemTray);
	config->writeEntry(DISABLE_IF_STOPPED, mDisableAlarmsIfStopped);
	config->writeEntry(AUTOSTART_TRAY, mAutostartTrayIcon);
	config->writeEntry(MODAL_MESSAGES, mModalMessages);
	config->writeEntry(MESSAGE_BUTTON_DELAY, mMessageButtonDelay);
	config->writeEntry(TOOLTIP_ALARM_COUNT, mTooltipAlarmCount);
	config->writeEntry(TOOLTIP_ALARM_TIME, mShowTooltipAlarmTime);
	config->writeEntry(TOOLTIP_TIME_TO_ALARM, mShowTooltipTimeToAlarm);
	config->writeEntry(TOOLTIP_TIME_TO_PREFIX, mTooltipTimeToPrefix);
	config->writeEntry(DAEMON_TRAY_INTERVAL, mDaemonTrayCheckInterval);
	config->writeEntry(EMAIL_CLIENT, (mEmailClient == SENDMAIL ? "sendmail" : "kmail"));
	config->writeEntry(EMAIL_COPY_TO_KMAIL, mEmailCopyToKMail);
	config->writeEntry(EMAIL_FROM, emailFrom(mEmailFrom, true, false));
	config->writeEntry(EMAIL_BCC_ADDRESS, emailFrom(mEmailBccFrom, true, true));
	config->writeEntry(CMD_XTERM_COMMAND, translateXTermPath(config, mCmdXTermCommand, true));
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->writeEntry(DISABLED_COLOUR, mDisabledColour);
	config->writeEntry(EXPIRED_COLOUR, mExpiredColour);
	config->writeEntry(EXPIRED_KEEP_DAYS, mExpiredKeepDays);

	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_AUTO_CLOSE, mDefaultAutoClose);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_COPY_TO_KORG, mDefaultCopyToKOrganizer);
	config->writeEntry(DEF_SOUND_TYPE, mDefaultSoundType);
	config->writePathEntry(DEF_SOUND_FILE, mDefaultSoundFile);
	config->writeEntry(DEF_SOUND_VOLUME, static_cast<double>(mDefaultSoundVolume));
	config->writeEntry(DEF_SOUND_REPEAT, mDefaultSoundRepeat);
	config->writeEntry(DEF_CMD_SCRIPT, mDefaultCmdScript);
	config->writeEntry(DEF_CMD_LOG_TYPE, mDefaultCmdLogType);
	config->writePathEntry(DEF_LOG_FILE, mDefaultCmdLogFile);
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config->writeEntry(DEF_RECUR_PERIOD, mDefaultRecurPeriod);
	config->writeEntry(FEB29_RECUR_TYPE, (mDefaultFeb29Type == KARecurrence::FEB29_MAR1 ? "Mar1" : mDefaultFeb29Type == KARecurrence::FEB29_FEB28 ? "Feb28" : "None"));
	QString value;
	switch (mDefaultReminderUnits)
	{
		case TimePeriod::MINUTES:       value = QString::fromLatin1("Minutes");      break;
		case TimePeriod::HOURS_MINUTES: value = QString::fromLatin1("HoursMinutes"); break;
		case TimePeriod::DAYS:          value = QString::fromLatin1("Days");         break;
		case TimePeriod::WEEKS:         value = QString::fromLatin1("Weeks");        break;
		default:                        value = QString::null; break;
	}
	config->writeEntry(DEF_REMIND_UNITS, value);
	config->writeEntry(DEF_PRE_ACTION, mDefaultPreAction);
	config->writeEntry(DEF_POST_ACTION, mDefaultPostAction);

	if (syncToDisc)
		config->sync();
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
			return useAddress ? (bcc ? mEmailBccAddress : mEmailAddress) : QString::null;
		default:
			return QString::null;
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
			return QString::null;
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
			return QString::null;
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
	int version = KAlarm::getVersionNumber(config->readEntry(VERSION_NUM));
	if (version >= KAlarm::Version(1,4,22))
		return;     // config format is up to date

	if (version <= KAlarm::Version(1,4,21))
	{
		// Convert KAlarm 1.4.21 preferences
		static const QString OLD_REMIND_UNITS = QString::fromLatin1("DefRemindUnits");
		config->setGroup(DEFAULTS_SECTION);
		int intUnit     = config->readNumEntry(OLD_REMIND_UNITS, 0);
		QString strUnit = (intUnit == 1) ? QString::fromLatin1("Days")
		                : (intUnit == 2) ? QString::fromLatin1("Weeks")
		                :                  QString::fromLatin1("HoursMinutes");
		config->deleteEntry(OLD_REMIND_UNITS);
		config->writeEntry(DEF_REMIND_UNITS, strUnit);
	}

	if (version <= KAlarm::Version(1,4,20))
	{
		// Convert KAlarm 1.4.20 preferences
		static const QString VIEW_SECTION = QString::fromLatin1("View");
		static const QString SHOW_ARCHIVED_ALARMS = QString::fromLatin1("ShowArchivedAlarms");
		static const QString SHOW_EXPIRED_ALARMS  = QString::fromLatin1("ShowExpiredAlarms");
		static const QString SHOW_ALARM_TIME      = QString::fromLatin1("ShowAlarmTime");
		static const QString SHOW_TIME_TO_ALARM   = QString::fromLatin1("ShowTimeToAlarm");
		config->setGroup(GENERAL_SECTION);
		bool showExpired = config->readBoolEntry(SHOW_EXPIRED_ALARMS, false);
		bool showTime    = config->readBoolEntry(SHOW_ALARM_TIME, true);
		bool showTimeTo  = config->readBoolEntry(SHOW_TIME_TO_ALARM, false);
		config->deleteEntry(SHOW_EXPIRED_ALARMS);
		config->deleteEntry(SHOW_ALARM_TIME);
		config->deleteEntry(SHOW_TIME_TO_ALARM);
		config->setGroup(VIEW_SECTION);
		config->writeEntry(SHOW_ARCHIVED_ALARMS, showExpired);
		config->writeEntry(SHOW_ALARM_TIME, showTime);
		config->writeEntry(SHOW_TIME_TO_ALARM, showTimeTo);
	}

	if (version <= KAlarm::Version(1,4,5))
	{
		// Convert KAlarm 1.4.5 preferences
		static const QString DEF_SOUND = QString::fromLatin1("DefSound");
		config->setGroup(DEFAULTS_SECTION);
		bool sound = config->readBoolEntry(DEF_SOUND, false);
		if (!sound)
			config->writeEntry(DEF_SOUND_TYPE, SoundPicker::NONE);
		config->deleteEntry(DEF_SOUND);
	}

	if (version < KAlarm::Version(1,3,0))
	{
		// Convert KAlarm pre-1.3 preferences
		static const QString EMAIL_ADDRESS             = QString::fromLatin1("EmailAddress");
		static const QString EMAIL_USE_CTRL_CENTRE     = QString::fromLatin1("EmailUseControlCenter");
		static const QString EMAIL_BCC_USE_CTRL_CENTRE = QString::fromLatin1("EmailBccUseControlCenter");
		QMap<QString, QString> entries = config->entryMap(GENERAL_SECTION);
		if (entries.find(EMAIL_FROM) == entries.end()
		&&  entries.find(EMAIL_USE_CTRL_CENTRE) != entries.end())
		{
			// Preferences were written by KAlarm pre-1.2.1
			config->setGroup(GENERAL_SECTION);
			bool useCC = false;
			bool bccUseCC = false;
			const bool default_emailUseControlCentre    = true;
			const bool default_emailBccUseControlCentre = true;
			useCC = config->readBoolEntry(EMAIL_USE_CTRL_CENTRE, default_emailUseControlCentre);
			// EmailBccUseControlCenter was missing in preferences written by KAlarm pre-0.9.5
			bccUseCC = config->hasKey(EMAIL_BCC_USE_CTRL_CENTRE)
			         ? config->readBoolEntry(EMAIL_BCC_USE_CTRL_CENTRE, default_emailBccUseControlCentre)
				 : useCC;
			config->writeEntry(EMAIL_FROM, (useCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_ADDRESS)));
			config->writeEntry(EMAIL_BCC_ADDRESS, (bccUseCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_BCC_ADDRESS)));
			config->deleteEntry(EMAIL_ADDRESS);
			config->deleteEntry(EMAIL_BCC_USE_CTRL_CENTRE);
			config->deleteEntry(EMAIL_USE_CTRL_CENTRE);
		}
		// Convert KAlarm 1.2 preferences
		static const QString DEF_CMD_XTERM = QString::fromLatin1("DefCmdXterm");
		config->setGroup(DEFAULTS_SECTION);
		if (config->hasKey(DEF_CMD_XTERM))
		{
			config->writeEntry(DEF_CMD_LOG_TYPE,
				(config->readBoolEntry(DEF_CMD_XTERM, false) ? EditAlarmDlg::EXEC_IN_TERMINAL : EditAlarmDlg::DISCARD_OUTPUT));
			config->deleteEntry(DEF_CMD_XTERM);
		}
	}
	config->setGroup(GENERAL_SECTION);
	config->writeEntry(VERSION_NUM, KALARM_VERSION);
	config->sync();
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
QString translateXTermPath(KConfig* config, const QString& cmdline, bool write)
{
	QString params;
	QString cmd = cmdline;
	if (cmdline.isEmpty())
		return cmdline;
	// Strip any leading quote
	QChar quote = cmdline[0];
	char q = static_cast<char>(quote);
	bool quoted = (q == '"' || q == '\'');
	if (quoted)
		cmd = cmdline.mid(1);
	// Split the command at the first non-escaped space
	for (int i = 0, count = cmd.length();  i < count;  ++i)
	{
		switch (cmd[i].latin1())
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
		config->writePathEntry(TEMP, cmd);
		cmd = config->readEntry(TEMP);
	}
	else
	{
		config->writeEntry(TEMP, cmd);
		cmd = config->readPathEntry(TEMP);
	}
	config->deleteEntry(TEMP);
	if (quoted)
		return quote + cmd + params;
	else
		return cmd + params;
}
