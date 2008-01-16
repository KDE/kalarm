/*
 *  prefdlg.cpp  -  program preferences dialog
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

#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <kvbox.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kshell.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kcombobox.h>
#include <kcolorcombo.h>
#include <KStandardGuiItem>
#include <ksystemtimezone.h>
#include <kicon.h>
#ifdef Q_WS_X11
#include <kwindowinfo.h>
#include <kwindowsystem.h>
#endif
#include <kdebug.h>

#include <kalarmd/kalarmd.h>
#include <ktoolinvocation.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "daemon.h"
#include "editdlg.h"
#include "editdlgtypes.h"
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
#include "timezonecombo.h"
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

KAlarmPrefDlg* KAlarmPrefDlg::mInstance = 0;

void KAlarmPrefDlg::display()
{
	if (!mInstance)
	{
		mInstance = new KAlarmPrefDlg;
		mInstance->show();
	}
	else
	{
#ifdef Q_WS_X11
		KWindowInfo info = KWindowSystem::windowInfo(mInstance->winId(), NET::WMGeometry | NET::WMDesktop);
		KWindowSystem::setCurrentDesktop(info.desktop());
#endif
		mInstance->showNormal();   // un-minimize it if necessary
		mInstance->raise();
		mInstance->activateWindow();
	}
}

KAlarmPrefDlg::KAlarmPrefDlg()
	: KPageDialog()
{
	setAttribute(Qt::WA_DeleteOnClose);
	setObjectName("PrefDlg");    // used by LikeBack
	setCaption(i18nc("@title:window", "Preferences"));
	setButtons(Help | Default | Ok | Apply | Cancel);
	setDefaultButton(Ok);
	setFaceType(List);
	showButtonSeparator(true);
	//setIconListAllVisible(true);

	mMiscPage = new MiscPrefTab;
	mMiscPageItem = new KPageWidgetItem(mMiscPage, i18nc("@title:tab General preferences", "General"));
	mMiscPageItem->setHeader(i18nc("@title General preferences", "General"));
	mMiscPageItem->setIcon(KIcon(DesktopIcon("preferences-other")));
	addPage(mMiscPageItem);

	mTimePage = new TimePrefTab;
	mTimePageItem = new KPageWidgetItem(mTimePage, i18nc("@title:tab", "Time & Date"));
	mTimePageItem->setHeader(i18nc("@title", "Time and Date"));
	mTimePageItem->setIcon(KIcon(DesktopIcon("preferences-system-time")));
	addPage(mTimePageItem);

	mStorePage = new StorePrefTab;
	mStorePageItem = new KPageWidgetItem(mStorePage, i18nc("@title:tab", "Storage"));
	mStorePageItem->setHeader(i18nc("@title", "Alarm Storage"));
	mStorePageItem->setIcon(KIcon(DesktopIcon("system-file-manager")));
	addPage(mStorePageItem);

	mEmailPage = new EmailPrefTab;
	mEmailPageItem = new KPageWidgetItem(mEmailPage, i18nc("@title:tab Email preferences", "Email"));
	mEmailPageItem->setHeader(i18nc("@title", "Email Alarm Settings"));
	mEmailPageItem->setIcon(KIcon(DesktopIcon("internet-mail")));
	addPage(mEmailPageItem);

	mViewPage = new ViewPrefTab;
	mViewPageItem = new KPageWidgetItem(mViewPage, i18nc("@title:tab", "View"));
	mViewPageItem->setHeader(i18nc("@title", "View Settings"));
	mViewPageItem->setIcon(KIcon(DesktopIcon("preferences-desktop-theme")));
	addPage(mViewPageItem);

	mFontColourPage = new FontColourPrefTab;
	mFontColourPageItem = new KPageWidgetItem(mFontColourPage, i18nc("@title:tab", "Font & Color"));
	mFontColourPageItem->setHeader(i18nc("@title", "Default Font and Color"));
	mFontColourPageItem->setIcon(KIcon(DesktopIcon("preferences-desktop-color")));
	addPage(mFontColourPageItem);

	mEditPage = new EditPrefTab;
	mEditPageItem = new KPageWidgetItem(mEditPage, i18nc("@title:tab", "Edit"));
	mEditPageItem->setHeader(i18nc("@title", "Default Alarm Edit Settings"));
	mEditPageItem->setIcon(KIcon(DesktopIcon("document-properties")));
	addPage(mEditPageItem);

	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));
	connect(this, SIGNAL(cancelClicked()), SLOT(slotCancel()));
	connect(this, SIGNAL(applyClicked()), SLOT(slotApply()));
	connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
	connect(this, SIGNAL(helpClicked()), SLOT(slotHelp()));
	restore(false);
	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
	mInstance = 0;
}

void KAlarmPrefDlg::slotHelp()
{
	KToolInvocation::invokeHelp("preferences");
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
	kDebug(5950);
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
	mTimePage->apply(false);
	mMiscPage->apply(false);
	Preferences::self()->writeConfig();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
	kDebug(5950);
	mValid = true;
	slotApply();
	if (mValid)
		KDialog::accept();
}

// Discard the current preferences and close the dialog
void KAlarmPrefDlg::slotCancel()
{
	kDebug(5950);
	restore(false);
	KDialog::reject();
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::restore(bool defaults)
{
	kDebug(5950) << (defaults ? "defaults" : "");
	if (defaults)
		Preferences::self()->useDefaults(true);
	mFontColourPage->restore(defaults);
	mEmailPage->restore(defaults);
	mViewPage->restore(defaults);
	mEditPage->restore(defaults);
	mStorePage->restore(defaults);
	mTimePage->restore(defaults);
	mMiscPage->restore(defaults);
	if (defaults)
		Preferences::self()->useDefaults(false);
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
	if (syncToDisc)
		Preferences::self()->writeConfig();
}



/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab()
	: PrefsTabBase()
{
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Run Mode"), this);
	QButtonGroup* buttonGroup = new QButtonGroup(group);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(2, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	grid->setColumnMinimumWidth(1, indentWidth());

	// Run-on-demand radio button
	mRunOnDemand = new QRadioButton(i18nc("@option:radio", "Run only on demand"), group);
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunOnDemand->setWhatsThis(
	      i18nc("@info:whatsthis", "<para>Check to run <application>KAlarm</application> only when required.</para>"
	           "<para><note label='Notes'></note></para><para>"
	           "<list><item>Alarms are displayed even when <application>KAlarm</application> is not running, since alarm monitoring is done by the alarm daemon.</item>"
	           "<item>With this option selected, the system tray icon can be displayed or hidden independently of <application>KAlarm</application>.</item></list></para>"));
	buttonGroup->addButton(mRunOnDemand);
	grid->addWidget(mRunOnDemand, 1, 0, 1, 3, Qt::AlignLeft);

	// Run-in-system-tray radio button
	mRunInSystemTray = new QRadioButton(i18nc("@option:radio", "Run continuously in system tray"), group);
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), SLOT(slotRunModeToggled(bool)));
	mRunInSystemTray->setWhatsThis(
	      i18nc("@info:whatsthis", "<para>Check to run <application>KAlarm</application> continuously in the KDE system tray.</para>"
	           "<para><note label='Notes'></note></para><para>"
	           "<list><item>With this option selected, closing the system tray icon will quit <application>KAlarm</application>.</item>"
	           "<item>You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon."
	           " Running in the system tray simply provides easy access and a status indication.</item></list></para>"));
	buttonGroup->addButton(mRunInSystemTray);
	grid->addWidget(mRunInSystemTray, 2, 0, 1, 3, Qt::AlignLeft);

	mDisableAlarmsIfStopped = new QCheckBox(i18nc("@option:check", "Disable alarms while not running"), group);
	mDisableAlarmsIfStopped->setFixedSize(mDisableAlarmsIfStopped->sizeHint());
	connect(mDisableAlarmsIfStopped, SIGNAL(toggled(bool)), SLOT(slotDisableIfStoppedToggled(bool)));
	mDisableAlarmsIfStopped->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to disable alarms whenever <application>KAlarm</application> is not running. Alarms will only appear while the system tray icon is visible."));
	grid->addWidget(mDisableAlarmsIfStopped, 3, 1, 1, 2, Qt::AlignLeft);

	mQuitWarn = new QCheckBox(i18nc("@option:check", "Warn before quitting"), group);
	mQuitWarn->setFixedSize(mQuitWarn->sizeHint());
	mQuitWarn->setWhatsThis(i18nc("@info:whatsthis", "Check to display a warning prompt before quitting <application>KAlarm</application>."));
	grid->addWidget(mQuitWarn, 4, 2, Qt::AlignLeft);

	mAutostartTrayIcon = new QCheckBox(i18nc("@option:check", "Autostart at login"), group);
	mAutostartTrayIcon->setFixedSize(mAutostartTrayIcon->sizeHint());
#ifdef AUTOSTART_BY_KALARMD
	connect(mAutostartTrayIcon, SIGNAL(toggled(bool)), SLOT(slotAutostartToggled(bool)));
#endif
	mAutostartTrayIcon->setWhatsThis(i18nc("@info:whatsthis", "Check to run <application>KAlarm</application> whenever you start KDE."));
	grid->addWidget(mAutostartTrayIcon, 5, 0, 1, 3, Qt::AlignLeft);

	// Autostart alarm daemon
	mAutostartDaemon = new QCheckBox(i18nc("@option:check", "Start alarm monitoring at login"), group);
	mAutostartDaemon->setFixedSize(mAutostartDaemon->sizeHint());
	connect(mAutostartDaemon, SIGNAL(clicked()), SLOT(slotAutostartDaemonClicked()));
	mAutostartDaemon->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Automatically start alarm monitoring whenever you start KDE, by running the alarm daemon (<command>%1</command>).</para>"
	      "<para>This option should always be checked unless you intend to discontinue use of <application>KAlarm</application>.</para>",
	      QLatin1String(DAEMON_APP_NAME)));
	grid->addWidget(mAutostartDaemon, 6, 0, 1, 3, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	// Confirm alarm deletion?
	KHBox* itemBox = new KHBox(this);   // this is to allow left adjustment
	itemBox->setMargin(0);
	mConfirmAlarmDeletion = new QCheckBox(i18nc("@option:check", "Confirm alarm deletions"), itemBox);
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	mConfirmAlarmDeletion->setWhatsThis(i18nc("@info:whatsthis", "Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(itemBox->sizeHint().height());

	// Terminal window to use for command alarms
	group = new QGroupBox(i18nc("@title:group", "Terminal for Command Alarms"), this);
	group->setWhatsThis(i18nc("@info:whatsthis", "Choose which application to use when a command alarm is executed in a terminal window"));
	grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	int row = 0;

	mXtermType = new ButtonGroup(group);
	int index = 0;
	mXtermFirst = -1;
	for (mXtermCount = 0;  !xtermCommands[mXtermCount].isNull();  ++mXtermCount)
	{
		QString cmd = xtermCommands[mXtermCount];
		QStringList args = KShell::splitArgs(cmd);
		if (args.isEmpty()  ||  KStandardDirs::findExe(args[0]).isEmpty())
			continue;
		QRadioButton* radio = new QRadioButton(args[0], group);
		radio->setMinimumSize(radio->sizeHint());
		mXtermType->addButton(radio, mXtermCount);
		if (mXtermFirst < 0)
			mXtermFirst = mXtermCount;   // note the id of the first button
		cmd.replace("%t", KGlobal::mainComponent().aboutData()->programName());
		cmd.replace("%c", "<command>");
		cmd.replace("%w", "<command; sleep>");
		cmd.replace("%C", "[command]");
		cmd.replace("%W", "[command; sleep]");
		radio->setWhatsThis(
		        i18nc("@info:whatsthis", "Check to execute command alarms in a terminal window by <icode>%1</icode>", cmd));
		grid->addWidget(radio, (row = index/3), index % 3, Qt::AlignLeft);
		++index;
	}

	itemBox = new KHBox(group);
	itemBox->setMargin(0);
	grid->addWidget(itemBox, row + 1, 0, 1, 3, Qt::AlignLeft);
	QRadioButton* radio = new QRadioButton(i18nc("@option:radio Other terminal window command", "Other:"), itemBox);
	radio->setFixedSize(radio->sizeHint());
	connect(radio, SIGNAL(toggled(bool)), SLOT(slotOtherTerminalToggled(bool)));
	mXtermType->addButton(radio, mXtermCount);
	if (mXtermFirst < 0)
		mXtermFirst = mXtermCount;   // note the id of the first button
	mXtermCommand = new KLineEdit(itemBox);
	itemBox->setWhatsThis(
	      i18nc("@info:whatsthis", "Enter the full command line needed to execute a command in your chosen terminal window. "
	           "By default the alarm's command string will be appended to what you enter here. "
	           "See the <application>KAlarm</application> Handbook for details of special codes to tailor the command line."));

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void MiscPrefTab::restore(bool defaults)
{
	mAutostartDaemon->setChecked(defaults ? true : Daemon::autoStart());
	bool systray = Preferences::runInSystemTray();
	mRunInSystemTray->setChecked(systray);
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Preferences::disableAlarmsIfStopped());
	mQuitWarn->setChecked(Preferences::quitWarn());
	mAutostartTrayIcon->setChecked(Preferences::autostartTrayIcon());
	mConfirmAlarmDeletion->setChecked(Preferences::confirmAlarmDeletion());
	QString xtermCmd = Preferences::cmdXTermCommand();
	int id = mXtermFirst;
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
			xtermID = -1;       // 'Other' is only acceptable if it's non-blank
		else
		{
			QStringList args = KShell::splitArgs(cmd);
			cmd = args.isEmpty() ? QString() : args[0];
			if (KStandardDirs::findExe(cmd).isEmpty())
			{
				mXtermCommand->setFocus();
				if (KMessageBox::warningContinueCancel(this, i18nc("@info", "Command to invoke terminal window not found: <command>%1</command>", cmd))
				                != KMessageBox::Continue)
					return;
			}
		}
	}
	if (xtermID < 0)
	{
		xtermID = mXtermFirst;
		mXtermType->setButton(mXtermFirst);
	}

	bool systray = mRunInSystemTray->isChecked();
	if (systray != Preferences::runInSystemTray())
		Preferences::setRunInSystemTray(systray);
	bool b = mDisableAlarmsIfStopped->isChecked();
	if (b != Preferences::disableAlarmsIfStopped())
		Preferences::setDisableAlarmsIfStopped(b);
	if (mQuitWarn->isEnabled())
	{
		b = mQuitWarn->isChecked();
		if (b != Preferences::quitWarn())
			Preferences::setQuitWarn(b);
	}
	b = mAutostartTrayIcon->isChecked();
	if (b != Preferences::autostartTrayIcon())
		Preferences::setAutostartTrayIcon(b);
#ifdef AUTOSTART_BY_KALARMD
	bool newAutostartDaemon = mAutostartDaemon->isChecked() || Preferences::autostartTrayIcon();
#else
	bool newAutostartDaemon = mAutostartDaemon->isChecked();
#endif
	if (newAutostartDaemon != Daemon::autoStart())
		Daemon::enableAutoStart(newAutostartDaemon);
	b = mConfirmAlarmDeletion->isChecked();
	if (b != Preferences::confirmAlarmDeletion())
		Preferences::setConfirmAlarmDeletion(b);
	QString text = (xtermID < mXtermCount) ? xtermCommands[xtermID] : mXtermCommand->text();
	if (text != Preferences::cmdXTermCommand())
		Preferences::setCmdXTermCommand(text);
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::slotAutostartDaemonClicked()
{
	if (!mAutostartDaemon->isChecked()
	&&  KMessageBox::warningYesNo(this,
		                      i18nc("@info", "You should not uncheck this option unless you intend to discontinue use of <application>KAlarm</application>"),
		                      QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
		                     ) != KMessageBox::Yes)
		mAutostartDaemon->setChecked(true);
}

void MiscPrefTab::slotRunModeToggled(bool)
{
	bool systray = mRunInSystemTray->isChecked();
	mAutostartTrayIcon->setText(systray ? i18nc("@option:check", "Autostart at login") : i18nc("@option:check", "Autostart system tray icon at login"));
	mAutostartTrayIcon->setWhatsThis((systray ? i18nc("@info:whatsthis", "Check to run <application>KAlarm</application> whenever you start KDE.")
	                                          : i18nc("@info:whatsthis", "Check to display the system tray icon whenever you start KDE.")));
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
= Class TimePrefTab
=============================================================================*/

TimePrefTab::TimePrefTab()
	: PrefsTabBase()
{
	// Default time zone
	KHBox* itemBox = new KHBox(this);
	itemBox->setMargin(0);
	KHBox* box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"), box);
#if 1
	mTimeZone = new TimeZoneCombo(box);
	mTimeZone->setMaxVisibleItems(15);
#else
	mTimeZone = new KComboBox(box);
	mTimeZone->setMaxVisibleItems(15);
	const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
	for (KTimeZones::ZoneMap::ConstIterator it = zones.begin();  it != zones.end();  ++it)
		mTimeZone->addItem(it.key());
#endif
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "Select the time zone which <application>KAlarm</application> should use "
	                        "as its default for displaying and entering dates and times."));
	label->setBuddy(mTimeZone);
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	// Start-of-day time
	itemBox = new KHBox(this);
	itemBox->setMargin(0);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:spinbox", "Start of day for date-only alarms:"), box);
	mStartOfDay = new TimeEdit(box);
	mStartOfDay->setFixedSize(mStartOfDay->sizeHint());
	label->setBuddy(mStartOfDay);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>The earliest time of day at which a date-only alarm will be triggered.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());


	// Working hours
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Working Hours"), this);
	QBoxLayout* layout = new QVBoxLayout(group);
	layout->setMargin(KDialog::marginHint());
	layout->setSpacing(KDialog::spacingHint());

	QWidget* daybox = new QWidget(group);   // this is to control the QWhatsThis text display area
	layout->addWidget(daybox);
	QGridLayout* wgrid = new QGridLayout(daybox);
	wgrid->setSpacing(KDialog::spacingHint());
	const KLocale* locale = KGlobal::locale();
	for (int i = 0;  i < 7;  ++i)
	{
		int day = KAlarm::localeDayInWeek_to_weekDay(i);
		mWorkDays[i] = new QCheckBox(KAlarm::weekDayName(day, locale), daybox);
		mWorkDays[i]->setFixedSize(mWorkDays[i]->sizeHint());
		wgrid->addWidget(mWorkDays[i], i/4, i%4, Qt::AlignLeft);
	}
	daybox->setFixedHeight(daybox->sizeHint().height());
	daybox->setWhatsThis(i18nc("@info:whatsthis", "Check the days in the week which are work days"));

	itemBox = new KHBox(group);
	itemBox->setMargin(0);
	layout->addWidget(itemBox);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* startLabel = new QLabel(i18nc("@label:spinbox", "Daily start time:"), box);
	mWorkStart = new TimeEdit(box);
	mWorkStart->setFixedSize(mWorkStart->sizeHint());
	startLabel->setBuddy(mWorkStart);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Enter the start time of the working day.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls

	itemBox = new KHBox(group);
	itemBox->setMargin(0);
	layout->addWidget(itemBox);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* endLabel = new QLabel(i18nc("@label:spinbox", "Daily end time:"), box);
	mWorkEnd = new TimeEdit(box);
	mWorkEnd->setFixedSize(mWorkEnd->sizeHint());
	endLabel->setBuddy(mWorkEnd);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Enter the end time of the working day.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());
	int w = qMax(startLabel->sizeHint().width(), endLabel->sizeHint().width());
	startLabel->setFixedWidth(w);
	endLabel->setFixedWidth(w);

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void TimePrefTab::restore(bool)
{
#if 1
	mTimeZone->setTimeZone(Preferences::timeZone());
#else
	int tzindex = 0;
	KTimeZone tz = Preferences::timeZone();
	if (tz.isValid())
	{
		QString zone = tz.name();
		int count = mTimeZone->count();
		while (tzindex < count  &&  mTimeZone->itemText(tzindex) != zone)
			++tzindex;
		if (tzindex >= count)
			tzindex = 0;
	}
	mTimeZone->setCurrentIndex(tzindex);
#endif
	mStartOfDay->setValue(Preferences::startOfDay());
	mWorkStart->setValue(Preferences::workDayStart());
	mWorkEnd->setValue(Preferences::workDayEnd());
	QBitArray days = Preferences::workDays();
	for (int i = 0;  i < 7;  ++i)
	{
		bool x = days.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1);
		mWorkDays[i]->setChecked(x);
	}
}

void TimePrefTab::apply(bool syncToDisc)
{
#if 1
	KTimeZone tz = mTimeZone->timeZone();
	if (tz.isValid())
		Preferences::setTimeZone(tz);
#else
	KTimeZone tz = KSystemTimeZones::zone(mTimeZone->currentText());
	if (tz.isValid()  &&  tz != Preferences::timeZone())
		Preferences::setTimeZone(tz);
#endif
	int t = mStartOfDay->value();
	QTime sodt(t/60, t%60, 0);
	if (sodt != Preferences::startOfDay())
		Preferences::setStartOfDay(sodt);
	t = mWorkStart->value();
	Preferences::setWorkDayStart(QTime(t/60, t%60, 0));
	t = mWorkEnd->value();
	Preferences::setWorkDayEnd(QTime(t/60, t%60, 0));
	QBitArray workDays(7);
	for (int i = 0;  i < 7;  ++i)
		if (mWorkDays[i]->isChecked())
			workDays.setBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1, 1);
	Preferences::setWorkDays(workDays);
	PrefsTabBase::apply(syncToDisc);
}


/*=============================================================================
= Class StorePrefTab
=============================================================================*/

StorePrefTab::StorePrefTab()
	: PrefsTabBase(),
	  mCheckKeepChanges(false)
{
	// Which resource to save to
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "New Alarms && Templates"), this);
	QButtonGroup* bgroup = new QButtonGroup(group);
	QBoxLayout* layout = new QVBoxLayout(group);
	layout->setMargin(KDialog::marginHint());
	layout->setSpacing(KDialog::spacingHint());

	mDefaultResource = new QRadioButton(i18nc("@option:radio", "Store in default resource"), group);
	bgroup->addButton(mDefaultResource);
	mDefaultResource->setFixedSize(mDefaultResource->sizeHint());
	mDefaultResource->setWhatsThis(i18nc("@info:whatsthis", "Add all new alarms and alarm templates to the default resources, without prompting."));
	layout->addWidget(mDefaultResource, 0, Qt::AlignLeft);
	mAskResource = new QRadioButton(i18nc("@option:radio", "Prompt for which resource to store in"), group);
	bgroup->addButton(mAskResource);
	mAskResource->setFixedSize(mAskResource->sizeHint());
	mAskResource->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>When saving a new alarm or alarm template, prompt for which resource to store it in, if there is more than one active resource.</para>"
	      "<para>Note that archived alarms are always stored in the default archived alarm resource.</para>"));
	layout->addWidget(mAskResource, 0, Qt::AlignLeft);

	// Archived alarms
	group = new QGroupBox(i18nc("@title:group", "Archived Alarms"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	mKeepArchived = new QCheckBox(i18nc("@option:check", "Keep alarms after expiry"), group);
	mKeepArchived->setFixedSize(mKeepArchived->sizeHint());
	connect(mKeepArchived, SIGNAL(toggled(bool)), SLOT(slotArchivedToggled(bool)));
	mKeepArchived->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to archive alarms after expiry or deletion (except deleted alarms which were never triggered)."));
	grid->addWidget(mKeepArchived, 0, 0, 1, 2, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mPurgeArchived = new QCheckBox(i18nc("@option:check", "Discard archived alarms after:"), box);
	mPurgeArchived->setMinimumSize(mPurgeArchived->sizeHint());
	connect(mPurgeArchived, SIGNAL(toggled(bool)), SLOT(slotArchivedToggled(bool)));
	mPurgeAfter = new SpinBox(box);
	mPurgeAfter->setMinimum(1);
	mPurgeAfter->setSingleShiftStep(10);
	mPurgeAfter->setMinimumSize(mPurgeAfter->sizeHint());
	mPurgeAfterLabel = new QLabel(i18nc("@label Time unit for user-entered number", "days"), box);
	mPurgeAfterLabel->setMinimumSize(mPurgeAfterLabel->sizeHint());
	mPurgeAfterLabel->setBuddy(mPurgeAfter);
	box->setWhatsThis(i18nc("@info:whatsthis", "Uncheck to store archived alarms indefinitely. Check to enter how long archived alarms should be stored."));
	grid->addWidget(box, 1, 1, Qt::AlignLeft);

	mClearArchived = new QPushButton(i18nc("@action:button", "Clear Archived Alarms"), group);
	mClearArchived->setFixedSize(mClearArchived->sizeHint());
	connect(mClearArchived, SIGNAL(clicked()), SLOT(slotClearArchived()));
	mClearArchived->setWhatsThis((AlarmResources::instance()->activeCount(AlarmResource::ARCHIVED, false) <= 1)
	        ? i18nc("@info:whatsthis", "Delete all existing archived alarms.")
	        : i18nc("@info:whatsthis", "Delete all existing archived alarms (from the default archived alarm resource only)."));
	grid->addWidget(mClearArchived, 2, 1, Qt::AlignLeft);
	group->setFixedHeight(group->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void StorePrefTab::restore(bool defaults)
{
	mCheckKeepChanges = defaults;
	mAskResource->setChecked(Preferences::askResource());
	setArchivedControls(Preferences::archivedKeepDays());
	if (!defaults)
		mOldKeepArchived = mKeepArchived->isChecked();
	mCheckKeepChanges = true;
}

void StorePrefTab::apply(bool syncToDisc)
{
	bool b = mAskResource->isChecked();
	if (b != Preferences::askResource())
		Preferences::setAskResource(mAskResource->isChecked());
	int days = !mKeepArchived->isChecked() ? 0 : mPurgeArchived->isChecked() ? mPurgeAfter->value() : -1;
	if (days != Preferences::archivedKeepDays())
		Preferences::setArchivedKeepDays(days);
	PrefsTabBase::apply(syncToDisc);
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
		     i18nc("@info", "<para>A default resource is required in order to archive alarms, but none is currently enabled.</para>"
		          "<para>If you wish to keep expired alarms, please first use the resources view to select a default "
		          "archived alarms resource.</para>"));
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
	bool single = AlarmResources::instance()->activeCount(AlarmResource::ARCHIVED, false) <= 1;
	if (KMessageBox::warningContinueCancel(this, single ? i18nc("@info", "Do you really want to delete all archived alarms?")
	                                                    : i18nc("@info", "Do you really want to delete all alarms in the default archived alarm resource?"))
			!= KMessageBox::Continue)
		return;
	theApp()->purgeAll();
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
	QLabel* label = new QLabel(i18nc("@label", "Email client:"), box);
	mEmailClient = new ButtonGroup(box);
	QString kmailOption = i18nc("@option:radio", "KMail");
	QString sendmailOption = i18nc("@option:radio", "Sendmail");
	mKMailButton = new RadioButton(kmailOption, box);
	mKMailButton->setMinimumSize(mKMailButton->sizeHint());
	mEmailClient->addButton(mKMailButton, Preferences::kmail);
	mSendmailButton = new RadioButton(sendmailOption, box);
	mSendmailButton->setMinimumSize(mSendmailButton->sizeHint());
	mEmailClient->addButton(mSendmailButton, Preferences::sendmail);
	connect(mEmailClient, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotEmailClientChanged(QAbstractButton*)));
	box->setFixedHeight(box->sizeHint().height());
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Choose how to send email when an email alarm is triggered."
	      "<list><item><interface>%1</interface>: The email is sent automatically via <application>KMail</application>. <application>KMail</application> is started first if necessary.</item>"
	      "<item><interface>%2</interface>: The email is sent automatically. This option will only work if "
	      "your system is configured to use <application>sendmail</application> or a sendmail compatible mail transport agent.</item></list></para>",
	      kmailOption, sendmailOption));

	box = new KHBox(this);   // this is to allow left adjustment
	box->setMargin(0);
	mEmailCopyToKMail = new QCheckBox(i18nc("@option:check", "Copy sent emails into <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()), box);
	mEmailCopyToKMail->setFixedSize(mEmailCopyToKMail->sizeHint());
	mEmailCopyToKMail->setWhatsThis(i18nc("@info:whatsthis", "After sending an email, store a copy in <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Your Email Address group box
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Your Email Address"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);

	// 'From' email address controls ...
	label = new Label(i18nc("@label 'From' email address", "From:"), group);
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
	mEmailAddress = new KLineEdit(group);
	connect(mEmailAddress, SIGNAL(textChanged(const QString&)), SLOT(slotAddressChanged()));
	QString whatsThis = i18nc("@info:whatsthis", "Your email address, used to identify you as the sender when sending email alarms.");
	mFromAddrButton->setWhatsThis(whatsThis);
	mEmailAddress->setWhatsThis(whatsThis);
	mFromAddrButton->setFocusWidget(mEmailAddress);
	grid->addWidget(mEmailAddress, 1, 2);

	// 'From' email address to be taken from Control Centre
	mFromCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from Control Center"), group);
	mFromCCentreButton->setFixedSize(mFromCCentreButton->sizeHint());
	mFromAddressGroup->addButton(mFromCCentreButton, Preferences::MAIL_FROM_CONTROL_CENTRE);
	mFromCCentreButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use the email address set in the KDE Control Center, to identify you as the sender when sending email alarms."));
	grid->addWidget(mFromCCentreButton, 2, 1, 1, 2, Qt::AlignLeft);

	// 'From' email address to be picked from KMail's identities when the email alarm is configured
	mFromKMailButton = new RadioButton(i18nc("@option:radio", "Use <application>KMail</application> identities"), group);
	mFromKMailButton->setFixedSize(mFromKMailButton->sizeHint());
	mFromAddressGroup->addButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
	mFromKMailButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use <application>KMail</application>'s email identities to identify you as the sender when sending email alarms. "
	           "For existing email alarms, <application>KMail</application>'s default identity will be used. "
	           "For new email alarms, you will be able to pick which of <application>KMail</application>'s identities to use."));
	grid->addWidget(mFromKMailButton, 3, 1, 1, 2, Qt::AlignLeft);

	// 'Bcc' email address controls ...
	grid->setRowMinimumHeight(4, KDialog::spacingHint());
	label = new Label(i18nc("@label 'Bcc' email address", "Bcc:"), group);
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
	mEmailBccAddress = new KLineEdit(group);
	whatsThis = i18nc("@info:whatsthis", "Your email address, used for blind copying email alarms to yourself. "
	                 "If you want blind copies to be sent to your account on the computer which <application>KAlarm</application> runs on, you can simply enter your user login name.");
	mBccAddrButton->setWhatsThis(whatsThis);
	mEmailBccAddress->setWhatsThis(whatsThis);
	mBccAddrButton->setFocusWidget(mEmailBccAddress);
	grid->addWidget(mEmailBccAddress, 5, 2);

	// 'Bcc' email address to be taken from Control Centre
	mBccCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from Control Center"), group);
	mBccCCentreButton->setFixedSize(mBccCCentreButton->sizeHint());
	mBccAddressGroup->addButton(mBccCCentreButton, Preferences::MAIL_FROM_CONTROL_CENTRE);
	mBccCCentreButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use the email address set in the KDE Control Center, for blind copying email alarms to yourself."));
	grid->addWidget(mBccCCentreButton, 6, 1, 1, 2, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	box = new KHBox(this);   // this is to allow left adjustment
	box->setMargin(0);
	mEmailQueuedNotify = new QCheckBox(i18nc("@option:check", "Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setFixedSize(mEmailQueuedNotify->sizeHint());
	mEmailQueuedNotify->setWhatsThis(
	      i18nc("@info:whatsthis", "Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void EmailPrefTab::restore(bool defaults)
{
	mEmailClient->setButton(Preferences::emailClient());
	mEmailCopyToKMail->setChecked(Preferences::emailCopyToKMail());
	setEmailAddress(Preferences::emailFrom(), Preferences::emailAddress());
	setEmailBccAddress((Preferences::emailBccFrom() == Preferences::MAIL_FROM_CONTROL_CENTRE), Preferences::emailBccAddress());
	mEmailQueuedNotify->setChecked(Preferences::emailQueuedNotify());
	if (!defaults)
		mAddressChanged = mBccAddressChanged = false;
}

void EmailPrefTab::apply(bool syncToDisc)
{
	int client = mEmailClient->selectedId();
	if (client >= 0  &&  static_cast<Preferences::MailClient>(client) != Preferences::emailClient())
		Preferences::setEmailClient(static_cast<Preferences::MailClient>(client));
	bool b = mEmailCopyToKMail->isChecked();
	if (b != Preferences::emailCopyToKMail())
		Preferences::setEmailCopyToKMail(b);
	int from = mFromAddressGroup->selectedId();
	QString text = mEmailAddress->text().trimmed();
	if (from >= 0  &&  static_cast<Preferences::MailFrom>(from) != Preferences::emailFrom()
	||  text != Preferences::emailAddress())
		Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(from), text);
	b = (mBccAddressGroup->checkedButton() == mBccCCentreButton);
	Preferences::MailFrom bfrom = b ? Preferences::MAIL_FROM_CONTROL_CENTRE : Preferences::MAIL_FROM_ADDR;;
	text = mEmailBccAddress->text().trimmed();
	if (bfrom != Preferences::emailBccFrom()  ||  text != Preferences::emailBccAddress())
		Preferences::setEmailBccAddress(b, text);
	b = mEmailQueuedNotify->isChecked();
	if (b != Preferences::emailQueuedNotify())
		Preferences::setEmailQueuedNotify(mEmailQueuedNotify->isChecked());
	PrefsTabBase::apply(syncToDisc);
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
		return validateAddr(mBccAddressGroup, mEmailBccAddress, i18nc("@info/plain", "No valid 'Bcc' email address is specified."));
	}
	return QString();
}

QString EmailPrefTab::validateAddr(ButtonGroup* group, KLineEdit* addr, const QString& msg)
{
	QString errmsg = i18nc("@info", "<para>%1</para><para>Are you sure you want to save your changes?</para>", msg);
	switch (group->selectedId())
	{
		case Preferences::MAIL_FROM_CONTROL_CENTRE:
			if (!KAMail::controlCentreAddress().isEmpty())
				return QString();
			errmsg = i18nc("@info", "No email address is currently set in the KDE Control Center. %1", errmsg);
			break;
		case Preferences::MAIL_FROM_KMAIL:
			if (KAMail::identitiesExist())
				return QString();
			errmsg = i18nc("@info", "No <application>KMail</application> identities currently exist. %1", errmsg);
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
	mFontChooser = new FontColourChooser(this, QStringList(), i18nc("@title:group", "Message Font && Color"), true, false);

	KHBox* layoutBox = new KHBox(this);
	layoutBox->setMargin(0);
	KHBox* box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label1 = new QLabel(i18nc("@label:listbox", "Disabled alarm color:"), box);
//	label1->setMinimumSize(label1->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mDisabledColour = new KColorCombo(box);
	mDisabledColour->setMinimumSize(mDisabledColour->sizeHint());
	label1->setBuddy(mDisabledColour);
	box->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for disabled alarms."));
	layoutBox->setStretchFactor(new QWidget(layoutBox), 1);    // left adjust the controls
	layoutBox->setFixedHeight(layoutBox->sizeHint().height());

	layoutBox = new KHBox(this);
	layoutBox->setMargin(0);
	box = new KHBox(layoutBox);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label2 = new QLabel(i18nc("@label:listbox", "Archived alarm color:"), box);
//	label2->setMinimumSize(label2->sizeHint());
	box->setStretchFactor(new QWidget(box), 1);
	mArchivedColour = new KColorCombo(box);
	mArchivedColour->setMinimumSize(mArchivedColour->sizeHint());
	label2->setBuddy(mArchivedColour);
	box->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for archived alarms."));
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

void FontColourPrefTab::restore(bool)
{
	mFontChooser->setBgColour(Preferences::defaultBgColour());
	mFontChooser->setColours(Preferences::messageColours());
	mFontChooser->setFont(Preferences::messageFont());
	mDisabledColour->setColor(Preferences::disabledColour());
	mArchivedColour->setColor(Preferences::archivedColour());
}

void FontColourPrefTab::apply(bool syncToDisc)
{
	QColor colour = mFontChooser->bgColour();
	if (colour != Preferences::defaultBgColour())
		Preferences::setDefaultBgColour(colour);
	ColourList clist = mFontChooser->colours();
	if (clist != Preferences::messageColours())
		Preferences::setMessageColours(clist);
	QFont font = mFontChooser->font();
	if (font != Preferences::messageFont())
		Preferences::setMessageFont(font);
	colour = mDisabledColour->color();
	if (colour != Preferences::disabledColour())
		Preferences::setDisabledColour(colour);
	colour = mArchivedColour->color();
	if (colour != Preferences::archivedColour())
		Preferences::setArchivedColour(colour);
	PrefsTabBase::apply(syncToDisc);
}


/*=============================================================================
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab()
	: PrefsTabBase()
{
#define DEFSETTING "The default setting for <interface>%1</interface> in the alarm edit dialog."

	// DISPLAY ALARMS
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Display Alarms"), this);
	QVBoxLayout* vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	mConfirmAck = new QCheckBox(EditDisplayAlarmDlg::i18n_chk_ConfirmAck(), group);
	mConfirmAck->setMinimumSize(mConfirmAck->sizeHint());
	mConfirmAck->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditDisplayAlarmDlg::i18n_chk_ConfirmAck()));
	vlayout->addWidget(mConfirmAck, 0, Qt::AlignLeft);

	mAutoClose = new QCheckBox(LateCancelSelector::i18n_chk_AutoCloseWinLC(), group);
	mAutoClose->setMinimumSize(mAutoClose->sizeHint());
	mAutoClose->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, LateCancelSelector::i18n_chk_AutoCloseWin()));
	vlayout->addWidget(mAutoClose, 0, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	QLabel* label = new QLabel(i18nc("@label:listbox", "Reminder units:"), box);
	label->setFixedSize(label->sizeHint());
	mReminderUnits = new KComboBox(box);
	mReminderUnits->addItem(i18nc("@item:inlistbox", "Minutes"), TimePeriod::Minutes);
	mReminderUnits->addItem(i18nc("@item:inlistbox", "Hours/Minutes"), TimePeriod::HoursMinutes);
	mReminderUnits->addItem(i18nc("@item:inlistbox Time unit option", "Days"), TimePeriod::Days);
	mReminderUnits->addItem(i18nc("@item:inlistbox", "Weeks"), TimePeriod::Weeks);
	mReminderUnits->setFixedSize(mReminderUnits->sizeHint());
	label->setBuddy(mReminderUnits);
	box->setWhatsThis(i18nc("@info:whatsthis", "The default units for the reminder in the alarm edit dialog."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	mSpecialActionsButton = new SpecialActionsButton(box);
	mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());

	// SOUND
	QGroupBox* bbox = new QGroupBox(i18nc("@title:group Audio options group", "Sound"), this);
	vlayout = new QVBoxLayout(bbox);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	vlayout->addLayout(hlayout);
	mSound = new KComboBox(bbox);
	mSound->addItem(SoundPicker::i18n_combo_None());         // index 0
	mSound->addItem(SoundPicker::i18n_combo_Beep());         // index 1
	mSound->addItem(SoundPicker::i18n_combo_File());         // index 2
	if (theApp()->speechEnabled())
		mSound->addItem(SoundPicker::i18n_combo_Speak());  // index 3
	mSound->setMinimumSize(mSound->sizeHint());
	mSound->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, SoundPicker::i18n_label_Sound()));
	hlayout->addWidget(mSound);
	hlayout->addStretch();

	mSoundRepeat = new QCheckBox(i18nc("@option:check", "Repeat sound file"), bbox);
	mSoundRepeat->setMinimumSize(mSoundRepeat->sizeHint());
	mSoundRepeat->setWhatsThis(
	      i18nc("@info:whatsthis sound file 'Repeat' checkbox", "The default setting for sound file <interface>%1</interface> in the alarm edit dialog.", SoundDlg::i18n_chk_Repeat()));
	hlayout->addWidget(mSoundRepeat);

	box = new KHBox(bbox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mSoundFileLabel = new QLabel(i18nc("@label:textbox", "Sound file:"), box);
	mSoundFileLabel->setFixedSize(mSoundFileLabel->sizeHint());
	mSoundFile = new KLineEdit(box);
	mSoundFileLabel->setBuddy(mSoundFile);
	mSoundFileBrowse = new QPushButton(box);
	mSoundFileBrowse->setIcon(KIcon(SmallIcon("document-open")));
	mSoundFileBrowse->setFixedSize(mSoundFileBrowse->sizeHint());
	connect(mSoundFileBrowse, SIGNAL(clicked()), SLOT(slotBrowseSoundFile()));
	mSoundFileBrowse->setToolTip(i18nc("@info:tooltip", "Choose a sound file"));
	box->setWhatsThis(i18nc("@info:whatsthis", "Enter the default sound file to use in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box);
	bbox->setFixedHeight(bbox->sizeHint().height());

	// COMMAND ALARMS
	group = new QGroupBox(i18nc("@title:group", "Command Alarms"), this);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	vlayout->addLayout(hlayout);

	mCmdScript = new QCheckBox(EditCommandAlarmDlg::i18n_chk_EnterScript(), group);
	mCmdScript->setMinimumSize(mCmdScript->sizeHint());
	mCmdScript->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditCommandAlarmDlg::i18n_chk_EnterScript()));
	hlayout->addWidget(mCmdScript);
	hlayout->addStretch();

	mCmdXterm = new QCheckBox(EditCommandAlarmDlg::i18n_chk_ExecInTermWindow(), group);
	mCmdXterm->setMinimumSize(mCmdXterm->sizeHint());
	mCmdXterm->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditCommandAlarmDlg::i18n_radio_ExecInTermWindow()));
	hlayout->addWidget(mCmdXterm);

	// EMAIL ALARMS
	group = new QGroupBox(i18nc("@title:group", "Email Alarms"), this);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	// BCC email to sender
	mEmailBcc = new QCheckBox(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf(), group);
	mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf()));
	vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

	// MISCELLANEOUS
	// Show in KOrganizer
	mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_chk_ShowInKOrganizer(), this);
	mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
	mCopyToKOrganizer->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditAlarmDlg::i18n_chk_ShowInKOrganizer()));

	// Late cancellation
	box = new KHBox(this);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mLateCancel = new QCheckBox(LateCancelSelector::i18n_chk_CancelIfLate(), box);
	mLateCancel->setMinimumSize(mLateCancel->sizeHint());
	mLateCancel->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, LateCancelSelector::i18n_chk_CancelIfLate()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	// Recurrence
	KHBox* itemBox = new KHBox(box);   // this is to control the QWhatsThis text display area
	itemBox->setMargin(0);
	itemBox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:listbox", "Recurrence:"), itemBox);
	label->setFixedSize(label->sizeHint());
	mRecurPeriod = new KComboBox(itemBox);
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_NoRecur());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_AtLogin());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_HourlyMinutely());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Daily());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Weekly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Monthly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Yearly());
	mRecurPeriod->setFixedSize(mRecurPeriod->sizeHint());
	label->setBuddy(mRecurPeriod);
	itemBox->setWhatsThis(i18nc("@info:whatsthis", "The default setting for the recurrence rule in the alarm edit dialog."));
	box->setFixedHeight(itemBox->sizeHint().height());

	// How to handle February 29th in yearly recurrences
	KVBox* vbox = new KVBox(this);   // this is to control the QWhatsThis text display area
	vbox->setMargin(0);
	vbox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label", "In non-leap years, repeat yearly February 29th alarms on:"), vbox);
	label->setAlignment(Qt::AlignLeft);
	label->setWordWrap(true);
	itemBox = new KHBox(vbox);
	itemBox->setMargin(0);
	itemBox->setSpacing(2*KDialog::spacingHint());
	mFeb29 = new ButtonGroup(itemBox);
	QWidget* widget = new QWidget(itemBox);
	widget->setFixedWidth(3*KDialog::spacingHint());
	QRadioButton* radio = new QRadioButton(i18nc("@option:radio", "February 2&8th"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, Preferences::Feb29_Feb28);
	radio = new QRadioButton(i18nc("@option:radio", "March &1st"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, Preferences::Feb29_Mar1);
	radio = new QRadioButton(i18nc("@option:radio", "Do not repeat"), itemBox);
	radio->setMinimumSize(radio->sizeHint());
	mFeb29->addButton(radio, Preferences::Feb29_None);
	itemBox->setFixedHeight(itemBox->sizeHint().height());
	vbox->setWhatsThis(i18nc("@info:whatsthis",
	      "For yearly recurrences, choose what date, if any, alarms due on February 29th should occur in non-leap years."
	      "<note>The next scheduled occurrence of existing alarms is not re-evaluated when you change this setting.</note>"));

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void EditPrefTab::restore(bool)
{
	int index;
	mAutoClose->setChecked(Preferences::defaultAutoClose());
	mConfirmAck->setChecked(Preferences::defaultConfirmAck());
	switch (Preferences::defaultReminderUnits())
	{
		case TimePeriod::Weeks:        index = 3; break;
		case TimePeriod::Days:         index = 2; break;
		default:
		case TimePeriod::HoursMinutes: index = 1; break;
		case TimePeriod::Minutes:      index = 0; break;
	}
	mReminderUnits->setCurrentIndex(index);
	mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction());
	mSound->setCurrentIndex(soundIndex(Preferences::defaultSoundType()));
	mSoundFile->setText(Preferences::defaultSoundFile());
	mSoundRepeat->setChecked(Preferences::defaultSoundRepeat());
	mCmdScript->setChecked(Preferences::defaultCmdScript());
	mCmdXterm->setChecked(Preferences::defaultCmdLogType() == Preferences::Log_Terminal);
	mEmailBcc->setChecked(Preferences::defaultEmailBcc());
	mCopyToKOrganizer->setChecked(Preferences::defaultCopyToKOrganizer());
	mLateCancel->setChecked(Preferences::defaultLateCancel());
	switch (Preferences::defaultRecurPeriod())
	{
		case Preferences::Recur_Yearly:   index = 6; break;
		case Preferences::Recur_Monthly:  index = 5; break;
		case Preferences::Recur_Weekly:   index = 4; break;
		case Preferences::Recur_Daily:    index = 3; break;
		case Preferences::Recur_SubDaily: index = 2; break;
		case Preferences::Recur_Login:    index = 1; break;
		case Preferences::Recur_None:
		default:                          index = 0; break;
	}
	mRecurPeriod->setCurrentIndex(index);
	mFeb29->setButton(Preferences::defaultFeb29Type());
}

void EditPrefTab::apply(bool syncToDisc)
{
	bool b = mAutoClose->isChecked();
	if (b != Preferences::defaultAutoClose())
		Preferences::setDefaultAutoClose(b);
	b = mConfirmAck->isChecked();
	if (b != Preferences::defaultConfirmAck())
		Preferences::setDefaultConfirmAck(b);
	TimePeriod::Units units;
	switch (mReminderUnits->currentIndex())
	{
		case 3:  units = TimePeriod::Weeks;        break;
		case 2:  units = TimePeriod::Days;         break;
		default:
		case 1:  units = TimePeriod::HoursMinutes; break;
		case 0:  units = TimePeriod::Minutes;      break;
	}
	if (units != Preferences::defaultReminderUnits())
		Preferences::setDefaultReminderUnits(units);
	QString text = mSpecialActionsButton->preAction();
	if (text != Preferences::defaultPreAction())
		Preferences::setDefaultPreAction(text);
	text = mSpecialActionsButton->postAction();
	if (text != Preferences::defaultPostAction())
		Preferences::setDefaultPostAction(text);
	Preferences::SoundType snd;
	switch (mSound->currentIndex())
	{
		case 3:  snd = Preferences::Sound_Speak; break;
		case 2:  snd = Preferences::Sound_File;  break;
		case 1:  snd = Preferences::Sound_Beep;  break;
		case 0:
		default: snd = Preferences::Sound_None;  break;
	}
	if (snd != Preferences::defaultSoundType())
		Preferences::setDefaultSoundType(snd);
	text = mSoundFile->text();
	if (text != Preferences::defaultSoundFile())
		Preferences::setDefaultSoundFile(text);
	b = mSoundRepeat->isChecked();
	if (b != Preferences::defaultSoundRepeat())
		Preferences::setDefaultSoundRepeat(b);
	b = mCmdScript->isChecked();
	if (b != Preferences::defaultCmdScript())
		Preferences::setDefaultCmdScript(b);
	Preferences::CmdLogType log = mCmdXterm->isChecked() ? Preferences::Log_Terminal : Preferences::Log_Discard;
	if (log != Preferences::defaultCmdLogType())
		Preferences::setDefaultCmdLogType(log);
	b = mEmailBcc->isChecked();
	if (b != Preferences::defaultEmailBcc())
		Preferences::setDefaultEmailBcc(b);
	b = mCopyToKOrganizer->isChecked();
	if (b != Preferences::defaultCopyToKOrganizer())
		Preferences::setDefaultCopyToKOrganizer(b);
	int i = mLateCancel->isChecked() ? 1 : 0;
	if (i != Preferences::defaultLateCancel())
		Preferences::setDefaultLateCancel(i);
	Preferences::RecurType period;
	switch (mRecurPeriod->currentIndex())
	{
		case 6:  period = Preferences::Recur_Yearly;   break;
		case 5:  period = Preferences::Recur_Monthly;  break;
		case 4:  period = Preferences::Recur_Weekly;   break;
		case 3:  period = Preferences::Recur_Daily;    break;
		case 2:  period = Preferences::Recur_SubDaily; break;
		case 1:  period = Preferences::Recur_Login;    break;
		case 0:
		default: period = Preferences::Recur_None;     break;
	}
	if (period != Preferences::defaultRecurPeriod())
		Preferences::setDefaultRecurPeriod(period);
	int feb29 = mFeb29->selectedId();
	if (feb29 >= 0  &&  static_cast<Preferences::Feb29Type>(feb29) != Preferences::defaultFeb29Type())
		Preferences::setDefaultFeb29Type(static_cast<Preferences::Feb29Type>(feb29));
	PrefsTabBase::apply(syncToDisc);
}

void EditPrefTab::slotBrowseSoundFile()
{
	QString defaultDir;
	QString url = SoundPicker::browseFile(defaultDir, mSoundFile->text());
	if (!url.isEmpty())
		mSoundFile->setText(url);
}

int EditPrefTab::soundIndex(Preferences::SoundType type)
{
	switch (type)
	{
		case Preferences::Sound_Speak: return 3;
		case Preferences::Sound_File:  return 2;
		case Preferences::Sound_Beep:  return 1;
		case Preferences::Sound_None:
		default:                       return 0;
	}
}

QString EditPrefTab::validate()
{
	if (mSound->currentIndex() == soundIndex(Preferences::Sound_File)  &&  mSoundFile->text().isEmpty())
	{
		mSoundFile->setFocus();
		return i18nc("@info", "You must enter a sound file when <interface>%1</interface> is selected as the default sound type", SoundPicker::i18n_combo_File());;
	}
	return QString();
}


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab()
	: PrefsTabBase()
{
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "System Tray Tooltip"), this);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(2, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	grid->setColumnMinimumWidth(1, indentWidth());

	mTooltipShowAlarms = new QCheckBox(i18nc("@option:check", "Show next &24 hours' alarms"), group);
	mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
	connect(mTooltipShowAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipAlarmsToggled(bool)));
	mTooltipShowAlarms->setWhatsThis(
	      i18nc("@info:whatsthis", "Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours."));
	grid->addWidget(mTooltipShowAlarms, 0, 0, 1, 3, Qt::AlignLeft);

	KHBox* box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mTooltipMaxAlarms = new QCheckBox(i18nc("@option:check", "Maximum number of alarms to show:"), box);
	mTooltipMaxAlarms->setMinimumSize(mTooltipMaxAlarms->sizeHint());
	connect(mTooltipMaxAlarms, SIGNAL(toggled(bool)), SLOT(slotTooltipMaxToggled(bool)));
	mTooltipMaxAlarmCount = new SpinBox(1, 99, box);
	mTooltipMaxAlarmCount->setSingleShiftStep(5);
	mTooltipMaxAlarmCount->setMinimumSize(mTooltipMaxAlarmCount->sizeHint());
	box->setWhatsThis(
	      i18nc("@info:whatsthis", "Uncheck to display all of the next 24 hours' alarms in the system tray tooltip. "
	           "Check to enter an upper limit on the number to be displayed."));
	grid->addWidget(box, 1, 1, 1, 2, Qt::AlignLeft);

	mTooltipShowTime = new QCheckBox(MainWindow::i18n_chk_ShowAlarmTime(), group);
	mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
	connect(mTooltipShowTime, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToggled(bool)));
	mTooltipShowTime->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, the time at which each alarm is due."));
	grid->addWidget(mTooltipShowTime, 2, 1, 1, 2, Qt::AlignLeft);

	mTooltipShowTimeTo = new QCheckBox(MainWindow::i18n_chk_ShowTimeToAlarm(), group);
	mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
	connect(mTooltipShowTimeTo, SIGNAL(toggled(bool)), SLOT(slotTooltipTimeToToggled(bool)));
	mTooltipShowTimeTo->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, how long until each alarm is due."));
	grid->addWidget(mTooltipShowTimeTo, 3, 1, 1, 2, Qt::AlignLeft);

	box = new KHBox(group);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mTooltipTimeToPrefixLabel = new QLabel(i18nc("@label:textbox", "Prefix:"), box);
	mTooltipTimeToPrefixLabel->setFixedSize(mTooltipTimeToPrefixLabel->sizeHint());
	mTooltipTimeToPrefix = new KLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	box->setWhatsThis(i18nc("@info:whatsthis", "Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip."));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 4, 2, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	mModalMessages = new QCheckBox(i18nc("@option:check", "Message windows have a title bar and take keyboard focus"), this);
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	mModalMessages->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Specify the characteristics of alarm message windows:"
	      "<list><item>If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.</item>"
	      "<item>If unchecked, the window does not interfere with your typing when "
	      "it is displayed, but it has no title bar and cannot be moved or resized.</item></list></para>"));

	KHBox* itemBox = new KHBox(this);   // this is to control the QWhatsThis text display area
	itemBox->setMargin(0);
	box = new KHBox(itemBox);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18nc("@label:spinbox", "System tray icon update interval:"), box);
	mDaemonTrayCheckInterval = new SpinBox(1, 9999, box);
	mDaemonTrayCheckInterval->setSingleShiftStep(10);
	mDaemonTrayCheckInterval->setMinimumSize(mDaemonTrayCheckInterval->sizeHint());
	label->setBuddy(mDaemonTrayCheckInterval);
	label = new QLabel(i18nc("@label", "seconds"), box);
	box->setWhatsThis(i18nc("@info:whatsthis", "How often to update the system tray icon to indicate whether or not the Alarm Daemon is monitoring alarms."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(box->sizeHint().height());

	this->setStretchFactor(new QWidget(this), 1);    // top adjust the widgets
}

void ViewPrefTab::restore(bool)
{
	setTooltip(Preferences::tooltipAlarmCount(),
	           Preferences::showTooltipAlarmTime(),
	           Preferences::showTooltipTimeToAlarm(),
	           Preferences::tooltipTimeToPrefix());
	mModalMessages->setChecked(Preferences::modalMessages());
	mDaemonTrayCheckInterval->setValue(Preferences::daemonTrayCheckInterval());
}

void ViewPrefTab::apply(bool syncToDisc)
{
	int n = mTooltipShowAlarms->isChecked() ? -1 : 0;
	if (n  &&  mTooltipMaxAlarms->isChecked())
		n = mTooltipMaxAlarmCount->value();
	if (n != Preferences::tooltipAlarmCount())
		Preferences::setTooltipAlarmCount(n);
	bool b = mTooltipShowTime->isChecked();
	if (b != Preferences::showTooltipAlarmTime())
		Preferences::setShowTooltipAlarmTime(b);
	b = mTooltipShowTimeTo->isChecked();
	if (b != Preferences::showTooltipTimeToAlarm())
		Preferences::setShowTooltipTimeToAlarm(b);
	QString text = mTooltipTimeToPrefix->text();
	if (text != Preferences::tooltipTimeToPrefix())
		Preferences::setTooltipTimeToPrefix(text);
	b = mModalMessages->isChecked();
	if (b != Preferences::modalMessages())
		Preferences::setModalMessages(b);
	int i = mDaemonTrayCheckInterval->value();
	if (i != Preferences::daemonTrayCheckInterval())
		Preferences::setDaemonTrayCheckInterval(i);
	PrefsTabBase::apply(syncToDisc);
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
