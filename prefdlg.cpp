/*
 *  prefdlg.cpp  -  program preferences dialog
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

#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <qtooltip.h>
#include <qstyle.h>

#include <kvbox.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kcolorcombo.h>
#include <kstdguiitem.h>
#include <kdebug.h>

#include <kalarmd/kalarmd.h>
#include <ktoolinvocation.h>

#include "alarmcalendar.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "editdlg.h"
#include "fontcolour.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "label.h"
#include "latecancel.h"
#include "mainwindow.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#ifndef WITHOUT_ARTS
#include "sounddlg.h"
#endif
#include "soundpicker.h"
#include "specialactions.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "traywindow.h"
#include "prefdlg.moc"

// Command strings for executing commands in different types of terminal windows.
// %t = window title parameter
// %c = command to execute in terminal
// %w = command to execute in terminal, with 'sleep 86400' appended
// %C = temporary command file to execute in terminal
// %W = temporary command file to execute in terminal, with 'sleep 86400' appended
static QString xtermCommands[] = {
	QString::fromLatin1("xterm -sb -hold -title %t -e %c"),
	QString::fromLatin1("konsole --noclose -T %t -e ${SHELL:-sh} -c %c"),
	QString::fromLatin1("gnome-terminal -t %t -e %W"),
	QString::fromLatin1("eterm --pause -T %t -e %C"),    // some systems use eterm...
	QString::fromLatin1("Eterm --pause -T %t -e %C"),    // while some use Eterm
	QString::fromLatin1("rxvt -title %t -e ${SHELL:-sh} -c %w"),
	QString()       // end of list indicator - don't change!
};


/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg::KAlarmPrefDlg()
	: KDialogBase(IconList, i18n("Preferences"), Help | Default | Ok | Apply | Cancel, Ok, 0, 0, true, true)
{
	setIconListAllVisible(true);

	KVBox* frame = addVBoxPage(i18n("General"), i18n("General"), DesktopIcon("misc"));
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
	KToolInvocation::invokeHelp("preferences");
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotApply()" << endl;
	QString errmsg = mEmailPage->validate();
	if (!errmsg.isEmpty())
	{
		showPage(pageIndex(mEmailPage->parentWidget()));
		if (KMessageBox::warningYesNo(this, errmsg) != KMessageBox::Yes)
		{
			mValid = false;
			return;
		}
	}
	errmsg = mEditPage->validate();
	if (!errmsg.isEmpty())
	{
		showPage(pageIndex(mEditPage->parentWidget()));
		KMessageBox::sorry(this, errmsg);
		mValid = false;
		return;
	}
	mValid = true;
	mFontColourPage->apply(false);
	mEmailPage->apply(false);
	mViewPage->apply(false);
	mEditPage->apply(false);
	mMiscPage->apply(false);
	Preferences::syncToDisc();
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
int PrefsTabBase::mIndentWidth = 0;

PrefsTabBase::PrefsTabBase(KVBox* frame)
	: QWidget(frame),
	  mPage(frame)
{
	if (!mIndentWidth)
		mIndentWidth = 3 * KDialog::spacingHint();
}

void PrefsTabBase::apply(bool syncToDisc)
{
	Preferences::save(syncToDisc);
}



/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab(KVBox* frame)
	: PrefsTabBase(frame)
{
	// Autostart alarm daemon
	KHBox* itemBox = new KHBox(mPage);   // this is to allow left adjustment
	mAutostartDaemon = new QCheckBox(i18n("Start alarm monitoring at lo&gin"), itemBox, "startDaemon");
	mAutostartDaemon->setFixedSize(mAutostartDaemon->sizeHint());
	connect(mAutostartDaemon, SIGNAL(clicked()), SLOT(slotAutostartDaemonClicked()));
	mAutostartDaemon->setWhatsThis(
	      i18n("Automatically start alarm monitoring whenever you start KDE, by running the alarm daemon (%1).\n\n"
	           "This option should always be checked unless you intend to discontinue use of KAlarm.")
	          .arg(QString::fromLatin1(DAEMON_APP_NAME)));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls

	QGroupBox* group = new QGroupBox(i18n("Run Mode"), mPage);
	QButtonGroup* buttonGroup = new QButtonGroup(group);
	QGridLayout* grid = new QGridLayout(group, 6, 3, KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(2, 1);
	grid->addColSpacing(0, indentWidth());
	grid->addColSpacing(1, indentWidth());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	int row = 1;

	// Run-in-system-tray radio button has an ID of 0
	mRunInSystemTray = new QRadioButton(i18n("Run continuously in system &tray"), group, "runTray");
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunInSystemTray->setWhatsThis(
	      i18n("Check to run KAlarm continuously in the KDE system tray.\n\n"
	           "Notes:\n"
	           "1. With this option selected, closing the system tray icon will quit KAlarm.\n"
	           "2. You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon."
	           " Running in the system tray simply provides easy access and a status indication."));
	buttonGroup->addButton(mRunInSystemTray);
	grid->addMultiCellWidget(mRunInSystemTray, row, row, 0, 2, Qt::AlignLeft);
	++row;

	mAutostartTrayIcon1 = new QCheckBox(i18n("Autostart at &login"), group, "autoTray");
	mAutostartTrayIcon1->setFixedSize(mAutostartTrayIcon1->sizeHint());
#ifdef AUTOSTART_BY_KALARMD
	connect(mAutostartTrayIcon1, SIGNAL(toggled(bool)), SLOT(slotAutostartToggled(bool)));
#endif
	mAutostartTrayIcon1->setWhatsThis(i18n("Check to run KAlarm whenever you start KDE."));
	grid->addMultiCellWidget(mAutostartTrayIcon1, row, row, 1, 2, Qt::AlignLeft);
	++row;

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disa&ble alarms while not running"), group, "disableAl");
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	connect(mDisableAlarmsIfStopped, SIGNAL(toggled(bool)), SLOT(slotDisableIfStoppedToggled(bool)));
	mDisableAlarmsIfStopped->setWhatsThis(i18n("Check to disable alarms whenever KAlarm is not running. Alarms will only appear while the system tray icon is visible."));
	grid->addMultiCellWidget(mDisableAlarmsIfStopped, row, row, 1, 2, Qt::AlignLeft);
	++row;

	mQuitWarn = new QCheckBox(i18n("Warn before &quitting"), group, "disableAl");
	mQuitWarn->setFixedSize(mQuitWarn->sizeHint());
	mQuitWarn->setWhatsThis(i18n("Check to display a warning prompt before quitting KAlarm."));
	grid->addWidget(mQuitWarn, row, 2, Qt::AlignLeft);
	++row;

	// Run-on-demand radio button has an ID of 3
	mRunOnDemand = new QRadioButton(i18n("&Run only on demand"), group, "runDemand");
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunOnDemand->setWhatsThis(
	      i18n("Check to run KAlarm only when required.\n\n"
	           "Notes:\n"
	           "1. Alarms are displayed even when KAlarm is not running, since alarm monitoring is done by the alarm daemon.\n"
	           "2. With this option selected, the system tray icon can be displayed or hidden independently of KAlarm."));
	buttonGroup->addButton(mRunOnDemand);
	grid->addMultiCellWidget(mRunOnDemand, row, row, 0, 2, Qt::AlignLeft);
	++row;

	mAutostartTrayIcon2 = new QCheckBox(i18n("Autostart system tray &icon at login"), group, "autoRun");
	mAutostartTrayIcon2->setFixedSize(mAutostartTrayIcon2->sizeHint());
#ifdef AUTOSTART_BY_KALARMD
	connect(mAutostartTrayIcon2, SIGNAL(toggled(bool)), SLOT(slotAutostartToggled(bool)));
#endif
	mAutostartTrayIcon2->setWhatsThis(i18n("Check to display the system tray icon whenever you start KDE."));
	grid->addMultiCellWidget(mAutostartTrayIcon2, row, row, 1, 2, Qt::AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	// Start-of-day time
	itemBox = new KHBox(mPage);
	KHBox* box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("&Start of day for date-only alarms:"), box);
	mStartOfDay = new TimeEdit(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	label->setBuddy(mStartOfDay);
	static const QString startOfDayText = i18n("The earliest time of day at which a date-only alarm (i.e. "
	                                           "an alarm with \"any time\" specified) will be triggered.");
	box->setWhatsThis(QString("%1\n\n%2").arg(startOfDayText).arg(TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Confirm alarm deletion?
	itemBox = new KHBox(mPage);   // this is to allow left adjustment
	mConfirmAlarmDeletion = new QCheckBox(i18n("Con&firm alarm deletions"), itemBox, "confirmDeletion");
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	mConfirmAlarmDeletion->setWhatsThis(i18n("Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Expired alarms
	group = new QGroupBox(i18n("Expired Alarms"), mPage);
	grid = new QGridLayout(group, 2, 2, KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(1, 1);
	grid->addColSpacing(0, indentWidth());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	mKeepExpired = new QCheckBox(i18n("Keep alarms after e&xpiry"), group, "keepExpired");
	mKeepExpired->setFixedSize(mKeepExpired->sizeHint());
	connect(mKeepExpired, SIGNAL(toggled(bool)), SLOT(slotExpiredToggled(bool)));
	mKeepExpired->setWhatsThis(i18n("Check to store alarms after expiry or deletion (except deleted alarms which were never triggered)."));
	grid->addMultiCellWidget(mKeepExpired, 1, 1, 0, 1, Qt::AlignLeft);

	box = new KHBox(group);
	box->setSpacing(KDialog::spacingHint());
	mPurgeExpired = new QCheckBox(i18n("Discard ex&pired alarms after:"), box, "purgeExpired");
	mPurgeExpired->setMinimumSize(mPurgeExpired->sizeHint());
	connect(mPurgeExpired, SIGNAL(toggled(bool)), SLOT(slotExpiredToggled(bool)));
	mPurgeAfter = new SpinBox(box);
	mPurgeAfter->setMinValue(1);
	mPurgeAfter->setSingleShiftStep(10);
	mPurgeAfter->setMinimumSize(mPurgeAfter->sizeHint());
	mPurgeAfterLabel = new QLabel(i18n("da&ys"), box);
	mPurgeAfterLabel->setMinimumSize(mPurgeAfterLabel->sizeHint());
	mPurgeAfterLabel->setBuddy(mPurgeAfter);
	box->setWhatsThis(i18n("Uncheck to store expired alarms indefinitely. Check to enter how long expired alarms should be stored."));
	grid->addWidget(box, 2, 1, Qt::AlignLeft);

	mClearExpired = new QPushButton(i18n("Clear Expired Alar&ms"), group);
	mClearExpired->setFixedSize(mClearExpired->sizeHint());
	connect(mClearExpired, SIGNAL(clicked()), SLOT(slotClearExpired()));
	mClearExpired->setWhatsThis(i18n("Delete all existing expired alarms."));
	grid->addWidget(mClearExpired, 3, 1, Qt::AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	// Terminal window to use for command alarms
	group = new QGroupBox(i18n("Terminal for Command Alarms"), mPage);
	group->setWhatsThis(i18n("Choose which application to use when a command alarm is executed in a terminal window"));
	grid = new QGridLayout(group, 1, 3, KDialog::marginHint(), KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	row = 0;

	mXtermType = new ButtonGroup(group);
	QString whatsThis = i18n("The parameter is a command line, e.g. 'xterm -e'", "Check to execute command alarms in a terminal window by '%1'");
	int index = 0;
	for (mXtermCount = 0;  !xtermCommands[mXtermCount].isNull();  ++mXtermCount)
	{
		QString cmd = xtermCommands[mXtermCount];
		int i = cmd.find(' ');    // find the end of the terminal window name
		QString term = cmd.left(i > 0 ? i : 1000);
		if (KStandardDirs::findExe(term).isEmpty())
			continue;
		QRadioButton* radio = new QRadioButton(term, group);
		radio->setMinimumSize(radio->sizeHint());
		mXtermType->addButton(radio, mXtermCount);
		cmd.replace("%t", kapp->aboutData()->programName());
		cmd.replace("%c", "<command>");
		cmd.replace("%w", "<command; sleep>");
		cmd.replace("%C", "[command]");
		cmd.replace("%W", "[command; sleep]");
		radio->setWhatsThis(whatsThis.arg(cmd));
		grid->addWidget(radio, (row = index/3 + 1), index % 3, Qt::AlignLeft);
		++index;
	}

	box = new KHBox(group);
	grid->addMultiCellWidget(box, row + 1, row + 1, 0, 2, Qt::AlignLeft);
	QRadioButton* radio = new QRadioButton(i18n("Other:"), box);
	radio->setFixedSize(radio->sizeHint());
	connect(radio, SIGNAL(toggled(bool)), SLOT(slotOtherTerminalToggled(bool)));
	mXtermType->addButton(radio, mXtermCount);
	mXtermCommand = new QLineEdit(box);
	box->setWhatsThis(
	      i18n("Enter the full command line needed to execute a command in your chosen terminal window. "
	           "By default the alarm's command string will be appended to what you enter here. "
	           "See the KAlarm Handbook for details of special codes to tailor the command line."));

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void MiscPrefTab::restore()
{
	mAutostartDaemon->setChecked(Preferences::mAutostartDaemon);
	bool systray = Preferences::mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::mDisableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::quitWarn());
	mAutostartTrayIcon1->setChecked(Preferences::mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Preferences::mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::confirmAlarmDeletion());
	mStartOfDay->setValue(Preferences::mStartOfDay);
	setExpiredControls(Preferences::mExpiredKeepDays);
	QString xtermCmd = Preferences::cmdXTermCommand();
	int id = 0;
	if (!xtermCmd.isEmpty())
	{
		for ( ;  id < mXtermCount;  ++id)
		{
			if (mXtermType->find(id)  &&  xtermCmd == xtermCommands[id])
				break;
		}
	}
	mXtermType->setButton(id);
	mXtermCommand->setEnabled(id == mXtermCount);
	mXtermCommand->setText(id == mXtermCount ? xtermCmd : "");
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::apply(bool syncToDisc)
{
	// First validate anything entered in Other X-terminal command
	int xtermID = mXtermType->selectedId();
	if (xtermID >= mXtermCount)
	{
		QString cmd = mXtermCommand->text();
		if (cmd.isEmpty())
			xtermID = 0;       // 'Other' is only acceptable if it's non-blank
		else
		{
			int i = cmd.find(' ');    // find the end of the terminal window name
			if (i > 0)
				cmd = cmd.left(i);
			if (KStandardDirs::findExe(cmd).isEmpty())
			{
				mXtermCommand->setFocus();
				if (KMessageBox::warningContinueCancel(this, i18n("Command to invoke terminal window not found:\n%1").arg(cmd))
				                != KMessageBox::Continue)
					return;
			}
		}
	}
	bool systray = mRunInSystemTray->isChecked();
	Preferences::mRunInSystemTray        = systray;
	Preferences::mDisableAlarmsIfStopped = mDisableAlarmsIfStopped->isChecked();
	if (mQuitWarn->isEnabled())
		Preferences::setQuitWarn(mQuitWarn->isChecked());
	Preferences::mAutostartTrayIcon = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
#ifdef AUTOSTART_BY_KALARMD
	Preferences::mAutostartDaemon = mAutostartDaemon->isChecked() || Preferences::mAutostartTrayIcon;
#else
	Preferences::mAutostartDaemon = mAutostartDaemon->isChecked();
#endif
	Preferences::setConfirmAlarmDeletion(mConfirmAlarmDeletion->isChecked());
	int sod = mStartOfDay->value();
	Preferences::mStartOfDay.setHMS(sod/60, sod%60, 0);
	Preferences::mExpiredKeepDays = !mKeepExpired->isChecked() ? 0
	                              : mPurgeExpired->isChecked() ? mPurgeAfter->value() : -1;
	Preferences::mCmdXTermCommand = (xtermID < mXtermCount) ? xtermCommands[xtermID] : mXtermCommand->text();
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::setDefaults()
{
	mAutostartDaemon->setChecked(Preferences::default_autostartDaemon);
	bool systray = Preferences::default_runInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::default_disableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::default_quitWarn);
	mAutostartTrayIcon1->setChecked(Preferences::default_autostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Preferences::default_autostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::default_confirmAlarmDeletion);
	mStartOfDay->setValue(Preferences::default_startOfDay);
	setExpiredControls(Preferences::default_expiredKeepDays);
	mXtermType->setButton(0);
	mXtermCommand->setEnabled(false);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::slotAutostartDaemonClicked()
{
	if (!mAutostartDaemon->isChecked()
	&&  KMessageBox::warningYesNo(this,
		                      i18n("You should not uncheck this option unless you intend to discontinue use of KAlarm"),
		                      QString::null, KStdGuiItem::cont(), KStdGuiItem::cancel()
		                     ) != KMessageBox::Yes)
		mAutostartDaemon->setChecked(true);	
}

void MiscPrefTab::slotRunModeToggled(bool)
{
	bool systray = (mRunInSystemTray->isOn());
	mAutostartTrayIcon2->setEnabled(!systray);
	mAutostartTrayIcon1->setEnabled(systray);
	mDisableAlarmsIfStopped->setEnabled(systray);
}

/******************************************************************************
* If autostart at login is selected, the daemon must be autostarted so that it
* can autostart KAlarm, in which case disable the daemon autostart option.
*/
void MiscPrefTab::slotAutostartToggled(bool)
{
#ifdef AUTOSTART_BY_KALARMD
	bool autostart = mRunInSystemTray->isChecked() ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	mAutostartDaemon->setEnabled(!autostart);
#endif
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

void MiscPrefTab::slotOtherTerminalToggled(bool on)
{
	mXtermCommand->setEnabled(on);
}


/*=============================================================================
= Class EmailPrefTab
=============================================================================*/

EmailPrefTab::EmailPrefTab(KVBox* frame)
	: PrefsTabBase(frame),
	  mAddressChanged(false),
	  mBccAddressChanged(false)
{
	KHBox* box = new KHBox(mPage);
	box->setSpacing(2*KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("Email client:"), box);
	mEmailClient = new ButtonGroup(box);
	mKMailButton = new RadioButton(i18n("&KMail"), box);
	mKMailButton->setMinimumSize(mKMailButton->sizeHint());
	mEmailClient->addButton(mKMailButton, Preferences::KMAIL);
	mSendmailButton = new RadioButton(i18n("&Sendmail"), box);
	mSendmailButton->setMinimumSize(mSendmailButton->sizeHint());
	mEmailClient->addButton(mSendmailButton, Preferences::SENDMAIL);
	connect(mEmailClient, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotEmailClientChanged(QAbstractButton*)));
	box->setFixedHeight(box->sizeHint().height());
	box->setWhatsThis(
	      i18n("Choose how to send email when an email alarm is triggered.\n"
	           "KMail: The email is added to KMail's outbox if KMail is running. If not, "
	           "a KMail composer window is displayed to enable you to send the email.\n"
	           "Sendmail: The email is sent automatically. This option will only work if "
	           "your system is configured to use 'sendmail' or a sendmail compatible mail transport agent."));

	box = new KHBox(mPage);   // this is to allow left adjustment
	mEmailCopyToKMail = new QCheckBox(i18n("Co&py sent emails into KMail's %1 folder").arg(KAMail::i18n_sent_mail()), box);
	mEmailCopyToKMail->setFixedSize(mEmailCopyToKMail->sizeHint());
	mEmailCopyToKMail->setWhatsThis(i18n("After sending an email, store a copy in KMail's %1 folder").arg(KAMail::i18n_sent_mail()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Your Email Address group box
	QGroupBox* group = new QGroupBox(i18n("Your Email Address"), mPage);
	QGridLayout* grid = new QGridLayout(group, 6, 3, KDialog::marginHint(), KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	grid->setColStretch(1, 1);

	// 'From' email address controls ...
	label = new Label(EditAlarmDlg::i18n_f_EmailFrom(), group);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);
	mFromAddressGroup = new ButtonGroup(group);
	connect(mFromAddressGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotFromAddrChanged(QAbstractButton*)));

	// Line edit to enter a 'From' email address
	mFromAddrButton = new RadioButton(group);
	mFromAddressGroup->addButton(mFromAddrButton, Preferences::MAIL_FROM_ADDR);
	mFromAddrButton->setFixedSize(mFromAddrButton->sizeHint());
	label->setBuddy(mFromAddrButton);
	grid->addWidget(mFromAddrButton, 1, 1);
	mEmailAddress = new QLineEdit(group);
	connect(mEmailAddress, SIGNAL(textChanged(const QString&)), SLOT(slotAddressChanged()));
	QString whatsThis = i18n("Your email address, used to identify you as the sender when sending email alarms.");
	mFromAddrButton->setWhatsThis(whatsThis);
	mEmailAddress->setWhatsThis(whatsThis);
	mFromAddrButton->setFocusWidget(mEmailAddress);
	grid->addWidget(mEmailAddress, 1, 2);

	// 'From' email address to be taken from Control Centre
	mFromCCentreButton = new RadioButton(i18n("&Use address from Control Center"), group);
	mFromCCentreButton->setFixedSize(mFromCCentreButton->sizeHint());
	mFromAddressGroup->addButton(mFromCCentreButton, Preferences::MAIL_FROM_CONTROL_CENTRE);
	mFromCCentreButton->setWhatsThis(i18n("Check to use the email address set in the KDE Control Center, to identify you as the sender when sending email alarms."));
	grid->addMultiCellWidget(mFromCCentreButton, 2, 2, 1, 2, Qt::AlignLeft);

	// 'From' email address to be picked from KMail's identities when the email alarm is configured
	mFromKMailButton = new RadioButton(i18n("Use KMail &identities"), group);
	mFromKMailButton->setFixedSize(mFromKMailButton->sizeHint());
	mFromAddressGroup->addButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
	mFromKMailButton->setWhatsThis(
	      i18n("Check to use KMail's email identities to identify you as the sender when sending email alarms. "
	           "For existing email alarms, KMail's default identity will be used. "
	           "For new email alarms, you will be able to pick which of KMail's identities to use."));
	grid->addMultiCellWidget(mFromKMailButton, 3, 3, 1, 2, Qt::AlignLeft);

	// 'Bcc' email address controls ...
	grid->addRowSpacing(4, KDialog::spacingHint());
	label = new Label(i18n("'Bcc' email address", "&Bcc:"), group);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 5, 0);
	mBccAddressGroup = new ButtonGroup(group);
	connect(mBccAddressGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotBccAddrChanged(QAbstractButton*)));

	// Line edit to enter a 'Bcc' email address
	mBccAddrButton = new RadioButton(group);
	mBccAddrButton->setFixedSize(mBccAddrButton->sizeHint());
	mBccAddressGroup->addButton(mBccAddrButton, Preferences::MAIL_FROM_ADDR);
	label->setBuddy(mBccAddrButton);
	grid->addWidget(mBccAddrButton, 5, 1);
	mEmailBccAddress = new QLineEdit(group);
	whatsThis = i18n("Your email address, used for blind copying email alarms to yourself. "
	                 "If you want blind copies to be sent to your account on the computer which KAlarm runs on, you can simply enter your user login name.");
	mBccAddrButton->setWhatsThis(whatsThis);
	mEmailBccAddress->setWhatsThis(whatsThis);
	mBccAddrButton->setFocusWidget(mEmailBccAddress);
	grid->addWidget(mEmailBccAddress, 5, 2);

	// 'Bcc' email address to be taken from Control Centre
	mBccCCentreButton = new RadioButton(i18n("Us&e address from Control Center"), group);
	mBccCCentreButton->setFixedSize(mBccCCentreButton->sizeHint());
	mBccAddressGroup->addButton(mBccCCentreButton, Preferences::MAIL_FROM_CONTROL_CENTRE);
	mBccCCentreButton->setWhatsThis(i18n("Check to use the email address set in the KDE Control Center, for blind copying email alarms to yourself."));
	grid->addMultiCellWidget(mBccCCentreButton, 6, 6, 1, 2, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	box = new KHBox(mPage);   // this is to allow left adjustment
	mEmailQueuedNotify = new QCheckBox(i18n("&Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setFixedSize(mEmailQueuedNotify->sizeHint());
	mEmailQueuedNotify->setWhatsThis(
	      i18n("Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void EmailPrefTab::restore()
{
	mEmailClient->setButton(Preferences::mEmailClient);
	mEmailCopyToKMail->setChecked(Preferences::emailCopyToKMail());
	setEmailAddress(Preferences::mEmailFrom, Preferences::mEmailAddress);
	setEmailBccAddress((Preferences::mEmailBccFrom == Preferences::MAIL_FROM_CONTROL_CENTRE), Preferences::mEmailBccAddress);
	mEmailQueuedNotify->setChecked(Preferences::emailQueuedNotify());
	mAddressChanged = mBccAddressChanged = false;
}

void EmailPrefTab::apply(bool syncToDisc)
{
	int client = mEmailClient->selectedId();
	Preferences::mEmailClient = (client >= 0) ? static_cast<Preferences::MailClient>(client) : Preferences::default_emailClient;
	Preferences::mEmailCopyToKMail = mEmailCopyToKMail->isChecked();
	Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(mFromAddressGroup->selectedId()), mEmailAddress->text().stripWhiteSpace());
	Preferences::setEmailBccAddress((mBccAddressGroup->checkedButton() == mBccCCentreButton), mEmailBccAddress->text().stripWhiteSpace());
	Preferences::setEmailQueuedNotify(mEmailQueuedNotify->isChecked());
	PrefsTabBase::apply(syncToDisc);
}

void EmailPrefTab::setDefaults()
{
	mEmailClient->setButton(Preferences::default_emailClient);
	setEmailAddress(Preferences::default_emailFrom(), Preferences::default_emailAddress);
	setEmailBccAddress((Preferences::default_emailBccFrom == Preferences::MAIL_FROM_CONTROL_CENTRE), Preferences::default_emailBccAddress);
	mEmailQueuedNotify->setChecked(Preferences::default_emailQueuedNotify);
}

void EmailPrefTab::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
	mFromAddressGroup->setButton(from);
	mEmailAddress->setText(from == Preferences::MAIL_FROM_ADDR ? address.stripWhiteSpace() : QString());
}

void EmailPrefTab::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	mBccAddressGroup->setButton(useControlCentre ? Preferences::MAIL_FROM_CONTROL_CENTRE : Preferences::MAIL_FROM_ADDR);
	mEmailBccAddress->setText(useControlCentre ? QString() : address.stripWhiteSpace());
}

void EmailPrefTab::slotEmailClientChanged(QAbstractButton* button)
{
	mEmailCopyToKMail->setEnabled(button == mSendmailButton);
}

void EmailPrefTab::slotFromAddrChanged(QAbstractButton* button)
{
	mEmailAddress->setEnabled(button == mFromAddrButton);
	mAddressChanged = true;
}

void EmailPrefTab::slotBccAddrChanged(QAbstractButton* button)
{
	mEmailBccAddress->setEnabled(button == mBccAddrButton);
	mBccAddressChanged = true;
}

QString EmailPrefTab::validate()
{
	if (mAddressChanged)
	{
		mAddressChanged = false;
		QString errmsg = validateAddr(mFromAddressGroup, mEmailAddress, KAMail::i18n_NeedFromEmailAddress());
		if (!errmsg.isEmpty())
			return errmsg;
	}
	if (mBccAddressChanged)
	{
		mBccAddressChanged = false;
		return validateAddr(mBccAddressGroup, mEmailBccAddress, i18n("No valid 'Bcc' email address is specified."));
	}
	return QString::null;
}

QString EmailPrefTab::validateAddr(ButtonGroup* group, QLineEdit* addr, const QString& msg)
{
	QString errmsg = i18n("%1\nAre you sure you want to save your changes?").arg(msg);
	switch (group->selectedId())
	{
		case Preferences::MAIL_FROM_CONTROL_CENTRE:
			if (!KAMail::controlCentreAddress().isEmpty())
				return QString::null;
			errmsg = i18n("No email address is currently set in the KDE Control Center. %1").arg(errmsg);
			break;
		case Preferences::MAIL_FROM_KMAIL:
			if (KAMail::identitiesExist())
				return QString::null;
			errmsg = i18n("No KMail identities currently exist. %1").arg(errmsg);
			break;
		case Preferences::MAIL_FROM_ADDR:
			if (!addr->text().stripWhiteSpace().isEmpty())
				return QString::null;
			break;
	}
	return errmsg;
}


/*=============================================================================
= Class FontColourPrefTab
=============================================================================*/

FontColourPrefTab::FontColourPrefTab(KVBox* frame)
	: PrefsTabBase(frame)
{
	mFontChooser = new FontColourChooser(mPage, 0, false, QStringList(), i18n("Message Font && Color"), true, false);

	KHBox* layoutBox = new KHBox(mPage);
	KHBox* box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	QLabel* label1 = new QLabel(i18n("Di&sabled alarm color:"), box);
//	label1->setMinimumSize(label1->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mDisabledColour = new KColorCombo(box);
	mDisabledColour->setMinimumSize(mDisabledColour->sizeHint());
	label1->setBuddy(mDisabledColour);
	box->setWhatsThis(i18n("Choose the text color in the alarm list for disabled alarms."));
	layoutBox->setStretchFactor(new QWidget(layoutBox), 1);    // left adjust the controls
	layoutBox->setFixedHeight(layoutBox->sizeHint().height());

	layoutBox = new KHBox(mPage);
	box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	QLabel* label2 = new QLabel(i18n("E&xpired alarm color:"), box);
//	label2->setMinimumSize(label2->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mExpiredColour = new KColorCombo(box);
	mExpiredColour->setMinimumSize(mExpiredColour->sizeHint());
	label2->setBuddy(mExpiredColour);
	box->setWhatsThis(i18n("Choose the text color in the alarm list for expired alarms."));
	layoutBox->setStretchFactor(new QWidget(layoutBox), 1);    // left adjust the controls
	layoutBox->setFixedHeight(layoutBox->sizeHint().height());

	// Line up the two sets of colour controls
	QSize size = label1->sizeHint();
	QSize size2 = label2->sizeHint();
	if (size2.width() > size.width())
		size.setWidth(size2.width());
	label1->setFixedSize(size);
	label2->setFixedSize(size);

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void FontColourPrefTab::restore()
{
	mFontChooser->setBgColour(Preferences::mDefaultBgColour);
	mFontChooser->setColours(Preferences::mMessageColours);
	mFontChooser->setFont(Preferences::mMessageFont);
	mDisabledColour->setColor(Preferences::mDisabledColour);
	mExpiredColour->setColor(Preferences::mExpiredColour);
}

void FontColourPrefTab::apply(bool syncToDisc)
{
	Preferences::mDefaultBgColour = mFontChooser->bgColour();
	Preferences::mMessageColours  = mFontChooser->colours();
	Preferences::mMessageFont     = mFontChooser->font();
	Preferences::mDisabledColour  = mDisabledColour->color();
	Preferences::mExpiredColour   = mExpiredColour->color();
	PrefsTabBase::apply(syncToDisc);
}

void FontColourPrefTab::setDefaults()
{
	mFontChooser->setBgColour(Preferences::default_defaultBgColour);
	mFontChooser->setColours(Preferences::default_messageColours);
	mFontChooser->setFont(Preferences::default_messageFont());
	mDisabledColour->setColor(Preferences::default_disabledColour);
	mExpiredColour->setColor(Preferences::default_expiredColour);
}


/*=============================================================================
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab(KVBox* frame)
	: PrefsTabBase(frame)
{
	int groupTopMargin = fontMetrics().lineSpacing()/2;
	QString defsetting   = i18n("The default setting for \"%1\" in the alarm edit dialog.");
	QString soundSetting = i18n("Check to select %1 as the default setting for \"%2\" in the alarm edit dialog.");

	// DISPLAY ALARMS
	QGroupBox* group = new QGroupBox(i18n("Display Alarms"), mPage);
	QVBoxLayout* vlayout = new QVBoxLayout(group, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(groupTopMargin);

	mConfirmAck = new QCheckBox(EditAlarmDlg::i18n_k_ConfirmAck(), group, "defConfAck");
	mConfirmAck->setMinimumSize(mConfirmAck->sizeHint());
	mConfirmAck->setWhatsThis(defsetting.arg(EditAlarmDlg::i18n_ConfirmAck()));
	vlayout->addWidget(mConfirmAck, 0, Qt::AlignLeft);

	mAutoClose = new QCheckBox(LateCancelSelector::i18n_i_AutoCloseWinLC(), group, "defAutoClose");
	mAutoClose->setMinimumSize(mAutoClose->sizeHint());
	mAutoClose->setWhatsThis(defsetting.arg(LateCancelSelector::i18n_AutoCloseWin()));
	vlayout->addWidget(mAutoClose, 0, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	QLabel* label = new QLabel(i18n("Reminder &units:"), box);
	label->setFixedSize(label->sizeHint());
	mReminderUnits = new QComboBox(box, "defWarnUnits");
	mReminderUnits->insertItem(TimePeriod::i18n_Hours_Mins(), TimePeriod::HOURS_MINUTES);
	mReminderUnits->insertItem(TimePeriod::i18n_Days(), TimePeriod::DAYS);
	mReminderUnits->insertItem(TimePeriod::i18n_Weeks(), TimePeriod::WEEKS);
	mReminderUnits->setFixedSize(mReminderUnits->sizeHint());
	label->setBuddy(mReminderUnits);
	box->setWhatsThis(i18n("The default units for the reminder in the alarm edit dialog."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	mSpecialActions = new SpecialActions(group);
	mSpecialActions->setFixedHeight(mSpecialActions->sizeHint().height());
	vlayout->addWidget(mSpecialActions);

	// SOUND
	QGroupBox* bbox = new QGroupBox(SoundPicker::i18n_Sound(), mPage);
	vlayout = new QVBoxLayout(bbox, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(groupTopMargin);
	QButtonGroup* bgroup = new QButtonGroup(bbox);

	mSound = new QCheckBox(SoundPicker::i18n_s_Sound(), bbox, "defSound");
	mSound->setMinimumSize(mSound->sizeHint());
	mSound->setWhatsThis(defsetting.arg(SoundPicker::i18n_Sound()));
	vlayout->addWidget(mSound, 0, Qt::AlignLeft);

	box = new KHBox(bbox);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box, 0, Qt::AlignLeft);

	mBeep = new QRadioButton(SoundPicker::i18n_b_Beep(), box, "defBeep");
	bgroup->addButton(mBeep);
	mBeep->setMinimumSize(mBeep->sizeHint());
	mBeep->setWhatsThis(soundSetting.arg(SoundPicker::i18n_Beep()).arg(SoundPicker::i18n_Sound()));
	mFile = new QRadioButton(SoundPicker::i18n_File(), box, "defFile");
	bgroup->addButton(mFile);
	mFile->setMinimumSize(mFile->sizeHint());
	mFile->setWhatsThis(soundSetting.arg(SoundPicker::i18n_File()).arg(SoundPicker::i18n_Sound()));
	if (theApp()->speechEnabled())
	{
		mSpeak = new QRadioButton(SoundPicker::i18n_Speak(), box, "defSpeak");
		mSpeak->setMinimumSize(mSpeak->sizeHint());
		mSpeak->setWhatsThis(soundSetting.arg(SoundPicker::i18n_Speak()).arg(SoundPicker::i18n_Sound()));
		bgroup->addButton(mSpeak);
	}
	else
		mSpeak = 0;
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls

	box = new KHBox(bbox);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mSoundFileLabel = new QLabel(i18n("Sound &file:"), box);
	mSoundFileLabel->setFixedSize(mSoundFileLabel->sizeHint());
	mSoundFile = new QLineEdit(box);
	mSoundFileLabel->setBuddy(mSoundFile);
	mSoundFileBrowse = new QPushButton(box);
	mSoundFileBrowse->setPixmap(SmallIcon("fileopen"));
	mSoundFileBrowse->setFixedSize(mSoundFileBrowse->sizeHint());
	connect(mSoundFileBrowse, SIGNAL(clicked()), SLOT(slotBrowseSoundFile()));
	QToolTip::add(mSoundFileBrowse, i18n("Choose a sound file"));
	box->setWhatsThis(i18n("Enter the default sound file to use in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box);

#ifndef WITHOUT_ARTS
	mSoundRepeat = new QCheckBox(i18n("Repea&t sound file"), bbox, "defRepeatSound");
	mSoundRepeat->setMinimumSize(mSoundRepeat->sizeHint());
	mSoundRepeat->setWhatsThis(i18n("sound file \"Repeat\" checkbox", "The default setting for sound file \"%1\" in the alarm edit dialog.").arg(SoundDlg::i18n_Repeat()));
	vlayout->addWidget(mSoundRepeat, 0, Qt::AlignLeft);
#endif
	bbox->setFixedHeight(bbox->sizeHint().height());

	// COMMAND ALARMS
	group = new QGroupBox(i18n("Command Alarms"), mPage);
	vlayout = new QVBoxLayout(group, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(groupTopMargin);
	QHBoxLayout* hlayout = new QHBoxLayout(vlayout, KDialog::spacingHint());

	mCmdScript = new QCheckBox(EditAlarmDlg::i18n_p_EnterScript(), group, "defCmdScript");
	mCmdScript->setMinimumSize(mCmdScript->sizeHint());
	mCmdScript->setWhatsThis(defsetting.arg(EditAlarmDlg::i18n_EnterScript()));
	hlayout->addWidget(mCmdScript);
	hlayout->addStretch();

	mCmdXterm = new QCheckBox(EditAlarmDlg::i18n_w_ExecInTermWindow(), group, "defCmdXterm");
	mCmdXterm->setMinimumSize(mCmdXterm->sizeHint());
	mCmdXterm->setWhatsThis(defsetting.arg(EditAlarmDlg::i18n_ExecInTermWindow()));
	hlayout->addWidget(mCmdXterm);

	// EMAIL ALARMS
	group = new QGroupBox(i18n("Email Alarms"), mPage);
	vlayout = new QVBoxLayout(group, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(groupTopMargin);

	// BCC email to sender
	mEmailBcc = new QCheckBox(EditAlarmDlg::i18n_e_CopyEmailToSelf(), group, "defEmailBcc");
	mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(defsetting.arg(EditAlarmDlg::i18n_CopyEmailToSelf()));
	vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

	// MISCELLANEOUS
	// Show in KOrganizer
	mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_g_ShowInKOrganizer(), mPage, "defShowKorg");
	mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
	mCopyToKOrganizer->setWhatsThis(defsetting.arg(EditAlarmDlg::i18n_ShowInKOrganizer()));

	// Late cancellation
	box = new KHBox(mPage);
	box->setSpacing(KDialog::spacingHint());
	mLateCancel = new QCheckBox(LateCancelSelector::i18n_n_CancelIfLate(), box, "defCancelLate");
	mLateCancel->setMinimumSize(mLateCancel->sizeHint());
	mLateCancel->setWhatsThis(defsetting.arg(LateCancelSelector::i18n_CancelIfLate()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	// Recurrence
	KHBox* itemBox = new KHBox(box);   // this is to control the QWhatsThis text display area
	itemBox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("&Recurrence:"), itemBox);
	label->setFixedSize(label->sizeHint());
	mRecurPeriod = new QComboBox(itemBox, "defRecur");
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_NoRecur());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_AtLogin());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_HourlyMinutely());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_Daily());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_Weekly());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_Monthly());
	mRecurPeriod->insertItem(RecurrenceEdit::i18n_Yearly());
	mRecurPeriod->setFixedSize(mRecurPeriod->sizeHint());
	label->setBuddy(mRecurPeriod);
	itemBox->setWhatsThis(i18n("The default setting for the recurrence rule in the alarm edit dialog."));
	box->setFixedHeight(itemBox->sizeHint().height());

	// How to handle February 29th in yearly recurrences
	QWidget* vbox = new QWidget(mPage);   // this is to control the QWhatsThis text display area
	vbox->setLayout(new QVBoxLayout(vbox, 0, KDialog::spacingHint()));
	label = new QLabel(i18n("In non-leap years, repeat yearly February 29th alarms on:"), vbox);
	label->setAlignment(Qt::AlignLeft | Qt::TextWordWrap);
	itemBox = new KHBox(vbox);
	itemBox->setSpacing(2*KDialog::spacingHint());
	mFeb29 = new ButtonGroup(itemBox);
	QWidget* widget = new QWidget(itemBox);
	widget->setFixedWidth(3*KDialog::spacingHint());
	QRadioButton* radio = new QRadioButton(i18n("February 2&8th"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, KARecurrence::FEB29_FEB28);
	radio = new QRadioButton(i18n("March &1st"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, KARecurrence::FEB29_MAR1);
	radio = new QRadioButton(i18n("Do &not repeat"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, KARecurrence::FEB29_FEB29);
	itemBox->setFixedHeight(itemBox->sizeHint().height());
	vbox->setWhatsThis(
	      i18n("For yearly recurrences, choose what date, if any, alarms due on February 29th should occur in non-leap years.\n"
	           "Note that the next scheduled occurrence of existing alarms is not re-evaluated when you change this setting."));

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void EditPrefTab::restore()
{
	mAutoClose->setChecked(Preferences::mDefaultAutoClose);
	mConfirmAck->setChecked(Preferences::mDefaultConfirmAck);
	mReminderUnits->setCurrentItem(Preferences::mDefaultReminderUnits);
	mSpecialActions->setActions(Preferences::mDefaultPreAction, Preferences::mDefaultPostAction);
	mSound->setChecked(Preferences::mDefaultSound);
	setSoundType(Preferences::mDefaultSoundType);
	mSoundFile->setText(Preferences::mDefaultSoundFile);
#ifndef WITHOUT_ARTS
	mSoundRepeat->setChecked(Preferences::mDefaultSoundRepeat);
#endif
	mCmdScript->setChecked(Preferences::mDefaultCmdScript);
	mCmdXterm->setChecked(Preferences::mDefaultCmdLogType == Preferences::EXEC_IN_TERMINAL);
	mEmailBcc->setChecked(Preferences::mDefaultEmailBcc);
	mCopyToKOrganizer->setChecked(Preferences::mDefaultCopyToKOrganizer);
	mLateCancel->setChecked(Preferences::mDefaultLateCancel);
	mRecurPeriod->setCurrentItem(recurIndex(Preferences::mDefaultRecurPeriod));
	mFeb29->setButton(Preferences::mDefaultFeb29Type);
}

void EditPrefTab::apply(bool syncToDisc)
{
	Preferences::mDefaultAutoClose        = mAutoClose->isChecked();
	Preferences::mDefaultConfirmAck       = mConfirmAck->isChecked();
	Preferences::mDefaultReminderUnits    = static_cast<TimePeriod::Units>(mReminderUnits->currentItem());
	Preferences::mDefaultPreAction        = mSpecialActions->preAction();
	Preferences::mDefaultPostAction       = mSpecialActions->postAction();
	Preferences::mDefaultSound            = mSound->isChecked();
	Preferences::mDefaultSoundFile        = mSoundFile->text();
	Preferences::mDefaultSoundType        = mSpeak && mSpeak->isOn() ? SoundPicker::SPEAK
	                                      : mFile->isOn()            ? SoundPicker::PLAY_FILE
	                                      :                            SoundPicker::BEEP;
#ifndef WITHOUT_ARTS
	Preferences::mDefaultSoundRepeat      = mSoundRepeat->isChecked();
#endif
	Preferences::mDefaultCmdScript        = mCmdScript->isChecked();
	Preferences::mDefaultCmdLogFile       = (mCmdXterm->isChecked() ? Preferences::EXEC_IN_TERMINAL : Preferences::DISCARD_OUTPUT);
	Preferences::mDefaultEmailBcc         = mEmailBcc->isChecked();
	Preferences::mDefaultCopyToKOrganizer = mCopyToKOrganizer->isChecked();
	Preferences::mDefaultLateCancel       = mLateCancel->isChecked() ? 1 : 0;
	switch (mRecurPeriod->currentItem())
	{
		case 6:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::ANNUAL;    break;
		case 5:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::MONTHLY;   break;
		case 4:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::WEEKLY;    break;
		case 3:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::DAILY;     break;
		case 2:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::SUBDAILY;  break;
		case 1:  Preferences::mDefaultRecurPeriod = RecurrenceEdit::AT_LOGIN;  break;
		case 0:
		default: Preferences::mDefaultRecurPeriod = RecurrenceEdit::NO_RECUR;  break;
	}
	int feb29 = mFeb29->selectedId();
	Preferences::mDefaultFeb29Type  = (feb29 >= 0) ? static_cast<KARecurrence::Feb29Type>(feb29) : Preferences::default_defaultFeb29Type;
	PrefsTabBase::apply(syncToDisc);
}

void EditPrefTab::setDefaults()
{
	mAutoClose->setChecked(Preferences::default_defaultAutoClose);
	mConfirmAck->setChecked(Preferences::default_defaultConfirmAck);
	mReminderUnits->setCurrentItem(Preferences::default_defaultReminderUnits);
	mSpecialActions->setActions(Preferences::default_defaultPreAction, Preferences::default_defaultPostAction);
	mSound->setChecked(Preferences::default_defaultSound);
	setSoundType(Preferences::default_defaultSoundType);
	mSoundFile->setText(Preferences::default_defaultSoundFile);
#ifndef WITHOUT_ARTS
	mSoundRepeat->setChecked(Preferences::default_defaultSoundRepeat);
#endif
	mCmdScript->setChecked(Preferences::default_defaultCmdScript);
	mCmdXterm->setChecked(Preferences::default_defaultCmdLogType == Preferences::EXEC_IN_TERMINAL);
	mEmailBcc->setChecked(Preferences::default_defaultEmailBcc);
	mCopyToKOrganizer->setChecked(Preferences::default_defaultCopyToKOrganizer);
	mLateCancel->setChecked(Preferences::default_defaultLateCancel);
	mRecurPeriod->setCurrentItem(recurIndex(Preferences::default_defaultRecurPeriod));
	mFeb29->setButton(Preferences::default_defaultFeb29Type);
}

void EditPrefTab::slotBrowseSoundFile()
{
	QString defaultDir;
	QString url = SoundPicker::browseFile(defaultDir, mSoundFile->text());
	if (!url.isEmpty())
		mSoundFile->setText(url);
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

void EditPrefTab::setSoundType(SoundPicker::Type type)
{
	switch (type)
	{
		case SoundPicker::PLAY_FILE:
			mFile->setChecked(true);
			break;
		case SoundPicker::SPEAK:
			if (mSpeak)
			{
				mSpeak->setChecked(true);
				break;
			}
			// fall through to BEEP
		case SoundPicker::BEEP:
		default:
			mBeep->setChecked(true);
			break;
	}
}

QString EditPrefTab::validate()
{
	if (mFile->isOn()  &&  mSoundFile->text().isEmpty())
	{
		mSoundFile->setFocus();
		return i18n("You must enter a sound file when %1 is selected as the default sound type").arg(SoundPicker::i18n_File());;
	}
	return QString::null;
}


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab(KVBox* frame)
	: PrefsTabBase(frame)
{
	QGroupBox* group = new QGroupBox(i18n("Alarm List"), mPage);
	QVBoxLayout* vlayout = new QVBoxLayout(group, KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);

	mListShowTime = new QCheckBox(MainWindow::i18n_t_ShowAlarmTime(), group, "listTime");
	mListShowTime->setMinimumSize(mListShowTime->sizeHint());
	connect(mListShowTime, SIGNAL(toggled(bool)), SLOT(slotListTimeToggled(bool)));
	mListShowTime->setWhatsThis(i18n("Specify whether to show in the alarm list, the time at which each alarm is due"));
	vlayout->addWidget(mListShowTime, 0, Qt::AlignLeft);

	mListShowTimeTo = new QCheckBox(MainWindow::i18n_n_ShowTimeToAlarm(), group, "listTimeTo");
	mListShowTimeTo->setMinimumSize(mListShowTimeTo->sizeHint());
	connect(mListShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotListTimeToToggled(bool)));
	mListShowTimeTo->setWhatsThis(i18n("Specify whether to show in the alarm list, how long until each alarm is due"));
	vlayout->addWidget(mListShowTimeTo, 0, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	group = new QGroupBox(i18n("System Tray Tooltip"), mPage);
	QGridLayout* grid = new QGridLayout(group, 5, 3, KDialog::marginHint(), KDialog::spacingHint());
	grid->setColStretch(2, 1);
	grid->addColSpacing(0, indentWidth());
	grid->addColSpacing(1, indentWidth());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	mTooltipShowAlarms = new QCheckBox(i18n("Show next &24 hours' alarms"), group, "tooltipShow");
	mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
	connect(mTooltipShowAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipAlarmsToggled(bool)));
	mTooltipShowAlarms->setWhatsThis(i18n("Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours"));
	grid->addMultiCellWidget(mTooltipShowAlarms, 1, 1, 0, 2, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setSpacing(KDialog::spacingHint());
	mTooltipMaxAlarms = new QCheckBox(i18n("Ma&ximum number of alarms to show:"), box, "tooltipMax");
	mTooltipMaxAlarms->setMinimumSize(mTooltipMaxAlarms->sizeHint());
	connect(mTooltipMaxAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipMaxToggled(bool)));
	mTooltipMaxAlarmCount = new SpinBox(1, 99, 1, box);
	mTooltipMaxAlarmCount->setSingleShiftStep(5);
	mTooltipMaxAlarmCount->setMinimumSize(mTooltipMaxAlarmCount->sizeHint());
	box->setWhatsThis(
	      i18n("Uncheck to display all of the next 24 hours' alarms in the system tray tooltip. "
	           "Check to enter an upper limit on the number to be displayed."));
	grid->addMultiCellWidget(box, 2, 2, 1, 2, Qt::AlignLeft);

	mTooltipShowTime = new QCheckBox(MainWindow::i18n_m_ShowAlarmTime(), group, "tooltipTime");
	mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
	connect(mTooltipShowTime, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToggled(bool)));
	mTooltipShowTime->setWhatsThis(i18n("Specify whether to show in the system tray tooltip, the time at which each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTime, 3, 3, 1, 2, Qt::AlignLeft);

	mTooltipShowTimeTo = new QCheckBox(MainWindow::i18n_l_ShowTimeToAlarm(), group, "tooltipTimeTo");
	mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
	connect(mTooltipShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToToggled(bool)));
	mTooltipShowTimeTo->setWhatsThis(i18n("Specify whether to show in the system tray tooltip, how long until each alarm is due"));
	grid->addMultiCellWidget(mTooltipShowTimeTo, 4, 4, 1, 2, Qt::AlignLeft);

	box = new KHBox(group);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	mTooltipTimeToPrefixLabel = new QLabel(i18n("&Prefix:"), box);
	mTooltipTimeToPrefixLabel->setFixedSize(mTooltipTimeToPrefixLabel->sizeHint());
	mTooltipTimeToPrefix = new QLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	box->setWhatsThis(i18n("Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip"));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 5, 2, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	mModalMessages = new QCheckBox(i18n("Message &windows have a title bar and take keyboard focus"), mPage, "modalMsg");
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	mModalMessages->setWhatsThis(
	      i18n("Specify the characteristics of alarm message windows:\n"
	           "- If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.\n"
	           "- If unchecked, the window does not interfere with your typing when "
	           "it is displayed, but it has no title bar and cannot be moved or resized."));

	mShowExpiredAlarms = new QCheckBox(MainWindow::i18n_e_ShowExpiredAlarms(), mPage, "showExpired");
	mShowExpiredAlarms->setMinimumSize(mShowExpiredAlarms->sizeHint());
	mShowExpiredAlarms->setWhatsThis(i18n("Specify whether to show expired alarms in the alarm list"));

	KHBox* itemBox = new KHBox(mPage);   // this is to control the QWhatsThis text display area
	box = new KHBox(itemBox);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("System tray icon &update interval:"), box);
	mDaemonTrayCheckInterval = new SpinBox(1, 9999, 1, box);
	mDaemonTrayCheckInterval->setSingleShiftStep(10);
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18n("seconds"), box);
	box->setWhatsThis(i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	mPage->setStretchFactor(new QWidget(mPage), 1);    // top adjust the widgets
}

void ViewPrefTab::restore()
{
	setList(Preferences::mShowAlarmTime,
	        Preferences::mShowTimeToAlarm);
	setTooltip(Preferences::mTooltipAlarmCount,
	           Preferences::mShowTooltipAlarmTime,
	           Preferences::mShowTooltipTimeToAlarm,
	           Preferences::mTooltipTimeToPrefix);
	mModalMessages->setChecked(Preferences::mModalMessages);
	mShowExpiredAlarms->setChecked(Preferences::mShowExpiredAlarms);
	mDaemonTrayCheckInterval->setValue(Preferences::mDaemonTrayCheckInterval);
}

void ViewPrefTab::apply(bool syncToDisc)
{
	Preferences::mShowAlarmTime           = mListShowTime->isChecked();
	Preferences::mShowTimeToAlarm         = mListShowTimeTo->isChecked();
	int n = mTooltipShowAlarms->isChecked() ? -1 : 0;
	if (n  &&  mTooltipMaxAlarms->isChecked())
		n = mTooltipMaxAlarmCount->value();
	Preferences::mTooltipAlarmCount       = n;
	Preferences::mShowTooltipAlarmTime    = mTooltipShowTime->isChecked();
	Preferences::mShowTooltipTimeToAlarm  = mTooltipShowTimeTo->isChecked();
	Preferences::mTooltipTimeToPrefix     = mTooltipTimeToPrefix->text();
	Preferences::mModalMessages           = mModalMessages->isChecked();
	Preferences::mShowExpiredAlarms       = mShowExpiredAlarms->isChecked();
	Preferences::mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
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
		time = true;    // ensure that at least one option is ticked

	// Set the states of the two checkboxes without calling signal
	// handlers, since these could change the checkboxes' states.
	mListShowTime->blockSignals(true);
	mListShowTimeTo->blockSignals(true);

	mListShowTime->setChecked(time);
	mListShowTimeTo->setChecked(timeTo);

	mListShowTime->blockSignals(false);
	mListShowTimeTo->blockSignals(false);
}

void ViewPrefTab::setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix)
{
	if (!timeTo)
		time = true;    // ensure that at least one time option is ticked

	// Set the states of the controls without calling signal
	// handlers, since these could change the checkboxes' states.
	mTooltipShowAlarms->blockSignals(true);
	mTooltipShowTime->blockSignals(true);
	mTooltipShowTimeTo->blockSignals(true);

	mTooltipShowAlarms->setChecked(maxAlarms);
	mTooltipMaxAlarms->setChecked(maxAlarms > 0);
	mTooltipMaxAlarmCount->setValue(maxAlarms > 0 ? maxAlarms : 1);
	mTooltipShowTime->setChecked(time);
	mTooltipShowTimeTo->setChecked(timeTo);
	mTooltipTimeToPrefix->setText(prefix);

	mTooltipShowAlarms->blockSignals(false);
	mTooltipShowTime->blockSignals(false);
	mTooltipShowTimeTo->blockSignals(false);

	// Enable/disable controls according to their states
	slotTooltipTimeToToggled(timeTo);
	slotTooltipAlarmsToggled(maxAlarms);
}

void ViewPrefTab::slotListTimeToggled(bool on)
{
	if (!on  &&  !mListShowTimeTo->isChecked())
		mListShowTimeTo->setChecked(true);
}

void ViewPrefTab::slotListTimeToToggled(bool on)
{
	if (!on  &&  !mListShowTime->isChecked())
		mListShowTime->setChecked(true);
}

void ViewPrefTab::slotTooltipAlarmsToggled(bool on)
{
	mTooltipMaxAlarms->setEnabled(on);
	mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isChecked());
	mTooltipShowTime->setEnabled(on);
	mTooltipShowTimeTo->setEnabled(on);
	on = on && mTooltipShowTimeTo->isChecked();
	mTooltipTimeToPrefix->setEnabled(on);
	mTooltipTimeToPrefixLabel->setEnabled(on);
}

void ViewPrefTab::slotTooltipMaxToggled(bool on)
{
	mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isEnabled());
}

void ViewPrefTab::slotTooltipTimeToggled(bool on)
{
	if (!on  &&  !mTooltipShowTimeTo->isChecked())
		mTooltipShowTimeTo->setChecked(true);
}

void ViewPrefTab::slotTooltipTimeToToggled(bool on)
{
	if (!on  &&  !mTooltipShowTime->isChecked())
		mTooltipShowTime->setChecked(true);
	on = on && mTooltipShowTimeTo->isEnabled();
	mTooltipTimeToPrefix->setEnabled(on);
	mTooltipTimeToPrefixLabel->setEnabled(on);
}
