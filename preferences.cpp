/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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
#include <kemailsettings.h>
#include <kapplication.h>
#include <kglobalsettings.h>

#include "preferences.moc"

Preferences* Preferences::mInstance = 0;

// Default config file settings
QColor defaultMessageColours[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::white, Qt::lightGray, Qt::black, QColor() };
const ColourList Preferences::default_messageColours(defaultMessageColours);
const QColor     Preferences::default_defaultBgColour(Qt::red);
const QColor     Preferences::default_defaultFgColour(Qt::black);
QFont            Preferences::default_messageFont;    // initialised in constructor
const QTime      Preferences::default_startOfDay(0, 0);
const bool       Preferences::default_runInSystemTray          = true;
const bool       Preferences::default_disableAlarmsIfStopped   = true;
const bool       Preferences::default_quitWarn                 = true;
const bool       Preferences::default_autostartTrayIcon        = true;
const bool       Preferences::default_confirmAlarmDeletion     = true;
const bool       Preferences::default_modalMessages            = true;
const int        Preferences::default_messageButtonDelay       = 0;
const bool       Preferences::default_showExpiredAlarms        = false;
const bool       Preferences::default_showAlarmTime            = true;
const bool       Preferences::default_showTimeToAlarm          = false;
const int        Preferences::default_tooltipAlarmCount        = 5;
const bool       Preferences::default_showTooltipAlarmTime     = true;
const bool       Preferences::default_showTooltipTimeToAlarm   = true;
const QString    Preferences::default_tooltipTimeToPrefix      = QString::fromLatin1("+");
const int        Preferences::default_daemonTrayCheckInterval  = 10;     // (seconds)
const bool       Preferences::default_emailQueuedNotify        = false;
#if KDE_VERSION >= 210
const bool       Preferences::default_emailUseControlCentre    = true;
const bool       Preferences::default_emailBccUseControlCentre = true;
#else
const bool       Preferences::default_emailUseControlCentre    = false;
const bool       Preferences::default_emailBccUseControlCentre = false;
#endif
const QColor     Preferences::default_expiredColour(darkRed);
const int        Preferences::default_expiredKeepDays          = 7;
const QString    Preferences::default_defaultSoundFile         = QString::null;
const float      Preferences::default_defaultSoundVolume       = -1;
const bool       Preferences::default_defaultSound             = false;
const bool       Preferences::default_defaultSoundRepeat       = false;
const bool       Preferences::default_defaultBeep              = false;
const bool       Preferences::default_defaultLateCancel        = false;
const bool       Preferences::default_defaultConfirmAck        = false;
const bool       Preferences::default_defaultEmailBcc          = false;
const QString    Preferences::default_emailAddress             = QString::null;
const QString    Preferences::default_emailBccAddress          = QString::null;
const Preferences::MailClient    Preferences::default_emailClient          = KMAIL;
const Preferences::Feb29Type     Preferences::default_feb29RecurType       = FEB29_MAR1;
const RecurrenceEdit::RepeatType Preferences::default_defaultRecurPeriod   = RecurrenceEdit::NO_RECUR;
const TimeSelector::Units        Preferences::default_defaultReminderUnits = TimeSelector::HOURS_MINUTES;
const QString    Preferences::default_defaultPreAction;
const QString    Preferences::default_defaultPostAction;

static const QString    defaultFeb29RecurType = QString::fromLatin1("Mar1");
static const QString    defaultEmailClient    = QString::fromLatin1("kmail");

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
static const QString MESSAGE_BUTTON_DELAY     = QString::fromLatin1("MessageButtonDelay");
static const QString SHOW_EXPIRED_ALARMS      = QString::fromLatin1("ShowExpiredAlarms");
static const QString SHOW_ALARM_TIME          = QString::fromLatin1("ShowAlarmTime");
static const QString SHOW_TIME_TO_ALARM       = QString::fromLatin1("ShowTimeToAlarm");
static const QString TOOLTIP_ALARM_COUNT      = QString::fromLatin1("TooltipAlarmCount");
static const QString TOOLTIP_ALARM_TIME       = QString::fromLatin1("ShowTooltipAlarmTime");
static const QString TOOLTIP_TIME_TO_ALARM    = QString::fromLatin1("ShowTooltipTimeToAlarm");
static const QString TOOLTIP_TIME_TO_PREFIX   = QString::fromLatin1("TooltipTimeToPrefix");
static const QString DAEMON_TRAY_INTERVAL     = QString::fromLatin1("DaemonTrayCheckInterval");
static const QString EMAIL_CLIENT             = QString::fromLatin1("EmailClient");
static const QString EMAIL_QUEUED_NOTIFY      = QString::fromLatin1("EmailQueuedNotify");
static const QString EMAIL_USE_CONTROL_CENTRE = QString::fromLatin1("EmailUseControlCenter");
static const QString EMAIL_BCC_USE_CONTROL_CENTRE = QString::fromLatin1("EmailBccUseControlCenter");
static const QString EMAIL_ADDRESS            = QString::fromLatin1("EmailAddress");
static const QString EMAIL_BCC_ADDRESS        = QString::fromLatin1("EmailBccAddress");
static const QString START_OF_DAY             = QString::fromLatin1("StartOfDay");
static const QString START_OF_DAY_CHECK       = QString::fromLatin1("Sod");
static const QString EXPIRED_COLOUR           = QString::fromLatin1("ExpiredColour");
static const QString EXPIRED_KEEP_DAYS        = QString::fromLatin1("ExpiredKeepDays");
static const QString DEFAULTS_SECTION         = QString::fromLatin1("Defaults");
static const QString DEF_LATE_CANCEL          = QString::fromLatin1("DefLateCancel");
static const QString DEF_CONFIRM_ACK          = QString::fromLatin1("DefConfirmAck");
static const QString DEF_SOUND                = QString::fromLatin1("DefSound");
static const QString DEF_SOUND_FILE           = QString::fromLatin1("DefSoundFile");
static const QString DEF_SOUND_VOLUME         = QString::fromLatin1("DefSoundVolume");
static const QString DEF_SOUND_REPEAT         = QString::fromLatin1("DefSoundRepeat");
static const QString DEF_BEEP                 = QString::fromLatin1("DefBeep");
static const QString DEF_EMAIL_BCC            = QString::fromLatin1("DefEmailBcc");
static const QString DEF_RECUR_PERIOD         = QString::fromLatin1("DefRecurPeriod");
static const QString DEF_REMIND_UNITS         = QString::fromLatin1("DefRemindUnits");
static const QString DEF_PRE_ACTION           = QString::fromLatin1("DefPreAction");
static const QString DEF_POST_ACTION          = QString::fromLatin1("DefPostAction");

// Config file entry names for notification messages
const QString Preferences::QUIT_WARN              = QString::fromLatin1("QuitWarn");
const QString Preferences::CONFIRM_ALARM_DELETION = QString::fromLatin1("ConfirmAlarmDeletion");

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
		mInstance = new Preferences;
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
	mDefaultBgColour         = config->readColorEntry(MESSAGE_BG_COLOUR, &default_defaultBgColour);
	mMessageFont             = config->readFontEntry(MESSAGE_FONT, &default_messageFont);
	mRunInSystemTray         = config->readBoolEntry(RUN_IN_SYSTEM_TRAY, default_runInSystemTray);
	mDisableAlarmsIfStopped  = config->readBoolEntry(DISABLE_IF_STOPPED, default_disableAlarmsIfStopped);
	mAutostartTrayIcon       = config->readBoolEntry(AUTOSTART_TRAY, default_autostartTrayIcon);
	QCString feb29           = config->readEntry(FEB29_RECUR_TYPE, defaultFeb29RecurType).local8Bit();
	mFeb29RecurType          = (feb29 == "Mar1") ? FEB29_MAR1 : (feb29 == "Feb28") ? FEB29_FEB28 : FEB29_NONE;
	mModalMessages           = config->readBoolEntry(MODAL_MESSAGES, default_modalMessages);
	mMessageButtonDelay      = config->readNumEntry(MESSAGE_BUTTON_DELAY, default_messageButtonDelay);
	mShowExpiredAlarms       = config->readBoolEntry(SHOW_EXPIRED_ALARMS, default_showExpiredAlarms);
	mShowTimeToAlarm         = config->readBoolEntry(SHOW_TIME_TO_ALARM, default_showTimeToAlarm);
	mShowAlarmTime           = !mShowTimeToAlarm ? true : config->readBoolEntry(SHOW_ALARM_TIME, default_showAlarmTime);
	mTooltipAlarmCount       = config->readNumEntry(TOOLTIP_ALARM_COUNT, default_tooltipAlarmCount);
	mShowTooltipAlarmTime    = config->readBoolEntry(TOOLTIP_ALARM_TIME, default_showTooltipAlarmTime);
	mShowTooltipTimeToAlarm  = config->readBoolEntry(TOOLTIP_TIME_TO_ALARM, default_showTooltipTimeToAlarm);
	mTooltipTimeToPrefix     = config->readEntry(TOOLTIP_TIME_TO_PREFIX, default_tooltipTimeToPrefix);
	mDaemonTrayCheckInterval = config->readNumEntry(DAEMON_TRAY_INTERVAL, default_daemonTrayCheckInterval);
	QCString client          = config->readEntry(EMAIL_CLIENT, defaultEmailClient).local8Bit();
	mEmailClient             = (client == "sendmail" ? SENDMAIL : KMAIL);
	mEmailQueuedNotify       = config->readBoolEntry(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
	bool bccFrom             = config->hasKey(EMAIL_USE_CONTROL_CENTRE) && !config->hasKey(EMAIL_BCC_USE_CONTROL_CENTRE);
#if KDE_VERSION >= 210
	mEmailUseControlCentre   = config->readBoolEntry(EMAIL_USE_CONTROL_CENTRE, default_emailUseControlCentre);
	mEmailBccUseControlCentre = bccFrom ? mEmailUseControlCentre      // compatibility with pre-0.9.5
	                          : config->readBoolEntry(EMAIL_BCC_USE_CONTROL_CENTRE, default_emailBccUseControlCentre);
	if (mEmailUseControlCentre || mEmailBccUseControlCentre)
	{
		KEMailSettings e;
		mEmailAddress = mEmailBccAddress = e.getSetting(KEMailSettings::EmailAddress);
	}
#else
	mEmailUseControlCentre = mEmailBccUseControlCentre = false;
#endif
	if (!mEmailUseControlCentre)
		mEmailAddress = config->readEntry(EMAIL_ADDRESS);
	if (!mEmailBccUseControlCentre)
		mEmailBccAddress = config->readEntry(EMAIL_BCC_ADDRESS);
	QDateTime defStartOfDay(QDate(1900,1,1), default_startOfDay);
	mStartOfDay              = config->readDateTimeEntry(START_OF_DAY, &defStartOfDay).time();
	mOldStartOfDay.setHMS(0,0,0);
	int sod = config->readNumEntry(START_OF_DAY_CHECK, 0);
	if (sod)
		mOldStartOfDay = mOldStartOfDay.addMSecs(sod ^ SODxor);
	mExpiredColour           = config->readColorEntry(EXPIRED_COLOUR, &default_expiredColour);
	mExpiredKeepDays         = config->readNumEntry(EXPIRED_KEEP_DAYS, default_expiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	mDefaultLateCancel       = config->readBoolEntry(DEF_LATE_CANCEL, default_defaultLateCancel);
	mDefaultConfirmAck       = config->readBoolEntry(DEF_CONFIRM_ACK, default_defaultConfirmAck);
	mDefaultSound            = config->readBoolEntry(DEF_SOUND, default_defaultSound);
	mDefaultBeep             = config->readBoolEntry(DEF_BEEP, default_defaultBeep);
	mDefaultSoundVolume      = static_cast<float>(config->readDoubleNumEntry(DEF_SOUND_VOLUME, default_defaultSoundVolume));
#ifdef WITHOUT_ARTS
	mDefaultSoundRepeat      = false;
#else
	mDefaultSoundRepeat      = config->readBoolEntry(DEF_SOUND_REPEAT, default_defaultSoundRepeat);
#endif
	mDefaultSoundFile        = config->readPathEntry(DEF_SOUND_FILE);
	mDefaultEmailBcc         = config->readBoolEntry(DEF_EMAIL_BCC, default_defaultEmailBcc);
	int recurPeriod          = config->readNumEntry(DEF_RECUR_PERIOD, default_defaultRecurPeriod);
	mDefaultRecurPeriod      = (recurPeriod < RecurrenceEdit::SUBDAILY || recurPeriod > RecurrenceEdit::ANNUAL)
	                         ? default_defaultRecurPeriod : (RecurrenceEdit::RepeatType)recurPeriod;
	int reminderUnits        = config->readNumEntry(DEF_REMIND_UNITS, default_defaultReminderUnits);
	mDefaultReminderUnits    = (reminderUnits < TimeSelector::HOURS_MINUTES || reminderUnits > TimeSelector::WEEKS)
	                         ? default_defaultReminderUnits : (TimeSelector::Units)reminderUnits;
	mDefaultPreAction        = config->readEntry(DEF_PRE_ACTION, default_defaultPreAction);
	mDefaultPostAction       = config->readEntry(DEF_POST_ACTION, default_defaultPostAction);
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
	config->writeEntry(EMAIL_QUEUED_NOTIFY, mEmailQueuedNotify);
	config->writeEntry(EMAIL_USE_CONTROL_CENTRE, mEmailUseControlCentre);
	config->writeEntry(EMAIL_BCC_USE_CONTROL_CENTRE, mEmailBccUseControlCentre);
	config->writeEntry(EMAIL_ADDRESS, (mEmailUseControlCentre ? QString() : mEmailAddress));
	config->writeEntry(EMAIL_BCC_ADDRESS, (mEmailBccUseControlCentre ? QString() : mEmailBccAddress));
	config->writeEntry(START_OF_DAY, QDateTime(QDate(1900,1,1), mStartOfDay));
	// Start-of-day check value is only written once the start-of-day time has been processed.
	config->writeEntry(EXPIRED_COLOUR, mExpiredColour);
	config->writeEntry(EXPIRED_KEEP_DAYS, mExpiredKeepDays);
	config->setGroup(DEFAULTS_SECTION);
	config->writeEntry(DEF_LATE_CANCEL, mDefaultLateCancel);
	config->writeEntry(DEF_CONFIRM_ACK, mDefaultConfirmAck);
	config->writeEntry(DEF_BEEP, mDefaultBeep);
	config->writeEntry(DEF_SOUND, mDefaultSound);
	config->writePathEntry(DEF_SOUND_FILE, mDefaultSoundFile);
	config->writeEntry(DEF_SOUND_VOLUME, static_cast<double>(mDefaultSoundVolume));
	config->writeEntry(DEF_SOUND_REPEAT, mDefaultSoundRepeat);
	config->writeEntry(DEF_EMAIL_BCC, mDefaultEmailBcc);
	config->writeEntry(DEF_RECUR_PERIOD, mDefaultRecurPeriod);
	config->writeEntry(DEF_REMIND_UNITS, mDefaultReminderUnits);
	config->writeEntry(DEF_PRE_ACTION, mDefaultPreAction);
	config->writeEntry(DEF_POST_ACTION, mDefaultPostAction);
	if (syncToDisc)
		config->sync();
	emit preferencesChanged();
	if (mStartOfDay != mOldStartOfDay)
	{
		emit startOfDayChanged(mOldStartOfDay);
		mOldStartOfDay = mStartOfDay;
	}
}

void Preferences::syncToDisc()
{
	KConfig* config = KGlobal::config();
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

bool Preferences::quitWarn() const
{
	// It's important to reinstate Quit warnings if default answer is Don't Quit.
	return const_cast<Preferences*>(this)->validateQuitWarn();
}

bool Preferences::validateQuitWarn()
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	QString dontask = config->readEntry(QUIT_WARN).lower();
	if (dontask == QString::fromLatin1("no"))
	{
		// Notification has been suppressed, and No (i.e. don't quit) is the default.
		// This effectively disables the Quit menu option, which is clearly undesirable,
		// so unsuppress notification.
		setQuitWarn(true);
		return true;
	}
	return dontask != QString::fromLatin1("yes");
}

void Preferences::setQuitWarn(bool yes)
{
	return setNotify(QUIT_WARN, true, yes);
}

bool Preferences::confirmAlarmDeletion() const
{
	return notifying(CONFIRM_ALARM_DELETION, false);
}

void Preferences::setConfirmAlarmDeletion(bool yes)
{
	return setNotify(CONFIRM_ALARM_DELETION, false, yes);
}

QString Preferences::emailAddress() const
{
	if (mEmailUseControlCentre)
	{
		KEMailSettings e;
		return e.getSetting(KEMailSettings::EmailAddress);
	}
	else
		return mEmailAddress;
}

QString Preferences::emailBccAddress() const
{
	if (mEmailBccUseControlCentre)
	{
		KEMailSettings e;
		return e.getSetting(KEMailSettings::EmailAddress);
	}
	else
		return mEmailBccAddress;
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

void Preferences::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	mEmailBccUseControlCentre = useControlCentre;
	if (useControlCentre)
	{
		KEMailSettings e;
		mEmailBccAddress = e.getSetting(KEMailSettings::EmailAddress);
	}
	else
		mEmailBccAddress = address;
}

/******************************************************************************
* Called to allow output of the specified message dialog again, where the
* dialog has a checkbox to turn notification off.
* Set 'yesNoMessage' true if the message is used in a KMessageBox::*YesNo*() call.
*/
void Preferences::setNotify(const QString& messageID, bool yesNoMessage, bool notify)
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	if (yesNoMessage)
		config->writeEntry(messageID, QString::fromLatin1(notify ? "" : "yes"));
	else
		config->writeEntry(messageID, notify);
	config->sync();
}

/******************************************************************************
* Return whether the specified message dialog is output, where the dialog has
* a checkbox to turn notification off.
* Set 'yesNoMessage' true if the message is used in a KMessageBox::*YesNo*() call.
* Reply = false if message has been suppressed (by preferences or by selecting
*                  "don't ask again")
*       = true in all other cases.
*/
bool Preferences::notifying(const QString& messageID, bool yesNoMessage)
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	if (yesNoMessage)
		return config->readEntry(messageID).lower() != QString::fromLatin1("yes");
	else
		return config->readBoolEntry(messageID, true);
}
