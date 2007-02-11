/*
 *  prefdlg.cpp  -  program preferences dialog
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <kvbox.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kshell.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kcolorcombo.h>
#include <KStandardGuiItem>
#include <ksystemtimezone.h>
#include <kdebug.h>
#include <kicon.h>

#include <kalarmd/kalarmd.h>
#include <ktoolinvocation.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "daemon.h"
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
#include "sounddlg.h"
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
	QLatin1String("xterm -sb -hold -title %t -e %c"),
	QLatin1String("konsole --noclose -T %t -e ${SHELL:-sh} -c %c"),
	QLatin1String("gnome-terminal -t %t -e %W"),
	QLatin1String("eterm --pause -T %t -e %C"),    // some systems use eterm...
	QLatin1String("Eterm --pause -T %t -e %C"),    // while some use Eterm
	QLatin1String("rxvt -title %t -e ${SHELL:-sh} -c %w"),
	QString()       // end of list indicator - don't change!
};


/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg::KAlarmPrefDlg()
	: KPageDialog()
{
	setObjectName("PrefDlg");    // used by LikeBack
	setCaption(i18n("Preferences"));
	setButtons(Help | Default | Ok | Apply | Cancel);
	setDefaultButton(Ok);
	setFaceType(List);
	setModal(true);
	showButtonSeparator(true);
	//setIconListAllVisible(true);

	mMiscPage = new MiscPrefTab;
	mMiscPageItem = new KPageWidgetItem(mMiscPage, i18n("General"));
	mMiscPageItem->setHeader(i18n("General"));
	mMiscPageItem->setIcon(KIcon(DesktopIcon("misc")));
	addPage(mMiscPageItem);

	mStorePage = new StorePrefTab;
	mStorePageItem = new KPageWidgetItem(mStorePage, i18n("Storage"));
	mStorePageItem->setHeader(i18n("Alarm Storage"));
	mStorePageItem->setIcon(KIcon(DesktopIcon("fileopen")));
	addPage(mStorePageItem);

	mEmailPage = new EmailPrefTab;
	mEmailPageItem = new KPageWidgetItem(mEmailPage, i18n("Email"));
	mEmailPageItem->setHeader(i18n("Email Alarm Settings"));
	mEmailPageItem->setIcon(KIcon(DesktopIcon("mail_generic")));
	addPage(mEmailPageItem);

	mViewPage = new ViewPrefTab;
	mViewPageItem = new KPageWidgetItem(mViewPage, i18n("View"));
	mViewPageItem->setHeader(i18n("View Settings"));
	mViewPageItem->setIcon(KIcon(DesktopIcon("view_choose")));
	addPage(mViewPageItem);

	mFontColourPage = new FontColourPrefTab;
	mFontColourPageItem = new KPageWidgetItem(mFontColourPage, i18n("Font & Color"));
	mFontColourPageItem->setHeader(i18n("Default Font and Color"));
	mFontColourPageItem->setIcon(KIcon(DesktopIcon("colorize")));
	addPage(mFontColourPageItem);

	mEditPage = new EditPrefTab;
	mEditPageItem = new KPageWidgetItem(mEditPage, i18n("Edit"));
	mEditPageItem->setHeader(i18n("Default Alarm Edit Settings"));
	mEditPageItem->setIcon(KIcon(DesktopIcon("edit")));
	addPage(mEditPageItem);

	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));
	connect(this, SIGNAL(cancelClicked()), SLOT(slotCancel()));
	connect(this, SIGNAL(applyClicked()), SLOT(slotApply()));
	connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
	connect(this, SIGNAL(helpClicked()), SLOT(slotHelp()));
	restore();
	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

// Restore all defaults in the options...
void KAlarmPrefDlg::slotDefault()
{
	kDebug(5950) << "KAlarmPrefDlg::slotDefault()" << endl;
	mFontColourPage->setDefaults();
	mEmailPage->setDefaults();
	mViewPage->setDefaults();
	mEditPage->setDefaults();
	mStorePage->setDefaults();
	mMiscPage->setDefaults();
}

void KAlarmPrefDlg::slotHelp()
{
	KToolInvocation::invokeHelp("preferences");
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
	kDebug(5950) << "KAlarmPrefDlg::slotApply()" << endl;
	QString errmsg = mEmailPage->validate();
	if (!errmsg.isEmpty())
	{
		setCurrentPage(mEmailPageItem);
		if (KMessageBox::warningYesNo(this, errmsg) != KMessageBox::Yes)
		{
			mValid = false;
			return;
		}
	}
	errmsg = mEditPage->validate();
	if (!errmsg.isEmpty())
	{
		setCurrentPage(mEditPageItem);
		KMessageBox::sorry(this, errmsg);
		mValid = false;
		return;
	}
	mValid = true;
	mFontColourPage->apply(false);
	mEmailPage->apply(false);
	mViewPage->apply(false);
	mEditPage->apply(false);
	mStorePage->apply(false);
	mMiscPage->apply(false);
	Preferences::syncToDisc();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
	kDebug(5950) << "KAlarmPrefDlg::slotOk()" << endl;
	mValid = true;
	slotApply();
	if (mValid)
		KDialog::accept();
}

// Discard the current preferences and close the dialogue
void KAlarmPrefDlg::slotCancel()
{
	kDebug(5950) << "KAlarmPrefDlg::slotCancel()" << endl;
	restore();
	KDialog::reject();
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::restore()
{
	kDebug(5950) << "KAlarmPrefDlg::restore()" << endl;
	mFontColourPage->restore();
	mEmailPage->restore();
	mViewPage->restore();
	mEditPage->restore();
	mStorePage->restore();
	mMiscPage->restore();
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/
int PrefsTabBase::mIndentWidth = 0;

PrefsTabBase::PrefsTabBase()
	: KVBox()
{
	setMargin(0);
	setSpacing(KDialog::spacingHint());
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

MiscPrefTab::MiscPrefTab()
	: PrefsTabBase()
{
	QGroupBox* group = new QGroupBox(i18n("Run Mode"), this);
	QButtonGroup* buttonGroup = new QButtonGroup(group);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(2, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	grid->setColumnMinimumWidth(1, indentWidth());

	// Run-on-demand radio button
	mRunOnDemand = new QRadioButton(i18n("&Run only on demand"), group);
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunOnDemand->setWhatsThis(
	      i18n("Check to run KAlarm only when required.\n\n"
	           "Notes:\n"
	           "1. Alarms are displayed even when KAlarm is not running, since alarm monitoring is done by the alarm daemon.\n"
	           "2. With this option selected, the system tray icon can be displayed or hidden independently of KAlarm."));
	buttonGroup->addButton(mRunOnDemand);
	grid->addWidget(mRunOnDemand, 1, 0, 1, 3, Qt::AlignLeft);

	// Run-in-system-tray radio button
	mRunInSystemTray = new QRadioButton(i18n("Run continuously in system &tray"), group);
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunInSystemTray->setWhatsThis(
	      i18n("Check to run KAlarm continuously in the KDE system tray.\n\n"
	           "Notes:\n"
	           "1. With this option selected, closing the system tray icon will quit KAlarm.\n"
	           "2. You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon."
	           " Running in the system tray simply provides easy access and a status indication."));
	buttonGroup->addButton(mRunInSystemTray);
	grid->addWidget(mRunInSystemTray, 2, 0, 1, 3, Qt::AlignLeft);

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disa&ble alarms while not running"), group);
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	connect(mDisableAlarmsIfStopped, SIGNAL(toggled(bool)), SLOT(slotDisableIfStoppedToggled(bool)));
	mDisableAlarmsIfStopped->setWhatsThis(i18n("Check to disable alarms whenever KAlarm is not running. Alarms will only appear while the system tray icon is visible."));
	grid->addWidget(mDisableAlarmsIfStopped, 3, 1, 1, 2, Qt::AlignLeft);

	mQuitWarn = new QCheckBox(i18n("Warn before &quitting"), group);
	mQuitWarn->setFixedSize(mQuitWarn->sizeHint());
	mQuitWarn->setWhatsThis(i18n("Check to display a warning prompt before quitting KAlarm."));
	grid->addWidget(mQuitWarn, 4, 2, Qt::AlignLeft);

	mAutostartTrayIcon = new QCheckBox(i18n("Autostart at &login"), group);
	mAutostartTrayIcon->setFixedSize(mAutostartTrayIcon->sizeHint());
#ifdef AUTOSTART_BY_KALARMD
	connect(mAutostartTrayIcon, SIGNAL(toggled(bool)), SLOT(slotAutostartToggled(bool)));
#endif
	mAutostartTrayIcon->setWhatsThis(i18n("Check to run KAlarm whenever you start KDE."));
	grid->addWidget(mAutostartTrayIcon, 5, 0, 1, 3, Qt::AlignLeft);

	// Autostart alarm daemon
	mAutostartDaemon = new QCheckBox(i18n("Start alarm monitoring at lo&gin"), group);
	mAutostartDaemon->setFixedSize(mAutostartDaemon->sizeHint());
	connect(mAutostartDaemon, SIGNAL(clicked()), SLOT(slotAutostartDaemonClicked()));
	mAutostartDaemon->setWhatsThis(
	      i18n("Automatically start alarm monitoring whenever you start KDE, by running the alarm daemon (%1).\n\n"
	           "This option should always be checked unless you intend to discontinue use of KAlarm.",
	           QLatin1String(DAEMON_APP_NAME)));
	grid->addWidget(mAutostartDaemon, 6, 0, 1, 3, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	// Default time zone
	KHBox* itemBox = new KHBox(this);
	itemBox->setMargin(0);
	KHBox* box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("Time &zone:"), box);
	mTimeZone = new QComboBox(box);
	mTimeZone->setMaxVisibleItems(15);
	const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
	for (KTimeZones::ZoneMap::ConstIterator it = zones.begin();  it != zones.end();  ++it)
		mTimeZone->addItem(it.key());
	box->setWhatsThis(i18n("Select the time zone which KAlarm should use as its default for displaying and entering dates and times."));
	label->setBuddy(mTimeZone);
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Start-of-day time
	itemBox = new KHBox(this);
	itemBox->setMargin(0);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("&Start of day for date-only alarms:"), box);
	mStartOfDay = new TimeEdit(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	label->setBuddy(mStartOfDay);
	static const QString startOfDayText = i18n("The earliest time of day at which a date-only alarm (i.e. "
	                                           "an alarm with \"any time\" specified) will be triggered.");
	box->setWhatsThis(QString("%1\n\n%2").arg(startOfDayText).arg(TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Confirm alarm deletion?
	itemBox = new KHBox(this);   // this is to allow left adjustment
	itemBox->setMargin(0);
	mConfirmAlarmDeletion = new QCheckBox(i18n("Con&firm alarm deletions"), itemBox);
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	mConfirmAlarmDeletion->setWhatsThis(i18n("Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Terminal window to use for command alarms
	group = new QGroupBox(i18n("Terminal for Command Alarms"), this);
	group->setWhatsThis(i18n("Choose which application to use when a command alarm is executed in a terminal window"));
	grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	int row = 0;

	mXtermType = new ButtonGroup(group);
	int index = 0;
	for (mXtermCount = 0;  !xtermCommands[mXtermCount].isNull();  ++mXtermCount)
	{
		QString cmd = xtermCommands[mXtermCount];
		QStringList args = KShell::splitArgs(cmd);
		if (args.isEmpty()  ||  KStandardDirs::findExe(args[0]).isEmpty())
			continue;
		QRadioButton* radio = new QRadioButton(args[0], group);
		radio->setMinimumSize(radio->sizeHint());
		mXtermType->addButton(radio, mXtermCount);
		cmd.replace("%t", KGlobal::mainComponent().aboutData()->programName());
		cmd.replace("%c", "<command>");
		cmd.replace("%w", "<command; sleep>");
		cmd.replace("%C", "[command]");
		cmd.replace("%W", "[command; sleep]");
		radio->setWhatsThis(
		        i18nc("The parameter is a command line, e.g. 'xterm -e'",
		              "Check to execute command alarms in a terminal window by '%1'", cmd));
		grid->addWidget(radio, (row = index/3), index % 3, Qt::AlignLeft);
		++index;
	}

	box = new KHBox(group);
	box->setMargin(0);
	grid->addWidget(box, row + 1, 0, 1, 3, Qt::AlignLeft);
	QRadioButton* radio = new QRadioButton(i18n("Other:"), box);
	radio->setFixedSize(radio->sizeHint());
	connect(radio, SIGNAL(toggled(bool)), SLOT(slotOtherTerminalToggled(bool)));
	mXtermType->addButton(radio, mXtermCount);
	mXtermCommand = new QLineEdit(box);
	box->setWhatsThis(
	      i18n("Enter the full command line needed to execute a command in your chosen terminal window. "
	           "By default the alarm's command string will be appended to what you enter here. "
	           "See the KAlarm Handbook for details of special codes to tailor the command line."));

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void MiscPrefTab::restore()
{
	mAutostartDaemon->setChecked(Daemon::autoStart());
	bool systray = Preferences::mRunInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::mDisableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::quitWarn());
	mAutostartTrayIcon->setChecked(Preferences::mAutostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::confirmAlarmDeletion());
	setTimeZone(Preferences::timeZone());
	mStartOfDay->setValue(Preferences::mStartOfDay);
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
			QStringList args = KShell::splitArgs(cmd);
			cmd = args.isEmpty() ? QString() : args[0];
			if (KStandardDirs::findExe(cmd).isEmpty())
			{
				mXtermCommand->setFocus();
				if (KMessageBox::warningContinueCancel(this, i18n("Command to invoke terminal window not found:\n%1", cmd))
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
	Preferences::mAutostartTrayIcon = mAutostartTrayIcon->isChecked();
#ifdef AUTOSTART_BY_KALARMD
	bool newAutostartDaemon = mAutostartDaemon->isChecked() || Preferences::mAutostartTrayIcon;
#else
	bool newAutostartDaemon = mAutostartDaemon->isChecked();
#endif
	if (newAutostartDaemon != Daemon::autoStart())
		Daemon::enableAutoStart(newAutostartDaemon);
	Preferences::setConfirmAlarmDeletion(mConfirmAlarmDeletion->isChecked());
	const KTimeZone* tz = KSystemTimeZones::zone(mTimeZone->currentText());
	if (tz)
		Preferences::mTimeZone = tz;
	int sod = mStartOfDay->value();
	Preferences::mStartOfDay.setHMS(sod/60, sod%60, 0);
	Preferences::mCmdXTermCommand = (xtermID < mXtermCount) ? xtermCommands[xtermID] : mXtermCommand->text();
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::setDefaults()
{
	mAutostartDaemon->setChecked(true);
	bool systray = Preferences::default_runInSystemTray;
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::default_disableAlarmsIfStopped);
	mQuitWarn->setChecked(Preferences::default_quitWarn);
	mAutostartTrayIcon->setChecked(Preferences::default_autostartTrayIcon);
	mConfirmAlarmDeletion->setChecked(Preferences::default_confirmAlarmDeletion);
	setTimeZone(Preferences::default_timeZone());
	mStartOfDay->setValue(Preferences::default_startOfDay);
	mXtermType->setButton(0);
	mXtermCommand->setEnabled(false);
	slotDisableIfStoppedToggled(true);
}

void MiscPrefTab::setTimeZone(const KTimeZone* tz)
{
	int tzindex = 0;
	if (tz)
	{
		QString zone = tz->name();
		int count = mTimeZone->count();
		while (tzindex < count  &&  mTimeZone->itemText(tzindex) != zone)
			++tzindex;
		if (tzindex >= count)
			tzindex = 0;
	}
	mTimeZone->setCurrentIndex(tzindex);
}

void MiscPrefTab::slotAutostartDaemonClicked()
{
	if (!mAutostartDaemon->isChecked()
	&&  KMessageBox::warningYesNo(this,
		                      i18n("You should not uncheck this option unless you intend to discontinue use of KAlarm"),
		                      QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
		                     ) != KMessageBox::Yes)
		mAutostartDaemon->setChecked(true);
}

void MiscPrefTab::slotRunModeToggled(bool)
{
	bool systray = mRunInSystemTray->isChecked();
	mAutostartTrayIcon->setText(systray ? i18n("Autostart at &login") : i18n("Autostart system tray &icon at login"));
	mAutostartTrayIcon->setWhatsThis((systray ? i18n("Check to run KAlarm whenever you start KDE.")
	                                          : i18n("Check to display the system tray icon whenever you start KDE.")));
	mDisableAlarmsIfStopped->setEnabled(systray);
	slotDisableIfStoppedToggled(true);
}

/******************************************************************************
* If autostart at login is selected, the daemon must be autostarted so that it
* can autostart KAlarm, in which case disable the daemon autostart option.
*/
void MiscPrefTab::slotAutostartToggled(bool)
{
#ifdef AUTOSTART_BY_KALARMD
	mAutostartDaemon->setEnabled(!mAutostartTrayIcon->isChecked());
#endif
}

void MiscPrefTab::slotDisableIfStoppedToggled(bool)
{
	bool enable = mDisableAlarmsIfStopped->isEnabled()  &&  mDisableAlarmsIfStopped->isChecked();
	mQuitWarn->setEnabled(enable);
}

void MiscPrefTab::slotOtherTerminalToggled(bool on)
{
	mXtermCommand->setEnabled(on);
}


/*=============================================================================
= Class StorePrefTab
=============================================================================*/

StorePrefTab::StorePrefTab()
	: PrefsTabBase(),
	  mCheckKeepChanges(false)
{
	// Which resource to save to
	QGroupBox* group = new QGroupBox(i18n("New Alarms && Templates"), this);
	QButtonGroup* bgroup = new QButtonGroup(group);
	QBoxLayout* layout = new QVBoxLayout(group);
	layout->setMargin(KDialog::marginHint());
	layout->setSpacing(KDialog::spacingHint());

	mDefaultResource = new QRadioButton(i18n("Store in default &resource"), group);
	bgroup->addButton(mDefaultResource);
	mDefaultResource->setFixedSize(mDefaultResource->sizeHint());
	mDefaultResource->setWhatsThis(i18n("Add all new alarms and alarm templates to the default resources, without prompting"));
	layout->addWidget(mDefaultResource, 0, Qt::AlignLeft);
	mAskResource = new QRadioButton(i18n("Prompt for &which resource to store in"), group);
	bgroup->addButton(mAskResource);
	mAskResource->setFixedSize(mAskResource->sizeHint());
	mAskResource->setWhatsThis(
	      i18n("When saving a new alarm or alarm template, prompt for which resource to store it in, if there is more than one active resource.\n\n"
	           "Note that archived alarms are always stored in the default archived alarm resource."));
	layout->addWidget(mAskResource, 0, Qt::AlignLeft);

	// Archived alarms
	group = new QGroupBox(i18n("Archived Alarms"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	mKeepArchived = new QCheckBox(i18n("Keep alarms after e&xpiry"), group);
	mKeepArchived->setFixedSize(mKeepArchived->sizeHint());
	connect(mKeepArchived, SIGNAL(toggled(bool)), SLOT(slotArchivedToggled(bool)));
	mKeepArchived->setWhatsThis(i18n("Check to archive alarms after expiry or deletion (except deleted alarms which were never triggered)."));
	grid->addWidget(mKeepArchived, 0, 0, 1, 2, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mPurgeArchived = new QCheckBox(i18n("Discard archi&ved alarms after:"), box);
	mPurgeArchived->setMinimumSize(mPurgeArchived->sizeHint());
	connect(mPurgeArchived, SIGNAL(toggled(bool)), SLOT(slotArchivedToggled(bool)));
	mPurgeAfter = new SpinBox(box);
	mPurgeAfter->setMinimum(1);
	mPurgeAfter->setSingleShiftStep(10);
	mPurgeAfter->setMinimumSize(mPurgeAfter->sizeHint());
	mPurgeAfterLabel = new QLabel(i18n("da&ys"), box);
	mPurgeAfterLabel->setMinimumSize(mPurgeAfterLabel->sizeHint());
	mPurgeAfterLabel->setBuddy(mPurgeAfter);
	box->setWhatsThis(i18n("Uncheck to store archived alarms indefinitely. Check to enter how long archived alarms should be stored."));
	grid->addWidget(box, 1, 1, Qt::AlignLeft);

	mClearArchived = new QPushButton(i18n("Clear Archived Alar&ms"), group);
	mClearArchived->setFixedSize(mClearArchived->sizeHint());
	connect(mClearArchived, SIGNAL(clicked()), SLOT(slotClearArchived()));
	mClearArchived->setWhatsThis((AlarmResources::instance()->activeCount(AlarmResource::ARCHIVED, false) <= 1)
	        ? i18n("Delete all existing archived alarms.")
	        : i18n("Delete all existing archived alarms (from the default archived alarm resource only)."));
	grid->addWidget(mClearArchived, 2, 1, Qt::AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void StorePrefTab::restore()
{
	mCheckKeepChanges = false;
	mAskResource->setChecked(Preferences::mAskResource);
	mDefaultResource->setChecked(!Preferences::mAskResource);
	setArchivedControls(Preferences::mArchivedKeepDays);
	mOldKeepArchived = mKeepArchived->isChecked();
	mCheckKeepChanges = true;
}

void StorePrefTab::apply(bool syncToDisc)
{
	Preferences::mAskResource = mAskResource->isChecked();
	Preferences::mArchivedKeepDays = !mKeepArchived->isChecked() ? 0 : mPurgeArchived->isChecked() ? mPurgeAfter->value() : -1;
	PrefsTabBase::apply(syncToDisc);
}

void StorePrefTab::setDefaults()
{
	mAskResource->setChecked(Preferences::default_askResource);
	setArchivedControls(Preferences::default_archivedKeepDays);
}

void StorePrefTab::setArchivedControls(int purgeDays)
{
	mKeepArchived->setChecked(purgeDays);
	mPurgeArchived->setChecked(purgeDays > 0);
	mPurgeAfter->setValue(purgeDays > 0 ? purgeDays : 0);
	slotArchivedToggled(true);
}

void StorePrefTab::slotArchivedToggled(bool)
{
	bool keep = mKeepArchived->isChecked();
	if (keep  &&  !mOldKeepArchived  &&  mCheckKeepChanges
	&&  !AlarmResources::instance()->getStandardResource(AlarmResource::ARCHIVED))
	{
		KMessageBox::sorry(this,
		     i18n("A default resource is required in order to archive alarms, but none is currently enabled.\n\n"
		          "If you wish to keep expired alarms, please first use the resources view to select a default "
		          "archived alarms resource."));
		mKeepArchived->setChecked(false);
		return;
	}
	mOldKeepArchived = keep;
	mPurgeArchived->setEnabled(keep);
	mPurgeAfter->setEnabled(keep && mPurgeArchived->isChecked());
	mPurgeAfterLabel->setEnabled(keep);
	mClearArchived->setEnabled(keep);
}

void StorePrefTab::slotClearArchived()
{
	if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to delete all archived alarms?"))
			!= KMessageBox::Continue)
		return;
	AlarmCalendar::resources()->purgeAll();
}


/*=============================================================================
= Class EmailPrefTab
=============================================================================*/

EmailPrefTab::EmailPrefTab()
	: PrefsTabBase(),
	  mAddressChanged(false),
	  mBccAddressChanged(false)
{
	KHBox* box = new KHBox(this);
	box->setMargin(0);
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
	           "KMail: The email is sent automatically via KMail. KMail is started first if necessary.\n"
	           "Sendmail: The email is sent automatically. This option will only work if "
	           "your system is configured to use 'sendmail' or a sendmail compatible mail transport agent."));

	box = new KHBox(this);   // this is to allow left adjustment
	box->setMargin(0);
	mEmailCopyToKMail = new QCheckBox(i18n("Co&py sent emails into KMail's %1 folder", KAMail::i18n_sent_mail()), box);
	mEmailCopyToKMail->setFixedSize(mEmailCopyToKMail->sizeHint());
	mEmailCopyToKMail->setWhatsThis(i18n("After sending an email, store a copy in KMail's %1 folder", KAMail::i18n_sent_mail()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Your Email Address group box
	QGroupBox* group = new QGroupBox(i18n("Your Email Address"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);

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
	grid->addWidget(mFromCCentreButton, 2, 1, 1, 2, Qt::AlignLeft);

	// 'From' email address to be picked from KMail's identities when the email alarm is configured
	mFromKMailButton = new RadioButton(i18n("Use KMail &identities"), group);
	mFromKMailButton->setFixedSize(mFromKMailButton->sizeHint());
	mFromAddressGroup->addButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
	mFromKMailButton->setWhatsThis(
	      i18n("Check to use KMail's email identities to identify you as the sender when sending email alarms. "
	           "For existing email alarms, KMail's default identity will be used. "
	           "For new email alarms, you will be able to pick which of KMail's identities to use."));
	grid->addWidget(mFromKMailButton, 3, 1, 1, 2, Qt::AlignLeft);

	// 'Bcc' email address controls ...
	grid->setRowMinimumHeight(4, KDialog::spacingHint());
	label = new Label(i18nc("'Bcc' email address", "&Bcc:"), group);
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
	grid->addWidget(mBccCCentreButton, 6, 1, 1, 2, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	box = new KHBox(this);   // this is to allow left adjustment
	box->setMargin(0);
	mEmailQueuedNotify = new QCheckBox(i18n("&Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setFixedSize(mEmailQueuedNotify->sizeHint());
	mEmailQueuedNotify->setWhatsThis(
	      i18n("Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
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
	Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(mFromAddressGroup->selectedId()), mEmailAddress->text().trimmed());
	Preferences::setEmailBccAddress((mBccAddressGroup->checkedButton() == mBccCCentreButton), mEmailBccAddress->text().trimmed());
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
	mEmailAddress->setText(from == Preferences::MAIL_FROM_ADDR ? address.trimmed() : QString());
}

void EmailPrefTab::setEmailBccAddress(bool useControlCentre, const QString& address)
{
	mBccAddressGroup->setButton(useControlCentre ? Preferences::MAIL_FROM_CONTROL_CENTRE : Preferences::MAIL_FROM_ADDR);
	mEmailBccAddress->setText(useControlCentre ? QString() : address.trimmed());
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
	return QString();
}

QString EmailPrefTab::validateAddr(ButtonGroup* group, QLineEdit* addr, const QString& msg)
{
	QString errmsg = i18n("%1\nAre you sure you want to save your changes?", msg);
	switch (group->selectedId())
	{
		case Preferences::MAIL_FROM_CONTROL_CENTRE:
			if (!KAMail::controlCentreAddress().isEmpty())
				return QString();
			errmsg = i18n("No email address is currently set in the KDE Control Center. %1", errmsg);
			break;
		case Preferences::MAIL_FROM_KMAIL:
			if (KAMail::identitiesExist())
				return QString();
			errmsg = i18n("No KMail identities currently exist. %1", errmsg);
			break;
		case Preferences::MAIL_FROM_ADDR:
			if (!addr->text().trimmed().isEmpty())
				return QString();
			break;
	}
	return errmsg;
}


/*=============================================================================
= Class FontColourPrefTab
=============================================================================*/

FontColourPrefTab::FontColourPrefTab()
	: PrefsTabBase()
{
	mFontChooser = new FontColourChooser(this, false, QStringList(), i18n("Message Font && Color"), true, false);

	KHBox* layoutBox = new KHBox(this);
	layoutBox->setMargin(0);
	KHBox* box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setMargin(0);
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

	layoutBox = new KHBox(this);
	layoutBox->setMargin(0);
	box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label2 = new QLabel(i18n("Archi&ved alarm color:"), box);
//	label2->setMinimumSize(label2->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mArchivedColour = new KColorCombo(box);
	mArchivedColour->setMinimumSize(mArchivedColour->sizeHint());
	label2->setBuddy(mArchivedColour);
	box->setWhatsThis(i18n("Choose the text color in the alarm list for archived alarms."));
	layoutBox->setStretchFactor(new QWidget(layoutBox), 1);    // left adjust the controls
	layoutBox->setFixedHeight(layoutBox->sizeHint().height());

	// Line up the two sets of colour controls
	QSize size = label1->sizeHint();
	QSize size2 = label2->sizeHint();
	if (size2.width() > size.width())
		size.setWidth(size2.width());
	label1->setFixedSize(size);
	label2->setFixedSize(size);

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void FontColourPrefTab::restore()
{
	mFontChooser->setBgColour(Preferences::mDefaultBgColour);
	mFontChooser->setColours(Preferences::mMessageColours);
	mFontChooser->setFont(Preferences::mMessageFont);
	mDisabledColour->setColor(Preferences::mDisabledColour);
	mArchivedColour->setColor(Preferences::mArchivedColour);
}

void FontColourPrefTab::apply(bool syncToDisc)
{
	Preferences::mDefaultBgColour = mFontChooser->bgColour();
	Preferences::mMessageColours  = mFontChooser->colours();
	Preferences::mMessageFont     = mFontChooser->font();
	Preferences::mDisabledColour  = mDisabledColour->color();
	Preferences::mArchivedColour  = mArchivedColour->color();
	PrefsTabBase::apply(syncToDisc);
}

void FontColourPrefTab::setDefaults()
{
	mFontChooser->setBgColour(Preferences::default_defaultBgColour);
	mFontChooser->setColours(Preferences::default_messageColours);
	mFontChooser->setFont(Preferences::default_messageFont());
	mDisabledColour->setColor(Preferences::default_disabledColour);
	mArchivedColour->setColor(Preferences::default_archivedColour);
}


/*=============================================================================
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab()
	: PrefsTabBase()
{
	KLocalizedString defsetting   = ki18n("The default setting for \"%1\" in the alarm edit dialog.");
	KLocalizedString soundSetting = ki18n("Check to select %1 as the default setting for \"%2\" in the alarm edit dialog.");

	// DISPLAY ALARMS
	QGroupBox* group = new QGroupBox(i18n("Display Alarms"), this);
	QVBoxLayout* vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	mConfirmAck = new QCheckBox(EditAlarmDlg::i18n_k_ConfirmAck(), group);
	mConfirmAck->setMinimumSize(mConfirmAck->sizeHint());
	mConfirmAck->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_ConfirmAck()).toString());
	vlayout->addWidget(mConfirmAck, 0, Qt::AlignLeft);

	mAutoClose = new QCheckBox(LateCancelSelector::i18n_i_AutoCloseWinLC(), group);
	mAutoClose->setMinimumSize(mAutoClose->sizeHint());
	mAutoClose->setWhatsThis(defsetting.subs(LateCancelSelector::i18n_AutoCloseWin()).toString());
	vlayout->addWidget(mAutoClose, 0, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	QLabel* label = new QLabel(i18n("Reminder &units:"), box);
	label->setFixedSize(label->sizeHint());
	mReminderUnits = new QComboBox(box);
	mReminderUnits->addItem(TimePeriod::i18n_Hours_Mins(), TimePeriod::HOURS_MINUTES);
	mReminderUnits->addItem(TimePeriod::i18n_Days(), TimePeriod::DAYS);
	mReminderUnits->addItem(TimePeriod::i18n_Weeks(), TimePeriod::WEEKS);
	mReminderUnits->setFixedSize(mReminderUnits->sizeHint());
	label->setBuddy(mReminderUnits);
	box->setWhatsThis(i18n("The default units for the reminder in the alarm edit dialog."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	mSpecialActionsButton = new SpecialActionsButton(EditAlarmDlg::i18n_SpecialActions(), box);
	mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());

	// SOUND
	QGroupBox* bbox = new QGroupBox(SoundPicker::i18n_Sound(), this);
	vlayout = new QVBoxLayout(bbox);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	vlayout->addLayout(hlayout);
	mSound = new QComboBox(bbox);
	mSound->addItem(SoundPicker::i18n_None());         // index 0
	mSound->addItem(SoundPicker::i18n_Beep());         // index 1
	mSound->addItem(SoundPicker::i18n_File());         // index 2
	if (theApp()->speechEnabled())
		mSound->addItem(SoundPicker::i18n_Speak());  // index 3
	mSound->setMinimumSize(mSound->sizeHint());
	mSound->setWhatsThis(defsetting.subs(SoundPicker::i18n_Sound()).toString());
	hlayout->addWidget(mSound);
	hlayout->addStretch();

	mSoundRepeat = new QCheckBox(i18n("Repea&t sound file"), bbox);
	mSoundRepeat->setMinimumSize(mSoundRepeat->sizeHint());
	mSoundRepeat->setWhatsThis(i18nc("sound file \"Repeat\" checkbox", "The default setting for sound file \"%1\" in the alarm edit dialog.", SoundDlg::i18n_Repeat()));
	hlayout->addWidget(mSoundRepeat);

	box = new KHBox(bbox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mSoundFileLabel = new QLabel(i18n("Sound &file:"), box);
	mSoundFileLabel->setFixedSize(mSoundFileLabel->sizeHint());
	mSoundFile = new QLineEdit(box);
	mSoundFileLabel->setBuddy(mSoundFile);
	mSoundFileBrowse = new QPushButton(box);
	mSoundFileBrowse->setIcon(KIcon( SmallIcon("fileopen") ) );
	mSoundFileBrowse->setFixedSize(mSoundFileBrowse->sizeHint());
	connect(mSoundFileBrowse, SIGNAL(clicked()), SLOT(slotBrowseSoundFile()));
	mSoundFileBrowse->setToolTip(i18n("Choose a sound file"));
	box->setWhatsThis(i18n("Enter the default sound file to use in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box);
	bbox->setFixedHeight(bbox->sizeHint().height());

	// COMMAND ALARMS
	group = new QGroupBox(i18n("Command Alarms"), this);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	vlayout->addLayout(hlayout);

	mCmdScript = new QCheckBox(EditAlarmDlg::i18n_p_EnterScript(), group);
	mCmdScript->setMinimumSize(mCmdScript->sizeHint());
	mCmdScript->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_EnterScript()).toString());
	hlayout->addWidget(mCmdScript);
	hlayout->addStretch();

	mCmdXterm = new QCheckBox(EditAlarmDlg::i18n_w_ExecInTermWindow(), group);
	mCmdXterm->setMinimumSize(mCmdXterm->sizeHint());
	mCmdXterm->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_ExecInTermWindow()).toString());
	hlayout->addWidget(mCmdXterm);

	// EMAIL ALARMS
	group = new QGroupBox(i18n("Email Alarms"), this);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	// BCC email to sender
	mEmailBcc = new QCheckBox(EditAlarmDlg::i18n_e_CopyEmailToSelf(), group);
	mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_CopyEmailToSelf()).toString());
	vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

	// MISCELLANEOUS
	// Show in KOrganizer
	mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_g_ShowInKOrganizer(), this);
	mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
	mCopyToKOrganizer->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_ShowInKOrganizer()).toString());

	// Late cancellation
	box = new KHBox(this);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mLateCancel = new QCheckBox(LateCancelSelector::i18n_n_CancelIfLate(), box);
	mLateCancel->setMinimumSize(mLateCancel->sizeHint());
	mLateCancel->setWhatsThis(defsetting.subs(LateCancelSelector::i18n_CancelIfLate()).toString());
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	// Recurrence
	KHBox* itemBox = new KHBox(box);   // this is to control the QWhatsThis text display area
	itemBox->setMargin(0);
	itemBox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("&Recurrence:"), itemBox);
	label->setFixedSize(label->sizeHint());
	mRecurPeriod = new QComboBox(itemBox);
	mRecurPeriod->addItem(RecurrenceEdit::i18n_NoRecur());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_AtLogin());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_HourlyMinutely());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_Daily());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_Weekly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_Monthly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_Yearly());
	mRecurPeriod->setFixedSize(mRecurPeriod->sizeHint());
	label->setBuddy(mRecurPeriod);
	itemBox->setWhatsThis(i18n("The default setting for the recurrence rule in the alarm edit dialog."));
	box->setFixedHeight(itemBox->sizeHint().height());

	// How to handle February 29th in yearly recurrences
	KVBox* vbox = new KVBox(this);   // this is to control the QWhatsThis text display area
	vbox->setMargin(0);
	vbox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("In non-leap years, repeat yearly February 29th alarms on:"), vbox);
	label->setAlignment(Qt::AlignLeft);
	label->setWordWrap(true);
	itemBox = new KHBox(vbox);
	itemBox->setMargin(0);
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

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void EditPrefTab::restore()
{
	mAutoClose->setChecked(Preferences::mDefaultAutoClose);
	mConfirmAck->setChecked(Preferences::mDefaultConfirmAck);
	mReminderUnits->setCurrentIndex(Preferences::mDefaultReminderUnits);
	mSpecialActionsButton->setActions(Preferences::mDefaultPreAction, Preferences::mDefaultPostAction);
	mSound->setCurrentIndex(Preferences::mDefaultSoundType);
	mSoundFile->setText(Preferences::mDefaultSoundFile);
	mSoundRepeat->setChecked(Preferences::mDefaultSoundRepeat);
	mCmdScript->setChecked(Preferences::mDefaultCmdScript);
	mCmdXterm->setChecked(Preferences::mDefaultCmdLogType == Preferences::EXEC_IN_TERMINAL);
	mEmailBcc->setChecked(Preferences::mDefaultEmailBcc);
	mCopyToKOrganizer->setChecked(Preferences::mDefaultCopyToKOrganizer);
	mLateCancel->setChecked(Preferences::mDefaultLateCancel);
	mRecurPeriod->setCurrentIndex(recurIndex(Preferences::mDefaultRecurPeriod));
	mFeb29->setButton(Preferences::mDefaultFeb29Type);
}

void EditPrefTab::apply(bool syncToDisc)
{
	Preferences::mDefaultAutoClose        = mAutoClose->isChecked();
	Preferences::mDefaultConfirmAck       = mConfirmAck->isChecked();
	Preferences::mDefaultReminderUnits    = static_cast<TimePeriod::Units>(mReminderUnits->currentIndex());
	Preferences::mDefaultPreAction        = mSpecialActionsButton->preAction();
	Preferences::mDefaultPostAction       = mSpecialActionsButton->postAction();
	switch (mSound->currentIndex())
	{
		case 3:  Preferences::mDefaultSoundType = SoundPicker::SPEAK;      break;
		case 2:  Preferences::mDefaultSoundType = SoundPicker::PLAY_FILE;  break;
		case 1:  Preferences::mDefaultSoundType = SoundPicker::BEEP;       break;
		case 0:
		default: Preferences::mDefaultSoundType = SoundPicker::NONE;       break;
	}
	Preferences::mDefaultSoundFile        = mSoundFile->text();
	Preferences::mDefaultSoundRepeat      = mSoundRepeat->isChecked();
	Preferences::mDefaultCmdScript        = mCmdScript->isChecked();
	Preferences::mDefaultCmdLogFile       = (mCmdXterm->isChecked() ? Preferences::EXEC_IN_TERMINAL : Preferences::DISCARD_OUTPUT);
	Preferences::mDefaultEmailBcc         = mEmailBcc->isChecked();
	Preferences::mDefaultCopyToKOrganizer = mCopyToKOrganizer->isChecked();
	Preferences::mDefaultLateCancel       = mLateCancel->isChecked() ? 1 : 0;
	switch (mRecurPeriod->currentIndex())
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
	mReminderUnits->setCurrentIndex(Preferences::default_defaultReminderUnits);
	mSpecialActionsButton->setActions(Preferences::default_defaultPreAction, Preferences::default_defaultPostAction);
	mSound->setCurrentIndex(soundIndex(Preferences::default_defaultSoundType));
	mSoundFile->setText(Preferences::default_defaultSoundFile);
	mSoundRepeat->setChecked(Preferences::default_defaultSoundRepeat);
	mCmdScript->setChecked(Preferences::default_defaultCmdScript);
	mCmdXterm->setChecked(Preferences::default_defaultCmdLogType == Preferences::EXEC_IN_TERMINAL);
	mEmailBcc->setChecked(Preferences::default_defaultEmailBcc);
	mCopyToKOrganizer->setChecked(Preferences::default_defaultCopyToKOrganizer);
	mLateCancel->setChecked(Preferences::default_defaultLateCancel);
	mRecurPeriod->setCurrentIndex(recurIndex(Preferences::default_defaultRecurPeriod));
	mFeb29->setButton(Preferences::default_defaultFeb29Type);
}

void EditPrefTab::slotBrowseSoundFile()
{
	QString defaultDir;
	QString url = SoundPicker::browseFile(defaultDir, mSoundFile->text());
	if (!url.isEmpty())
		mSoundFile->setText(url);
}

int EditPrefTab::soundIndex(SoundPicker::Type type)
{
	switch (type)
	{
		case SoundPicker::SPEAK:      return 3;
		case SoundPicker::PLAY_FILE:  return 2;
		case SoundPicker::BEEP:       return 1;
		case SoundPicker::NONE:
		default:                      return 0;
	}
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

QString EditPrefTab::validate()
{
	if (mSound->currentIndex() == SoundPicker::PLAY_FILE  &&  mSoundFile->text().isEmpty())
	{
		mSoundFile->setFocus();
		return i18n("You must enter a sound file when %1 is selected as the default sound type", SoundPicker::i18n_File());;
	}
	return QString();
}


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab()
	: PrefsTabBase()
{
	mShowResources = new QCheckBox(i18n("Show &resources"), this);
	mShowResources->setMinimumSize(mShowResources->sizeHint());
	mShowResources->setWhatsThis(
	      i18n("Specify whether to show the list of alarm resources beside the alarm list"));

	QGroupBox* group = new QGroupBox(i18n("Alarm List"), this);
	QVBoxLayout* vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	mListShowTime = new QCheckBox(MainWindow::i18n_t_ShowAlarmTime(), group);
	mListShowTime->setMinimumSize(mListShowTime->sizeHint());
	connect(mListShowTime, SIGNAL(toggled(bool)), SLOT(slotListTimeToggled(bool)));
	mListShowTime->setWhatsThis(i18n("Specify whether to show in the alarm list, the time at which each alarm is due"));
	vlayout->addWidget(mListShowTime, 0, Qt::AlignLeft);

	mListShowTimeTo = new QCheckBox(MainWindow::i18n_n_ShowTimeToAlarm(), group);
	mListShowTimeTo->setMinimumSize(mListShowTimeTo->sizeHint());
	connect(mListShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotListTimeToToggled(bool)));
	mListShowTimeTo->setWhatsThis(i18n("Specify whether to show in the alarm list, how long until each alarm is due"));
	vlayout->addWidget(mListShowTimeTo, 0, Qt::AlignLeft);

	mShowArchivedAlarms = new QCheckBox(MainWindow::i18n_e_ShowArchivedAlarms(), group);
	mShowArchivedAlarms->setMinimumSize(mShowArchivedAlarms->sizeHint());
	mShowArchivedAlarms->setWhatsThis(i18n("Specify whether to show archived alarms in the alarm list"));
	vlayout->addWidget(mShowArchivedAlarms, 0, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	group = new QGroupBox(i18n("System Tray Tooltip"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(2, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	grid->setColumnMinimumWidth(1, indentWidth());

	mTooltipShowAlarms = new QCheckBox(i18n("Show next &24 hours' alarms"), group);
	mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
	connect(mTooltipShowAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipAlarmsToggled(bool)));
	mTooltipShowAlarms->setWhatsThis(i18n("Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours"));
	grid->addWidget(mTooltipShowAlarms, 0, 0, 1, 3, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mTooltipMaxAlarms = new QCheckBox(i18n("Ma&ximum number of alarms to show:"), box);
	mTooltipMaxAlarms->setMinimumSize(mTooltipMaxAlarms->sizeHint());
	connect(mTooltipMaxAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipMaxToggled(bool)));
	mTooltipMaxAlarmCount = new SpinBox(1, 99, box);
	mTooltipMaxAlarmCount->setSingleShiftStep(5);
	mTooltipMaxAlarmCount->setMinimumSize(mTooltipMaxAlarmCount->sizeHint());
	box->setWhatsThis(
	      i18n("Uncheck to display all of the next 24 hours' alarms in the system tray tooltip. "
	           "Check to enter an upper limit on the number to be displayed."));
	grid->addWidget(box, 1, 1, 1, 2, Qt::AlignLeft);

	mTooltipShowTime = new QCheckBox(MainWindow::i18n_m_ShowAlarmTime(), group);
	mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
	connect(mTooltipShowTime, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToggled(bool)));
	mTooltipShowTime->setWhatsThis(i18n("Specify whether to show in the system tray tooltip, the time at which each alarm is due"));
	grid->addWidget(mTooltipShowTime, 2, 1, 1, 2, Qt::AlignLeft);

	mTooltipShowTimeTo = new QCheckBox(MainWindow::i18n_l_ShowTimeToAlarm(), group);
	mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
	connect(mTooltipShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToToggled(bool)));
	mTooltipShowTimeTo->setWhatsThis(i18n("Specify whether to show in the system tray tooltip, how long until each alarm is due"));
	grid->addWidget(mTooltipShowTimeTo, 3, 1, 1, 2, Qt::AlignLeft);

	box = new KHBox(group);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mTooltipTimeToPrefixLabel = new QLabel(i18n("&Prefix:"), box);
	mTooltipTimeToPrefixLabel->setFixedSize(mTooltipTimeToPrefixLabel->sizeHint());
	mTooltipTimeToPrefix = new QLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	box->setWhatsThis(i18n("Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip"));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 4, 2, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	mModalMessages = new QCheckBox(i18n("Message &windows have a title bar and take keyboard focus"), this);
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	mModalMessages->setWhatsThis(
	      i18n("Specify the characteristics of alarm message windows:\n"
	           "- If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.\n"
	           "- If unchecked, the window does not interfere with your typing when "
	           "it is displayed, but it has no title bar and cannot be moved or resized."));

	KHBox* itemBox = new KHBox(this);   // this is to control the QWhatsThis text display area
	itemBox->setMargin(0);
	box = new KHBox(itemBox);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("System tray icon &update interval:"), box);
	mDaemonTrayCheckInterval = new SpinBox(1, 9999, box);
	mDaemonTrayCheckInterval->setSingleShiftStep(10);
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18n("seconds"), box);
	box->setWhatsThis(i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void ViewPrefTab::restore()
{
	mShowResources->setChecked(Preferences::mShowResources);
	setList(Preferences::mShowAlarmTime,
	        Preferences::mShowTimeToAlarm);
	setTooltip(Preferences::mTooltipAlarmCount,
	           Preferences::mShowTooltipAlarmTime,
	           Preferences::mShowTooltipTimeToAlarm,
	           Preferences::mTooltipTimeToPrefix);
	mModalMessages->setChecked(Preferences::mModalMessages);
	mShowArchivedAlarms->setChecked(Preferences::mShowArchivedAlarms);
	mDaemonTrayCheckInterval->setValue(Preferences::mDaemonTrayCheckInterval);
}

void ViewPrefTab::apply(bool syncToDisc)
{
	Preferences::mShowResources           = mShowResources->isChecked();
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
	Preferences::mShowArchivedAlarms      = mShowArchivedAlarms->isChecked();
	Preferences::mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	PrefsTabBase::apply(syncToDisc);
}

void ViewPrefTab::setDefaults()
{
	mShowResources->setChecked(Preferences::default_showResources);
	setList(Preferences::default_showAlarmTime,
	        Preferences::default_showTimeToAlarm);
	setTooltip(Preferences::default_tooltipAlarmCount,
	           Preferences::default_showTooltipAlarmTime,
	           Preferences::default_showTooltipTimeToAlarm,
	           Preferences::default_tooltipTimeToPrefix);
	mModalMessages->setChecked(Preferences::default_modalMessages);
	mShowArchivedAlarms->setChecked(Preferences::default_showArchivedAlarms);
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
