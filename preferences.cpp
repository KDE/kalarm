/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include "kamail.h"
#include "messagebox.h"
#include "preferences.moc"


Preferences* Preferences::mInstance = 0;

// Default config file settings
QColor defaultMessageColours[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::white, Qt::lightGray, Qt::black, QColor() };
const ColourList Preferences::default_messageColours(defaultMessageColours);
const QColor     Preferences::default_defaultBgColour(Qt::red);
const QColor     Preferences::default_defaultFgColour(Qt::black);
QFont            Preferences::default_messageFont;    // initialised in constructor
const QTime      Preferences::default_startOfDay(0, 0);
const bool       Preferences::default_autostartDaemon          = true;
const bool       Preferences::default_runInSystemTray          = true;
const bool       Preferences::default_disableAlarmsIfStopped   = true;
const bool       Preferences::default_quitWarn                 = true;
const bool       Preferences::default_autostartTrayIcon        = true;
const bool       Preferences::default_confirmAlarmDeletion     = true;
const bool       Preferences::default_modalMessages            = true;
const bool       Preferences::default_showExpiredAlarms        = false;
const bool       Preferences::default_showAlarmTime            = true;
const bool       Preferences::default_showTimeToAlarm          = false;
const int        Preferences::default_tooltipAlarmCount        = 5;
const bool       Preferences::default_showTooltipAlarmTime     = true;
const bool       Preferences::default_showTooltipTimeToAlarm   = true;
const QString    Preferences::default_tooltipTimeToPrefix      = QString::fromLatin1("+");
const int        Preferences::default_daemonTrayCheckInterval  = 10;     // (seconds)
const bool       Preferences::default_emailCopyToKMail         = false;
const bool       Preferences::default_emailQueuedNotify        = false;
const QColor     Preferences::default_disabledColour(Qt::lightGray);
const QColor     Preferences::default_expiredColour(Qt::darkRed);
const int        Preferences::default_expiredKeepDays          = 7;
const QString    Preferences::default_defaultSoundFile         = QString::null;
const float      Preferences::default_defaultSoundVolume       = -1;
const int        Preferences::default_defaultLateCancel        = 0;
const bool       Preferences::default_defaultAutoClose         = false;
const bool       Preferences::default_defaultSound             = false;
const bool       Preferences::default_defaultSoundRepeat       = false;
const bool       Preferences::default_defaultBeep              = false;
const bool       Preferences::default_defaultConfirmAck        = false;
const bool       Preferences::default_defaultCmdScript         = false;
const bool       Preferences::default_defaultCmdXterm          = false;
const bool       Preferences::default_defaultEmailBcc          = false;
const QString    Preferences::default_emailAddress             = QString::null;
const QString    Preferences::default_emailBccAddress          = QString::null;
const Preferences::MailClient    Preferences::default_emailClient          = KMAIL;
#if KDE_VERSION >= 210
const Preferences::MailFrom      Preferences::default_emailBccFrom         = MAIL_FROM_CONTROL_CENTRE;
#else
const Preferences::MailFrom      Preferences::default_emailBccFrom         = MAIL_FROM_ADDR;
#endif
const Preferences::Feb29Type     Preferences::default_feb29RecurType       = FEB29_MAR1;
const RecurrenceEdit::RepeatType Preferences::default_defaultRecurPeriod   = RecurrenceEdit::NO_RECUR;
const TimePeriod::Units          Preferences::default_defaultReminderUnits = TimePeriod::HOURS_MINUTES;
const QString    Preferences::default_defaultPreAction;
const QString    Preferences::default_defaultPostAction;

Preferences::MailFrom Preferences::default_emailFrom()
{
#if KDE_VERSION >= 210
	return KAMail::identitiesExist() ? MAIL_FROM_KMAIL : MAIL_FROM_CONTROL_CENTRE;
#else
	return KAMail::identitiesExist() ? MAIL_FROM_KMAIL : MAIL_FROM_ADDR;
#endif
}

static const QString defaultFeb29RecurType    = QString::fromLatin1("Mar1");
static const QString defaultEmailClient       = QString::fromLatin1("kmail");

// Config file entry names
static const QString GENERAL_SECTION          = QString::fromLatin1("General");
static const QString MESSAGE_COLOURS          = QString::fromLatin1("MessageColours");
static const QString MESSAGE_BG_COLOUR        = QString::fromLatin1("MessageBackgroundColour");
static const QString MESSAGE_FONT             = QString::fromLatin1("MessageFont");
static const QString RUN_IN_SYSTEM_TRAY       = QString::fromLatin1("RunInSystemTray");
static const QString DISABLE_IF_STOPPED       = QString::fromLatin1("DisableAlarmsIfStopped");
static const QString AUTOSTART_TRAY           = QString::fromLatin1("AutostartTray");
static const QString FEB29_RECUR_TYPE         = QString::fromLatin1("Feb29Recur");
static const QString MODAL_MESSAGES           = QString::fromLatin1("ModalMessages");
static const QString SHOW_EXPIRED_ALARMS      = QString::fromLatin1("ShowExpiredAlarms");
static const QString SHOW_ALARM_TIME          = QString::fromLatin1("ShowAlarmTime");
static const QString SHOW_TIME_TO_ALARM       = QString::fromLatin1("ShowTimeToAlarm");
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
static const QString DEF_SOUND                = QString::fromLatin1("DefSound");
static const QString DEF_SOUND_FILE           = QString::fromLatin1("DefSoundFile");
static const QString DEF_SOUND_VOLUME         = QString::fromLatin1("DefSoundVolume");
static const QString DEF_SOUND_REPEAT         = QString::fromLatin1("DefSoundRepeat");
static const QString DEF_BEEP                 = QString::fromLatin1("DefBeep");
static const QString DEF_CMD_SCRIPT           = QString::fromLatin1("DefCmdScript");
static const QString DEF_CMD_XTERM            = QString::fromLatin1("DefCmdXterm");
static const QString DEF_EMAIL_BCC            = QString::fromLatin1("DefEmailBcc");
static const QString DEF_RECUR_PERIOD         = QString::fromLatin1("DefRecurPeriod");
static const QString DEF_REMIND_UNITS         = QString::fromLatin1("DefRemindUnits");
static const QString DEF_PRE_ACTION           = QString::fromLatin1("DefPreAction");
static const QString DEF_POST_ACTION          = QString::fromLatin1("DefPostAction");
// Obsolete - compatibility with pre-1.2.1
static const QString EMAIL_ADDRESS            = QString::fromLatin1("EmailAddress");
static const QString EMAIL_USE_CONTROL_CENTRE = QString::fromLatin1("EmailUseControlCenter");
static const QString EMAIL_BCC_USE_CONTROL_CENTRE = QString::fromLatin1("EmailBccUseControlCenter");

// Values for EmailFrom entry
static const QString FROM_CONTROL_CENTRE      = QString::fromLatin1("@ControlCenter");
static const QString FROM_KMAIL               = QString::fromLatin1("@KMail");

// Config file entry names for notification messages
const QString Preferences::QUIT_WARN              = QString::fromLatin1("QuitWarn");
const QString Preferences::CONFIRM_ALARM_DELETION = QString::fromLatin1("ConfirmAlarmDeletion");
const QString Preferences::EMAIL_QUEUED_NOTIFY    = QString::fromLatin1("EmailQueuedNotify");

static const int SODxor = 0x82451630;
inline int Preferences::startOfDayCheck() const
{
	// Combine with a 'random' constant to prevent 'clever' people fiddling the
	// value, and thereby screwing things up.
	return QTime().msecsTo(mStartOfDay) ^ SODxor;
}


Preferences* Preferences::instance()
{
	if (!mInstance)
	{
		convertOldPrefs();    // convert preferences written by previous KAlarm versions

		mInstance = new Preferences;

		// Set the default button for the Quit warning message box to Cancel
		MessageBox::setContinueDefault(QUIT_WARN, KMessageBox::Cancel);
		MessageBox::setDefaultShouldBeShownContinue(QUIT_WARN, default_quitWarn);
		MessageBox::setDefaultShouldBeShownContinue(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
		MessageBox::setDefaultShouldBeShownContinue(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);
	}
	return mInstance;
}

Preferences::Preferences()
	: QObject(0)
{
	// Initialise static variables here to avoid static initialisation
	// sequencing errors.
	default_messageFont = QFont(KGlobalSettings::generalFont().family(), 16, QFont::Bold);

	// Read preference values from the config file
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
	mMessageFont              = config->readFontEntry(MESSAGE_FONT, &default_messageFont);
	mRunInSystemTray          = config->readBoolEntry(RUN_IN_SYSTEM_TRAY, default_runInSystemTray);
	mDisableAlarmsIfStopped   = config->readBoolEntry(DISABLE_IF_STOPPED, default_disableAlarmsIfStopped);
	mAutostartTrayIcon        = config->readBoolEntry(AUTOSTART_TRAY, default_autostartTrayIcon);
	QCString feb29            = config->readEntry(FEB29_RECUR_TYPE, defaultFeb29RecurType).local8Bit();
	mFeb29RecurType           = (feb29 == "Mar1") ? FEB29_MAR1 : (feb29 == "Feb28") ? FEB29_FEB28 : FEB29_NONE;
	mModalMessages            = config->readBoolEntry(MODAL_MESSAGES, default_modalMessages);
	mShowExpiredAlarms        = config->readBoolEntry(SHOW_EXPIRED_ALARMS, default_showExpiredAlarms);
	mShowTimeToAlarm          = config->readBoolEntry(SHOW_TIME_TO_ALARM, default_showTimeToAlarm);
	mShowAlarmTime            = !mShowTimeToAlarm ? true : config->readBoolEntry(SHOW_ALARM_TIME, default_showAlarmTime);
	mTooltipAlarmCount        = config->readNumEntry(TOOLTIP_ALARM_COUNT, default_tooltipAlarmCount);
	mShowTooltipAlarmTime     = config->readBoolEntry(TOOLTIP_ALARM_TIME, default_showTooltipAlarmTime);
	mShowTooltipTimeToAlarm   = config->readBoolEntry(TOOLTIP_TIME_TO_ALARM, default_showTooltipTimeToAlarm);
	mTooltipTimeToPrefix      = config->readEntry(TOOLTIP_TIME_TO_PREFIX, default_tooltipTimeToPrefix);
	mDaemonTrayCheckInterval  = config->readNumEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	QCString client           = config->readEntry(EMAIL_CLIENT, defaultEmailClient).local8Bit();
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
	mCmdXTermCommand          = config->readEntry(CMD_XTERM_COMMAND);
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
	mDefaultLateCancel        = config->readNumEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	mDefaultAutoClose         = config->readBoolEntry(DEF_AUTO_CLOSE, default_defaultAutoClose);
	mDefaultConfirmAck        = config->readBoolEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultSound             = config->readBoolEntry(DEF_SOUND, default_defaultSound);
	mDefaultBeep              = config->readBoolEntry(DEF_BEEP, default_defaultBeep);
	mDefaultSoundVolume       = static_cast<float>(config->readDoubleNumEntry(DEF_SOUND_VOLUME, default_defaultSoundVolume));
#ifdef WITHOUT_ARTS
	mDefaultSoundRepeat       = false;
#else
	mDefaultSoundRepeat       = config->readBoolEntry(DEF_SOUND_REPEAT, default_defaultSoundRepeat);
#endif
	mDefaultSoundFile         = config->readPathEntry(DEF_SOUND_FILE);
	mDefaultCmdScript         = config->readBoolEntry(DEF_CMD_SCRIPT, default_defaultCmdScript);
	mDefaultCmdXterm          = config->readBoolEntry(DEF_CMD_XTERM, default_defaultCmdXterm);
	mDefaultEmailBcc          = config->readBoolEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod           = config->readNumEntry(DEF_RECUR_PERIOD, default_defaultRecurPeriod);
	mDefaultRecurPeriod       = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                          ? default_defaultRecurPeriod : (RecurrenceEdit::RepeatType)recurPeriod;
	int reminderUnits         = config->readNumEntry(DEF_REMIND_UNITS, default_defaultReminderUnits);
	mDefaultReminderUnits     = (reminderUnits < TimePeriod::HOURS_MINUTES || reminderUnits > TimePeriod::WEEKS)
	                          ? default_defaultReminderUnits : (TimePeriod::Units)reminderUnits;
	mDefaultPreAction         = config->readEntry(DEF_PRE_ACTION, default_defaultPreAction);
	mDefaultPostAction        = config->readEntry(DEF_POST_ACTION, default_defaultPostAction);
	mAutostartDaemon          = Daemon::autoStart(default_autostartDaemon);
	mOldAutostartDaemon       = mAutostartDaemon;
	emit preferencesChanged();
	mStartOfDayChanged = (mStartOfDay != mOldStartOfDay);
	if (mStartOfDayChanged)
	{
		emit startOfDayChanged(mOldStartOfDay);
		mOldStartOfDay = mStartOfDay;
	}
}

void Preferences::save(bool syncToDisc)
{
	KConfig* config = KGlobal::config();
	config->setGroup(GENERAL_SECTION);
	QStringList colours;
	for (ColourList::const_iterator it = mMessageColours.begin();  it != mMessageColours.end();  ++it)
		colours.append(QColor(*it).name());
	config->writeEntry(MESSAGE_COLOURS, colours);
	config->writeEntry(MESSAGE_BG_COLOUR, mDefaultBgColour);
	config->writeEntry(MESSAGE_FONT, mMessageFont);
	config->writeEntry(RUN_IN_SYSTEM_TRAY, mRunInSystemTray);
	config->writeEntry(DISABLE_IF_STOPPED, mDisableAlarmsIfStopped);
	config->writeEntry(AUTOSTART_TRAY, mAutostartTrayIcon);
	config->writeEntry(FEB29_RECUR_TYPE, (mFeb29RecurType == FEB29_MAR1 ? "Mar1" : mFeb29RecurType == FEB29_FEB28 ? "Feb28" : "None"));
	config->writeEntry(MODAL_MESSAGES, mModalMessages);
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
	config->writeEntry(CMD_XTERM_COMMAND, mCmdXTermCommand);
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->writeEntry(DISABLED_COLOUR, mDisabledColour);
	config->writeEntry(EXPIRED_COLOUR, mExpiredColour);
	config->writeEntry(EXPIRED_KEEP_DAYS, mExpiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_AUTO_CLOSE, mDefaultAutoClose);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_BEEP, mDefaultBeep);
	config->writeEntry(DEF_SOUND, mDefaultSound);
	config->writePathEntry(DEF_SOUND_FILE, mDefaultSoundFile);
	config->writeEntry(DEF_SOUND_VOLUME, static_cast<double>(mDefaultSoundVolume));
	config->writeEntry(DEF_SOUND_REPEAT, mDefaultSoundRepeat);
	config->writeEntry(DEF_CMD_SCRIPT, mDefaultCmdScript);
	config->writeEntry(DEF_CMD_XTERM, mDefaultCmdXterm);
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config->writeEntry(DEF_RECUR_PERIOD, mDefaultRecurPeriod);
	config->writeEntry(DEF_REMIND_UNITS, mDefaultReminderUnits);
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
	emit preferencesChanged();
	if (mStartOfDay != mOldStartOfDay)
	{
		emit startOfDayChanged(mOldStartOfDay);
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

QString Preferences::emailFrom(Preferences::MailFrom from, bool useAddress, bool bcc) const
{
	switch (from)
	{
		case MAIL_FROM_KMAIL:
			return FROM_KMAIL;
#if KDE_VERSION >= 210
		case MAIL_FROM_CONTROL_CENTRE:
			return FROM_CONTROL_CENTRE;
#endif
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
#if KDE_VERSION >= 210
	if (str == FROM_CONTROL_CENTRE)
		return MAIL_FROM_CONTROL_CENTRE;
#endif
	return MAIL_FROM_ADDR;
}

/******************************************************************************
* Get user's default 'From' email address.
*/
QString Preferences::emailAddress() const
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

QString Preferences::emailBccAddress() const
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
#if KDE_VERSION >= 210
		case MAIL_FROM_CONTROL_CENTRE:
			mEmailAddress = KAMail::controlCentreAddress();
			break;
#endif
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
#if KDE_VERSION < 210
	if (useControlCentre)
		return;
#endif
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
	QMap<QString, QString> entries = config->entryMap(GENERAL_SECTION);
	if (entries.find(EMAIL_FROM) == entries.end()
	&&  entries.find(EMAIL_USE_CONTROL_CENTRE) != entries.end())
	{
		// Preferences were written by KAlarm pre-1.2.1
		config->setGroup(GENERAL_SECTION);
		bool useCC = false;
		bool bccUseCC = false;
#if KDE_VERSION >= 210
		const bool default_emailUseControlCentre    = true;
		const bool default_emailBccUseControlCentre = true;
		useCC = config->readBoolEntry(EMAIL_USE_CONTROL_CENTRE, default_emailUseControlCentre);
		// EmailBccUseControlCenter was missing in preferences written by KAlarm pre-0.9.5
		bccUseCC = config->hasKey(EMAIL_BCC_USE_CONTROL_CENTRE)
		         ? config->readBoolEntry(EMAIL_BCC_USE_CONTROL_CENTRE, default_emailBccUseControlCentre)
			 : useCC;
#endif
		config->writeEntry(EMAIL_FROM, (useCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_ADDRESS)));
		config->writeEntry(EMAIL_BCC_ADDRESS, (bccUseCC ? FROM_CONTROL_CENTRE : config->readEntry(EMAIL_BCC_ADDRESS)));
		config->deleteEntry(EMAIL_ADDRESS);
		config->deleteEntry(EMAIL_BCC_USE_CONTROL_CENTRE);
		config->deleteEntry(EMAIL_USE_CONTROL_CENTRE);
		config->sync();
	}
}
