/*
 *  prefdlg.cpp  -  program preferences dialog
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#include <qobjectlist.h>
#include <qlayout.h>
#include <qbuttongroup.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
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
#include "kamail.h"
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

	frame = addVBoxPage(i18n("Email"), i18n("Email Alarm Settings"), DesktopIcon("mail_generic"));
	mEmailPage = new EmailPrefTab(frame);
	mEmailPage->setPreferences(sets);

	frame = addVBoxPage(i18n("View"), i18n("View Settings"), DesktopIcon("view_choose"));
	mViewPage = new ViewPrefTab(frame);
	mViewPage->setPreferences(sets);

	frame = addVBoxPage(i18n("Appearance"), i18n("Default Message Appearance"), DesktopIcon("colorize"));
	mMessagePage = new MessagePrefTab(frame);
	mMessagePage->setPreferences(sets);

	frame = addVBoxPage(i18n("Edit"), i18n("Default Alarm Edit Settings"), DesktopIcon("edit"));
	mDefaultPage = new DefaultPrefTab(frame);
	mDefaultPage->setPreferences(sets);

	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

// Restore all defaults in the options...
void KAlarmPrefDlg::slotDefault()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotDefault()" << endl;
	mMessagePage->setDefaults();
	mEmailPage->setDefaults();
	mViewPage->setDefaults();
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
	mMessagePage->apply(false);
	mEmailPage->apply(false);
	mViewPage->apply(false);
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
	mMessagePage->restore();
	mEmailPage->restore();
	mViewPage->restore();
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

	mQuitWarn = new QCheckBox(i18n("Warn before &quitting"), group, "disableAl");
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
	QLabel* label = new QLabel(i18n("&Start of day for date-only alarms:"), box);
	mStartOfDay = new TimeSpinBox(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	label->setBuddy(mStartOfDay);
	static const QString startOfDayText = i18n("The earliest time of day at which a date-only alarm (i.e. "
	                                           "an alarm with \"any time\" specified) will be triggered.");
	QWhatsThis::add(box, QString("%1\n\n%2").arg(startOfDayText).arg(TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	QVBox* vbox = new QVBox(mPage);   // this is to control the QWhatsThis text display area
	vbox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("In non-leap years, repeat yearly February 29th alarms on:"), vbox);
	label->setAlignment(Qt::WordBreak);
	box = new QHBox(vbox);
	box->setSpacing(2*KDialog::spacingHint());
	mFeb29 = new QButtonGroup(box);
	mFeb29->hide();
	QWidget* widget = new QWidget(box);
	widget->setFixedWidth(3*KDialog::spacingHint());
	QRadioButton* radio = new QRadioButton(i18n("February 2&8th"), box);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->insert(radio, Preferences::FEB29_FEB28);
	radio = new QRadioButton(i18n("March &1st"), box);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->insert(radio, Preferences::FEB29_MAR1);
	radio = new QRadioButton(i18n("Do &not repeat"), box);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->insert(radio, Preferences::FEB29_NONE);
	box->setFixedHeight(box->sizeHint().height());
	QWhatsThis::add(vbox,
	      i18n("For yearly recurrences, choose what date, if any, alarms due on February 29th should occur in non-leap years.\n"
	           "Note that the next scheduled occurrence of existing alarms is not re-evaluated when you change this setting."));

	itemBox = new QHBox(mPage);   // this is to allow left adjustment
	mConfirmAlarmDeletion = new QCheckBox(i18n("Con&firm alarm deletions"), itemBox, "confirmDeletion");
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	QWhatsThis::add(mConfirmAlarmDeletion,
	      i18n("Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

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
	mPurgeAfter = new SpinBox(box);
	mPurgeAfter->setMinValue(1);
	mPurgeAfter->setLineShiftStep(10);
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
	group->setFixedHeight(group->sizeHint().height());

	box = new QHBox(mPage);     // top-adjust all the widgets
	vbox->setMaximumHeight(vbox->sizeHint().height());
}

void MiscPrefTab::restore()
{
	bool systray = mPreferences->mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(mPreferences->mDisableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::notifying(TrayWindow::QUIT_WARN, true));
	mAutostartTrayIcon1->setChecked(mPreferences->mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(mPreferences->mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(mPreferences->mConfirmAlarmDeletion);
	mStartOfDay->setValue(mPreferences->mStartOfDay.hour()*60 + mPreferences->mStartOfDay.minute());
	mFeb29->setButton(mPreferences->mFeb29RecurType);
	setExpiredControls(mPreferences->mExpiredKeepDays);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::apply(bool syncToDisc)
{
	bool systray = mRunInSystemTray->isChecked();
	mPreferences->mRunInSystemTray         = systray;
	mPreferences->mDisableAlarmsIfStopped  = mDisableAlarmsIfStopped->isChecked();
	if (mQuitWarn->isEnabled())
		Preferences::setNotify(TrayWindow::QUIT_WARN, true, mQuitWarn->isChecked());
	mPreferences->mAutostartTrayIcon       = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	mPreferences->mConfirmAlarmDeletion    = mConfirmAlarmDeletion->isChecked();
	int sod = mStartOfDay->value();
	mPreferences->mStartOfDay.setHMS(sod/60, sod%60, 0);
	int feb29 = mFeb29->id(mFeb29->selected());
	mPreferences->mFeb29RecurType = (feb29 >= 0) ? Preferences::Feb29Type(feb29) : Preferences::default_feb29RecurType;
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
	mStartOfDay->setValue(Preferences::default_startOfDay.hour()*60 + Preferences::default_startOfDay.minute());
	mFeb29->setButton(Preferences::default_feb29RecurType);
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


/*=============================================================================
= Class EmailPrefTab
=============================================================================*/

EmailPrefTab::EmailPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QHBox* box = new QHBox(mPage);
	box->setSpacing(2*KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("Email client:"), box);
	mEmailClient = new QButtonGroup(box);
	mEmailClient->hide();
	QRadioButton* radio = new QRadioButton(i18n("&KMail"), box, "kmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Preferences::KMAIL);
	radio = new QRadioButton(i18n("&Sendmail"), box, "sendmail");
	radio->setMinimumSize(radio->sizeHint());
	mEmailClient->insert(radio, Preferences::SENDMAIL);
	box->setFixedHeight(box->sizeHint().height());
	QWhatsThis::add(box,
	      i18n("Choose how to send email when an email alarm is triggered.\n"
	           "KMail: The email is added to KMail's outbox if KMail is running. If not, "
	           "a KMail composer window is displayed to enable you to send the email.\n"
	           "Sendmail: The email is sent automatically. This option will only work if "
	           "your system is configured to use 'sendmail' or a sendmail compatible mail transport agent."));

	QGroupBox* group = new QGroupBox(i18n("Your Email Address"), mPage);
	QGridLayout* grid = new QGridLayout(group, 2, 2, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	grid->setColStretch(1, 1);
	label = new QLabel(i18n("'From' email address", "&From:"), group);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);
	mEmailAddress = new QLineEdit(group);
	label->setBuddy(mEmailAddress);
	QWhatsThis::add(mEmailAddress,
	      i18n("Your email address, used to identify you as the sender when sending email alarms."));
	grid->addWidget(mEmailAddress, 1, 1);

#if KDE_VERSION >= 210
	mEmailUseControlCentre = new QCheckBox(i18n("&Use address from Control Center"), group);
	mEmailUseControlCentre->setFixedSize(mEmailUseControlCentre->sizeHint());
	connect(mEmailUseControlCentre, SIGNAL(toggled(bool)), SLOT(slotEmailUseCCToggled(bool)));
	QWhatsThis::add(mEmailUseControlCentre,
	      i18n("Check to use the email address set in the KDE Control Center, to identify you as the sender when sending email alarms."));
	grid->addWidget(mEmailUseControlCentre, 2, 1, AlignLeft);
#endif

	label = new QLabel(i18n("'Bcc' email address", "&Bcc:"), group);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 3, 0);
	mEmailBccAddress = new QLineEdit(group);
	label->setBuddy(mEmailBccAddress);
	QWhatsThis::add(mEmailBccAddress,
	      i18n("Your email address, used for blind copying email alarms to yourself. "
	           "If you want blind copies to be sent to your account on the computer which KAlarm runs on, you can simply enter your user login name."));
	grid->addWidget(mEmailBccAddress, 3, 1);

#if KDE_VERSION >= 210
	mEmailBccUseControlCentre = new QCheckBox(i18n("Us&e address from Control Center"), group);
	mEmailBccUseControlCentre->setFixedSize(mEmailBccUseControlCentre->sizeHint());
	connect(mEmailBccUseControlCentre, SIGNAL(toggled(bool)), SLOT(slotEmailBccUseCCToggled(bool)));
	QWhatsThis::add(mEmailBccUseControlCentre,
	      i18n("Check to use the email address set in the KDE Control Center, for blind copying email alarms to yourself."));
	grid->addWidget(mEmailBccUseControlCentre, 4, 1, AlignLeft);
#endif
	group->setFixedHeight(group->sizeHint().height());

	box = new QHBox(mPage);   // this is to allow left adjustment
	mEmailQueuedNotify = new QCheckBox(i18n("&Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setFixedSize(mEmailQueuedNotify->sizeHint());
	QWhatsThis::add(mEmailQueuedNotify,
	      i18n("Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);     // top-adjust all the widgets
}

void EmailPrefTab::restore()
{
	mEmailClient->setButton(mPreferences->mEmailClient);
	setEmailAddress(mPreferences->mEmailUseControlCentre, mPreferences->emailAddress());
	setEmailBccAddress(mPreferences->mEmailBccUseControlCentre, mPreferences->emailBccAddress());
	mEmailQueuedNotify->setChecked(Preferences::notifying(KAMail::EMAIL_QUEUED_NOTIFY, false));
}

void EmailPrefTab::apply(bool syncToDisc)
{
	int client = mEmailClient->id(mEmailClient->selected());
	mPreferences->mEmailClient = (client >= 0) ? Preferences::MailClient(client) : Preferences::default_emailClient;
#if KDE_VERSION >= 210
	mPreferences->setEmailAddress(mEmailUseControlCentre->isChecked(), mEmailAddress->text());
	mPreferences->setEmailBccAddress(mEmailBccUseControlCentre->isChecked(), mEmailBccAddress->text());
#else
	mPreferences->setEmailAddress(false, mEmailAddress->text());
	mPreferences->setEmailBccAddress(false, mEmailBccAddress->text());
#endif
	Preferences::setNotify(KAMail::EMAIL_QUEUED_NOTIFY, false, mEmailQueuedNotify->isChecked());
	PrefsTabBase::apply(syncToDisc);
}

void EmailPrefTab::setDefaults()
{
	mEmailClient->setButton(Preferences::default_emailClient);
	setEmailAddress(Preferences::default_emailUseControlCentre, Preferences::default_emailAddress);
	setEmailBccAddress(Preferences::default_emailBccUseControlCentre, Preferences::default_emailBccAddress);
	mEmailQueuedNotify->setChecked(Preferences::default_emailQueuedNotify);
}

void EmailPrefTab::setEmailAddress(bool useControlCentre, const QString& address)
{
#if KDE_VERSION >= 210
	mEmailUseControlCentre->setChecked(useControlCentre);
	mEmailAddress->setText(useControlCentre ? QString() : address);
	slotEmailUseCCToggled(true);
#else
	mEmailAddress->setText(address);
#endif
}

void EmailPrefTab::slotEmailUseCCToggled(bool)
{
#if KDE_VERSION >= 210
	mEmailAddress->setEnabled(!mEmailUseControlCentre->isChecked());
#endif
}

void EmailPrefTab::setEmailBccAddress(bool useControlCentre, const QString& address)
{
#if KDE_VERSION >= 210
	mEmailBccUseControlCentre->setChecked(useControlCentre);
	mEmailBccAddress->setText(useControlCentre ? QString() : address);
	slotEmailBccUseCCToggled(true);
#else
	mEmailBccAddress->setText(address);
#endif
}

void EmailPrefTab::slotEmailBccUseCCToggled(bool)
{
#if KDE_VERSION >= 210
	mEmailBccAddress->setEnabled(!mEmailBccUseControlCentre->isChecked());
#endif
}


/*=============================================================================
= Class MessagePrefTab
=============================================================================*/

MessagePrefTab::MessagePrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	mFontChooser = new FontColourChooser(mPage, 0, false, QStringList(), i18n("Font && Color"), true, false);

	QHBox* layoutBox = new QHBox(mPage);
	QHBox* box = new QHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("E&xpired alarm color:"), box);
	label->setMinimumSize(label->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mExpiredColour = new KColorCombo(box);
	mExpiredColour->setMinimumSize(mExpiredColour->sizeHint());
	label->setBuddy(mExpiredColour);
	QWhatsThis::add(box,
	      i18n("Choose the text color in the alarm list for expired alarms."));
	layoutBox->setStretchFactor(new QWidget(layoutBox), 1);    // left adjust the controls
	layoutBox->setFixedHeight(layoutBox->sizeHint().height());
}

void MessagePrefTab::restore()
{
	mFontChooser->setBgColour(mPreferences->mDefaultBgColour);
	mFontChooser->setColours(mPreferences->mMessageColours);
	mFontChooser->setFont(mPreferences->mMessageFont);
	mExpiredColour->setColor(mPreferences->mExpiredColour);
}

void MessagePrefTab::apply(bool syncToDisc)
{
	mPreferences->mDefaultBgColour = mFontChooser->bgColour();
	mPreferences->mMessageColours  = mFontChooser->colours();
	mPreferences->mMessageFont     = mFontChooser->font();
	mPreferences->mExpiredColour   = mExpiredColour->color();
	PrefsTabBase::apply(syncToDisc);
}

void MessagePrefTab::setDefaults()
{
	mFontChooser->setBgColour(Preferences::default_defaultBgColour);
	mFontChooser->setColours(mPreferences->default_messageColours);
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
	QWhatsThis::add(mDefaultConfirmAck, defsetting.arg(i18n("Confirm acknowledgment")));
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
	mDefaultLateCancel->setChecked(Preferences::default_defaultLateCancel);
	mDefaultConfirmAck->setChecked(Preferences::default_defaultConfirmAck);
	mDefaultBeep->setChecked(Preferences::default_defaultBeep);
	mDefaultSoundFile->setText(Preferences::default_defaultSoundFile);
	mDefaultEmailBcc->setChecked(Preferences::default_defaultEmailBcc);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(Preferences::default_defaultRecurPeriod));
	mDefaultReminderUnits->setCurrentItem(Preferences::default_defaultReminderUnits);
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
	KURL url = KFileDialog::getOpenURL(defaultDir, i18n("*.wav|Wav Files"), 0, i18n("Choose Sound File"));
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


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QGroupBox* group = new QGroupBox(i18n("Alarm List"), mPage);
	QBoxLayout* layout = new QVBoxLayout(group, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);

	mListShowTime = new QCheckBox(i18n("Show alarm &time"), group, "listTime");
	mListShowTime->setMinimumSize(mListShowTime->sizeHint());
	connect(mListShowTime, SIGNAL(toggled(bool)), SLOT(slotListTimeToggled(bool)));
	QWhatsThis::add(mListShowTime,
	      i18n("Specify whether to show in the alarm list, the time at which each alarm is due"));
	layout->addWidget(mListShowTime, 0, Qt::AlignLeft);

	mListShowTimeTo = new QCheckBox(i18n("Show time u&ntil alarm"), group, "listTimeTo");
	mListShowTimeTo->setMinimumSize(mListShowTimeTo->sizeHint());
	connect(mListShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotListTimeToToggled(bool)));
	QWhatsThis::add(mListShowTimeTo,
	      i18n("Specify whether to show in the alarm list, how long until each alarm is due"));
	layout->addWidget(mListShowTimeTo, 0, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	group = new QGroupBox(i18n("System Tray Tooltip"), mPage);
	QGridLayout* grid = new QGridLayout(group, 5, 3, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(2, 1);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	grid->addColSpacing(1, 3*KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	mTooltipShowAlarms = new QCheckBox(i18n("Show next &24 hours' alarms"), group, "tooltipShow");
	mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
	connect(mTooltipShowAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipAlarmsToggled(bool)));
	QWhatsThis::add(mTooltipShowAlarms,
	      i18n("Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours"));
	grid->addMultiCellWidget(mTooltipShowAlarms, 1, 1, 0, 2, AlignLeft);

	QHBox* box = new QHBox(group);
	box->setSpacing(KDialog::spacingHint());
	mTooltipMaxAlarms = new QCheckBox(i18n("Ma&ximum number of alarms to show:"), box, "tooltipMax");
	mTooltipMaxAlarms->setMinimumSize(mTooltipMaxAlarms->sizeHint());
	connect(mTooltipMaxAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipMaxToggled(bool)));
	mTooltipMaxAlarmCount = new SpinBox(1, 99, 1, box);
	mTooltipMaxAlarmCount->setLineShiftStep(5);
	mTooltipMaxAlarmCount->setMinimumSize(mTooltipMaxAlarmCount->sizeHint());
	QWhatsThis::add(box,
	      i18n("Uncheck to display all of the next 24 hours' alarms in the system tray tooltip. "
	           "Check to enter an upper limit on the number to be displayed."));
	grid->addMultiCellWidget(box, 2, 2, 1, 2, AlignLeft);

	mTooltipShowTime = new QCheckBox(i18n("Show alarm ti&me"), group, "tooltipTime");
	mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
	connect(mTooltipShowTime, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToggled(bool)));
	QWhatsThis::add(mTooltipShowTime,
	      i18n("Specify whether to show in the system tray tooltip, the time at which each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTime, 3, 3, 1, 2, AlignLeft);

	mTooltipShowTimeTo = new QCheckBox(i18n("Show time unti&l alarm"), group, "tooltipTimeTo");
	mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
	connect(mTooltipShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToToggled(bool)));
	QWhatsThis::add(mTooltipShowTimeTo,
	      i18n("Specify whether to show in the system tray tooltip, how long until each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTimeTo, 4, 4, 1, 2, AlignLeft);

	box = new QHBox(group);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mTooltipTimeToPrefixLabel = new QLabel(i18n("&Prefix:"), box);
	mTooltipTimeToPrefixLabel->setFixedSize(mTooltipTimeToPrefixLabel->sizeHint());
	mTooltipTimeToPrefix = new QLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	QWhatsThis::add(box,
	      i18n("Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip"));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 5, 2, AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	mModalMessages = new QCheckBox(i18n("Message &windows have a title bar and take keyboard focus"), mPage, "modalMsg");
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	QWhatsThis::add(mModalMessages,
	      i18n("Specify the characteristics of alarm message windows:\n"
	           "- If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.\n"
	           "- If unchecked, the window does not interfere with your typing when "
	           "it is displayed, but it has no title bar and cannot be moved or resized."));

	mShowExpiredAlarms = new QCheckBox(i18n("&Show expired alarms"), mPage, "showExpired");
	mShowExpiredAlarms->setMinimumSize(mShowExpiredAlarms->sizeHint());
	QWhatsThis::add(mShowExpiredAlarms,
	      i18n("Specify whether to show expired alarms in the alarm list"));

	QHBox* itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("System tray icon &update interval:"), box);
	mDaemonTrayCheckInterval = new SpinBox(1, 9999, 1, box, "daemonCheck");
	mDaemonTrayCheckInterval->setLineShiftStep(10);
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18n("seconds"), box);
	QWhatsThis::add(box,
	      i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // top-adjust all the widgets
}

void ViewPrefTab::restore()
{
	setList(mPreferences->mShowAlarmTime,
	        mPreferences->mShowTimeToAlarm);
	setTooltip(mPreferences->mTooltipAlarmCount,
	           mPreferences->mShowTooltipAlarmTime,
	           mPreferences->mShowTooltipTimeToAlarm,
	           mPreferences->mTooltipTimeToPrefix);
	mModalMessages->setChecked(mPreferences->mModalMessages);
	mShowExpiredAlarms->setChecked(mPreferences->mShowExpiredAlarms);
	mDaemonTrayCheckInterval->setValue(mPreferences->mDaemonTrayCheckInterval);
}

void ViewPrefTab::apply(bool syncToDisc)
{
	mPreferences->mShowAlarmTime           = mListShowTime->isChecked();
	mPreferences->mShowTimeToAlarm         = mListShowTimeTo->isChecked();
	int n = mTooltipShowAlarms->isChecked() ? -1 : 0;
	if (n  &&  mTooltipMaxAlarms->isChecked())
		n = mTooltipMaxAlarmCount->value();
	mPreferences->mTooltipAlarmCount       = n;
	mPreferences->mShowTooltipAlarmTime    = mTooltipShowTime->isChecked();
	mPreferences->mShowTooltipTimeToAlarm  = mTooltipShowTimeTo->isChecked();
	mPreferences->mTooltipTimeToPrefix     = mTooltipTimeToPrefix->text();
	mPreferences->mModalMessages           = mModalMessages->isChecked();
	mPreferences->mShowExpiredAlarms       = mShowExpiredAlarms->isChecked();
	mPreferences->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	PrefsTabBase::apply(syncToDisc);
}

void ViewPrefTab::setDefaults()
{
	setList(Preferences::default_showAlarmTime,
	        Preferences::default_showTimeToAlarm);
	setTooltip(Preferences::default_tooltipAlarmCount,
	           Preferences::default_showTooltipAlarmTime,
	           Preferences::default_showTooltipTimeToAlarm,
	           Preferences::default_tooltipTimeToPrefix);
	mModalMessages->setChecked(Preferences::default_modalMessages);
	mShowExpiredAlarms->setChecked(Preferences::default_showExpiredAlarms);
	mDaemonTrayCheckInterval->setValue(Preferences::default_daemonTrayCheckInterval);
}

void ViewPrefTab::setList(bool time, bool timeTo)
{
	mIgnoreToggle = true;
	mListShowTime->setChecked(time);
	mListShowTimeTo->setChecked(timeTo);
	mIgnoreToggle = false;
	slotListTimeToToggled(timeTo);
}

void ViewPrefTab::setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix)
{
	mIgnoreToggle = true;
	mTooltipShowAlarms->setChecked(maxAlarms);
	mTooltipMaxAlarms->setChecked(maxAlarms > 0);
	mTooltipMaxAlarmCount->setValue(maxAlarms > 0 ? maxAlarms : 1);
	mTooltipShowTime->setChecked(time);
	mTooltipShowTimeTo->setChecked(timeTo);
	mTooltipTimeToPrefix->setText(prefix);
	mIgnoreToggle = false;
	slotTooltipTimeToToggled(timeTo);
	slotTooltipAlarmsToggled(maxAlarms);
}

void ViewPrefTab::slotListTimeToggled(bool)
{
	if (!mIgnoreToggle)
	{
		if (!mListShowTime->isChecked()  &&  !mListShowTimeTo->isChecked())
			mListShowTimeTo->setChecked(true);
	}
}

void ViewPrefTab::slotListTimeToToggled(bool)
{
	if (!mIgnoreToggle)
	{
		if (!mListShowTimeTo->isChecked()  &&  !mListShowTime->isChecked())
			mListShowTime->setChecked(true);
	}
}

void ViewPrefTab::slotTooltipAlarmsToggled(bool)
{
	if (!mIgnoreToggle)
	{
		bool on = mTooltipShowAlarms->isChecked();
		mTooltipMaxAlarms->setEnabled(on);
		mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isChecked());
		mTooltipShowTime->setEnabled(on);
		mTooltipShowTimeTo->setEnabled(on);
		on = on && mTooltipShowTimeTo->isChecked();
		mTooltipTimeToPrefix->setEnabled(on);
		mTooltipTimeToPrefixLabel->setEnabled(on);
	}
}

void ViewPrefTab::slotTooltipMaxToggled(bool)
{
	bool on = mTooltipMaxAlarms->isChecked();
	mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isEnabled());
}

void ViewPrefTab::slotTooltipTimeToggled(bool)
{
	if (!mIgnoreToggle)
	{
		if (!mTooltipShowTime->isChecked()  &&  !mTooltipShowTimeTo->isChecked())
			mTooltipShowTimeTo->setChecked(true);
	}
}

void ViewPrefTab::slotTooltipTimeToToggled(bool)
{
	if (!mIgnoreToggle)
	{
		bool on = mTooltipShowTimeTo->isChecked();
		if (!on  &&  !mTooltipShowTime->isChecked())
			mTooltipShowTime->setChecked(true);
		on = on && mTooltipShowTimeTo->isEnabled();
		mTooltipTimeToPrefix->setEnabled(on);
		mTooltipTimeToPrefixLabel->setEnabled(on);
	}
}
