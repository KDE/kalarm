/*
 *  prefdlg.cpp  -  program preferences dialog
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qobjectlist.h>
#include <qlayout.h>
#include <qbuttongroup.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qstyle.h>

#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kfiledialog.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kcolorcombo.h>
#include <kdebug.h>

#include "fontcolour.h"
#include "alarmtimewidget.h"
#include "timespinbox.h"
#include "preferences.h"
#include "alarmcalendar.h"
#include "traywindow.h"
#include "kalarmapp.h"
#include "prefdlg.moc"


/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg::KAlarmPrefDlg(Preferences* sets)
   : KDialogBase(IconList, i18n("Preferences"), Help | Default | Ok | Apply | Cancel, Ok, 0, 0, true, true)
{
   setIconListAllVisible(true);

   QVBox* frame = addVBoxPage(i18n("General"), i18n("General"), DesktopIcon("misc"));
   mMiscPage = new MiscPrefTab(frame);
   mMiscPage->setPreferences(sets);

   frame = addVBoxPage(i18n("Alarm Defaults"), i18n("Default Alarm Settings"), DesktopIcon("edit"));
   mDefaultPage = new DefaultPrefTab(frame);
   mDefaultPage->setPreferences(sets);

   frame = addVBoxPage(i18n("Appearance"), i18n("Default Message Appearance"), DesktopIcon("colorize"));
   mAppearancePage = new AppearancePrefTab(frame);
   mAppearancePage->setPreferences(sets);

   adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

// Restore all defaults in the options...
void KAlarmPrefDlg::slotDefault()
{
   kdDebug(5950) << "KAlarmPrefDlg::slotDefault()" << endl;
   mAppearancePage->setDefaults();
   mDefaultPage->setDefaults();
   mMiscPage->setDefaults();
}

void KAlarmPrefDlg::slotHelp()
{
   kapp->invokeHelp("preferences");
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
   kdDebug(5950) << "KAlarmPrefDlg::slotApply()" << endl;
   mAppearancePage->apply(false);
   mDefaultPage->apply(false);
   mMiscPage->apply(true);
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
   kdDebug(5950) << "KAlarmPrefDlg::slotOk()" << endl;
   slotApply();
   KDialogBase::slotOk();
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::slotCancel()
{
   kdDebug(5950) << "KAlarmPrefDlg::slotCancel()" << endl;
   mAppearancePage->restore();
   mDefaultPage->restore();
   mMiscPage->restore();

   KDialogBase::slotCancel();
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/

PrefsTabBase::PrefsTabBase(QVBox* frame)
	: mPage(frame)
{
	mPage->setMargin(KDialog::marginHint());
}

void PrefsTabBase::setPreferences(Preferences* setts)
{
	mPreferences = setts;
	restore();
}

void PrefsTabBase::apply(bool syncToDisc)
{
	mPreferences->savePreferences(syncToDisc);
	mPreferences->emitPreferencesChanged();
}



/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QGroupBox* group = new QButtonGroup(i18n("Run Mode"), mPage, "modeGroup");
	QGridLayout* grid = new QGridLayout(group, 6, 3, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(2, 1);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	grid->addColSpacing(1, 3*KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	int row = 1;

	// Run-in-system-tray radio button has an ID of 0
	mRunInSystemTray = new QRadioButton(i18n("Run continuously in system &tray"), group, "runTray");
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	QWhatsThis::add(mRunInSystemTray,
	      i18n("Check to run %1 continuously in the KDE system tray.\n\n"
	           "Notes:\n"
	           "1. With this option selected, closing the system tray icon will quit %2.\n"
	           "2. You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon. Running in the system tray simply provides easy access and a status indication.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mRunInSystemTray, row, row, 0, 2, AlignLeft);
	++row;

	mAutostartTrayIcon1 = new QCheckBox(i18n("Autostart at &login"), group, "autoTray");
	mAutostartTrayIcon1->setFixedSize(mAutostartTrayIcon1->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon1,
	      i18n("Check to run %1 whenever you start KDE.").arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mAutostartTrayIcon1, row, row, 1, 2, AlignLeft);
	++row;

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disa&ble alarms while not running"), group, "disableAl");
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	connect(mDisableAlarmsIfStopped, SIGNAL(toggled(bool)), SLOT(slotDisableIfStoppedToggled(bool)));
	QWhatsThis::add(mDisableAlarmsIfStopped,
	      i18n("Check to disable alarms whenever %1 is not running. Alarms will only appear while the system tray icon is visible.").arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mDisableAlarmsIfStopped, row, row, 1, 2, AlignLeft);
	++row;

	mQuitWarn = new QCheckBox(i18n("Warn before quitting"), group, "disableAl");
	mQuitWarn->setFixedSize(mQuitWarn->sizeHint());
	QWhatsThis::add(mQuitWarn,
	      i18n("Check to display a warning prompt before quitting %1.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mQuitWarn, row, 2, AlignLeft);
	++row;

	// Run-on-demand radio button has an ID of 3
	mRunOnDemand = new QRadioButton(i18n("&Run only on demand"), group, "runDemand");
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	QWhatsThis::add(mRunOnDemand,
	      i18n("Check to run %1 only when required.\n\n"
	           "Notes:\n"
	           "1. Alarms are displayed even when %2 is not running, since alarm monitoring is done by the alarm daemon.\n"
	           "2. With this option selected, the system tray icon can be displayed or hidden independently of %3.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mRunOnDemand, row, row, 0, 2, AlignLeft);
	++row;

	mAutostartTrayIcon2 = new QCheckBox(i18n("Autostart system tray &icon at login"), group, "autoRun");
	mAutostartTrayIcon2->setFixedSize(mAutostartTrayIcon2->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon2,
	      i18n("Check to display the system tray icon whenever you start KDE."));
	grid->addMultiCellWidget(mAutostartTrayIcon2, row, row, 1, 2, AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	QHBox* itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	QHBox* box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("System tray icon &update interval:"), box);
	mDaemonTrayCheckInterval = new QSpinBox(1, 9999, 1, box, "daemonCheck");
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18n("seconds"), box);
	QWhatsThis::add(box,
	      i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("&Start of day for date-only alarms:"), box);
	mStartOfDay = new TimeSpinBox(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	label->setBuddy(mStartOfDay);
	QWhatsThis::add(box,
	      i18n("The earliest time of day at which a date-only alarm (i.e. an alarm with \"any time\" specified) will be triggered.\n%1")
	           .arg(TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mConfirmAlarmDeletion = new QCheckBox(i18n("Con&firm alarm deletions"), itemBox, "confirmDeletion");
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	QWhatsThis::add(mConfirmAlarmDeletion,
	      i18n("Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Email preferences
	group = new QGroupBox(i18n("Email Alarms"), mPage);
	QBoxLayout* layout = new QVBoxLayout(group, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);

	box = new QHBox(group);
	box->setSpacing(2*KDialog::spacingHint());
	label = new QLabel(i18n("Email client:"), box);
	mEmailClient = new QButtonGroup(box);
	mEmailClient->hide();
	QRadioButton* radio = new QRadioButton(i18n("&KMail"), box, "kmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Preferences::KMAIL);
	radio = new QRadioButton(i18n("S&endmail"), box, "sendmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Preferences::SENDMAIL);
	box->setFixedHeight(box->sizeHint().height());
	QWhatsThis::add(box,
	      i18n("Choose how to send email when an email alarm is triggered.\n"
	           "KMail: A KMail composer window is displayed to enable you to send the email.\n"
	           "Sendmail: The email is sent automatically. This option will only work if your system is configured to use 'sendmail' or 'mail'."));
	layout->addWidget(box, 0, AlignLeft);

	mEmailUseControlCentre = new QCheckBox(i18n("Use email address from Co&ntrol Center"), group);
	mEmailUseControlCentre->setFixedSize(mEmailUseControlCentre->sizeHint());
	connect(mEmailUseControlCentre, SIGNAL(toggled(bool)), SLOT(slotEmailUseCCToggled(bool)));
	QWhatsThis::add(mEmailUseControlCentre,
	      i18n("Check to use the email address set in the KDE Control Center."));
	layout->addWidget(mEmailUseControlCentre, 0, AlignLeft);

	box = new QHBox(group);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("Emai&l address:"), box);
	label->setFixedSize(label->sizeHint());
	mEmailAddress = new QLineEdit(box);
	label->setBuddy(mEmailAddress);
	QWhatsThis::add(box,
	      i18n("Your email address, used for blind copying email alarms to self."));
	box->setFixedHeight(box->sizeHint().height());
	layout->addWidget(box);

	group = new QGroupBox(i18n("Expired Alarms"), mPage);
	grid = new QGridLayout(group, 2, 2, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(1, 1);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	mKeepExpired = new QCheckBox(i18n("Keep alarms after e&xpiry"), group, "keepExpired");
	mKeepExpired->setMinimumSize(mKeepExpired->sizeHint());
	connect(mKeepExpired, SIGNAL(toggled(bool)), SLOT(slotExpiredToggled(bool)));
	QWhatsThis::add(mKeepExpired,
	      i18n("Check to store alarms after expiry or deletion (except deleted alarms which were never triggered)."));
	grid->addMultiCellWidget(mKeepExpired, 1, 1, 0, 1, AlignLeft);

	box = new QHBox(group);
	box->setSpacing(KDialog::spacingHint());
	mPurgeExpired = new QCheckBox(i18n("Discard ex&pired alarms after:"), box, "purgeExpired");
	mPurgeExpired->setMinimumSize(mPurgeExpired->sizeHint());
	connect(mPurgeExpired, SIGNAL(toggled(bool)), SLOT(slotExpiredToggled(bool)));
	mPurgeAfter = new QSpinBox(box);
	mPurgeAfter->setMinValue(1);
	mPurgeAfter->setMinimumSize(mPurgeAfter->sizeHint());
	mPurgeAfterLabel = new QLabel(i18n("da&ys"), box);
	mPurgeAfterLabel->setMinimumSize(mPurgeAfterLabel->sizeHint());
	mPurgeAfterLabel->setBuddy(mPurgeAfter);
	QWhatsThis::add(box,
	      i18n("Uncheck to store expired alarms indefinitely. Check to enter how long expired alarms should be stored."));
	grid->addWidget(box, 2, 1, AlignLeft);

	mClearExpired = new QPushButton(i18n("Clear Expired Alar&ms"), group);
	mClearExpired->setMinimumSize(mClearExpired->sizeHint());
	connect(mClearExpired, SIGNAL(clicked()), SLOT(slotClearExpired()));
	QWhatsThis::add(mClearExpired,
	      i18n("Delete all existing expired alarms."));
	grid->addWidget(mClearExpired, 3, 1, AlignLeft);

	box = new QHBox(mPage);     // top-adjust all the widgets
}

void MiscPrefTab::restore()
{
	bool systray = mPreferences->mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(mPreferences->mDisableAlarmsIfStopped);
	mQuitWarn->setChecked(TrayWindow::quitWarning());
	mAutostartTrayIcon1->setChecked(mPreferences->mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(mPreferences->mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(mPreferences->mConfirmAlarmDeletion);
	mDaemonTrayCheckInterval->setValue(mPreferences->mDaemonTrayCheckInterval);
	mStartOfDay->setValue(mPreferences->mStartOfDay.hour()*60 + mPreferences->mStartOfDay.minute());
	mEmailClient->setButton(mPreferences->mEmailClient);
	setEmailAddress(mPreferences->mEmailUseControlCentre, mPreferences->emailAddress());
	setExpiredControls(mPreferences->mExpiredKeepDays);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::apply(bool syncToDisc)
{
	bool systray = mRunInSystemTray->isChecked();
	mPreferences->mRunInSystemTray         = systray;
	mPreferences->mDisableAlarmsIfStopped  = mDisableAlarmsIfStopped->isChecked();
	if (mQuitWarn->isEnabled())
		TrayWindow::setQuitWarning(mQuitWarn->isChecked());
	mPreferences->mAutostartTrayIcon       = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	mPreferences->mConfirmAlarmDeletion    = mConfirmAlarmDeletion->isChecked();
	mPreferences->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	int sod = mStartOfDay->value();
	mPreferences->mStartOfDay.setHMS(sod/60, sod%60, 0);
	int client = mEmailClient->id(mEmailClient->selected());
	mPreferences->mEmailClient             = (client >= 0) ? Preferences::MailClient(client) : Preferences::default_emailClient;
	mPreferences->setEmailAddress(mEmailUseControlCentre->isChecked(), mEmailAddress->text());
	mPreferences->mExpiredKeepDays         = !mKeepExpired->isChecked() ? 0
	                                       : mPurgeExpired->isChecked() ? mPurgeAfter->value() : -1;
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::setDefaults()
{
	bool systray = Preferences::default_runInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::default_disableAlarmsIfStopped);
	mQuitWarn->setChecked(true);
	mAutostartTrayIcon1->setChecked(Preferences::default_autostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Preferences::default_autostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::default_confirmAlarmDeletion);
	mDaemonTrayCheckInterval->setValue(Preferences::default_daemonTrayCheckInterval);
	mStartOfDay->setValue(Preferences::default_startOfDay.hour()*60 + Preferences::default_startOfDay.minute());
	mEmailClient->setButton(Preferences::default_emailClient);
	setEmailAddress(Preferences::default_emailUseControlCentre, Preferences::default_emailAddress);
	setExpiredControls(Preferences::default_expiredKeepDays);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::slotRunModeToggled(bool)
{
	bool systray = (mRunInSystemTray->isOn());
	mAutostartTrayIcon2->setEnabled(!systray);
	mAutostartTrayIcon1->setEnabled(systray);
	mDisableAlarmsIfStopped->setEnabled(systray);
}

void MiscPrefTab::slotDisableIfStoppedToggled(bool)
{
	bool disable = (mDisableAlarmsIfStopped->isChecked());
	mQuitWarn->setEnabled(disable);
}

void MiscPrefTab::setExpiredControls(int purgeDays)
{
	mKeepExpired->setChecked(purgeDays);
	mPurgeExpired->setChecked(purgeDays > 0);
	mPurgeAfter->setValue(purgeDays > 0 ? purgeDays : 0);
	slotExpiredToggled(true);
}

void MiscPrefTab::slotExpiredToggled(bool)
{
	bool keep = mKeepExpired->isChecked();
	bool after = keep && mPurgeExpired->isChecked();
	mPurgeExpired->setEnabled(keep);
	mPurgeAfter->setEnabled(after);
	mPurgeAfterLabel->setEnabled(keep);
	mClearExpired->setEnabled(keep);
}

void MiscPrefTab::slotClearExpired()
{
	AlarmCalendar* calendar = theApp()->expiredCalendar(false);
	calendar->purge(0, true);
}

void MiscPrefTab::setEmailAddress(bool useControlCentre, const QString& address)
{
	mEmailUseControlCentre->setChecked(useControlCentre);
	mEmailAddress->setText(useControlCentre ? QString() : address);
	slotEmailUseCCToggled(true);
}

void MiscPrefTab::slotEmailUseCCToggled(bool)
{
	mEmailAddress->setEnabled(!mEmailUseControlCentre->isChecked());
}


/*=============================================================================
= Class AppearancePrefTab
=============================================================================*/

AppearancePrefTab::AppearancePrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	mFontChooser = new FontColourChooser(mPage, 0, false, QStringList(), i18n("Font && Color"), false);

	QHBox* box = new QHBox(mPage);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("E&xpired alarm color:"), box);
	label->setMinimumSize(label->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mExpiredColour = new KColorCombo(box);
	mExpiredColour->setMinimumSize(mExpiredColour->sizeHint());
	label->setBuddy(mExpiredColour);
	QWhatsThis::add(box,
	      i18n("Choose the text color in the alarm list for expired alarms."));
}

void AppearancePrefTab::restore()
{
	mFontChooser->setBgColour(mPreferences->mDefaultBgColour);
	mFontChooser->setFont(mPreferences->mMessageFont);
	mExpiredColour->setColor(mPreferences->mExpiredColour);
}

void AppearancePrefTab::apply(bool syncToDisc)
{
	mPreferences->mDefaultBgColour = mFontChooser->bgColour();
	mPreferences->mMessageFont     = mFontChooser->font();
	mPreferences->mExpiredColour   = mExpiredColour->color();
	PrefsTabBase::apply(syncToDisc);
}

void AppearancePrefTab::setDefaults()
{
	mFontChooser->setBgColour(Preferences::default_defaultBgColour);
	mFontChooser->setFont(Preferences::default_messageFont);
	mExpiredColour->setColor(Preferences::default_expiredColour);
}


/*=============================================================================
= Class DefaultPrefTab
=============================================================================*/

DefaultPrefTab::DefaultPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QString defsetting = i18n("The default setting for \"%1\" in the alarm edit dialog.");

	QHBox* box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultLateCancel = new QCheckBox(i18n("Cancel if &late"), box, "defCancelLate");
	mDefaultLateCancel->setMinimumSize(mDefaultLateCancel->sizeHint());
	QWhatsThis::add(mDefaultLateCancel, defsetting.arg(i18n("Cancel if late")));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultConfirmAck = new QCheckBox(i18n("Confirm ac&knowledgement"), box, "defConfAck");
	mDefaultConfirmAck->setMinimumSize(mDefaultConfirmAck->sizeHint());
	QWhatsThis::add(mDefaultConfirmAck, defsetting.arg(i18n("Confirm acknowledgement")));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// BCC email to sender
	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultEmailBcc = new QCheckBox(i18n("Copy email to &self"), box, "defEmailBcc");
	mDefaultEmailBcc->setMinimumSize(mDefaultEmailBcc->sizeHint());
	QWhatsThis::add(mDefaultEmailBcc, defsetting.arg(i18n("Copy email to self")));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultBeep = new QCheckBox(i18n("&Beep"), box, "defBeep");
	mDefaultBeep->setMinimumSize(mDefaultBeep->sizeHint());
	connect(mDefaultBeep, SIGNAL(toggled(bool)), SLOT(slotBeepToggled(bool)));
	QWhatsThis::add(mDefaultBeep,
	      i18n("Check to select Beep as the default setting for \"Sound\" in the alarm edit dialog."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mDefaultSoundFileLabel = new QLabel(i18n("Sound &file:"), box);
	mDefaultSoundFileLabel->setFixedSize(mDefaultSoundFileLabel->sizeHint());
	mDefaultSoundFile = new QLineEdit(box);
	mDefaultSoundFileLabel->setBuddy(mDefaultSoundFile);
	mDefaultSoundFileBrowse = new QPushButton(box);
	mDefaultSoundFileBrowse->setPixmap(SmallIcon("fileopen"));
	mDefaultSoundFileBrowse->setFixedSize(mDefaultSoundFileBrowse->sizeHint());
	connect(mDefaultSoundFileBrowse, SIGNAL(clicked()), SLOT(slotBrowseSoundFile()));
	QToolTip::add(mDefaultSoundFileBrowse, i18n("Choose a sound file"));
	QWhatsThis::add(box,
	      i18n("Enter the sound file to use as the default setting for \"Sound\" in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());

	QHBox* itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("&Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mDefaultRecurPeriod = new QComboBox(box, "defRecur");
	mDefaultRecurPeriod->insertItem(i18n("No Recurrence"));
	mDefaultRecurPeriod->insertItem(i18n("At Login"));
	mDefaultRecurPeriod->insertItem(i18n("Hourly/Minutely"));
	mDefaultRecurPeriod->insertItem(i18n("Daily"));
	mDefaultRecurPeriod->insertItem(i18n("Weekly"));
	mDefaultRecurPeriod->insertItem(i18n("Monthly"));
	mDefaultRecurPeriod->insertItem(i18n("Yearly"));
	mDefaultRecurPeriod->setFixedSize(mDefaultRecurPeriod->sizeHint());
	label->setBuddy(mDefaultRecurPeriod);
	QWhatsThis::add(box,
	      i18n("The default setting for the recurrence rule in the alarm edit dialog."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);
	itemBox->setFixedHeight(box->sizeHint().height());

	itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("Reminder &units:"), box);
	label->setFixedSize(label->sizeHint());
	mDefaultReminderUnits = new QComboBox(box, "defWarnUnits");
	mDefaultReminderUnits->insertItem(i18n("Hours/Minutes"), Reminder::HOURS_MINUTES);
	mDefaultReminderUnits->insertItem(i18n("Days"), Reminder::DAYS);
	mDefaultReminderUnits->insertItem(i18n("Weeks"), Reminder::WEEKS);
	mDefaultReminderUnits->setFixedSize(mDefaultReminderUnits->sizeHint());
	label->setBuddy(mDefaultReminderUnits);
	QWhatsThis::add(box,
	      i18n("The default units for the reminder in the alarm edit dialog."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);
	itemBox->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // top-adjust all the widgets
}

void DefaultPrefTab::restore()
{
	mDefaultLateCancel->setChecked(mPreferences->mDefaultLateCancel);
	mDefaultConfirmAck->setChecked(mPreferences->mDefaultConfirmAck);
	mDefaultBeep->setChecked(mPreferences->mDefaultBeep);
	mDefaultSoundFile->setText(mPreferences->mDefaultSoundFile);
	mDefaultEmailBcc->setChecked(mPreferences->mDefaultEmailBcc);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(mPreferences->mDefaultRecurPeriod));
	mDefaultReminderUnits->setCurrentItem(mPreferences->mDefaultReminderUnits);
	slotBeepToggled(true);
}

void DefaultPrefTab::apply(bool syncToDisc)
{
	mPreferences->mDefaultLateCancel = mDefaultLateCancel->isChecked();
	mPreferences->mDefaultConfirmAck = mDefaultConfirmAck->isChecked();
	mPreferences->mDefaultBeep       = mDefaultBeep->isChecked();
	mPreferences->mDefaultSoundFile  = mPreferences->mDefaultBeep ? QString::null : mDefaultSoundFile->text();
	mPreferences->mDefaultEmailBcc   = mDefaultEmailBcc->isChecked();
	switch (mDefaultRecurPeriod->currentItem())
	{
		case 6:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::ANNUAL;    break;
		case 5:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::MONTHLY;   break;
		case 4:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::WEEKLY;    break;
		case 3:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::DAILY;     break;
		case 2:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::SUBDAILY;  break;
		case 1:  mPreferences->mDefaultRecurPeriod = RecurrenceEdit::AT_LOGIN;  break;
		case 0:
		default: mPreferences->mDefaultRecurPeriod = RecurrenceEdit::NO_RECUR;  break;
	}
	mPreferences->mDefaultReminderUnits = static_cast<Reminder::Units>(mDefaultReminderUnits->currentItem());
	PrefsTabBase::apply(syncToDisc);
}

void DefaultPrefTab::setDefaults()
{
	mDefaultLateCancel->setChecked(mPreferences->default_defaultLateCancel);
	mDefaultConfirmAck->setChecked(mPreferences->default_defaultConfirmAck);
	mDefaultBeep->setChecked(mPreferences->default_defaultBeep);
	mDefaultSoundFile->setText(mPreferences->default_defaultSoundFile);
	mDefaultEmailBcc->setChecked(mPreferences->default_defaultEmailBcc);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(mPreferences->default_defaultRecurPeriod));
	mDefaultReminderUnits->setCurrentItem(mPreferences->default_defaultReminderUnits);
	slotBeepToggled(true);
}

void DefaultPrefTab::slotBeepToggled(bool)
{
	bool beep = mDefaultBeep->isChecked();
	mDefaultSoundFileLabel->setEnabled(!beep);
	mDefaultSoundFile->setEnabled(!beep);
	mDefaultSoundFileBrowse->setEnabled(!beep);
}

void DefaultPrefTab::slotBrowseSoundFile()
{
	QString	defaultDir = KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
	KURL url = KFileDialog::getOpenURL(defaultDir, i18n("*.wav|Wav Files"), 0, i18n("Choose a Sound File"));
	if (!url.isEmpty())
		mDefaultSoundFile->setText(url.prettyURL());
}

int DefaultPrefTab::recurIndex(RecurrenceEdit::RepeatType type)
{
	switch (type)
	{
		case RecurrenceEdit::ANNUAL:   return 6;
		case RecurrenceEdit::MONTHLY:  return 5;
		case RecurrenceEdit::WEEKLY:   return 4;
		case RecurrenceEdit::DAILY:    return 3;
		case RecurrenceEdit::SUBDAILY: return 2;
		case RecurrenceEdit::AT_LOGIN: return 1;
		case RecurrenceEdit::NO_RECUR:
		default:                       return 0;
	}
}
