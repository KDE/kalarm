/*
 *  prefdlg.cpp  -  program preferences dialog
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
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kemailsettings.h>
#include <kiconloader.h>
#include <kcolorcombo.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmtimewidget.h"
#include "editdlg.h"
#include "fontcolour.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "mainwindow.h"
#include "preferences.h"
#include "recurrenceedit.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "timespinbox.h"
#include "traywindow.h"
#include "prefdlg.moc"


/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg::KAlarmPrefDlg()
	: KDialogBase(IconList, i18n("Preferences"), Help | Default | Ok | Apply | Cancel, Ok, 0, 0, true, true)
{
	setIconListAllVisible(true);

	QVBox* frame = addVBoxPage(i18n("General"), i18n("General"), DesktopIcon("misc"));
	mMiscPage = new MiscPrefTab(frame);

	frame = addVBoxPage(i18n("Email"), i18n("Email Alarm Settings"), DesktopIcon("mail_generic"));
	mEmailPage = new EmailPrefTab(frame);

	frame = addVBoxPage(i18n("View"), i18n("View Settings"), DesktopIcon("view_choose"));
	mViewPage = new ViewPrefTab(frame);

	frame = addVBoxPage(i18n("Font & Color"), i18n("Default Font and Color"), DesktopIcon("colorize"));
	mFontColourPage = new FontColourPrefTab(frame);

	frame = addVBoxPage(i18n("Edit"), i18n("Default Alarm Edit Settings"), DesktopIcon("edit"));
	mEditPage = new EditPrefTab(frame);

	restore();
	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

// Restore all defaults in the options...
void KAlarmPrefDlg::slotDefault()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotDefault()" << endl;
	mFontColourPage->setDefaults();
	mEmailPage->setDefaults();
	mViewPage->setDefaults();
	mEditPage->setDefaults();
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
	QString errmsg = mEmailPage->validateAddress();
	if (!errmsg.isEmpty())
	{
	
		showPage(pageIndex(mEmailPage->parentWidget()));
		if (KMessageBox::warningYesNo(this, errmsg) != KMessageBox::Yes)
		{
			mValid = false;
			return;
		}
	}
	mValid = true;
	mFontColourPage->apply(false);
	mEmailPage->apply(false);
	mViewPage->apply(false);
	mEditPage->apply(false);
	mMiscPage->apply(false);
	Preferences::instance()->syncToDisc();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotOk()" << endl;
	mValid = true;
	slotApply();
	if (mValid)
		KDialogBase::slotOk();
}

// Discard the current preferences and close the dialogue
void KAlarmPrefDlg::slotCancel()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotCancel()" << endl;
	restore();
	KDialogBase::slotCancel();
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::restore()
{
	kdDebug(5950) << "KAlarmPrefDlg::restore()" << endl;
	mFontColourPage->restore();
	mEmailPage->restore();
	mViewPage->restore();
	mEditPage->restore();
	mMiscPage->restore();
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/

PrefsTabBase::PrefsTabBase(QVBox* frame)
	: QWidget(frame),
	  mPage(frame)
{
}

void PrefsTabBase::apply(bool syncToDisc)
{
	Preferences::instance()->save(syncToDisc);
}



/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	// Get alignment to use in QGridLayout (AlignAuto doesn't work correctly there)
	int alignment = QApplication::reverseLayout() ? Qt::AlignRight : Qt::AlignLeft;

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
	grid->addMultiCellWidget(mRunInSystemTray, row, row, 0, 2, alignment);
	++row;

	mAutostartTrayIcon1 = new QCheckBox(i18n("Autostart at &login"), group, "autoTray");
	mAutostartTrayIcon1->setFixedSize(mAutostartTrayIcon1->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon1,
	      i18n("Check to run %1 whenever you start KDE.").arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mAutostartTrayIcon1, row, row, 1, 2, alignment);
	++row;

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disa&ble alarms while not running"), group, "disableAl");
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	connect(mDisableAlarmsIfStopped, SIGNAL(toggled(bool)), SLOT(slotDisableIfStoppedToggled(bool)));
	QWhatsThis::add(mDisableAlarmsIfStopped,
	      i18n("Check to disable alarms whenever %1 is not running. Alarms will only appear while the system tray icon is visible.").arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mDisableAlarmsIfStopped, row, row, 1, 2, alignment);
	++row;

	mQuitWarn = new QCheckBox(i18n("Warn before &quitting"), group, "disableAl");
	mQuitWarn->setFixedSize(mQuitWarn->sizeHint());
	QWhatsThis::add(mQuitWarn,
	      i18n("Check to display a warning prompt before quitting %1.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mQuitWarn, row, 2, alignment);
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
	grid->addMultiCellWidget(mRunOnDemand, row, row, 0, 2, alignment);
	++row;

	mAutostartTrayIcon2 = new QCheckBox(i18n("Autostart system tray &icon at login"), group, "autoRun");
	mAutostartTrayIcon2->setFixedSize(mAutostartTrayIcon2->sizeHint());
	QWhatsThis::add(mAutostartTrayIcon2,
	      i18n("Check to display the system tray icon whenever you start KDE."));
	grid->addMultiCellWidget(mAutostartTrayIcon2, row, row, 1, 2, alignment);
	group->setFixedHeight(group->sizeHint().height());

	QHBox* itemBox = new QHBox(mPage);
	QHBox* box = new QHBox(itemBox);   // this is to control the QWhatsThis text display area
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
	grid->addMultiCellWidget(mKeepExpired, 1, 1, 0, 1, AlignAuto);

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
	grid->addWidget(box, 2, 1, AlignAuto);

	mClearExpired = new QPushButton(i18n("Clear Expired Alar&ms"), group);
	mClearExpired->setMinimumSize(mClearExpired->sizeHint());
	connect(mClearExpired, SIGNAL(clicked()), SLOT(slotClearExpired()));
	QWhatsThis::add(mClearExpired,
	      i18n("Delete all existing expired alarms."));
	grid->addWidget(mClearExpired, 3, 1, AlignAuto);
	group->setFixedHeight(group->sizeHint().height());

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void MiscPrefTab::restore()
{
	Preferences* preferences = Preferences::instance();
	bool systray = preferences->mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(preferences->mDisableAlarmsIfStopped);
	mQuitWarn->setChecked(preferences->quitWarn());
	mAutostartTrayIcon1->setChecked(preferences->mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(preferences->mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(preferences->confirmAlarmDeletion());
	mStartOfDay->setTime(preferences->mStartOfDay);
	mFeb29->setButton(preferences->mFeb29RecurType);
	setExpiredControls(preferences->mExpiredKeepDays);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::apply(bool syncToDisc)
{
	Preferences* preferences = Preferences::instance();
	bool systray = mRunInSystemTray->isChecked();
	preferences->mRunInSystemTray        = systray;
	preferences->mDisableAlarmsIfStopped = mDisableAlarmsIfStopped->isChecked();
	if (mQuitWarn->isEnabled())
		preferences->setQuitWarn(mQuitWarn->isChecked());
	preferences->mAutostartTrayIcon = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	preferences->setConfirmAlarmDeletion(mConfirmAlarmDeletion->isChecked());
	int sod = mStartOfDay->value();
	preferences->mStartOfDay.setHMS(sod/60, sod%60, 0);
	int feb29 = mFeb29->id(mFeb29->selected());
	preferences->mFeb29RecurType  = (feb29 >= 0) ? Preferences::Feb29Type(feb29) : Preferences::default_feb29RecurType;
	preferences->mExpiredKeepDays = !mKeepExpired->isChecked() ? 0
	                              : mPurgeExpired->isChecked() ? mPurgeAfter->value() : -1;
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::setDefaults()
{
	bool systray = Preferences::default_runInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::default_disableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::default_quitWarn);
	mAutostartTrayIcon1->setChecked(Preferences::default_autostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Preferences::default_autostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::default_confirmAlarmDeletion);
	mStartOfDay->setTime(Preferences::default_startOfDay);
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
	AlarmCalendar* cal = AlarmCalendar::expiredCalendarOpen();
	if (cal)
		cal->purgeAll();
}


/*=============================================================================
= Class EmailPrefTab
=============================================================================*/

EmailPrefTab::EmailPrefTab(QVBox* frame)
	: PrefsTabBase(frame),
	  mAddressChanged(false)
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
	connect(mEmailAddress, SIGNAL(textChanged(const QString&)), SLOT(slotAddressChanged()));
	label->setBuddy(mEmailAddress);
	QWhatsThis::add(mEmailAddress,
	      i18n("Your email address, used to identify you as the sender when sending email alarms."));
	grid->addWidget(mEmailAddress, 1, 1);

	mEmailUseControlCentre = new QCheckBox(i18n("&Use address from Control Center"), group);
	mEmailUseControlCentre->setFixedSize(mEmailUseControlCentre->sizeHint());
#if KDE_VERSION < 210
	mEmailUseControlCentre->hide();
#endif
	connect(mEmailUseControlCentre, SIGNAL(toggled(bool)), SLOT(slotEmailUseCCToggled(bool)));
	QWhatsThis::add(mEmailUseControlCentre,
	      i18n("Check to use the email address set in the KDE Control Center, to identify you as the sender when sending email alarms."));
	grid->addWidget(mEmailUseControlCentre, 2, 1, AlignAuto);

	label = new QLabel(i18n("'Bcc' email address", "&Bcc:"), group);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 3, 0);
	mEmailBccAddress = new QLineEdit(group);
	label->setBuddy(mEmailBccAddress);
	QWhatsThis::add(mEmailBccAddress,
	      i18n("Your email address, used for blind copying email alarms to yourself. "
	           "If you want blind copies to be sent to your account on the computer which KAlarm runs on, you can simply enter your user login name."));
	grid->addWidget(mEmailBccAddress, 3, 1);

	mEmailBccUseControlCentre = new QCheckBox(i18n("Us&e address from Control Center"), group);
	mEmailBccUseControlCentre->setFixedSize(mEmailBccUseControlCentre->sizeHint());
#if KDE_VERSION < 210
	mEmailBccUseControlCentre->hide();
#endif
	connect(mEmailBccUseControlCentre, SIGNAL(toggled(bool)), SLOT(slotEmailBccUseCCToggled(bool)));
	QWhatsThis::add(mEmailBccUseControlCentre,
	      i18n("Check to use the email address set in the KDE Control Center, for blind copying email alarms to yourself."));
	grid->addWidget(mEmailBccUseControlCentre, 4, 1, AlignAuto);

	group->setFixedHeight(group->sizeHint().height());

	box = new QHBox(mPage);   // this is to allow left adjustment
	mEmailQueuedNotify = new QCheckBox(i18n("&Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setFixedSize(mEmailQueuedNotify->sizeHint());
	QWhatsThis::add(mEmailQueuedNotify,
	      i18n("Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void EmailPrefTab::restore()
{
	Preferences* preferences = Preferences::instance();
	mEmailClient->setButton(preferences->mEmailClient);
	setEmailAddress(preferences->mEmailUseControlCentre, preferences->mEmailAddress);
	setEmailBccAddress(preferences->mEmailBccUseControlCentre, preferences->mEmailBccAddress);
	mEmailQueuedNotify->setChecked(Preferences::notifying(KAMail::EMAIL_QUEUED_NOTIFY, false));
	mAddressChanged = false;
}

void EmailPrefTab::apply(bool syncToDisc)
{
	Preferences* preferences = Preferences::instance();
	int client = mEmailClient->id(mEmailClient->selected());
	preferences->mEmailClient = (client >= 0) ? Preferences::MailClient(client) : Preferences::default_emailClient;
	preferences->setEmailAddress(mEmailUseControlCentre->isChecked(), mEmailAddress->text().stripWhiteSpace());
	preferences->setEmailBccAddress(mEmailBccUseControlCentre->isChecked(), mEmailBccAddress->text().stripWhiteSpace());
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
	mEmailUseControlCentre->setChecked(useControlCentre);
	mEmailAddress->setText(useControlCentre ? QString() : address.stripWhiteSpace());
	slotEmailUseCCToggled(true);
}

void EmailPrefTab::slotEmailUseCCToggled(bool)
{
	mEmailAddress->setEnabled(!mEmailUseControlCentre->isChecked());
	mAddressChanged = true;
}

void EmailPrefTab::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	mEmailBccUseControlCentre->setChecked(useControlCentre);
	mEmailBccAddress->setText(useControlCentre ? QString() : address.stripWhiteSpace());
	slotEmailBccUseCCToggled(true);
}

void EmailPrefTab::slotEmailBccUseCCToggled(bool)
{
	mEmailBccAddress->setEnabled(!mEmailBccUseControlCentre->isChecked());
}

QString EmailPrefTab::validateAddress()
{
	if (!mAddressChanged)
		return QString::null;
	mAddressChanged = false;
	QString errmsg = i18n("%1\nAre you sure you want to save your changes?").arg(KAMail::i18n_NeedFromEmailAddress());
	if (mEmailUseControlCentre->isChecked())
	{
		KEMailSettings e;
		if (!e.getSetting(KEMailSettings::EmailAddress).isEmpty())
			return QString::null;
		errmsg = i18n("No email address is currently set in the KDE Control Center. %1").arg(errmsg);
	}
	else
	if (!mEmailAddress->text().stripWhiteSpace().isEmpty())
		return QString::null;
	return errmsg;
}


/*=============================================================================
= Class FontColourPrefTab
=============================================================================*/

FontColourPrefTab::FontColourPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	mFontChooser = new FontColourChooser(mPage, 0, false, QStringList(), i18n("Message Font && Color"), true, false);

#if 0
	QLabel* label = new QLabel(i18n("Disabled alarm color:"), box);
	label->setMinimumSize(label->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mDisabledColour = new KColorCombo(box);
	mDisabledColour->setMinimumSize(mDisabledColour->sizeHint());
	label->setBuddy(mDisabledColour);
	QWhatsThis::add(box,
	      i18n("Choose the text color in the alarm list for disabled alarms."));
#endif

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

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void FontColourPrefTab::restore()
{
	Preferences* preferences = Preferences::instance();
	mFontChooser->setBgColour(preferences->mDefaultBgColour);
	mFontChooser->setColours(preferences->mMessageColours);
	mFontChooser->setFont(preferences->mMessageFont);
	mExpiredColour->setColor(preferences->mExpiredColour);
}

void FontColourPrefTab::apply(bool syncToDisc)
{
	Preferences* preferences = Preferences::instance();
	preferences->mDefaultBgColour = mFontChooser->bgColour();
	preferences->mMessageColours  = mFontChooser->colours();
	preferences->mMessageFont     = mFontChooser->font();
	preferences->mExpiredColour   = mExpiredColour->color();
	PrefsTabBase::apply(syncToDisc);
}

void FontColourPrefTab::setDefaults()
{
	mFontChooser->setBgColour(Preferences::default_defaultBgColour);
	mFontChooser->setColours(Preferences::instance()->default_messageColours);
	mFontChooser->setFont(Preferences::default_messageFont);
	mExpiredColour->setColor(Preferences::default_expiredColour);
}


/*=============================================================================
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab(QVBox* frame)
	: PrefsTabBase(frame)
{
	QString defsetting = i18n("The default setting for \"%1\" in the alarm edit dialog.");

	QHBox* box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultLateCancel = new QCheckBox(EditAlarmDlg::i18n_n_CancelIfLate(), box, "defCancelLate");
	mDefaultLateCancel->setMinimumSize(mDefaultLateCancel->sizeHint());
	QWhatsThis::add(mDefaultLateCancel, defsetting.arg(EditAlarmDlg::i18n_CancelIfLate()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultConfirmAck = new QCheckBox(EditAlarmDlg::i18n_k_ConfirmAck(), box, "defConfAck");
	mDefaultConfirmAck->setMinimumSize(mDefaultConfirmAck->sizeHint());
	QWhatsThis::add(mDefaultConfirmAck, defsetting.arg(EditAlarmDlg::i18n_ConfirmAck()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// BCC email to sender
	box = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	mDefaultEmailBcc = new QCheckBox(EditAlarmDlg::i18n_e_CopyEmailToSelf(), box, "defEmailBcc");
	mDefaultEmailBcc->setMinimumSize(mDefaultEmailBcc->sizeHint());
	QWhatsThis::add(mDefaultEmailBcc, defsetting.arg(EditAlarmDlg::i18n_CopyEmailToSelf()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	QGroupBox* group = new QButtonGroup(SoundPicker::i18n_Sound(), mPage, "soundGroup");
	QGridLayout* grid = new QGridLayout(group, 4, 3, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(2, 1);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	grid->addColSpacing(1, 3*KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	mDefaultSound = new QCheckBox(SoundPicker::i18n_s_Sound(), group, "defSound");
	mDefaultSound->setMinimumSize(mDefaultSound->sizeHint());
	QWhatsThis::add(mDefaultSound, defsetting.arg(SoundPicker::i18n_Sound()));
	grid->addMultiCellWidget(mDefaultSound, 1, 1, 0, 2, AlignAuto);

	mDefaultBeep = new QCheckBox(i18n("&Beep"), group, "defBeep");
	mDefaultBeep->setMinimumSize(mDefaultBeep->sizeHint());
	QWhatsThis::add(mDefaultBeep,
	      i18n("Check to select Beep as the default setting for \"%1\" in the alarm edit dialog.").arg(SoundPicker::i18n_Sound()));
	grid->addMultiCellWidget(mDefaultBeep, 2, 2, 1, 2, AlignAuto);

	box = new QHBox(group);   // this is to control the QWhatsThis text display area
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
	      i18n("Enter the default sound file to use in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());
	grid->addMultiCellWidget(box, 3, 3, 1, 2);

	mDefaultSoundRepeat = new QCheckBox(i18n("Re&peat sound file"), group, "defRepeatSound");
	mDefaultSoundRepeat->setMinimumSize(mDefaultSoundRepeat->sizeHint());
	QWhatsThis::add(mDefaultSoundRepeat, i18n("sound file \"Repeat\" checkbox", "The default setting for sound file \"%1\" in the alarm edit dialog.").arg(SoundPicker::i18n_Repeat()));
	grid->addWidget(mDefaultSoundRepeat, 4, 2, AlignAuto);
#ifdef WITHOUT_ARTS
	mDefaultSoundRepeat->hide();
#endif
	group->setFixedHeight(group->sizeHint().height());

	QHBox* itemBox = new QHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new QHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("&Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mDefaultRecurPeriod = new QComboBox(box, "defRecur");
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_NoRecur());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_AtLogin());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_HourlyMinutely());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_Daily());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_Weekly());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_Monthly());
	mDefaultRecurPeriod->insertItem(RecurrenceEdit::i18n_Yearly());
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
	mDefaultReminderUnits->insertItem(TimeSelector::i18n_Hours_Mins(), TimeSelector::HOURS_MINUTES);
	mDefaultReminderUnits->insertItem(TimeSelector::i18n_Days(), TimeSelector::DAYS);
	mDefaultReminderUnits->insertItem(TimeSelector::i18n_Weeks(), TimeSelector::WEEKS);
	mDefaultReminderUnits->setFixedSize(mDefaultReminderUnits->sizeHint());
	label->setBuddy(mDefaultReminderUnits);
	QWhatsThis::add(box,
	      i18n("The default units for the reminder in the alarm edit dialog."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);
	itemBox->setFixedHeight(box->sizeHint().height());

	mDefaultSpecialActions = new SpecialActions(i18n("Special Display Alarm Actions"), mPage);
	mDefaultSpecialActions->setFixedHeight(mDefaultSpecialActions->sizeHint().height());

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void EditPrefTab::restore()
{
	Preferences* preferences = Preferences::instance();
	mDefaultLateCancel->setChecked(preferences->mDefaultLateCancel);
	mDefaultConfirmAck->setChecked(preferences->mDefaultConfirmAck);
	mDefaultBeep->setChecked(preferences->mDefaultBeep);
	mDefaultSoundFile->setText(preferences->mDefaultSoundFile);
	mDefaultSoundRepeat->setChecked(preferences->mDefaultSoundRepeat);
	mDefaultEmailBcc->setChecked(preferences->mDefaultEmailBcc);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(preferences->mDefaultRecurPeriod));
	mDefaultReminderUnits->setCurrentItem(preferences->mDefaultReminderUnits);
	mDefaultSpecialActions->setActions(preferences->mDefaultPreAction, preferences->mDefaultPostAction);
}

void EditPrefTab::apply(bool syncToDisc)
{
	Preferences* preferences = Preferences::instance();
#ifdef NEW_CANCEL_IF_LATE
	preferences->mDefaultLateCancel  = mDefaultLateCancel->isChecked() ? 1 : 0;
#else
	preferences->mDefaultLateCancel  = mDefaultLateCancel->isChecked();
#endif
	preferences->mDefaultConfirmAck  = mDefaultConfirmAck->isChecked();
	preferences->mDefaultBeep        = mDefaultBeep->isChecked();
	preferences->mDefaultSoundFile   = mDefaultSoundFile->text();
	preferences->mDefaultSoundRepeat = mDefaultSoundRepeat->isChecked();
	preferences->mDefaultEmailBcc    = mDefaultEmailBcc->isChecked();
	preferences->mDefaultPreAction   = mDefaultSpecialActions->preAction();
	preferences->mDefaultPostAction  = mDefaultSpecialActions->postAction();
	switch (mDefaultRecurPeriod->currentItem())
	{
		case 6:  preferences->mDefaultRecurPeriod = RecurrenceEdit::ANNUAL;    break;
		case 5:  preferences->mDefaultRecurPeriod = RecurrenceEdit::MONTHLY;   break;
		case 4:  preferences->mDefaultRecurPeriod = RecurrenceEdit::WEEKLY;    break;
		case 3:  preferences->mDefaultRecurPeriod = RecurrenceEdit::DAILY;     break;
		case 2:  preferences->mDefaultRecurPeriod = RecurrenceEdit::SUBDAILY;  break;
		case 1:  preferences->mDefaultRecurPeriod = RecurrenceEdit::AT_LOGIN;  break;
		case 0:
		default: preferences->mDefaultRecurPeriod = RecurrenceEdit::NO_RECUR;  break;
	}
	preferences->mDefaultReminderUnits = static_cast<TimeSelector::Units>(mDefaultReminderUnits->currentItem());
	PrefsTabBase::apply(syncToDisc);
}

void EditPrefTab::setDefaults()
{
	mDefaultLateCancel->setChecked(Preferences::default_defaultLateCancel);
	mDefaultConfirmAck->setChecked(Preferences::default_defaultConfirmAck);
	mDefaultBeep->setChecked(Preferences::default_defaultBeep);
	mDefaultSoundFile->setText(Preferences::default_defaultSoundFile);
	mDefaultSoundRepeat->setChecked(Preferences::default_defaultSoundRepeat);
	mDefaultEmailBcc->setChecked(Preferences::default_defaultEmailBcc);
	mDefaultRecurPeriod->setCurrentItem(recurIndex(Preferences::default_defaultRecurPeriod));
	mDefaultReminderUnits->setCurrentItem(Preferences::default_defaultReminderUnits);
	mDefaultSpecialActions->setActions(Preferences::default_defaultPreAction, Preferences::default_defaultPostAction);
}

void EditPrefTab::slotBrowseSoundFile()
{
	KURL url = SoundPicker::browseFile(mDefaultSoundFile->text());
	if (!url.isEmpty())
		mDefaultSoundFile->setText(url.prettyURL());
}

int EditPrefTab::recurIndex(RecurrenceEdit::RepeatType type)
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

	mListShowTime = new QCheckBox(KAlarmMainWindow::i18n_t_ShowAlarmTimes(), group, "listTime");
	mListShowTime->setMinimumSize(mListShowTime->sizeHint());
	connect(mListShowTime, SIGNAL(toggled(bool)), SLOT(slotListTimeToggled(bool)));
	QWhatsThis::add(mListShowTime,
	      i18n("Specify whether to show in the alarm list, the time at which each alarm is due"));
	layout->addWidget(mListShowTime, 0, Qt::AlignAuto);

	mListShowTimeTo = new QCheckBox(KAlarmMainWindow::i18n_n_ShowTimeToAlarms(), group, "listTimeTo");
	mListShowTimeTo->setMinimumSize(mListShowTimeTo->sizeHint());
	connect(mListShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotListTimeToToggled(bool)));
	QWhatsThis::add(mListShowTimeTo,
	      i18n("Specify whether to show in the alarm list, how long until each alarm is due"));
	layout->addWidget(mListShowTimeTo, 0, Qt::AlignAuto);
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
	grid->addMultiCellWidget(mTooltipShowAlarms, 1, 1, 0, 2, AlignAuto);

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
	grid->addMultiCellWidget(box, 2, 2, 1, 2, AlignAuto);

	mTooltipShowTime = new QCheckBox(KAlarmMainWindow::i18n_m_ShowAlarmTimes(), group, "tooltipTime");
	mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
	connect(mTooltipShowTime, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToggled(bool)));
	QWhatsThis::add(mTooltipShowTime,
	      i18n("Specify whether to show in the system tray tooltip, the time at which each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTime, 3, 3, 1, 2, AlignAuto);

	mTooltipShowTimeTo = new QCheckBox(KAlarmMainWindow::i18n_l_ShowTimeToAlarms(), group, "tooltipTimeTo");
	mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
	connect(mTooltipShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToToggled(bool)));
	QWhatsThis::add(mTooltipShowTimeTo,
	      i18n("Specify whether to show in the system tray tooltip, how long until each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTimeTo, 4, 4, 1, 2, AlignAuto);

	box = new QHBox(group);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mTooltipTimeToPrefixLabel = new QLabel(i18n("&Prefix:"), box);
	mTooltipTimeToPrefixLabel->setFixedSize(mTooltipTimeToPrefixLabel->sizeHint());
	mTooltipTimeToPrefix = new QLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	QWhatsThis::add(box,
	      i18n("Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip"));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 5, 2, AlignAuto);
	group->setMaximumHeight(group->sizeHint().height());

	mModalMessages = new QCheckBox(i18n("Message &windows have a title bar and take keyboard focus"), mPage, "modalMsg");
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	QWhatsThis::add(mModalMessages,
	      i18n("Specify the characteristics of alarm message windows:\n"
	           "- If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.\n"
	           "- If unchecked, the window does not interfere with your typing when "
	           "it is displayed, but it has no title bar and cannot be moved or resized."));

	mShowExpiredAlarms = new QCheckBox(KAlarmMainWindow::i18n_s_ShowExpiredAlarms(), mPage, "showExpired");
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

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void ViewPrefTab::restore()
{
	Preferences* preferences = Preferences::instance();
	setList(preferences->mShowAlarmTime,
	        preferences->mShowTimeToAlarm);
	setTooltip(preferences->mTooltipAlarmCount,
	           preferences->mShowTooltipAlarmTime,
	           preferences->mShowTooltipTimeToAlarm,
	           preferences->mTooltipTimeToPrefix);
	mModalMessages->setChecked(preferences->mModalMessages);
	mShowExpiredAlarms->setChecked(preferences->mShowExpiredAlarms);
	mDaemonTrayCheckInterval->setValue(preferences->mDaemonTrayCheckInterval);
}

void ViewPrefTab::apply(bool syncToDisc)
{
	Preferences* preferences = Preferences::instance();
	preferences->mShowAlarmTime           = mListShowTime->isChecked();
	preferences->mShowTimeToAlarm         = mListShowTimeTo->isChecked();
	int n = mTooltipShowAlarms->isChecked() ? -1 : 0;
	if (n  &&  mTooltipMaxAlarms->isChecked())
		n = mTooltipMaxAlarmCount->value();
	preferences->mTooltipAlarmCount       = n;
	preferences->mShowTooltipAlarmTime    = mTooltipShowTime->isChecked();
	preferences->mShowTooltipTimeToAlarm  = mTooltipShowTimeTo->isChecked();
	preferences->mTooltipTimeToPrefix     = mTooltipTimeToPrefix->text();
	preferences->mModalMessages           = mModalMessages->isChecked();
	preferences->mShowExpiredAlarms       = mShowExpiredAlarms->isChecked();
	preferences->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
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
	if (!timeTo)
		time = true;    // ensure that at least one time option is ticked
	mIgnoreToggle = true;
	mListShowTime->setChecked(time);
	mListShowTimeTo->setChecked(timeTo);
	mIgnoreToggle = false;
	slotListTimeToToggled(timeTo);
}

void ViewPrefTab::setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix)
{
	if (!timeTo)
		time = true;    // ensure that at least one time option is ticked
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
