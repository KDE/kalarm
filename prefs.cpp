/*
 *  prefs.cpp  -  program preferences
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

#include "kalarm.h"

#include <qobjectlist.h>
#include <qlayout.h>
#include <qbuttongroup.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qwhatsthis.h>
#include <qstyle.h>

#include <kdialog.h>
#include <kglobal.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kdebug.h>

#include "fontcolour.h"
#include "datetime.h"
#include "prefsettings.h"
#include "prefs.moc"


PrefsTabBase::PrefsTabBase(QVBox* frame)
	: mPage(frame)
{
	mPage->setMargin(KDialog::marginHint());
}

void PrefsTabBase::setSettings(Settings* setts)
{
	mSettings = setts;
	restore();
}

void PrefsTabBase::apply(bool syncToDisc)
{
	mSettings->saveSettings(syncToDisc);
	mSettings->emitSettingsChanged();
}



MiscPrefTab::MiscPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QGroupBox* group = new QButtonGroup(i18n("Run Mode"), mPage, "modeGroup");
	QGridLayout* grid = new QGridLayout(group, 6, 2, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(0, 0);
	grid->setColStretch(1, 2);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	int row = 1;

	// Run-in-system-tray radio button has an ID of 0
	mRunInSystemTray = new QRadioButton(i18n("Run continuously in system &tray"), group, "runTray");
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), this, SLOT(slotRunModeToggled(bool)));
	QWhatsThis::add(mRunInSystemTray,
	      i18n("Check to run %1 continuously in the KDE system tray.\n\n"
	           "Notes:\n"
	           "1. With this option selected, closing the system tray icon will quit %2.\n"
	           "2. You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon. Running in the system tray simply provides easy access and a status indication.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mRunInSystemTray, row, row, 0, 1, AlignLeft);

	mAutostartTrayIcon1 = new QCheckBox(i18n("Autostart at &login"), group, "autoTray");
	mAutostartTrayIcon1->setFixedSize(mAutostartTrayIcon1->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon1,
	      i18n("Check to run %1 whenever you start KDE.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mAutostartTrayIcon1, ++row, 1, AlignLeft);

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disa&ble alarms while not running"), group, "disableAl");
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	QWhatsThis::add(mDisableAlarmsIfStopped,
	      i18n("Check to disable alarms whenever %1 is not running. Alarms will only appear while the system tray icon is visible.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mDisableAlarmsIfStopped, ++row, 1, AlignLeft);

	// Run-on-demand radio button has an ID of 3
	mRunOnDemand = new QRadioButton(i18n("&Run only on demand"), group, "runDemand");
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), this, SLOT(slotRunModeToggled(bool)));
	QWhatsThis::add(mRunOnDemand,
	      i18n("Check to run %1 only when required.\n\n"
	           "Notes:\n"
	           "1. Alarms are displayed even when %2 is not running, since alarm monitoring is done by the alarm daemon.\n"
	           "2. With this option selected, the system tray icon can be displayed or hidden independently of %3.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	++row;
	grid->addMultiCellWidget(mRunOnDemand, row, row, 0, 1, AlignLeft);

	mAutostartTrayIcon2 = new QCheckBox(i18n("Autostart system tray &icon at login"), group, "autoRun");
	mAutostartTrayIcon2->setFixedSize(mAutostartTrayIcon2->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon2,
	      i18n("Check to display the system tray icon whenever you start KDE."));
	grid->addWidget(mAutostartTrayIcon2, ++row, 1, AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	QHBox* box = new QHBox(mPage);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("System tray icon &update interval:"), box);
	box->setStretchFactor(label, 1);
	mDaemonTrayCheckInterval = new QSpinBox(1, 9999, 1, box, "daemonCheck");
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	QWhatsThis::add(mDaemonTrayCheckInterval,
	      i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18n("seconds"), box);
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("&Start of day for date-only alarms:"), box);
	box->setStretchFactor(label, 1);
	mStartOfDay = new TimeSpinBox(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	QWhatsThis::add(mStartOfDay,
	      i18n("The earliest time of day at which a date-only alarm (i.e. an alarm with \"any time\" specified) will be triggered."));
	label->setBuddy(mStartOfDay);
	box->setFixedHeight(box->sizeHint().height());

#ifdef KALARM_EMAIL
	box = new QHBox(mPage);
	box->setSpacing(2*KDialog::spacingHint());
	label = new QLabel(i18n("Email client:"), box);
	box->setStretchFactor(label, 1);
	mEmailClient = new QButtonGroup(box);
	mEmailClient->hide();
	QRadioButton* radio = new QRadioButton(i18n("&KMail"), box, "kmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Settings::KMAIL);
	radio = new QRadioButton(i18n("S&endmail"), box, "sendmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Settings::SENDMAIL);
	box->setFixedHeight(box->sizeHint().height());
	QWhatsThis::add(box,
	      i18n("Choose how to send email when an email alarm is triggered.\n"
	           "KMail: A KMail composer window is displayed to enable you to send the email.\n"
	           "Sendmail: The email is sent automatically. This option will only work if your system is configured to use 'sendmail' or 'mail'."));
#endif

	mConfirmAlarmDeletion = new QCheckBox(i18n("Con&firm alarm deletions"), mPage, "confirmDeletion");
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	QWhatsThis::add(mConfirmAlarmDeletion,
	      i18n("Check to be prompted for confirmation each time you delete an alarm."));

	box = new QHBox(mPage);   // top-adjust all the widgets
}

void MiscPrefTab::restore()
{
	bool systray = mSettings->mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(mSettings->mDisableAlarmsIfStopped);
	mAutostartTrayIcon1->setChecked(mSettings->mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(mSettings->mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(mSettings->mConfirmAlarmDeletion);
	mDaemonTrayCheckInterval->setValue(mSettings->mDaemonTrayCheckInterval);
	mStartOfDay->setValue(mSettings->mStartOfDay.hour()*60 + mSettings->mStartOfDay.minute());
}

void MiscPrefTab::apply(bool syncToDisc)
{
	bool systray = mRunInSystemTray->isChecked();
	mSettings->mRunInSystemTray         = systray;
	mSettings->mDisableAlarmsIfStopped  = mDisableAlarmsIfStopped->isChecked();
	mSettings->mAutostartTrayIcon       = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	mSettings->mConfirmAlarmDeletion    = mConfirmAlarmDeletion->isChecked();
	mSettings->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	int sod = mStartOfDay->value();
	mSettings->mStartOfDay.setHMS(sod/60, sod%60, 0);
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::setDefaults()
{
	bool systray = Settings::default_runInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Settings::default_disableAlarmsIfStopped);
	mAutostartTrayIcon1->setChecked(Settings::default_autostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Settings::default_autostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Settings::default_confirmAlarmDeletion);
	mDaemonTrayCheckInterval->setValue(Settings::default_daemonTrayCheckInterval);
	mStartOfDay->setValue(Settings::default_startOfDay.hour()*60 + Settings::default_startOfDay.minute());
}

void MiscPrefTab::slotRunModeToggled(bool)
{
	bool systray = (mRunInSystemTray->isOn());
	mAutostartTrayIcon2->setEnabled(!systray);
	mAutostartTrayIcon1->setEnabled(systray);
	mDisableAlarmsIfStopped->setEnabled(systray);
}


AppearancePrefTab::AppearancePrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	mFontChooser = new FontColourChooser(mPage, 0L, false, QStringList(), true, i18n("Font && Color"), false);
}

void AppearancePrefTab::restore()
{
	mFontChooser->setBgColour(mSettings->mDefaultBgColour);
	mFontChooser->setFont(mSettings->mMessageFont);
}

void AppearancePrefTab::apply(bool syncToDisc)
{
	mSettings->mDefaultBgColour = mFontChooser->bgColour();
	mSettings->mMessageFont     = mFontChooser->font();
	PrefsTabBase::apply(syncToDisc);
}

void AppearancePrefTab::setDefaults()
{
	mFontChooser->setBgColour(Settings::default_defaultBgColour);
	mFontChooser->setFont(Settings::default_messageFont);
}


DefaultPrefTab::DefaultPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QString defsetting = i18n("The default setting for \"%1\" in the alarm edit dialog.");

	QString setting = i18n("Cancel if &late");
	mDefaultLateCancel = new QCheckBox(setting, mPage, "defCancelLate");
	mDefaultLateCancel->setMinimumSize(mDefaultLateCancel->sizeHint());
	QWhatsThis::add(mDefaultLateCancel, defsetting.arg(setting));

	setting = i18n("Confirm ac&knowledgement");
	mDefaultConfirmAck = new QCheckBox(setting, mPage, "defConfAck");
	mDefaultConfirmAck->setMinimumSize(mDefaultConfirmAck->sizeHint());
	QWhatsThis::add(mDefaultConfirmAck, defsetting.arg(setting));

	mDefaultBeep = new QCheckBox(i18n("&Beep"), mPage, "defBeep");
	mDefaultBeep->setMinimumSize(mDefaultBeep->sizeHint());
	QWhatsThis::add(mDefaultBeep,
	      i18n("Check to select Beep as the default setting for \"Sound\" in the alarm edit dialog."));

#ifdef KALARM_EMAIL
	// BCC email to sender
	setting = i18n("Copy email to &self");
	mDefaultEmailBcc = new QCheckBox(setting, mPage, "defEmailBcc");
	mDefaultEmailBcc->setMinimumSize(mDefaultEmailBcc->sizeHint());
	QWhatsThis::add(mDefaultEmailBcc, defsetting.arg(setting));
#endif

	QHBox* box = new QHBox(mPage);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("Recurrence &period:"), box);
	label->setFixedSize(label->sizeHint());
	mDefaultRecurPeriod = new QComboBox(box, "defRecur");
	mDefaultRecurPeriod->insertItem(i18n("Hours/Minutes"));
	mDefaultRecurPeriod->insertItem(i18n("Days"));
	mDefaultRecurPeriod->insertItem(i18n("Weeks"));
	mDefaultRecurPeriod->insertItem(i18n("Months"));
	mDefaultRecurPeriod->insertItem(i18n("Years"));
	mDefaultRecurPeriod->setFixedSize(mDefaultRecurPeriod->sizeHint());
	QWhatsThis::add(mDefaultRecurPeriod,
	      i18n("The default setting for the recurrence period in the alarm edit dialog."));
	label->setBuddy(mDefaultRecurPeriod);
	label = new QLabel(box);   // dummy to left-adjust the controls
	box->setStretchFactor(label, 1);
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // top-adjust all the widgets
}

void DefaultPrefTab::restore()
{
	mDefaultLateCancel->setChecked(mSettings->mDefaultLateCancel);
	mDefaultConfirmAck->setChecked(mSettings->mDefaultConfirmAck);
	mDefaultBeep->setChecked(mSettings->mDefaultBeep);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(mSettings->mDefaultRecurPeriod));
}

void DefaultPrefTab::apply(bool syncToDisc)
{
	mSettings->mDefaultLateCancel = mDefaultLateCancel->isChecked();
	mSettings->mDefaultConfirmAck = mDefaultConfirmAck->isChecked();
	mSettings->mDefaultBeep       = mDefaultBeep->isChecked();
	switch (mDefaultRecurPeriod->currentItem())
	{
		case 4:  mSettings->mDefaultRecurPeriod = RecurrenceEdit::ANNUAL;    break;
		case 3:  mSettings->mDefaultRecurPeriod = RecurrenceEdit::MONTHLY;   break;
		case 2:  mSettings->mDefaultRecurPeriod = RecurrenceEdit::WEEKLY;    break;
		case 1:  mSettings->mDefaultRecurPeriod = RecurrenceEdit::DAILY;     break;
		case 0:
		default: mSettings->mDefaultRecurPeriod = RecurrenceEdit::SUBDAILY;  break;
	}
	PrefsTabBase::apply(syncToDisc);
}

void DefaultPrefTab::setDefaults()
{
	mDefaultLateCancel->setChecked(mSettings->default_defaultLateCancel);
	mDefaultConfirmAck->setChecked(mSettings->default_defaultConfirmAck);
	mDefaultBeep->setChecked(mSettings->default_defaultBeep);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(mSettings->default_defaultRecurPeriod));
}

int DefaultPrefTab::recurIndex(RecurrenceEdit::RepeatType type)
{
	switch (type)
	{
		case RecurrenceEdit::ANNUAL:   return 4;
		case RecurrenceEdit::MONTHLY:  return 3;
		case RecurrenceEdit::WEEKLY:   return 2;
		case RecurrenceEdit::DAILY:    return 1;
		case RecurrenceEdit::SUBDAILY:
		default:                       return 0;
	}
}
