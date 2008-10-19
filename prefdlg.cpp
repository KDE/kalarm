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
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QResizeEvent>

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
#include <ktabwidget.h>
#include <KStandardGuiItem>
#include <ksystemtimezone.h>
#include <kicon.h>
#ifdef Q_WS_X11
#include <kwindowinfo.h>
#include <kwindowsystem.h>
#endif
#include <kdebug.h>

#include <ktoolinvocation.h>
#include <libkholidays/kholidays.h>
using namespace LibKHolidays;

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "colourbutton.h"
#include "editdlg.h"
#include "editdlgtypes.h"
#include "fontcolour.h"
#include "functions.h"
#include "itembox.h"
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
#include "prefdlg_p.moc"
#include "prefdlg.moc"

static const char PREF_DIALOG_NAME[] = "PrefDialog";

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
		if (DialogScroll<KAlarmPrefDlg>::heightReduction())
		{
			// Evaluating the scroll size and then displaying the dialog
			// doesn't adjust the dialog size to fit the minimum height of
			// the pages. The only way to size the dialog correctly seems
			// to be to delete the dialog and create it a second time!?!
			delete mInstance;
			mInstance = new KAlarmPrefDlg;
		}
		QSize s;
		if (KAlarm::readConfigWindowSize(PREF_DIALOG_NAME, s))
			mInstance->resize(s);
		mInstance->show();
	}
	else
	{
#ifdef Q_WS_X11
		KWindowInfo info = KWindowSystem::windowInfo(mInstance->winId(), NET::WMGeometry | NET::WMDesktop);
		KWindowSystem::setCurrentDesktop(info.desktop());
#endif
		mInstance->setWindowState(mInstance->windowState() & ~Qt::WindowMinimized); // un-minimize it if necessary
		mInstance->raise();
		mInstance->activateWindow();
	}
}

KAlarmPrefDlg::KAlarmPrefDlg()
	: KPageDialog()
{
	setAttribute(Qt::WA_DeleteOnClose);
	setObjectName("PrefDlg");    // used by LikeBack
	setCaption(i18nc("@title:window", "Configure"));
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
	kDebug();
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
	kDebug();
	mValid = true;
	slotApply();
	if (mValid)
		KDialog::accept();
}

// Discard the current preferences and close the dialog
void KAlarmPrefDlg::slotCancel()
{
	kDebug();
	restore(false);
	KDialog::reject();
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::restore(bool defaults)
{
	kDebug() << (defaults ? "defaults" : "");
	if (defaults)
		Preferences::self()->useDefaults(true);
	mEmailPage->restore(defaults);
	mViewPage->restore(defaults);
	mEditPage->restore(defaults);
	mStorePage->restore(defaults);
	mTimePage->restore(defaults);
	mMiscPage->restore(defaults);
	if (defaults)
		Preferences::self()->useDefaults(false);
}

/******************************************************************************
* Return the minimum size for the dialog.
* If the minimum size would be too high to fit the desktop, the tab contents
* are made scrollable.
*/
QSize KAlarmPrefDlg::minimumSizeHint() const
{
	if (!DialogScroll<KAlarmPrefDlg>::sized())
	{
		KAlarmPrefDlg* thisvar = const_cast<KAlarmPrefDlg*>(this);
		QSize s = DialogScroll<KAlarmPrefDlg>::initMinimumHeight(thisvar);
		if (s.isValid())
		{
			if (DialogScroll<KAlarmPrefDlg>::heightReduction())
			{
				s = QSize(s.width(), s.height() - DialogScroll<KAlarmPrefDlg>::heightReduction());
				thisvar->resize(s);
			}
			return s;
		}
	}
	return KDialog::minimumSizeHint();
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size in the config file.
*/
void KAlarmPrefDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
		KAlarm::writeConfigWindowSize(PREF_DIALOG_NAME, re->size());
	KPageDialog::resizeEvent(re);
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/
int PrefsTabBase::mIndentWidth = 0;

PrefsTabBase::PrefsTabBase()
	: mLabelsAligned(false)
{
	mTopWidget = new KVBox(this);
	mTopWidget->setMargin(0);
	mTopWidget->setSpacing(KDialog::spacingHint());
	setWidget(mTopWidget);
	if (!mIndentWidth)
	{
		QRadioButton radio(this);
		QStyleOptionButton opt;
		opt.initFrom(&radio);
		mIndentWidth = style()->subElementRect(QStyle::SE_RadioButtonIndicator, &opt).width();
	}
	mTopLayout = qobject_cast<QVBoxLayout*>(mTopWidget->layout());
	Q_ASSERT(mTopLayout);
}

void PrefsTabBase::apply(bool syncToDisc)
{
	if (syncToDisc)
		Preferences::self()->writeConfig();
}

void PrefsTabBase::addAlignedLabel(QLabel* label)
{
	mLabels += label;
}

void PrefsTabBase::showEvent(QShowEvent*)
{
	if (!mLabelsAligned)
	{
		int wid = 0;
		int i;
		int end = mLabels.count();
		QList<int> xpos;
		for (i = 0;  i < end;  ++i)
		{
			int x = mLabels[i]->mapTo(this, QPoint(0, 0)).x();
			xpos += x;
			int w = x + mLabels[i]->sizeHint().width();
			if (w > wid)
				wid = w;
		}
		for (i = 0;  i < end;  ++i)
		{
			mLabels[i]->setFixedWidth(wid - xpos[i]);
			mLabels[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		}
		mLabelsAligned = true;
	}
}


/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab()
	: PrefsTabBase()
{
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Run Mode"), topWidget());
	QVBoxLayout* vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	// Start at login
	mAutoStart = new QCheckBox(i18nc("@option:check", "Start at login"), group);
	connect(mAutoStart, SIGNAL(clicked()), SLOT(slotAutostartClicked()));
	mAutoStart->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Automatically start <application>KAlarm</application> whenever you start KDE.</para>"
	      "<para>This option should always be checked unless you intend to discontinue use of <application>KAlarm</application>.</para>"));
	vlayout->addWidget(mAutoStart, 0, Qt::AlignLeft);

	mQuitWarn = new QCheckBox(i18nc("@option:check", "Warn before quitting"), group);
	mQuitWarn->setWhatsThis(i18nc("@info:whatsthis", "Check to display a warning prompt before quitting <application>KAlarm</application>."));
	vlayout->addWidget(mQuitWarn, 0, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	// Confirm alarm deletion?
	KHBox* itemBox = new KHBox(topWidget());   // this is to allow left adjustment
	itemBox->setMargin(0);
	mConfirmAlarmDeletion = new QCheckBox(i18nc("@option:check", "Confirm alarm deletions"), itemBox);
	mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
	mConfirmAlarmDeletion->setWhatsThis(i18nc("@info:whatsthis", "Check to be prompted for confirmation each time you delete an alarm."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	itemBox->setFixedHeight(itemBox->sizeHint().height());

	// Terminal window to use for command alarms
	group = new QGroupBox(i18nc("@title:group", "Terminal for Command Alarms"), topWidget());
	group->setWhatsThis(i18nc("@info:whatsthis", "Choose which application to use when a command alarm is executed in a terminal window"));
	QGridLayout* grid = new QGridLayout(group);
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

	// QHBox used here doesn't allow the KLineEdit to expand!?
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setSpacing(KDialog::spacingHint());
	grid->addLayout(hlayout, row + 1, 0, 1, 3, Qt::AlignLeft);
	QRadioButton* radio = new QRadioButton(i18nc("@option:radio Other terminal window command", "Other:"), group);
	hlayout->addWidget(radio);
	connect(radio, SIGNAL(toggled(bool)), SLOT(slotOtherTerminalToggled(bool)));
	mXtermType->addButton(radio, mXtermCount);
	if (mXtermFirst < 0)
		mXtermFirst = mXtermCount;   // note the id of the first button
	mXtermCommand = new KLineEdit(group);
	mXtermCommand->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
	hlayout->addWidget(mXtermCommand);
	QString wt = 
	      i18nc("@info:whatsthis", "Enter the full command line needed to execute a command in your chosen terminal window. "
	           "By default the alarm's command string will be appended to what you enter here. "
	           "See the <application>KAlarm</application> Handbook for details of special codes to tailor the command line.");
	radio->setWhatsThis(wt);
	mXtermCommand->setWhatsThis(wt);

	topLayout()->addStretch();    // top adjust the widgets
}

void MiscPrefTab::restore(bool defaults)
{
	Q_UNUSED(defaults);
	mAutoStart->setChecked(Preferences::autoStart());
	mQuitWarn->setChecked(Preferences::quitWarn());
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
				if (KMessageBox::warningContinueCancel(topWidget(), i18nc("@info", "Command to invoke terminal window not found: <command>%1</command>", cmd))
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

	if (mQuitWarn->isEnabled())
	{
		bool b = mQuitWarn->isChecked();
		if (b != Preferences::quitWarn())
			Preferences::setQuitWarn(b);
	}
	bool b = mAutoStart->isChecked();
	if (b != Preferences::autoStart())
		Preferences::setAutoStart(b);
	b = mConfirmAlarmDeletion->isChecked();
	if (b != Preferences::confirmAlarmDeletion())
		Preferences::setConfirmAlarmDeletion(b);
	QString text = (xtermID < mXtermCount) ? xtermCommands[xtermID] : mXtermCommand->text();
	if (text != Preferences::cmdXTermCommand())
		Preferences::setCmdXTermCommand(text);
	PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::slotAutostartClicked()
{
	if (!mAutoStart->isChecked()
	&&  KMessageBox::warningYesNo(topWidget(),
		                      i18nc("@info", "You should not uncheck this option unless you intend to discontinue use of <application>KAlarm</application>"),
		                      QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
		                     ) != KMessageBox::Yes)
		mAutoStart->setChecked(true);
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
	ItemBox* itemBox = new ItemBox(topWidget());
	itemBox->setMargin(0);
	KHBox* box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"), box);
	addAlignedLabel(label);
#if 1
	mTimeZone = new TimeZoneCombo(box);
	mTimeZone->setMaxVisibleItems(15);
#else
	mTimeZone = new KComboBox(box);
	mTimeZone->setMaxVisibleItems(15);
	const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
	for (KTimeZones::ZoneMap::ConstIterator it = zones.constBegin();  it != zones.constEnd();  ++it)
		mTimeZone->addItem(it.key());
#endif
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "Select the time zone which <application>KAlarm</application> should use "
	                        "as its default for displaying and entering dates and times."));
	label->setBuddy(mTimeZone);
	itemBox->leftAlign();
	itemBox->setFixedHeight(box->sizeHint().height());

	// Holiday region
	itemBox = new ItemBox(topWidget());
	itemBox->setMargin(0);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:listbox", "Holiday region:"), box);
	addAlignedLabel(label);
	mHolidays = new KComboBox(box);
	mHolidays->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	itemBox->leftAlign();
	label->setBuddy(mHolidays);
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "Select which holiday region to use"));
	QStringList holidays;
	QStringList countryList = KHolidays::locations();
	foreach (const QString& country, countryList)
	{
		QString countryFile = KStandardDirs::locate("locale", "l10n/" + country + "/entry.desktop");
		QString regionName;  // name to display
		if (!countryFile.isEmpty())
		{
			KConfig config(countryFile, KConfig::SimpleConfig);
			KConfigGroup cfg(&config, "KCM Locale");
			regionName = cfg.readEntry("Name");
		}
		if (regionName.isEmpty())
			regionName = country;   // default to file name

		holidays << regionName;
		mHolidayNames[regionName] = country; // store region for saving to config file
	}
	qSort(holidays.begin(), holidays.end(), caseInsensitiveLessThan);
	holidays.push_front(i18nc("@item:inlistbox Do not use holidays", "(None)"));
	mHolidays->addItems(holidays);

	// Start-of-day time
	itemBox = new ItemBox(topWidget());
	itemBox->setMargin(0);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:spinbox", "Start of day for date-only alarms:"), box);
	addAlignedLabel(label);
	mStartOfDay = new TimeEdit(box);
	label->setBuddy(mStartOfDay);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>The earliest time of day at which a date-only alarm will be triggered.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->leftAlign();
	itemBox->setFixedHeight(box->sizeHint().height());

	// Working hours
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Working Hours"), topWidget());
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
		wgrid->addWidget(mWorkDays[i], i/3, i%3, Qt::AlignLeft);
	}
	daybox->setFixedHeight(daybox->sizeHint().height());
	daybox->setWhatsThis(i18nc("@info:whatsthis", "Check the days in the week which are work days"));

	itemBox = new ItemBox(group);
	itemBox->setMargin(0);
	layout->addWidget(itemBox);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:spinbox", "Daily start time:"), box);
	addAlignedLabel(label);
	mWorkStart = new TimeEdit(box);
	label->setBuddy(mWorkStart);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Enter the start time of the working day.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->leftAlign();

	itemBox = new ItemBox(group);
	itemBox->setMargin(0);
	layout->addWidget(itemBox);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label:spinbox", "Daily end time:"), box);
	addAlignedLabel(label);
	mWorkEnd = new TimeEdit(box);
	label->setBuddy(mWorkEnd);
	box->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Enter the end time of the working day.</para>"
	      "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
	itemBox->leftAlign();
	box->setFixedHeight(box->sizeHint().height());

	topLayout()->addStretch();    // top adjust the widgets
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
	QString region;
	QString hol = Preferences::holidays().location();
	if (!hol.isEmpty())
	{
		for (QMap<QString, QString>::ConstIterator it = mHolidayNames.constBegin();  it != mHolidayNames.constEnd();  ++it)
		{
			if (it.value() == hol)
			{
				region = it.key();
				break;
			}
		}
	}
	int i;
	for (i = mHolidays->count();  --i > 0 && mHolidays->itemText(i) != region; ) ;
	mHolidays->setCurrentIndex(i);
	mStartOfDay->setValue(Preferences::startOfDay());
	mWorkStart->setValue(Preferences::workDayStart());
	mWorkEnd->setValue(Preferences::workDayEnd());
	QBitArray days = Preferences::workDays();
	for (i = 0;  i < 7;  ++i)
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
	QString hol = mHolidays->currentIndex() ? mHolidayNames[mHolidays->currentText()] : QString();
	if (hol != Preferences::holidays().location())
		Preferences::setHolidayRegion(hol);
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
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "New Alarms && Templates"), topWidget());
	QButtonGroup* bgroup = new QButtonGroup(group);
	QBoxLayout* layout = new QVBoxLayout(group);
	layout->setMargin(KDialog::marginHint());
	layout->setSpacing(KDialog::spacingHint());

	mDefaultResource = new QRadioButton(i18nc("@option:radio", "Store in default resource"), group);
	bgroup->addButton(mDefaultResource);
	mDefaultResource->setWhatsThis(i18nc("@info:whatsthis", "Add all new alarms and alarm templates to the default resources, without prompting."));
	layout->addWidget(mDefaultResource, 0, Qt::AlignLeft);
	mAskResource = new QRadioButton(i18nc("@option:radio", "Prompt for which resource to store in"), group);
	bgroup->addButton(mAskResource);
	mAskResource->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>When saving a new alarm or alarm template, prompt for which resource to store it in, if there is more than one active resource.</para>"
	      "<para>Note that archived alarms are always stored in the default archived alarm resource.</para>"));
	layout->addWidget(mAskResource, 0, Qt::AlignLeft);

	// Archived alarms
	group = new QGroupBox(i18nc("@title:group", "Archived Alarms"), topWidget());
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	mKeepArchived = new QCheckBox(i18nc("@option:check", "Keep alarms after expiry"), group);
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

	topLayout()->addStretch();    // top adjust the widgets
}

void StorePrefTab::restore(bool defaults)
{
	mCheckKeepChanges = defaults;
	mAskResource->setChecked(Preferences::askResource());
	int keepDays = Preferences::archivedKeepDays();
	if (!defaults)
		mOldKeepArchived = keepDays;
	setArchivedControls(keepDays);
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
		KMessageBox::sorry(topWidget(),
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
	if (KMessageBox::warningContinueCancel(topWidget(), single ? i18nc("@info", "Do you really want to delete all archived alarms?")
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
	KHBox* box = new KHBox(topWidget());
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

	box = new KHBox(topWidget());   // this is to allow left adjustment
	box->setMargin(0);
	mEmailCopyToKMail = new QCheckBox(i18nc("@option:check", "Copy sent emails into <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()), box);
	mEmailCopyToKMail->setWhatsThis(i18nc("@info:whatsthis", "After sending an email, store a copy in <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	box = new KHBox(topWidget());   // this is to allow left adjustment
	box->setMargin(0);
	mEmailQueuedNotify = new QCheckBox(i18nc("@option:check", "Notify when remote emails are queued"), box);
	mEmailQueuedNotify->setWhatsThis(
	      i18nc("@info:whatsthis", "Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
	           "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Your Email Address group box
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Your Email Address"), topWidget());
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(2, 1);

	// 'From' email address controls ...
	label = new Label(i18nc("@label 'From' email address", "From:"), group);
	grid->addWidget(label, 1, 0);
	mFromAddressGroup = new ButtonGroup(group);
	connect(mFromAddressGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotFromAddrChanged(QAbstractButton*)));

	// Line edit to enter a 'From' email address
	mFromAddrButton = new RadioButton(group);
	mFromAddressGroup->addButton(mFromAddrButton, Preferences::MAIL_FROM_ADDR);
	label->setBuddy(mFromAddrButton);
	grid->addWidget(mFromAddrButton, 1, 1);
	mEmailAddress = new KLineEdit(group);
	connect(mEmailAddress, SIGNAL(textChanged(const QString&)), SLOT(slotAddressChanged()));
	QString whatsThis = i18nc("@info:whatsthis", "Your email address, used to identify you as the sender when sending email alarms.");
	mFromAddrButton->setWhatsThis(whatsThis);
	mEmailAddress->setWhatsThis(whatsThis);
	mFromAddrButton->setFocusWidget(mEmailAddress);
	grid->addWidget(mEmailAddress, 1, 2);

	// 'From' email address to be taken from System Settings
	mFromCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from System Settings"), group);
	mFromAddressGroup->addButton(mFromCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
	mFromCCentreButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use the email address set in KDE System Settings, to identify you as the sender when sending email alarms."));
	grid->addWidget(mFromCCentreButton, 2, 1, 1, 2, Qt::AlignLeft);

	// 'From' email address to be picked from KMail's identities when the email alarm is configured
	mFromKMailButton = new RadioButton(i18nc("@option:radio", "Use <application>KMail</application> identities"), group);
	mFromAddressGroup->addButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
	mFromKMailButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use <application>KMail</application>'s email identities to identify you as the sender when sending email alarms. "
	           "For existing email alarms, <application>KMail</application>'s default identity will be used. "
	           "For new email alarms, you will be able to pick which of <application>KMail</application>'s identities to use."));
	grid->addWidget(mFromKMailButton, 3, 1, 1, 2, Qt::AlignLeft);

	// 'Bcc' email address controls ...
	grid->setRowMinimumHeight(4, KDialog::spacingHint());
	label = new Label(i18nc("@label 'Bcc' email address", "Bcc:"), group);
	grid->addWidget(label, 5, 0);
	mBccAddressGroup = new ButtonGroup(group);
	connect(mBccAddressGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotBccAddrChanged(QAbstractButton*)));

	// Line edit to enter a 'Bcc' email address
	mBccAddrButton = new RadioButton(group);
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

	// 'Bcc' email address to be taken from System Settings
	mBccCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from System Settings"), group);
	mBccAddressGroup->addButton(mBccCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
	mBccCCentreButton->setWhatsThis(
	      i18nc("@info:whatsthis", "Check to use the email address set in KDE System Settings, for blind copying email alarms to yourself."));
	grid->addWidget(mBccCCentreButton, 6, 1, 1, 2, Qt::AlignLeft);

	group->setFixedHeight(group->sizeHint().height());

	topLayout()->addStretch();    // top adjust the widgets
}

void EmailPrefTab::restore(bool defaults)
{
	mEmailClient->setButton(Preferences::emailClient());
	mEmailCopyToKMail->setChecked(Preferences::emailCopyToKMail());
	setEmailAddress(Preferences::emailFrom(), Preferences::emailAddress());
	setEmailBccAddress((Preferences::emailBccFrom() == Preferences::MAIL_FROM_SYS_SETTINGS), Preferences::emailBccAddress());
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
	if ((from >= 0  &&  static_cast<Preferences::MailFrom>(from) != Preferences::emailFrom())
	||  text != Preferences::emailAddress())
		Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(from), text);
	b = (mBccAddressGroup->checkedButton() == mBccCCentreButton);
	Preferences::MailFrom bfrom = b ? Preferences::MAIL_FROM_SYS_SETTINGS : Preferences::MAIL_FROM_ADDR;;
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

void EmailPrefTab::setEmailBccAddress(bool useSystemSettings, const QString& address)
{
	mBccAddressGroup->setButton(useSystemSettings ? Preferences::MAIL_FROM_SYS_SETTINGS : Preferences::MAIL_FROM_ADDR);
	mEmailBccAddress->setText(useSystemSettings ? QString() : address.trimmed());
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
		case Preferences::MAIL_FROM_SYS_SETTINGS:
			if (!KAMail::controlCentreAddress().isEmpty())
				return QString();
			errmsg = i18nc("@info", "No email address is currently set in KDE System Settings. %1", errmsg);
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
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab()
	: PrefsTabBase()
{
#define DEFSETTING "The default setting for <interface>%1</interface> in the alarm edit dialog."

	KTabWidget* tabs = new KTabWidget(topWidget());
	KVBox* topGeneral = new KVBox();
	topGeneral->setMargin(KDialog::marginHint()/2);
	topGeneral->setSpacing(KDialog::spacingHint());
	tabs->addTab(topGeneral, i18nc("@title:tab", "General"));
	KVBox* topTypes = new KVBox();
	topTypes->setMargin(KDialog::marginHint()/2);
	topTypes->setSpacing(KDialog::spacingHint());
	tabs->addTab(topTypes, i18nc("@title:tab", "Alarm Types"));
	KVBox* topFontColour = new KVBox();
	topFontColour->setMargin(KDialog::marginHint()/2);
	topFontColour->setSpacing(KDialog::spacingHint());
	tabs->addTab(topFontColour, i18nc("@title:tab", "Font & Color"));

	// MISCELLANEOUS
	// Show in KOrganizer
	mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_chk_ShowInKOrganizer(), topGeneral);
	mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
	mCopyToKOrganizer->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditAlarmDlg::i18n_chk_ShowInKOrganizer()));

	// Late cancellation
	KHBox* box = new KHBox(topGeneral);
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mLateCancel = new QCheckBox(LateCancelSelector::i18n_chk_CancelIfLate(), box);
	mLateCancel->setMinimumSize(mLateCancel->sizeHint());
	mLateCancel->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, LateCancelSelector::i18n_chk_CancelIfLate()));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	// Recurrence
	QFrame* iBox = new QFrame(topGeneral);   // this is to control the QWhatsThis text display area
	QHBoxLayout* hlayout = new QHBoxLayout(iBox);
	hlayout->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18nc("@label:listbox", "Recurrence:"), iBox);
	hlayout->addWidget(label);
	mRecurPeriod = new KComboBox(iBox);
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_NoRecur());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_AtLogin());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_HourlyMinutely());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Daily());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Weekly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Monthly());
	mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Yearly());
	mRecurPeriod->setFixedSize(mRecurPeriod->sizeHint());
	hlayout->addWidget(mRecurPeriod);
	label->setBuddy(mRecurPeriod);
	iBox->setWhatsThis(i18nc("@info:whatsthis", "The default setting for the recurrence rule in the alarm edit dialog."));
	hlayout->addStretch();    // left adjust the control

	// How to handle February 29th in yearly recurrences
	KVBox* vbox = new KVBox(topGeneral);   // this is to control the QWhatsThis text display area
	vbox->setMargin(0);
	vbox->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18nc("@label", "In non-leap years, repeat yearly February 29th alarms on:"), vbox);
	label->setAlignment(Qt::AlignLeft);
	label->setWordWrap(true);
	KHBox* itemBox = new KHBox(vbox);
//	itemBox->setMargin(0);
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

	QVBoxLayout* lay = qobject_cast<QVBoxLayout*>(topGeneral->layout());
	if (lay)
		lay->addStretch();    // top adjust the widgets

	// DISPLAY ALARMS
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Display Alarms"), topTypes);
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

	box = new KHBox(group);
//	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	label = new QLabel(i18nc("@label:listbox", "Reminder units:"), box);
	mReminderUnits = new KComboBox(box);
	mReminderUnits->addItem(i18nc("@item:inlistbox", "Minutes"), TimePeriod::Minutes);
	mReminderUnits->addItem(i18nc("@item:inlistbox", "Hours/Minutes"), TimePeriod::HoursMinutes);
	mReminderUnits->setFixedSize(mReminderUnits->sizeHint());
	label->setBuddy(mReminderUnits);
	box->setWhatsThis(i18nc("@info:whatsthis", "The default units for the reminder in the alarm edit dialog, for alarms due soon."));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control

	mSpecialActionsButton = new SpecialActionsButton(true, box);
	mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());

	// SOUND
	QGroupBox* bbox = new QGroupBox(i18nc("@title:group Audio options group", "Sound"), topTypes);
	vlayout = new QVBoxLayout(bbox);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	hlayout = new QHBoxLayout();
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
	mSoundFile = new KLineEdit(box);
	mSoundFileLabel->setBuddy(mSoundFile);
	mSoundFileBrowse = new QPushButton(box);
	mSoundFileBrowse->setIcon(KIcon(SmallIcon("document-open")));
	int size = mSoundFileBrowse->sizeHint().height();
	mSoundFileBrowse->setFixedSize(size, size);
	connect(mSoundFileBrowse, SIGNAL(clicked()), SLOT(slotBrowseSoundFile()));
	mSoundFileBrowse->setToolTip(i18nc("@info:tooltip", "Choose a sound file"));
	box->setWhatsThis(i18nc("@info:whatsthis", "Enter the default sound file to use in the alarm edit dialog."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box);
	bbox->setFixedHeight(bbox->sizeHint().height());

	// COMMAND ALARMS
	group = new QGroupBox(i18nc("@title:group", "Command Alarms"), topTypes);
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
	group = new QGroupBox(i18nc("@title:group", "Email Alarms"), topTypes);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	// BCC email to sender
	mEmailBcc = new QCheckBox(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf(), group);
	mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(i18nc("@info:whatsthis", DEFSETTING, EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf()));
	vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

	lay = qobject_cast<QVBoxLayout*>(topTypes->layout());
	if (lay)
		lay->addStretch();    // top adjust the widgets

	// FONT / COLOUR TAB
	mFontChooser = new FontColourChooser(topFontColour, QStringList(), i18nc("@title:group", "Message Font && Color"), true);
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
	mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction(), Preferences::defaultCancelOnPreActionError());
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
	mFontChooser->setFgColour(Preferences::defaultFgColour());
	mFontChooser->setBgColour(Preferences::defaultBgColour());
	mFontChooser->setFont(Preferences::messageFont());
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
	b = mSpecialActionsButton->cancelOnError();
	if (b != Preferences::defaultCancelOnPreActionError())
		Preferences::setDefaultCancelOnPreActionError(b);
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
	QColor colour = mFontChooser->fgColour();
	if (colour != Preferences::defaultFgColour())
		Preferences::setDefaultFgColour(colour);
	colour = mFontChooser->bgColour();
	if (colour != Preferences::defaultBgColour())
		Preferences::setDefaultBgColour(colour);
	QFont font = mFontChooser->font();
	if (font != Preferences::messageFont())
		Preferences::setMessageFont(font);
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
	KTabWidget* tabs = new KTabWidget(topWidget());
	KVBox* topGeneral = new KVBox();
	topGeneral->setMargin(KDialog::marginHint()/2);
	topGeneral->setSpacing(KDialog::spacingHint());
	tabs->addTab(topGeneral, i18nc("@title:tab", "General"));
	KVBox* topWindows = new KVBox();
	topWindows->setMargin(KDialog::marginHint()/2);
	topWindows->setSpacing(KDialog::spacingHint());
	tabs->addTab(topWindows, i18nc("@title:tab", "Alarm Windows"));

	// Run-in-system-tray radio button
	KHBox* box = new KHBox(topGeneral);   // this is to allow left adjustment
	box->setMargin(0);
	mShowInSystemTray = new QCheckBox(i18nc("@option:check", "Show in system tray"), box);
	mShowInSystemTray->setWhatsThis(
	      i18nc("@info:whatsthis", "<para>Check to show <application>KAlarm</application>'s icon in the system tray."
	           " Showing it in the system tray provides easy access and a status indication.</para>"));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	QGroupBox* group = new QGroupBox(i18nc("@title:group", "System Tray Tooltip"), topGeneral);
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

	box = new KHBox(group);
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
	mTooltipTimeToPrefix = new KLineEdit(box);
	mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
	box->setWhatsThis(i18nc("@info:whatsthis", "Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip."));
	box->setFixedHeight(box->sizeHint().height());
	grid->addWidget(box, 4, 2, Qt::AlignLeft);
	group->setMaximumHeight(group->sizeHint().height());

	group = new QGroupBox(i18nc("@title:group", "Alarm List"), topGeneral);
	QHBoxLayout* hlayout = new QHBoxLayout(group);
	hlayout->setMargin(KDialog::marginHint());
	QVBoxLayout* colourLayout = new QVBoxLayout();
	colourLayout->setMargin(0);
	hlayout->addLayout(colourLayout);

	box = new KHBox(group);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint()/2);
	colourLayout->addWidget(box);
	QLabel* label1 = new QLabel(i18nc("@label:listbox", "Disabled alarm color:"), box);
	box->setStretchFactor(new QWidget(box), 0);
	mDisabledColour = new ColourButton(box);
	label1->setBuddy(mDisabledColour);
	box->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for disabled alarms."));

	box = new KHBox(group);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint()/2);
	colourLayout->addWidget(box);
	QLabel* label2 = new QLabel(i18nc("@label:listbox", "Archived alarm color:"), box);
	box->setStretchFactor(new QWidget(box), 0);
	mArchivedColour = new ColourButton(box);
	label2->setBuddy(mArchivedColour);
	box->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for archived alarms."));
	hlayout->addStretch();

	QVBoxLayout* lay = qobject_cast<QVBoxLayout*>(topGeneral->layout());
	if (lay)
		lay->addStretch();    // top adjust the widgets

	group = new QGroupBox(i18nc("@title:group", "Alarm Message Windows"), topWindows);
	grid = new QGridLayout(group);
	grid->setMargin(KDialog::marginHint());
	grid->setSpacing(KDialog::spacingHint());
	grid->setColumnStretch(1, 1);
	grid->setColumnMinimumWidth(0, indentWidth());
	mWindowPosition = new ButtonGroup(group);
	connect(mWindowPosition, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotWindowPosChanged(QAbstractButton*)));

	QString whatsthis = i18nc("@info:whatsthis",
	      "<para>Choose how to reduce the chance of alarm messages being accidentally acknowledged:"
	      "<list><item>Position alarm message windows as far as possible from the current mouse cursor location, or</item>"
	      "<item>Position alarm message windows in the center of the screen, but disable buttons for a short time after the window is displayed.</item></list></para>");
	QRadioButton* radio = new QRadioButton(i18nc("@option:radio", "Position windows far from mouse cursor"), group);
	mWindowPosition->addButton(radio, 0);
	radio->setWhatsThis(whatsthis);
	grid->addWidget(radio, 0, 0, 1, 2, Qt::AlignLeft);
	radio = new QRadioButton(i18nc("@option:radio", "Center windows, delay activating window buttons"), group);
	mWindowPosition->addButton(radio, 1);
	radio->setWhatsThis(whatsthis);
	grid->addWidget(radio, 1, 0, 1, 2, Qt::AlignLeft);

	KHBox* itemBox = new KHBox(group);
	itemBox->setMargin(0);
	box = new KHBox(itemBox);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	mWindowButtonDelayLabel = new QLabel(i18nc("@label:spinbox", "Button activation delay (seconds):"), box);
	mWindowButtonDelay = new QSpinBox(box);
	mWindowButtonDelay->setRange(1, 10);
	mWindowButtonDelayLabel->setBuddy(mWindowButtonDelay);
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "Enter how long its buttons should remain disabled after the alarm message window is shown."));
	itemBox->setStretchFactor(new QWidget(itemBox), 1);    // left adjust the controls
	grid->addWidget(itemBox, 2, 1, Qt::AlignLeft);

	grid->setRowMinimumHeight(3, KDialog::spacingHint());

	mModalMessages = new QCheckBox(i18nc("@option:check", "Message windows have a title bar and take keyboard focus"), group);
	mModalMessages->setMinimumSize(mModalMessages->sizeHint());
	mModalMessages->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>Specify the characteristics of alarm message windows:"
	      "<list><item>If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.</item>"
	      "<item>If unchecked, the window does not interfere with your typing when "
	      "it is displayed, but it has no title bar and cannot be moved or resized.</item></list></para>"));
	grid->addWidget(mModalMessages, 4, 0, 1, 2, Qt::AlignLeft);

	lay = qobject_cast<QVBoxLayout*>(topWindows->layout());
	if (lay)
		lay->addStretch();    // top adjust the widgets
}

void ViewPrefTab::restore(bool)
{
	mDisabledColour->setColor(Preferences::disabledColour());
	mArchivedColour->setColor(Preferences::archivedColour());
	setTooltip(Preferences::tooltipAlarmCount(),
	           Preferences::showTooltipAlarmTime(),
	           Preferences::showTooltipTimeToAlarm(),
	           Preferences::tooltipTimeToPrefix());
	mShowInSystemTray->setChecked(Preferences::showInSystemTray());
	mWindowPosition->setButton(Preferences::messageButtonDelay() ? 1 : 0);
	mWindowButtonDelay->setValue(Preferences::messageButtonDelay());
	mModalMessages->setChecked(Preferences::modalMessages());
}

void ViewPrefTab::apply(bool syncToDisc)
{
	QColor colour = mDisabledColour->color();
	if (colour != Preferences::disabledColour())
		Preferences::setDisabledColour(colour);
	colour = mArchivedColour->color();
	if (colour != Preferences::archivedColour())
		Preferences::setArchivedColour(colour);
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
	b = mShowInSystemTray->isChecked();
	if (b != Preferences::showInSystemTray())
		Preferences::setShowInSystemTray(b);
	n = mWindowPosition->selectedId();
	if (n)
		n = mWindowButtonDelay->value();
	if (n != Preferences::messageButtonDelay())
		Preferences::setMessageButtonDelay(n);
	b = mModalMessages->isChecked();
	if (b != Preferences::modalMessages())
		Preferences::setModalMessages(b);
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

void ViewPrefTab::slotWindowPosChanged(QAbstractButton* button)
{
	bool enable = mWindowPosition->id(button);
	mWindowButtonDelay->setEnabled(enable);
	mWindowButtonDelayLabel->setEnabled(enable);
}
