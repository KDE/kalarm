/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <limits.h>

#include <QLabel>
#include <QDir>
#include <QStyle>
#include <QFrame>
#include <QGroupBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <khbox.h>
#include <kvbox.h>
#include <kurlcompletion.h>
#include <kwindowsystem.h>
#include <kstandarddirs.h>
#include <KStandardGuiItem>
#include <kabc/addresseedialog.h>
#include <kdebug.h>

#include <libkdepim/maillistdrag.h>
#include <libkdepim/kvcarddrag.h>
#include <kcal/icaldrag.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "checkbox.h"
#include "colourcombo.h"
#include "deferdlg.h"
#include "emailidcombo.h"
#include "fontcolourbutton.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "latecancel.h"
#include "lineedit.h"
#include "mainwindow.h"
#include "pickfileradio.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#include "reminder.h"
#include "repetition.h"
#include "shellprocess.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "spinbox.h"
#include "templatepickdlg.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "editdlg.moc"
#include "editdlgprivate.moc"

using namespace KCal;

static const char EDIT_DIALOG_NAME[] = "EditDialog";
static const int  maxDelayTime = 99*60 + 59;    // < 100 hours

/*=============================================================================
= Class PickAlarmFileRadio
=============================================================================*/
class PickAlarmFileRadio : public PickFileRadio
{
    public:
	PickAlarmFileRadio(const QString& text, ButtonGroup* group, QWidget* parent)
		: PickFileRadio(text, group, parent) { }
	virtual QString pickFile()    // called when browse button is pressed to select a file to display
	{
		return KAlarm::browseFile(i18n("Choose Text or Image File to Display"), mDefaultDir, fileEdit()->text(),
		                          QString(), KFile::ExistingOnly, parentWidget());
	}
    private:
	QString mDefaultDir;   // default directory for file browse button
};

/*=============================================================================
= Class PickLogFileRadio
=============================================================================*/
class PickLogFileRadio : public PickFileRadio
{
    public:
	PickLogFileRadio(QPushButton* b, LineEdit* e, const QString& text, ButtonGroup* group, QWidget* parent)
		: PickFileRadio(b, e, text, group, parent) { }
	virtual QString pickFile()    // called when browse button is pressed to select a log file
	{
		return KAlarm::browseFile(i18n("Choose Log File"), mDefaultDir, fileEdit()->text(), QString(),
		                          KFile::LocalOnly, parentWidget());
	}
    private:
	QString mDefaultDir;   // default directory for log file browse button
};

inline QString recurText(const KAEvent& event)
{
	return QString::fromLatin1("%1 / %2").arg(event.recurrenceText()).arg(event.repetitionText());
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_ConfirmAck()         { return i18n("Confirm acknowledgment"); }
QString EditAlarmDlg::i18n_SpecialActions()     { return i18n("Special Actions..."); }
QString EditAlarmDlg::i18n_ShowInKOrganizer()   { return i18n("Show in KOrganizer"); }
QString EditAlarmDlg::i18n_EnterScript()        { return i18n("Enter a script"); }
QString EditAlarmDlg::i18n_p_EnterScript()      { return i18n("Enter a scri&pt"); }
QString EditAlarmDlg::i18n_ExecInTermWindow()   { return i18n("Execute in terminal window"); }
QString EditAlarmDlg::i18n_w_ExecInTermWindow() { return i18n("Execute in terminal &window"); }
QString EditAlarmDlg::i18n_u_ExecInTermWindow() { return i18n("Exec&ute in terminal window"); }
QString EditAlarmDlg::i18n_g_LogToFile()        { return i18n("Lo&g to file"); }
QString EditAlarmDlg::i18n_CopyEmailToSelf()    { return i18n("Copy email to self"); }
QString EditAlarmDlg::i18n_e_CopyEmailToSelf()  { return i18n("Copy &email to self"); }
QString EditAlarmDlg::i18n_s_CopyEmailToSelf()  { return i18n("Copy email to &self"); }
QString EditAlarmDlg::i18n_EmailFrom()          { return i18nc("'From' email address", "From:"); }
QString EditAlarmDlg::i18n_f_EmailFrom()        { return i18nc("'From' email address", "&From:"); }
QString EditAlarmDlg::i18n_EmailTo()            { return i18nc("Email addressee", "To:"); }
QString EditAlarmDlg::i18n_EmailSubject()       { return i18nc("Email subject", "Subject:"); }
QString EditAlarmDlg::i18n_j_EmailSubject()     { return i18nc("Email subject", "Sub&ject:"); }


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialogue to show the specified event's data.
*/
EditAlarmDlg::EditAlarmDlg(bool Template, const QString& caption, QWidget* parent, const KAEvent* event,
                           GetResourceType getResource, bool readOnly)
	: KDialog(parent),
	  mMainPageShown(false),
	  mRecurPageShown(false),
	  mRecurSetDefaultEndDate(true),
	  mTemplateName(0),
	  mSpecialActionsButton(0),
	  mReminderDeferral(false),
	  mReminderArchived(false),
	  mEmailRemoveButton(0),
	  mDeferGroup(0),
	  mTimeWidget(0),
	  mShowInKorganizer(0),
	  mResource(0),
	  mDeferGroupHeight(0),
	  mTemplate(Template),
	  mDesiredReadOnly(readOnly),
	  mReadOnly(readOnly),
	  mSavedEvent(0)
{
	setObjectName(mTemplate ? "TemplEditDlg" : "EditDlg");    // used by LikeBack
	setCaption(caption);
	setButtons((readOnly ? Cancel|Try : Template ? Ok|Cancel|Try : Ok|Cancel|Try|Default));
	setDefaultButton(readOnly ? Cancel : Ok);
	setButtonText(Default, i18n("Load Template..."));
	connect(this, SIGNAL(tryClicked()), SLOT(slotTry()));
	connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
	switch (getResource)
	{
		case RES_USE_EVENT_ID:
			if (event)
			{
				mResourceEventId = event->id();
				break;
			}
			// fall through to RES_PROMPT
		case RES_PROMPT:
			mResourceEventId = QString("");   // empty but non-null
			break;
		case RES_IGNORE:
		default:
			mResourceEventId.clear();
			break;
	}
	KVBox* mainWidget = new KVBox(this);
	mainWidget->setMargin(0);
	setMainWidget(mainWidget);
	if (mTemplate)
	{
		KHBox* box = new KHBox(mainWidget);
		box->setMargin(0);
		box->setSpacing(spacingHint());
		QLabel* label = new QLabel(i18n("Template name:"), box);
		label->setFixedSize(label->sizeHint());
		mTemplateName = new QLineEdit(box);
		mTemplateName->setReadOnly(mReadOnly);
		label->setBuddy(mTemplateName);
		box->setWhatsThis(i18n("Enter the name of the alarm template"));
		box->setFixedHeight(box->sizeHint().height());
	}
	mTabs = new QTabWidget(mainWidget);
//	mTabs->setMargin(marginHint());

	KVBox* mainPageBox = new KVBox;
	mainPageBox->setMargin(marginHint());
	mTabs->addTab(mainPageBox, i18n("&Alarm"));
	mMainPageIndex = 0;
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacingHint());

	// Recurrence tab
	KVBox* recurTab = new KVBox;
	recurTab->setMargin(marginHint());
	mTabs->addTab(recurTab, i18n("&Recurrence"));
	mRecurPageIndex = 1;
	mRecurrenceEdit = new RecurrenceEdit(readOnly, recurTab);
	connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));
	connect(mRecurrenceEdit, SIGNAL(frequencyChanged()), SLOT(slotRecurFrequencyChange()));

	// Alarm action

	QGroupBox* actionBox = new QGroupBox(i18n("Action"), mainPage);
	topLayout->addWidget(actionBox, 1);
	QBoxLayout* layout = new QVBoxLayout(actionBox);
	layout->setMargin(marginHint());
	layout->setSpacing(spacingHint());
	QGridLayout* grid = new QGridLayout();
	grid->setSpacing(spacingHint());
	layout->addLayout(grid);
	mActionGroup = new ButtonGroup(actionBox);
	connect(mActionGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotAlarmTypeChanged(QAbstractButton*)));

	// Message radio button
	mMessageRadio = new RadioButton(i18n("Te&xt"), actionBox);
	mMessageRadio->setFixedSize(mMessageRadio->sizeHint());
	mMessageRadio->setWhatsThis(i18n("If checked, the alarm will display a text message."));
	mActionGroup->addButton(mMessageRadio);
	grid->addWidget(mMessageRadio, 1, 0);
	grid->setColumnStretch(1, 1);

	// File radio button
	mFileRadio = new PickAlarmFileRadio(i18n("&File"), mActionGroup, actionBox);
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	mFileRadio->setWhatsThis(i18n("If checked, the alarm will display the contents of a text or image file."));
	mActionGroup->addButton(mFileRadio);
	grid->addWidget(mFileRadio, 1, 2);
	grid->setColumnStretch(3, 1);

	// Command radio button
	mCommandRadio = new RadioButton(i18n("Co&mmand"), actionBox);
	mCommandRadio->setFixedSize(mCommandRadio->sizeHint());
	mCommandRadio->setWhatsThis(i18n("If checked, the alarm will execute a shell command."));
	mActionGroup->addButton(mCommandRadio);
	grid->addWidget(mCommandRadio, 1, 4);
	grid->setColumnStretch(5, 1);

	// Email radio button
	mEmailRadio = new RadioButton(i18n("&Email"), actionBox);
	mEmailRadio->setFixedSize(mEmailRadio->sizeHint());
	mEmailRadio->setWhatsThis(i18n("If checked, the alarm will send an email."));
	mActionGroup->addButton(mEmailRadio);
	grid->addWidget(mEmailRadio, 1, 6);

	initDisplayAlarms(actionBox);
	layout->addWidget(mDisplayAlarmsFrame);
	initCommand(actionBox);
	layout->addWidget(mCommandFrame);
	initEmail(actionBox);
	layout->addWidget(mEmailFrame);

	// Deferred date/time: visible only for a deferred recurring event.
	mDeferGroup = new QGroupBox(i18n("Deferred Alarm"), mainPage);
	topLayout->addWidget(mDeferGroup);
	QHBoxLayout* hlayout = new QHBoxLayout(mDeferGroup);
	hlayout->setMargin(marginHint());
	hlayout->setSpacing(spacingHint());
	QLabel* label = new QLabel(i18n("Deferred to:"), mDeferGroup);
	label->setFixedSize(label->sizeHint());
	hlayout->addWidget(label);
	mDeferTimeLabel = new QLabel(mDeferGroup);
	hlayout->addWidget(mDeferTimeLabel);

	mDeferChangeButton = new QPushButton(i18n("C&hange..."), mDeferGroup);
	mDeferChangeButton->setFixedSize(mDeferChangeButton->sizeHint());
	connect(mDeferChangeButton, SIGNAL(clicked()), SLOT(slotEditDeferral()));
	mDeferChangeButton->setWhatsThis(i18n("Change the alarm's deferred time, or cancel the deferral"));
	hlayout->addWidget(mDeferChangeButton);
//??	mDeferGroup->addSpace(0);

	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	topLayout->addLayout(hlayout);

	// Date and time entry
	if (mTemplate)
	{
		QGroupBox* templateTimeBox = new QGroupBox(i18n("Time"), mainPage);
		hlayout->addWidget(templateTimeBox);
		grid = new QGridLayout(templateTimeBox);
		grid->setMargin(marginHint());
		grid->setSpacing(spacingHint());
		mTemplateTimeGroup = new ButtonGroup(templateTimeBox);
		connect(mTemplateTimeGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotTemplateTimeType(QAbstractButton*)));

		mTemplateDefaultTime = new RadioButton(i18n("&Default time"), templateTimeBox);
		mTemplateDefaultTime->setFixedSize(mTemplateDefaultTime->sizeHint());
		mTemplateDefaultTime->setReadOnly(mReadOnly);
		mTemplateDefaultTime->setWhatsThis(i18n("Do not specify a start time for alarms based on this template. "
		                                        "The normal default start time will be used."));
		mTemplateTimeGroup->addButton(mTemplateDefaultTime);
		grid->addWidget(mTemplateDefaultTime, 0, 0, Qt::AlignLeft);

		KHBox* box = new KHBox(templateTimeBox);
		box->setMargin(0);
		box->setSpacing(spacingHint());
		mTemplateUseTime = new RadioButton(i18n("Time:"), box);
		mTemplateUseTime->setFixedSize(mTemplateUseTime->sizeHint());
		mTemplateUseTime->setReadOnly(mReadOnly);
		mTemplateUseTime->setWhatsThis(i18n("Specify a start time for alarms based on this template."));
		mTemplateTimeGroup->addButton(mTemplateUseTime);
		mTemplateTime = new TimeEdit(box);
		mTemplateTime->setFixedSize(mTemplateTime->sizeHint());
		mTemplateTime->setReadOnly(mReadOnly);
		mTemplateTime->setWhatsThis(QString("%1\n\n%2").arg(i18n("Enter the start time for alarms based on this template."))
		                                               .arg(TimeSpinBox::shiftWhatsThis()));
		box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 0, 1, Qt::AlignLeft);

		mTemplateAnyTime = new RadioButton(i18n("An&y time"), templateTimeBox);
		mTemplateAnyTime->setFixedSize(mTemplateAnyTime->sizeHint());
		mTemplateAnyTime->setReadOnly(mReadOnly);
		mTemplateAnyTime->setWhatsThis(i18n("Set the '%1' option for alarms based on this template.", i18n("Any time")));
		mTemplateTimeGroup->addButton(mTemplateAnyTime);
		grid->addWidget(mTemplateAnyTime, 1, 0, Qt::AlignLeft);

		box = new KHBox(templateTimeBox);
		box->setMargin(0);
		box->setSpacing(spacingHint());
		mTemplateUseTimeAfter = new RadioButton(AlarmTimeWidget::i18n_w_TimeFromNow(), box);
		mTemplateUseTimeAfter->setFixedSize(mTemplateUseTimeAfter->sizeHint());
		mTemplateUseTimeAfter->setReadOnly(mReadOnly);
		mTemplateUseTimeAfter->setWhatsThis(i18n("Set alarms based on this template to start after the specified time "
		                                         "interval from when the alarm is created."));
		mTemplateTimeGroup->addButton(mTemplateUseTimeAfter);
		mTemplateTimeAfter = new TimeSpinBox(1, maxDelayTime, box);
		mTemplateTimeAfter->setValue(1439);
		mTemplateTimeAfter->setFixedSize(mTemplateTimeAfter->sizeHint());
		mTemplateTimeAfter->setReadOnly(mReadOnly);
		mTemplateTimeAfter->setWhatsThis(QString("%1\n\n%2").arg(AlarmTimeWidget::i18n_TimeAfterPeriod())
		                                                    .arg(TimeSpinBox::shiftWhatsThis()));
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 1, 1, Qt::AlignLeft);

		hlayout->addStretch();
	}
	else
	{
		mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME, mainPage);
		connect(mTimeWidget, SIGNAL(dateOnlyToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		topLayout->addWidget(mTimeWidget);
	}

	// Recurrence type display
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(2*spacingHint());
	topLayout->addLayout(hlayout);
	KHBox* box = new KHBox(mainPage);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(spacingHint());
	label = new QLabel(i18n("Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mRecurrenceText = new QLabel(box);
	box->setWhatsThis(i18n("How often the alarm recurs.\nThe times shown are those configured in the Recurrence tab and in the Simple Repetition dialog."));
	box->setFixedHeight(box->sizeHint().height());
	hlayout->addWidget(box);
	hlayout->addStretch();

	// Simple repetition button
	mSimpleRepetition = new RepetitionButton(i18n("Simple Repetition"), true, mainPage);
	mSimpleRepetition->setFixedSize(mSimpleRepetition->sizeHint());
	connect(mSimpleRepetition, SIGNAL(needsInitialisation()), SLOT(slotSetSimpleRepetition()));
	connect(mSimpleRepetition, SIGNAL(changed()), SLOT(slotRecurFrequencyChange()));
	mSimpleRepetition->setWhatsThis(i18n("Set up a simple, or additional, alarm repetition"));
	hlayout->addWidget(mSimpleRepetition);

	// Reminder
	static const QString reminderText = i18n("Enter how long in advance of the main alarm to display a reminder alarm.");
	mReminder = new Reminder(i18n("Rem&inder:"),
	                         i18n("Check to additionally display a reminder in advance of the main alarm time(s)."),
	                         QString("%1\n\n%2").arg(reminderText).arg(TimeSpinBox::shiftWhatsThis()),
	                         true, true, mainPage);
	mReminder->setFixedSize(mReminder->sizeHint());
	topLayout->addWidget(mReminder, 0, Qt::AlignLeft);

	// Late cancel selector - default = allow late display
	mLateCancel = new LateCancelSelector(true, mainPage);
	topLayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

	// Acknowledgement confirmation required - default = no confirmation
	hlayout = new QHBoxLayout();
	topLayout->addLayout(hlayout);
	mConfirmAck = createConfirmAckCheckbox(mainPage);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	hlayout->addWidget(mConfirmAck);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	if (theApp()->korganizerEnabled())
	{
		// Show in KOrganizer checkbox
		mShowInKorganizer = new CheckBox(i18n_ShowInKOrganizer(), mainPage);
		mShowInKorganizer->setFixedSize(mShowInKorganizer->sizeHint());
		mShowInKorganizer->setWhatsThis(i18n("Check to copy the alarm into KOrganizer's calendar"));
		hlayout->addWidget(mShowInKorganizer);
	}

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	// Initialise the state of all controls according to the specified event, if any
	initialise(event);
	if (mTemplateName)
		mTemplateName->setFocus();

	// Save the initial state of all controls so that we can later tell if they have changed
	saveState((event && (mTemplate || !event->isTemplate())) ? event : 0);

	// Note the current desktop so that the dialog can be shown on it.
	// If a main window is visible, the dialog will by KDE default always appear on its
	// desktop. If the user invokes the dialog via the system tray on a different desktop,
	// that can cause confusion.
	mDesktop = KWindowSystem::currentDesktop();
}

EditAlarmDlg::~EditAlarmDlg()
{
	delete mSavedEvent;
}

/******************************************************************************
 * Set up the dialog controls common to display alarms.
 */
void EditAlarmDlg::initDisplayAlarms(QWidget* parent)
{
	mDisplayAlarmsFrame = new QFrame(parent);
	QVBoxLayout* frameLayout = new QVBoxLayout(mDisplayAlarmsFrame);
	frameLayout->setMargin(0);
	frameLayout->setSpacing(spacingHint());

	// Text message edit box
	mTextMessageEdit = new TextEdit(mDisplayAlarmsFrame);
	mTextMessageEdit->setLineWrapMode(QTextEdit::NoWrap);
	mTextMessageEdit->setWhatsThis(i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new KHBox(mDisplayAlarmsFrame);
	mFileBox->setMargin(0);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(LineEdit::Url, mFileBox);
	mFileMessageEdit->setAcceptDrops(true);
	mFileMessageEdit->setWhatsThis(i18n("Enter the name or URL of a text or image file to display."));

	// File browse button
	mFileBrowseButton = new QPushButton(mFileBox);
	mFileBrowseButton->setIcon(SmallIcon("document-open"));
	mFileBrowseButton->setFixedSize(mFileBrowseButton->sizeHint());
	mFileBrowseButton->setToolTip(i18n("Choose a file"));
	mFileBrowseButton->setWhatsThis(i18n("Select a text or image file to display."));
	mFileRadio->init(mFileBrowseButton, mFileMessageEdit);

	// Colour choice drop-down list
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	frameLayout->addLayout(hlayout);
	KHBox* box;
	mBgColourChoose = createBgColourChooser(&box, mDisplayAlarmsFrame);
//	mBgColourChoose->setFixedSize(mBgColourChoose->sizeHint());
	connect(mBgColourChoose, SIGNAL(highlighted(const QColor&)), SLOT(slotBgColourSelected(const QColor&)));
	hlayout->addWidget(box);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	// Font and colour choice drop-down list
	mFontColourButton = new FontColourButton(mDisplayAlarmsFrame);
	mFontColourButton->setFixedSize(mFontColourButton->sizeHint());
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	hlayout->addWidget(mFontColourButton);

	// Sound checkbox and file selector
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	frameLayout->addLayout(hlayout);
	mSoundPicker = new SoundPicker(mDisplayAlarmsFrame);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	hlayout->addWidget(mSoundPicker);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(i18n_SpecialActions(), mDisplayAlarmsFrame);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		hlayout->addWidget(mSpecialActionsButton);
	}

	// Top-adjust the controls
	mFilePadding = new KHBox(mDisplayAlarmsFrame);
	mFilePadding->setMargin(0);
	frameLayout->addWidget(mFilePadding);
	frameLayout->setStretchFactor(mFilePadding, 1);
}

/******************************************************************************
 * Set up the command alarm dialog controls.
 */
void EditAlarmDlg::initCommand(QWidget* parent)
{
	mCommandFrame = new QFrame(parent);
	QVBoxLayout* frameLayout = new QVBoxLayout(mCommandFrame);
	frameLayout->setMargin(0);
	frameLayout->setSpacing(spacingHint());

	mCmdTypeScript = new CheckBox(i18n_p_EnterScript(), mCommandFrame);
	mCmdTypeScript->setFixedSize(mCmdTypeScript->sizeHint());
	connect(mCmdTypeScript, SIGNAL(toggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	mCmdTypeScript->setWhatsThis(i18n("Check to enter the contents of a script instead of a shell command line"));
	frameLayout->addWidget(mCmdTypeScript, 0, Qt::AlignLeft);

	mCmdCommandEdit = new LineEdit(LineEdit::Url, mCommandFrame);
	mCmdCommandEdit->setWhatsThis(i18n("Enter a shell command to execute."));
	frameLayout->addWidget(mCmdCommandEdit);

	mCmdScriptEdit = new TextEdit(mCommandFrame);
	mCmdScriptEdit->setWhatsThis(i18n("Enter the contents of a script to execute"));
	frameLayout->addWidget(mCmdScriptEdit);

	// What to do with command output

	QGroupBox* cmdOutputBox = new QGroupBox(i18n("Command Output"), mCommandFrame);
	frameLayout->addWidget(cmdOutputBox);
	QVBoxLayout* vlayout = new QVBoxLayout(cmdOutputBox);
	vlayout->setMargin(marginHint());
	vlayout->setSpacing(spacingHint());
	mCmdOutputGroup = new ButtonGroup(cmdOutputBox);

	// Execute in terminal window
	mCmdExecInTerm = new RadioButton(i18n_u_ExecInTermWindow(), cmdOutputBox);
	mCmdExecInTerm->setFixedSize(mCmdExecInTerm->sizeHint());
	mCmdExecInTerm->setWhatsThis(i18n("Check to execute the command in a terminal window"));
	mCmdOutputGroup->addButton(mCmdExecInTerm, Preferences::Log_Terminal);
	vlayout->addWidget(mCmdExecInTerm, 0, Qt::AlignLeft);

	// Log file name edit box
	KHBox* box = new KHBox(cmdOutputBox);
	box->setMargin(0);
#ifdef __GNUC__
#warning Check pixelMetric() / subRect()
#endif
//	(new QWidget(box))->setFixedWidth(mCmdExecInTerm->style()->subRect(QStyle::SR_RadioButtonIndicator, mCmdExecInTerm).width());   // indent the edit box
	(new QWidget(box))->setFixedWidth(mCmdExecInTerm->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth));   // indent the edit box
	mCmdLogFileEdit = new LineEdit(LineEdit::Url, box);
	mCmdLogFileEdit->setAcceptDrops(true);
	mCmdLogFileEdit->setWhatsThis(i18n("Enter the name or path of the log file."));

	// Log file browse button.
	// The file browser dialogue is activated by the PickLogFileRadio class.
	QPushButton* browseButton = new QPushButton(box);
	browseButton->setIcon(SmallIcon("document-open"));
	browseButton->setFixedSize(browseButton->sizeHint());
	browseButton->setToolTip(i18n("Choose a file"));
	browseButton->setWhatsThis(i18n("Select a log file."));

	// Log output to file
	mCmdLogToFile = new PickLogFileRadio(browseButton, mCmdLogFileEdit, i18n_g_LogToFile(), mCmdOutputGroup, cmdOutputBox);
	mCmdLogToFile->setFixedSize(mCmdLogToFile->sizeHint());
	mCmdLogToFile->setWhatsThis(i18n("Check to log the command output to a local file. The output will be appended to any existing contents of the file."));
	mCmdOutputGroup->addButton(mCmdLogToFile, Preferences::Log_File);
	vlayout->addWidget(mCmdLogToFile, 0, Qt::AlignLeft);
	vlayout->addWidget(box);

	// Discard output
	mCmdDiscardOutput = new RadioButton(i18n("Discard"), cmdOutputBox);
	mCmdDiscardOutput->setFixedSize(mCmdDiscardOutput->sizeHint());
	mCmdDiscardOutput->setWhatsThis(i18n("Check to discard command output."));
	mCmdOutputGroup->addButton(mCmdDiscardOutput, Preferences::Log_Discard);
	vlayout->addWidget(mCmdDiscardOutput, 0, Qt::AlignLeft);

	// Top-adjust the controls
	mCmdPadding = new KHBox(mCommandFrame);
	mCmdPadding->setMargin(0);
	frameLayout->addWidget(mCmdPadding);
	frameLayout->setStretchFactor(mCmdPadding, 1);
}

/******************************************************************************
 * Set up the email alarm dialog controls.
 */
void EditAlarmDlg::initEmail(QWidget* parent)
{
	mEmailFrame = new QFrame(parent);
	QVBoxLayout* frameLayout = new QVBoxLayout(mEmailFrame);
	frameLayout->setMargin(0);
	frameLayout->setSpacing(spacingHint());
	QGridLayout* grid = new QGridLayout();
	grid->setMargin(0);
	grid->setColumnStretch(1, 1);
	frameLayout->addLayout(grid);

	mEmailFromList = 0;
	if (Preferences::emailFrom() == Preferences::MAIL_FROM_KMAIL)
	{
		// Email sender identity
		QLabel* label = new QLabel(i18n_EmailFrom(), mEmailFrame);
		label->setFixedSize(label->sizeHint());
		grid->addWidget(label, 0, 0);

		mEmailFromList = new EmailIdCombo(KAMail::identityManager(), mEmailFrame);
		mEmailFromList->setMinimumSize(mEmailFromList->sizeHint());
		label->setBuddy(mEmailFromList);
		mEmailFromList->setWhatsThis(i18n("Your email identity, used to identify you as the sender when sending email alarms."));
		grid->addWidget(mEmailFromList, 0, 1, 1, 2);
	}

	// Email recipients
	QLabel* label = new QLabel(i18n_EmailTo(), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	mEmailToEdit = new LineEdit(LineEdit::Emails, mEmailFrame);
	mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
	mEmailToEdit->setWhatsThis(i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	                                "commas or semicolons."));
	grid->addWidget(mEmailToEdit, 1, 1);

	mEmailAddressButton = new QPushButton(mEmailFrame);
	mEmailAddressButton->setIcon(SmallIcon("help-contents"));
	mEmailAddressButton->setFixedSize(mEmailAddressButton->sizeHint());
	connect(mEmailAddressButton, SIGNAL(clicked()), SLOT(openAddressBook()));
	mEmailAddressButton->setToolTip(i18n("Open address book"));
	mEmailAddressButton->setWhatsThis(i18n("Select email addresses from your address book."));
	grid->addWidget(mEmailAddressButton, 1, 2);

	// Email subject
	label = new QLabel(i18n_j_EmailSubject(), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 2, 0);

	mEmailSubjectEdit = new LineEdit(mEmailFrame);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	label->setBuddy(mEmailSubjectEdit);
	mEmailSubjectEdit->setWhatsThis(i18n("Enter the email subject."));
	grid->addWidget(mEmailSubjectEdit, 2, 1, 1, 2);

	// Email body
	mEmailMessageEdit = new TextEdit(mEmailFrame);
	mEmailMessageEdit->setWhatsThis(i18n("Enter the email message."));
	frameLayout->addWidget(mEmailMessageEdit);

	// Email attachments
	grid = new QGridLayout();
	grid->setMargin(0);
	frameLayout->addLayout(grid);
	label = new QLabel(i18n("Attachment&s:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	mEmailAttachList = new QComboBox(mEmailFrame);
	mEmailAttachList->setEditable(true);
	mEmailAttachList->setMinimumSize(mEmailAttachList->sizeHint());
	if (mEmailAttachList->lineEdit())
		mEmailAttachList->lineEdit()->setReadOnly(true);
//Q3ListBox* list = mEmailAttachList->listBox();
//QRect rect = list->geometry();
//list->setGeometry(rect.left() - 50, rect.top(), rect.width(), rect.height());
	label->setBuddy(mEmailAttachList);
	mEmailAttachList->setWhatsThis(i18n("Files to send as attachments to the email."));
	grid->addWidget(mEmailAttachList, 0, 1);
	grid->setColumnStretch(1, 1);

	mEmailAddAttachButton = new QPushButton(i18n("Add..."), mEmailFrame);
	connect(mEmailAddAttachButton, SIGNAL(clicked()), SLOT(slotAddAttachment()));
	mEmailAddAttachButton->setWhatsThis(i18n("Add an attachment to the email."));
	grid->addWidget(mEmailAddAttachButton, 0, 2);

	mEmailRemoveButton = new QPushButton(i18n("Remo&ve"), mEmailFrame);
	connect(mEmailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
	mEmailRemoveButton->setWhatsThis(i18n("Remove the highlighted attachment from the email."));
	grid->addWidget(mEmailRemoveButton, 1, 2);

	// BCC email to sender
	mEmailBcc = new CheckBox(i18n_s_CopyEmailToSelf(), mEmailFrame);
	mEmailBcc->setFixedSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(i18n("If checked, the email will be blind copied to you."));
	grid->addWidget(mEmailBcc, 1, 0, 1, 2, Qt::AlignLeft);
}

/******************************************************************************
 * Initialise the dialogue controls from the specified event.
 */
void EditAlarmDlg::initialise(const KAEvent* event)
{
	mReadOnly = mDesiredReadOnly;
	if (!mTemplate  &&  event  &&  event->action() == KAEvent::COMMAND  &&  !ShellProcess::authorised())
		mReadOnly = true;     // don't allow editing of existing command alarms in kiosk mode
	setReadOnly();

	mChanged           = false;
	mOnlyDeferred      = false;
	mExpiredRecurrence = false;
	mKMailSerialNumber = 0;
	bool deferGroupVisible = false;
	if (event)
	{
		// Set the values to those for the specified event
		if (mTemplate)
			mTemplateName->setText(event->templateName());
		bool recurs = event->recurs();
		if ((recurs || event->repeatCount())  &&  !mTemplate  &&  event->deferred())
		{
			deferGroupVisible = true;
			mDeferDateTime = event->deferDateTime();
			mDeferTimeLabel->setText(mDeferDateTime.formatLocale());
			mDeferGroup->show();
		}
		if (event->defaultFont())
			mFontColourButton->setDefaultFont();
		else
			mFontColourButton->setFont(event->font());
		mFontColourButton->setBgColour(event->bgColour());
		mFontColourButton->setFgColour(event->fgColour());
		mBgColourChoose->setColour(event->bgColour());     // set colour before setting alarm type buttons
		if (mTemplate)
		{
			// Editing a template
			int afterTime = event->isTemplate() ? event->templateAfterTime() : -1;
			bool noTime   = !afterTime;
			bool useTime  = !event->mainDateTime().isDateOnly();
			RadioButton* button = noTime          ? mTemplateDefaultTime :
			                      (afterTime > 0) ? mTemplateUseTimeAfter :
			                      useTime         ? mTemplateUseTime : mTemplateAnyTime;
			button->setChecked(true);
			mTemplateTimeAfter->setValue(afterTime > 0 ? afterTime : 1);
			if (!noTime && useTime)
				mTemplateTime->setValue(KDateTime(event->mainDateTime()).time());
			else
				mTemplateTime->setValue(0);
		}
		else
		{
			if (event->isTemplate())
			{
				// Initialising from an alarm template: use current date
				KDateTime now = KDateTime::currentUtcDateTime();
				int afterTime = event->templateAfterTime();
				if (afterTime >= 0)
				{
					mTimeWidget->setDateTime(now.addSecs(afterTime * 60));
					mTimeWidget->selectTimeFromNow();
				}
				else
				{
					KDateTime dt = event->startDateTime();
					now = now.toTimeSpec(dt);
					QDate d = now.date();
					if (!dt.isDateOnly()  &&  now.time() >= dt.time())
						d = d.addDays(1);     // alarm time has already passed, so use tomorrow
					dt.setDate(d);
					mTimeWidget->setDateTime(dt);
				}
			}
			else
			{
				mExpiredRecurrence = recurs && event->mainExpired();
				mTimeWidget->setDateTime(!event->mainExpired() ? event->mainDateTime()
				                         : recurs ? DateTime() : event->deferDateTime());
			}
		}

		KAEvent::Action action = event->action();
		AlarmText altext;
		if (event->commandScript())
			altext.setScript(event->cleanText());
		else
			altext.setText(event->cleanText());
		setAction(action, altext);
		if (action == KAEvent::MESSAGE  &&  event->kmailSerialNumber()
		&&  AlarmText::checkIfEmail(event->cleanText()))
			mKMailSerialNumber = event->kmailSerialNumber();
		if (action == KAEvent::EMAIL)
			mEmailAttachList->addItems(event->emailAttachments());

		mLateCancel->setMinutes(event->lateCancel(), event->startDateTime().isDateOnly(),
		                        TimePeriod::HoursMinutes);
		mLateCancel->showAutoClose(action == KAEvent::MESSAGE || action == KAEvent::FILE);
		mLateCancel->setAutoClose(event->autoClose());
		mLateCancel->setFixedSize(mLateCancel->sizeHint());
		if (mShowInKorganizer)
			mShowInKorganizer->setChecked(event->copyToKOrganizer());
		mConfirmAck->setChecked(event->confirmAck());
		int reminder = event->reminder();
		if (!reminder  &&  event->reminderDeferral()  &&  !recurs)
		{
			reminder = event->reminderDeferral();
			mReminderDeferral = true;
		}
		if (!reminder  &&  event->reminderArchived()  &&  recurs)
		{
			reminder = event->reminderArchived();
			mReminderArchived = true;
		}
		mReminder->setMinutes(reminder, (mTimeWidget ? mTimeWidget->anyTime() : mTemplateAnyTime->isChecked()));
		mReminder->setOnceOnly(event->reminderOnceOnly());
		mReminder->enableOnceOnly(event->recurs());
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(event->preAction(), event->postAction());
		mSimpleRepetition->set(event->repeatInterval(), event->repeatCount());
		mRecurrenceText->setText(recurText(*event));
		mRecurrenceEdit->set(*event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		Preferences::SoundType soundType = event->speak()                ? Preferences::Sound_Speak
		                                 : event->beep()                 ? Preferences::Sound_Beep
		                                 : !event->audioFile().isEmpty() ? Preferences::Sound_File
		                                 :                                 Preferences::Sound_None;
		mSoundPicker->set(soundType, event->audioFile(), event->soundVolume(),
		                  event->fadeVolume(), event->fadeSeconds(), event->repeatSound());
		RadioButton* logType = event->commandXterm()       ? mCmdExecInTerm
		                     : !event->logFile().isEmpty() ? mCmdLogToFile
		                     :                               mCmdDiscardOutput;
		if (logType == mCmdLogToFile)
			mCmdLogFileEdit->setText(event->logFile());    // set file name before setting radio button
		logType->setChecked(true);
		mEmailToEdit->setText(event->emailAddresses(", "));
		mEmailSubjectEdit->setText(event->emailSubject());
		mEmailBcc->setChecked(event->emailBcc());
		if (mEmailFromList)
			mEmailFromList->setCurrentIdentity(event->emailFromKMail());
	}
	else
	{
		// Set the values to their defaults
		if (!ShellProcess::authorised())
		{
			// Don't allow shell commands in kiosk mode
			mCommandRadio->setEnabled(false);
			if (mSpecialActionsButton)
				mSpecialActionsButton->setEnabled(false);
		}
		mFontColourButton->setDefaultFont();
		mFontColourButton->setBgColour(Preferences::defaultBgColour());
		mFontColourButton->setFgColour(Preferences::defaultFgColour());
		mBgColourChoose->setColour(Preferences::defaultBgColour());     // set colour before setting alarm type buttons
		KDateTime defaultTime = KDateTime::currentUtcDateTime().addSecs(60).toTimeSpec(Preferences::timeZone());
		if (mTemplate)
		{
			mTemplateDefaultTime->setChecked(true);
			mTemplateTime->setValue(0);
			mTemplateTimeAfter->setValue(1);
		}
		else
			mTimeWidget->setDateTime(defaultTime);
		mMessageRadio->setChecked(true);
		mLateCancel->setMinutes((Preferences::defaultLateCancel() ? 1 : 0), false, TimePeriod::HoursMinutes);
		mLateCancel->showAutoClose(true);
		mLateCancel->setAutoClose(Preferences::defaultAutoClose());
		mLateCancel->setFixedSize(mLateCancel->sizeHint());
		if (mShowInKorganizer)
			mShowInKorganizer->setChecked(Preferences::defaultCopyToKOrganizer());
		mConfirmAck->setChecked(Preferences::defaultConfirmAck());
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction());
		mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		slotRecurFrequencyChange();      // update the Recurrence text
		mReminder->setMinutes(0, false);
		mReminder->enableOnceOnly(mRecurrenceEdit->isTimedRepeatType());   // must be called after mRecurrenceEdit is set up
		mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
		                  Preferences::defaultSoundVolume(), -1, 0, Preferences::defaultSoundRepeat());
		mCmdTypeScript->setChecked(Preferences::defaultCmdScript());
		mCmdLogFileEdit->setText(Preferences::defaultCmdLogFile());    // set file name before setting radio button
		mCmdOutputGroup->setButton(Preferences::defaultCmdLogType());
		mEmailBcc->setChecked(Preferences::defaultEmailBcc());
	}
	slotCmdScriptToggled(mCmdTypeScript->isChecked());

	if (!deferGroupVisible)
		mDeferGroup->hide();

	bool enable = !!mEmailAttachList->count();
	mEmailAttachList->setEnabled(enable);
	if (mEmailRemoveButton)
		mEmailRemoveButton->setEnabled(enable);
	bool empty = AlarmCalendar::resources()->events(KCalEvent::TEMPLATE).isEmpty();
	enableButton(Default, !empty);
}

/******************************************************************************
 * Set the read-only status of all non-template controls.
 */
void EditAlarmDlg::setReadOnly()
{
	// Common controls
	mMessageRadio->setReadOnly(mReadOnly);
	mFileRadio->setReadOnly(mReadOnly);
	mCommandRadio->setReadOnly(mReadOnly);
	mEmailRadio->setReadOnly(mReadOnly);
	if (mTimeWidget)
		mTimeWidget->setReadOnly(mReadOnly);
	mLateCancel->setReadOnly(mReadOnly);
	if (mReadOnly)
		mDeferChangeButton->hide();
	else
		mDeferChangeButton->show();
	mSimpleRepetition->setReadOnly(mReadOnly);
	if (mShowInKorganizer)
		mShowInKorganizer->setReadOnly(mReadOnly);

	// Message alarm controls
	mTextMessageEdit->setReadOnly(mReadOnly);
	mFileMessageEdit->setReadOnly(mReadOnly);
	mBgColourChoose->setReadOnly(mReadOnly);
	mFontColourButton->setReadOnly(mReadOnly);
	mSoundPicker->setReadOnly(mReadOnly);
	mConfirmAck->setReadOnly(mReadOnly);
	mReminder->setReadOnly(mReadOnly);
	if (mSpecialActionsButton)
		mSpecialActionsButton->setReadOnly(mReadOnly);
	if (mReadOnly)
	{
		mFileBrowseButton->hide();
		mFontColourButton->hide();
	}
	else
	{
		mFileBrowseButton->show();
		mFontColourButton->show();
	}

	// Command alarm controls
	mCmdTypeScript->setReadOnly(mReadOnly);
	mCmdCommandEdit->setReadOnly(mReadOnly);
	mCmdScriptEdit->setReadOnly(mReadOnly);
	mCmdExecInTerm->setReadOnly(mReadOnly);
	mCmdLogToFile->setReadOnly(mReadOnly);
	mCmdDiscardOutput->setReadOnly(mReadOnly);

	// Email alarm controls
	mEmailToEdit->setReadOnly(mReadOnly);
	mEmailSubjectEdit->setReadOnly(mReadOnly);
	mEmailMessageEdit->setReadOnly(mReadOnly);
	mEmailBcc->setReadOnly(mReadOnly);
	if (mEmailFromList)
		mEmailFromList->setReadOnly(mReadOnly);
	if (mReadOnly)
	{
		mEmailAddressButton->hide();
		mEmailAddAttachButton->hide();
		mEmailRemoveButton->hide();
	}
	else
	{
		mEmailAddressButton->show();
		mEmailAddAttachButton->show();
		mEmailRemoveButton->show();
	}
}

/******************************************************************************
 * Set the dialog's action and the action's text.
 */
void EditAlarmDlg::setAction(KAEvent::Action action, const AlarmText& alarmText)
{
	QString text = alarmText.displayText();
	bool script;
	QRadioButton* radio;
	switch (action)
	{
		case KAEvent::FILE:
			radio = mFileRadio;
			mFileMessageEdit->setText(text);
			break;
		case KAEvent::COMMAND:
			radio = mCommandRadio;
			script = alarmText.isScript();
			mCmdTypeScript->setChecked(script);
			if (script)
				mCmdScriptEdit->setPlainText(text);
			else
				mCmdCommandEdit->setText(text);
			break;
		case KAEvent::EMAIL:
			radio = mEmailRadio;
			mEmailMessageEdit->setPlainText(text);
			break;
		case KAEvent::MESSAGE:
		default:
			radio = mMessageRadio;
			mTextMessageEdit->setPlainText(text);
			mKMailSerialNumber = 0;
			if (alarmText.isEmail())
			{
				mKMailSerialNumber = alarmText.kmailSerialNumber();

				// Set up email fields also, in case the user wants an email alarm
				mEmailToEdit->setText(alarmText.to());
				mEmailSubjectEdit->setText(alarmText.subject());
				mEmailMessageEdit->setPlainText(alarmText.body());
			}
			else if (alarmText.isScript())
			{
				// Set up command script field also, in case the user wants a command alarm
				mCmdScriptEdit->setPlainText(text);
				mCmdTypeScript->setChecked(true);
			}
			break;
	}
	radio->setChecked(true);
}

/******************************************************************************
 * Create a widget to choose the alarm message background colour.
 */
ColourCombo* EditAlarmDlg::createBgColourChooser(KHBox** box, QWidget* parent)
{
	*box = new KHBox(parent);   // this is to control the QWhatsThis text display area
	(*box)->setMargin(0);
	QLabel* label = new QLabel(i18n("&Background color:"), *box);
	label->setFixedSize(label->sizeHint());
	ColourCombo* widget = new ColourCombo(*box);
	QSize size = widget->sizeHint();
	widget->setMinimumHeight(size.height() + 4);
	widget->setToolTip(i18n("Message color"));
	label->setBuddy(widget);
	(*box)->setFixedHeight((*box)->sizeHint().height());
	(*box)->setWhatsThis(i18n("Choose the background color for the alarm message."));
	return widget;
}

/******************************************************************************
 * Create an "acknowledgement confirmation required" checkbox.
 */
CheckBox* EditAlarmDlg::createConfirmAckCheckbox(QWidget* parent)
{
	CheckBox* widget = new CheckBox(i18n_ConfirmAck(), parent);
	widget->setWhatsThis(i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
	return widget;
}

/******************************************************************************
 * Save the state of all controls.
 */
void EditAlarmDlg::saveState(const KAEvent* event)
{
	delete mSavedEvent;
	mSavedEvent = 0;
	if (event)
		mSavedEvent = new KAEvent(*event);
	if (mTemplate)
	{
		mSavedTemplateName      = mTemplateName->text();
		mSavedTemplateTimeType  = mTemplateTimeGroup->checkedButton();
		mSavedTemplateTime      = mTemplateTime->time();
		mSavedTemplateAfterTime = mTemplateTimeAfter->value();
	}
	mSavedTypeRadio        = mActionGroup->checkedButton();
	mSavedSoundType        = mSoundPicker->sound();
	mSavedSoundFile        = mSoundPicker->file();
	mSavedSoundVolume      = mSoundPicker->volume(mSavedSoundFadeVolume, mSavedSoundFadeSeconds);
	mSavedRepeatSound      = mSoundPicker->repeat();
	mSavedConfirmAck       = mConfirmAck->isChecked();
	mSavedFont             = mFontColourButton->font();
	mSavedFgColour         = mFontColourButton->fgColour();
	mSavedBgColour         = mBgColourChoose->color();
	mSavedReminder         = mReminder->minutes();
	mSavedOnceOnly         = mReminder->isOnceOnly();
	if (mSpecialActionsButton)
	{
		mSavedPreAction  = mSpecialActionsButton->preAction();
		mSavedPostAction = mSpecialActionsButton->postAction();
	}
	checkText(mSavedTextFileCommandMessage, false);
	mSavedCmdScript        = mCmdTypeScript->isChecked();
	mSavedCmdOutputRadio   = mCmdOutputGroup->checkedButton();
	mSavedCmdLogFile       = mCmdLogFileEdit->text();
	if (mEmailFromList)
		mSavedEmailFrom = mEmailFromList->currentIdentityName();
	mSavedEmailTo          = mEmailToEdit->text();
	mSavedEmailSubject     = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
		mSavedEmailAttach += mEmailAttachList->itemText(i);
	mSavedEmailBcc         = mEmailBcc->isChecked();
	if (mTimeWidget)
		mSavedDateTime = mTimeWidget->getDateTime(0, false, false);
	mSavedLateCancel       = mLateCancel->minutes();
	mSavedAutoClose        = mLateCancel->isAutoClose();
	if (mShowInKorganizer)
		mSavedShowInKorganizer = mShowInKorganizer->isChecked();
	mSavedRecurrenceType   = mRecurrenceEdit->repeatType();
	mSavedRepeatInterval   = mSimpleRepetition->interval();
	mSavedRepeatCount      = mSimpleRepetition->count();
}

/******************************************************************************
 * Check whether any of the controls has changed state since the dialog was
 * first displayed.
 * Reply = true if any non-deferral controls have changed, or if it's a new event.
 *       = false if no non-deferral controls have changed. In this case,
 *         mOnlyDeferred indicates whether deferral controls may have changed.
 */
bool EditAlarmDlg::stateChanged() const
{
	mChanged      = true;
	mOnlyDeferred = false;
	if (!mSavedEvent)
		return true;
	QString textFileCommandMessage;
	checkText(textFileCommandMessage, false);
	if (mTemplate)
	{
		if (mSavedTemplateName     != mTemplateName->text()
		||  mSavedTemplateTimeType != mTemplateTimeGroup->checkedButton()
		||  mTemplateUseTime->isChecked()  &&  mSavedTemplateTime != mTemplateTime->time()
		||  mTemplateUseTimeAfter->isChecked()  &&  mSavedTemplateAfterTime != mTemplateTimeAfter->value())
			return true;
	}
	else
	{
		KDateTime dt = mTimeWidget->getDateTime(0, false, false);
		if (mSavedDateTime.timeSpec() != dt.timeSpec()  ||  mSavedDateTime != dt)
			return true;
	}
	if (mSavedTypeRadio        != mActionGroup->checkedButton()
	||  mSavedLateCancel       != mLateCancel->minutes()
	||  mShowInKorganizer && mSavedShowInKorganizer != mShowInKorganizer->isChecked()
	||  textFileCommandMessage != mSavedTextFileCommandMessage
	||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType()
	||  mSavedRepeatInterval   != mSimpleRepetition->interval()
	||  mSavedRepeatCount      != mSimpleRepetition->count())
		return true;
	if (mMessageRadio->isChecked()  ||  mFileRadio->isChecked())
	{
		if (mSavedSoundType  != mSoundPicker->sound()
		||  mSavedConfirmAck != mConfirmAck->isChecked()
		||  mSavedFont       != mFontColourButton->font()
		||  mSavedFgColour   != mFontColourButton->fgColour()
		||  mSavedBgColour   != mBgColourChoose->color()
		||  mSavedReminder   != mReminder->minutes()
		||  mSavedOnceOnly   != mReminder->isOnceOnly()
		||  mSavedAutoClose  != mLateCancel->isAutoClose())
			return true;
		if (mSpecialActionsButton)
		{
			if (mSavedPreAction  != mSpecialActionsButton->preAction()
			||  mSavedPostAction != mSpecialActionsButton->postAction())
				return true;
		}
		if (mSavedSoundType == Preferences::Sound_File)
		{
			if (mSavedSoundFile != mSoundPicker->file())
				return true;
			if (!mSavedSoundFile.isEmpty())
			{
				float fadeVolume;
				int   fadeSecs;
				if (mSavedRepeatSound != mSoundPicker->repeat()
				||  mSavedSoundVolume != mSoundPicker->volume(fadeVolume, fadeSecs)
				||  mSavedSoundFadeVolume != fadeVolume
				||  mSavedSoundFadeSeconds != fadeSecs)
					return true;
			}
		}
	}
	else if (mCommandRadio->isChecked())
	{
		if (mSavedCmdScript      != mCmdTypeScript->isChecked()
		||  mSavedCmdOutputRadio != mCmdOutputGroup->checkedButton())
			return true;
		if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
		{
			if (mSavedCmdLogFile != mCmdLogFileEdit->text())
				return true;
		}
	}
	else if (mEmailRadio->isChecked())
	{
		QStringList emailAttach;
		for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
			emailAttach += mEmailAttachList->itemText(i);
		if (mEmailFromList  &&  mSavedEmailFrom != mEmailFromList->currentIdentityName()
		||  mSavedEmailTo      != mEmailToEdit->text()
		||  mSavedEmailSubject != mEmailSubjectEdit->text()
		||  mSavedEmailAttach  != emailAttach
		||  mSavedEmailBcc     != mEmailBcc->isChecked())
			return true;
	}
	if (mRecurrenceEdit->stateChanged())
		return true;
	if (mSavedEvent  &&  mSavedEvent->deferred())
		mOnlyDeferred = true;
	mChanged = false;
	return false;
}

/******************************************************************************
 * Get the currently entered dialogue data.
 * The data is returned in the supplied KAEvent instance.
 * Reply = false if the only change has been to an existing deferral.
 */
bool EditAlarmDlg::getEvent(KAEvent& event, AlarmResource*& resource)
{
	resource = mResource;
	if (mChanged)
	{
		// It's a new event, or the edit controls have changed
		setEvent(event, mAlarmMessage, false);
		return true;
	}

	// Only the deferral time may have changed
	event = *mSavedEvent;
	if (mOnlyDeferred)
	{
		// Just modify the original event, to avoid expired recurring events
		// being returned as rubbish.
		if (mDeferDateTime.isValid())
			event.defer(mDeferDateTime, event.reminderDeferral(), false);
		else
			event.cancelDefer();
	}
	return false;
}

/******************************************************************************
*  Extract the data in the dialogue and set up a KAEvent from it.
*  If 'trial' is true, the event is set up for a simple one-off test, ignoring
*  recurrence, reminder, template etc. data.
*/
void EditAlarmDlg::setEvent(KAEvent& event, const QString& text, bool trial)
{
	KDateTime dt;
	if (!trial)
	{
		if (!mTemplate)
			dt = mAlarmDateTime.effectiveKDateTime();
		else if (mTemplateUseTime->isChecked())
			dt = KDateTime(QDate(2000,1,1), mTemplateTime->time());
	}
	KAEvent::Action type = getAlarmType();
	event.set(dt, text, mBgColourChoose->color(), mFontColourButton->fgColour(), mFontColourButton->font(),
	          type, (trial ? 0 : mLateCancel->minutes()), getAlarmFlags());
	switch (type)
	{
		case KAEvent::MESSAGE:
			if (AlarmText::checkIfEmail(text))
				event.setKMailSerialNumber(mKMailSerialNumber);
			// fall through to FILE
		case KAEvent::FILE:
		{
			float fadeVolume;
			int   fadeSecs;
			float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
			event.setAudioFile(mSoundPicker->file().prettyUrl(), volume, fadeVolume, fadeSecs);
			if (!trial)
				event.setReminder(mReminder->minutes(), mReminder->isOnceOnly());
			if (mSpecialActionsButton)
				event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction());
			break;
		}
		case KAEvent::EMAIL:
		{
			QString from;
			if (mEmailFromList)
				from = mEmailFromList->currentIdentityName();
			event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
			break;
		}
		case KAEvent::COMMAND:
			if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
				event.setLogFile(mCmdLogFileEdit->text());
			break;
		default:
			break;
	}
	if (!trial)
	{
		if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
		{
			mRecurrenceEdit->updateEvent(event, !mTemplate);
			mAlarmDateTime = event.startDateTime();
			if (mDeferDateTime.isValid()  &&  mDeferDateTime < mAlarmDateTime)
			{
				bool deferral = true;
				bool deferReminder = false;
				int reminder = mReminder->minutes();
				if (reminder)
				{
					DateTime remindTime = mAlarmDateTime.addMins(-reminder);
					if (mDeferDateTime >= remindTime)
					{
						if (remindTime > KDateTime::currentUtcDateTime())
							deferral = false;    // ignore deferral if it's after next reminder
						else if (mDeferDateTime > remindTime)
							deferReminder = true;    // it's the reminder which is being deferred
					}
				}
				if (deferral)
					event.defer(mDeferDateTime, deferReminder, false);
			}
		}
		if (mSimpleRepetition->count())
			event.setRepetition(mSimpleRepetition->interval(), mSimpleRepetition->count());
		if (mTemplate)
		{
			int afterTime = mTemplateDefaultTime->isChecked() ? 0
			              : mTemplateUseTimeAfter->isChecked() ? mTemplateTimeAfter->value() : -1;
			event.setTemplate(mTemplateName->text(), afterTime);
		}
	}
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	bool displayAlarm = mMessageRadio->isChecked() || mFileRadio->isChecked();
	bool cmdAlarm     = mCommandRadio->isChecked();
	bool emailAlarm   = mEmailRadio->isChecked();
	return (displayAlarm && mSoundPicker->sound() == Preferences::Sound_Beep     ? KAEvent::BEEP : 0)
	     | (displayAlarm && mSoundPicker->sound() == Preferences::Sound_Speak    ? KAEvent::SPEAK : 0)
	     | (displayAlarm && mSoundPicker->repeat()                               ? KAEvent::REPEAT_SOUND : 0)
	     | (displayAlarm && mConfirmAck->isChecked()                             ? KAEvent::CONFIRM_ACK : 0)
	     | (displayAlarm && mLateCancel->isAutoClose()                           ? KAEvent::AUTO_CLOSE : 0)
	     | (cmdAlarm     && mCmdTypeScript->isChecked()                          ? KAEvent::SCRIPT : 0)
	     | (cmdAlarm     && mCmdOutputGroup->checkedButton() == mCmdExecInTerm   ? KAEvent::EXEC_IN_XTERM : 0)
	     | (emailAlarm   && mEmailBcc->isChecked()                               ? KAEvent::EMAIL_BCC : 0)
	     | (mShowInKorganizer && mShowInKorganizer->isChecked()                  ? KAEvent::COPY_KORGANIZER : 0)
	     | (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN            ? KAEvent::REPEAT_AT_LOGIN : 0)
	     | ((mTemplate ? mTemplateAnyTime->isChecked() : mAlarmDateTime.isDateOnly()) ? KAEvent::ANY_TIME : 0)
	     | (mFontColourButton->defaultFont()                                     ? KAEvent::DEFAULT_FONT : 0);
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAEvent::Action EditAlarmDlg::getAlarmType() const
{
	return mFileRadio->isChecked()    ? KAEvent::FILE
	     : mCommandRadio->isChecked() ? KAEvent::COMMAND
	     : mEmailRadio->isChecked()   ? KAEvent::EMAIL
	     :                              KAEvent::MESSAGE;
}

/******************************************************************************
*  Called when the dialog is displayed.
*  The first time through, sets the size to the same as the last time it was
*  displayed.
*/
void EditAlarmDlg::showEvent(QShowEvent* se)
{
	if (!mDeferGroupHeight)
	{
		mDeferGroupHeight = mDeferGroup->height() + spacingHint();
		QSize s;
		if (KAlarm::readConfigWindowSize(EDIT_DIALOG_NAME, s))
			s.setHeight(s.height() + (mDeferGroup->isHidden() ? 0 : mDeferGroupHeight));
		else
			s = minimumSize();
		resize(s);
	}
	KWindowSystem::setOnDesktop(winId(), mDesktop);    // ensure it displays on the desktop expected by the user
	KDialog::showEvent(se);
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size (adjusted to ignore the optional height of the deferred
*  time edit widget) in the config file.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
	{
		QSize s = re->size();
		s.setHeight(s.height() - (mDeferGroup->isHidden() ? 0 : mDeferGroupHeight));
		KAlarm::writeConfigWindowSize(EDIT_DIALOG_NAME, s);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
*  Called when any button is clicked.
*/
void EditAlarmDlg::slotButtonClicked(int button)
{
	if (button == Ok)
	{
		if (validate())
			accept();
	}
	else
		KDialog::slotButtonClicked(button);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Validate the input data.
*/
bool EditAlarmDlg::validate()
{
	if (!stateChanged())
	{
		// No changes have been made except possibly to an existing deferral
		if (!mOnlyDeferred)
			reject();
		return mOnlyDeferred;
	}
	RecurrenceEdit::RepeatType recurType = mRecurrenceEdit->repeatType();
	if (mTimeWidget
	&&  mTabs->currentIndex() == mRecurPageIndex  &&  recurType == RecurrenceEdit::AT_LOGIN)
		mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
	bool timedRecurrence = mRecurrenceEdit->isTimedRepeatType();    // does it recur other than at login?
	bool repeated = mSimpleRepetition->count();
	if (mTemplate)
	{
		// Check that the template name is not blank and is unique
		QString errmsg;
		QString name = mTemplateName->text();
		if (name.isEmpty())
			errmsg = i18n("You must enter a name for the alarm template");
		else if (name != mSavedTemplateName)
		{
			if (AlarmCalendar::resources()->templateEvent(name).valid())
				errmsg = i18n("Template name is already in use");
		}
		if (!errmsg.isEmpty())
		{
			mTemplateName->setFocus();
			KMessageBox::sorry(this, errmsg);
			return false;
		}
	}
	else if(mTimeWidget)
	{
		QWidget* errWidget;
		mAlarmDateTime = mTimeWidget->getDateTime(0, !(timedRecurrence || repeated), false, &errWidget);
		if (errWidget)
		{
			// It's more than just an existing deferral being changed, so the time matters
			mTabs->setCurrentIndex(mMainPageIndex);
			errWidget->setFocus();
			mTimeWidget->getDateTime();   // display the error message now
			return false;
		}
	}
	if (!checkCommandData()
	||  !checkEmailData())
		return false;
	if (!mTemplate)
	{
		if (timedRecurrence)
		{
			KAEvent event;
			AlarmResource* r;
			getEvent(event, r);     // this may adjust mAlarmDateTime
			KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
			bool dateOnly = mAlarmDateTime.isDateOnly();
			if (dateOnly  &&  mAlarmDateTime.date() < now.date()
			||  !dateOnly  &&  KDateTime(mAlarmDateTime).dateTime() < now.dateTime())
			{
				// A timed recurrence has an entered start date which
				// has already expired, so we must adjust it.
				dateOnly = mAlarmDateTime.isDateOnly();
				if ((dateOnly  &&  mAlarmDateTime.date() < now.date()
				     || !dateOnly  &&  KDateTime(mAlarmDateTime).dateTime() < now.dateTime())
				&&  event.nextOccurrence(now, mAlarmDateTime, KAEvent::ALLOW_FOR_REPETITION) == KAEvent::NO_OCCURRENCE)
				{
					KMessageBox::sorry(this, i18n("Recurrence has already expired"));
					return false;
				}
				if (event.workTimeOnly()  &&  !event.displayDateTime().isValid())
				{
					if (KMessageBox::warningContinueCancel(this, i18n("The alarm will never occur during working hours"))
					    != KMessageBox::Continue)
						return false;
				}
			}
		}
		QString errmsg;
		QWidget* errWidget = mRecurrenceEdit->checkData(mAlarmDateTime.effectiveKDateTime(), errmsg);
		if (errWidget)
		{
			mTabs->setCurrentIndex(mRecurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, errmsg);
			return false;
		}
	}
	int longestRecurInterval = -1;
	if (recurType != RecurrenceEdit::NO_RECUR)
	{
		int reminder = mReminder->minutes();
		if (reminder)
		{
			KAEvent event;
			mRecurrenceEdit->updateEvent(event, false);
			if (!mReminder->isOnceOnly())
			{
				longestRecurInterval = event.longestRecurrenceInterval();
				if (longestRecurInterval  &&  reminder >= longestRecurInterval)
				{
					mTabs->setCurrentIndex(mMainPageIndex);
					mReminder->setFocusOnCount();
					KMessageBox::sorry(this, i18n("Reminder period must be less than the recurrence interval, unless '%1' is checked."
					                             , Reminder::i18n_FirstRecurrenceOnly()));
					return false;
				}
			}
		}
	}
	if (mSimpleRepetition->count())
	{
		switch (recurType)
		{
			case RecurrenceEdit::AT_LOGIN:    // alarm repeat not allowed
				mSimpleRepetition->set(0, 0);
				break;
			default:          // repeat duration must be less than recurrence interval
				if (longestRecurInterval < 0)
				{
					KAEvent event;
					mRecurrenceEdit->updateEvent(event, false);
					longestRecurInterval = event.longestRecurrenceInterval();
				}
				if (mSimpleRepetition->count() * mSimpleRepetition->interval() >= longestRecurInterval - mReminder->minutes())
				{
					KMessageBox::sorry(this, i18n("Simple alarm repetition duration must be less than the recurrence interval minus any reminder period"));
					mSimpleRepetition->activate();   // display the alarm repetition dialog again
					return false;
				}
				// fall through to NO_RECUR
			case RecurrenceEdit::NO_RECUR:    // no restriction on repeat duration
				if (mSimpleRepetition->interval() % 1440
				&&  (mTemplate && mTemplateAnyTime->isChecked()  ||  !mTemplate && mAlarmDateTime.isDateOnly()))
				{
					KMessageBox::sorry(this, i18n("Simple alarm repetition period must be in units of days or weeks for a date-only alarm"));
					mSimpleRepetition->activate();   // display the alarm repetition dialog again
					return false;
				}
				break;
		}
	}
	if (!checkText(mAlarmMessage))
		return false;

	mResource = 0;
	// A null resource event ID indicates that the caller already
	// knows which resource to use.
	if (!mResourceEventId.isNull())
	{
		if (!mResourceEventId.isEmpty())
		{
			mResource = AlarmResources::instance()->resourceForIncidence(mResourceEventId);
			AlarmResource::Type type = mTemplate ? AlarmResource::TEMPLATE : AlarmResource::ACTIVE;
			if (mResource->alarmType() != type)
				mResource = 0;   // event may have expired while dialogue was open
		}
		if (!mResource  ||  !mResource->writable())
		{
			KCalEvent::Status type = mTemplate ? KCalEvent::TEMPLATE : KCalEvent::ACTIVE;
			mResource = AlarmResources::instance()->destination(type, this);
		}
		if (!mResource)
		{
			KMessageBox::sorry(this, i18n("You must select a resource to save the alarm in"));
			return false;
		}
	}
	return true;
}

/******************************************************************************
*  Called when the Try button is clicked.
*  Display/execute the alarm immediately for the user to check its configuration.
*/
void EditAlarmDlg::slotTry()
{
	QString text;
	if (checkText(text))
	{
kDebug()<<"Text="<<text<<":";
		if (mEmailRadio->isChecked())
		{
			if (!checkEmailData()
			||  KMessageBox::warningContinueCancel(this, i18n("Do you really want to send the email now to the specified recipient(s)?"),
			                                       i18n("Confirm Email"), KGuiItem(i18n("&Send"))) != KMessageBox::Continue)
				return;
		}
		KAEvent event;
		setEvent(event, text, true);
		void* proc = theApp()->execAlarm(event, event.firstAlarm(), false, false);
		if (proc)
		{
			if (mCommandRadio->isChecked()  &&  mCmdOutputGroup->checkedButton() != mCmdExecInTerm)
			{
				theApp()->commandMessage((ShellProcess*)proc, this);
				KMessageBox::information(this, i18n("Command executed:\n%1", text));
				theApp()->commandMessage((ShellProcess*)proc, 0);
			}
			else if (mEmailRadio->isChecked())
			{
				QString bcc;
				if (mEmailBcc->isChecked())
					bcc = i18n("\nBcc: %1", Preferences::emailBccAddress());
				KMessageBox::information(this, i18n("Email sent to:\n%1%2", mEmailAddresses.join("\n"), bcc));
			}
		}
	}
}

/******************************************************************************
*  Called when the Load Template button is clicked.
*  Prompt to select a template and initialise the dialogue with its contents.
*/
void EditAlarmDlg::slotDefault()
{
	TemplatePickDlg dlg(this);
	if (dlg.exec() == QDialog::Accepted)
	{
		const Event* kcalEvent = dlg.selectedTemplate();
		if (kcalEvent)
		{
			KAEvent event(kcalEvent);
			initialise(&event);
		}
	}
}

/******************************************************************************
 * Called when the Change deferral button is clicked.
 */
void EditAlarmDlg::slotEditDeferral()
{
	if (!mTimeWidget)
		return;
	bool limit = true;
	int repeatCount = mSimpleRepetition->count();
	DateTime start = mTimeWidget->getDateTime(0, !repeatCount, !mExpiredRecurrence);
	if (!start.isValid())
	{
		if (!mExpiredRecurrence)
			return;
		limit = false;
	}
	KDateTime now = KDateTime::currentUtcDateTime();
	if (limit)
	{
		if (repeatCount  &&  start < now)
		{
			// Simple repetition - find the time of the next one
			int interval = mSimpleRepetition->interval() * 60;
			int repetition = (start.secsTo(now) + interval - 1) / interval;
			if (repetition > repeatCount)
			{
				mTimeWidget->getDateTime();    // output the appropriate error message
				return;
			}
			start = start.addSecs(repetition * interval);
		}
	}

	bool deferred = mDeferDateTime.isValid();
	DeferAlarmDlg deferDlg(i18n("Defer Alarm"), (deferred ? mDeferDateTime : DateTime(now.addSecs(60))),
	                       deferred, this);
	deferDlg.setObjectName("EditDeferDlg");    // used by LikeBack
	if (limit)
	{
		// Don't allow deferral past the next recurrence
		int reminder = mReminder->minutes();
		if (reminder)
		{
			DateTime remindTime = start.addMins(-reminder);
			if (KDateTime::currentUtcDateTime() < remindTime)
				start = remindTime;
		}
		deferDlg.setLimit(start.addSecs(-60));
	}
	if (deferDlg.exec() == QDialog::Accepted)
	{
		mDeferDateTime = deferDlg.getDateTime();
		mDeferTimeLabel->setText(mDeferDateTime.isValid() ? mDeferDateTime.formatLocale() : QString());
	}
}

/******************************************************************************
*  Called when the main page is shown.
*  Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
	slotAlarmTypeChanged(0);
	if (!mMainPageShown)
	{
		if (mTemplateName)
			mTemplateName->setFocus();
		mMainPageShown = true;
	}
	if (mTimeWidget)
	{
		if (!mReadOnly  &&  mRecurPageShown  &&  mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
			mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
		if (mReadOnly  ||  mRecurrenceEdit->isTimedRepeatType())
			mTimeWidget->setMinDateTime();             // don't set a minimum date/time
		else
			mTimeWidget->setMinDateTimeIsCurrent();    // set the minimum date/time to track the clock
	}
}

/******************************************************************************
*  Called when the recurrence edit page is shown.
*  The recurrence defaults are set to correspond to the start date.
*  The first time, for a new alarm, the recurrence end date is set according to
*  the alarm start time.
*/
void EditAlarmDlg::slotShowRecurrenceEdit()
{
	mRecurPageIndex = mTabs->currentIndex();
	if (!mReadOnly  &&  !mTemplate)
	{
		mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
		KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
		bool expired = (mAlarmDateTime.effectiveKDateTime() < now);
		if (mRecurSetDefaultEndDate)
		{
			mRecurrenceEdit->setDefaultEndDate(expired ? now.date() : mAlarmDateTime.date());
			mRecurSetDefaultEndDate = false;
		}
		mRecurrenceEdit->setStartDate(mAlarmDateTime.date(), now.date());
		if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
			mRecurrenceEdit->setEndDateTime(expired ? now : KDateTime(mAlarmDateTime));
	}
	mRecurPageShown = true;
}

/******************************************************************************
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
	if (!mTemplate)
	{
		bool recurs = (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR);
		if (mDeferGroup)
			mDeferGroup->setEnabled(recurs);
		mTimeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
		bool atLogin = (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN);
		if (atLogin)
		{
			mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
			mRecurrenceEdit->setEndDateTime(mAlarmDateTime);
		}
		mReminder->enableOnceOnly(recurs && !atLogin);
	}
	slotRecurFrequencyChange();
}

/******************************************************************************
*  Called when the recurrence frequency selection changes, or the simple
*  repetition interval changes.
*  Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
	KAEvent event;
	mRecurrenceEdit->updateEvent(event, false);
	event.setRepetition(mSimpleRepetition->interval(), mSimpleRepetition->count());
	mRecurrenceText->setText(recurText(event));
}

/******************************************************************************
*  Called when the Simple Repetition button has been pressed to display the
*  alarm repetition dialog.
*  Alarm repetition has the following restrictions:
*  1) Not allowed for a repeat-at-login alarm
*  2) For a date-only alarm, the repeat interval must be a whole number of days.
*  3) The overall repeat duration must be less than the recurrence interval.
*/
void EditAlarmDlg::slotSetSimpleRepetition()
{
	bool dateOnly = mTemplate ? mTemplateAnyTime->isChecked() : mTimeWidget->anyTime();
	int maxDuration;
	switch (mRecurrenceEdit->repeatType())
	{
		case RecurrenceEdit::NO_RECUR:  maxDuration = -1;  break;  // no restriction on repeat duration
		case RecurrenceEdit::AT_LOGIN:  maxDuration = 0;  break;   // alarm repeat not allowed
		default:          // repeat duration must be less than recurrence interval
		{
			KAEvent event;
			mRecurrenceEdit->updateEvent(event, false);
			maxDuration = event.longestRecurrenceInterval() - mReminder->minutes() - 1;
			break;
		}
	}
	mSimpleRepetition->initialise(mSimpleRepetition->interval(), mSimpleRepetition->count(), dateOnly, maxDuration);
}

/******************************************************************************
*  Validate and convert command alarm data.
*/
bool EditAlarmDlg::checkCommandData()
{
	if (mCommandRadio->isChecked()  &&  mCmdOutputGroup->checkedButton() == mCmdLogToFile)
	{
		// Validate the log file name
		QString file = mCmdLogFileEdit->text();
		QFileInfo info(file);
		QDir::setCurrent(QDir::homePath());
		bool err = file.isEmpty()  ||  info.isDir();
		if (!err)
		{
			if (info.exists())
			{
				err = !info.isWritable();
			}
			else
			{
				QFileInfo dirinfo(info.absolutePath());    // get absolute directory path
				err = (!dirinfo.isDir()  ||  !dirinfo.isWritable());
			}
		}
		if (err)
		{
			mTabs->setCurrentIndex(mMainPageIndex);
			mCmdLogFileEdit->setFocus();
			KMessageBox::sorry(this, i18n("Log file must be the name or path of a local file, with write permission."));
			return false;
		}
		// Convert the log file to an absolute path
		mCmdLogFileEdit->setText(info.absoluteFilePath());
	}
	return true;
}

/******************************************************************************
*  Convert the email addresses to a list, and validate them. Convert the email
*  attachments to a list.
*/
bool EditAlarmDlg::checkEmailData()
{
	if (mEmailRadio->isChecked())
	{
		QString addrs = mEmailToEdit->text();
		if (addrs.isEmpty())
			mEmailAddresses.clear();
		else
		{
			QString bad = KAMail::convertAddresses(addrs, mEmailAddresses);
			if (!bad.isEmpty())
			{
				mEmailToEdit->setFocus();
				KMessageBox::error(this, i18n("Invalid email address:\n%1", bad));
				return false;
			}
		}
		if (mEmailAddresses.isEmpty())
		{
			mEmailToEdit->setFocus();
			KMessageBox::error(this, i18n("No email address specified"));
			return false;
		}

		mEmailAttachments.clear();
		for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
		{
			QString att = mEmailAttachList->itemText(i);
			switch (KAMail::checkAttachment(att))
			{
				case 1:
					mEmailAttachments.append(att);
					break;
				case 0:
					break;      // empty
				case -1:
					mEmailAttachList->setFocus();
					KMessageBox::error(this, i18n("Invalid email attachment:\n%1", att));
					return false;
			}
		}
	}
	return true;
}

/******************************************************************************
*  Called when one of the alarm action type radio buttons is clicked,
*  to display the appropriate set of controls for that action type.
*/
void EditAlarmDlg::slotAlarmTypeChanged(QAbstractButton*)
{
	bool displayAlarm = false;
	QWidget* focus = 0;
	if (mMessageRadio->isChecked())
	{
		mFileBox->hide();
		mFilePadding->hide();
		mTextMessageEdit->show();
		mFontColourButton->show();
		mSoundPicker->showSpeak(true);
		mDisplayAlarmsFrame->show();
		mCommandFrame->hide();
		mEmailFrame->hide();
		mReminder->show();
		mConfirmAck->show();
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
		focus = mTextMessageEdit;
		displayAlarm = true;
	}
	else if (mFileRadio->isChecked())
	{
		mTextMessageEdit->hide();
		mFileBox->show();
		mFilePadding->show();
		mFontColourButton->hide();
		mSoundPicker->showSpeak(false);
		mDisplayAlarmsFrame->show();
		mCommandFrame->hide();
		mEmailFrame->hide();
		mReminder->show();
		mConfirmAck->show();
		setButtonWhatsThis(Try, i18n("Display the file now"));
		mFileMessageEdit->setNoSelect();
		focus = mFileMessageEdit;
		displayAlarm = true;
	}
	else if (mCommandRadio->isChecked())
	{
		mDisplayAlarmsFrame->hide();
		mCommandFrame->show();
		mEmailFrame->hide();
		mReminder->hide();
		mConfirmAck->hide();
		setButtonWhatsThis(Try, i18n("Execute the specified command now"));
		mCmdCommandEdit->setNoSelect();
		focus = mCmdCommandEdit;
	}
	else if (mEmailRadio->isChecked())
	{
		mDisplayAlarmsFrame->hide();
		mCommandFrame->hide();
		mEmailFrame->show();
		mReminder->hide();
		mConfirmAck->hide();
		setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
		mEmailToEdit->setNoSelect();
		focus = mEmailToEdit;
	}
	mLateCancel->showAutoClose(displayAlarm);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	if (focus)
		focus->setFocus();
}

/******************************************************************************
*  Called when one of the command type radio buttons is clicked,
*  to display the appropriate edit field.
*/
void EditAlarmDlg::slotCmdScriptToggled(bool on)
{
	if (on)
	{
		mCmdCommandEdit->hide();
		mCmdPadding->hide();
		mCmdScriptEdit->show();
		mCmdScriptEdit->setFocus();
	}
	else
	{
		mCmdScriptEdit->hide();
		mCmdCommandEdit->show();
		mCmdPadding->show();
		mCmdCommandEdit->setFocus();
	}
}

/******************************************************************************
*  Called when one of the template time radio buttons is clicked,
*  to enable or disable the template time entry spin boxes.
*/
void EditAlarmDlg::slotTemplateTimeType(QAbstractButton*)
{
	mTemplateTime->setEnabled(mTemplateUseTime->isChecked());
	mTemplateTimeAfter->setEnabled(mTemplateUseTimeAfter->isChecked());
}

/******************************************************************************
*  Called when the a new background colour has been selected using the colour
*  combo box.
*/
void EditAlarmDlg::slotBgColourSelected(const QColor& colour)
{
	mFontColourButton->setBgColour(colour);
}

/******************************************************************************
*  Called when the a new font and colour have been selected using the font &
*  colour pushbutton.
*/
void EditAlarmDlg::slotFontColourSelected()
{
	mBgColourChoose->setColour(mFontColourButton->bgColour());
}

/******************************************************************************
*  Called when the "Any time" checkbox is toggled in the date/time widget.
*  Sets the advance reminder and late cancel units to days if any time is checked.
*/
void EditAlarmDlg::slotAnyTimeToggled(bool anyTime)
{
	if (mReminder->isReminder())
		mReminder->setDateOnly(anyTime);
	mLateCancel->setDateOnly(anyTime);
}

/******************************************************************************
 * Get a selection from the Address Book.
 */
void EditAlarmDlg::openAddressBook()
{
	KABC::Addressee a = KABC::AddresseeDialog::getAddressee(this);
	if (a.isEmpty())
		return;
	Person person(a.realName(), a.preferredEmail());
	QString addrs = mEmailToEdit->text().trimmed();
	if (!addrs.isEmpty())
		addrs += ", ";
	addrs += person.fullName();
	mEmailToEdit->setText(addrs);
}

/******************************************************************************
 * Select a file to attach to the email.
 */
void EditAlarmDlg::slotAddAttachment()
{
	QString url = KAlarm::browseFile(i18n("Choose File to Attach"), mAttachDefaultDir, QString(),
	                                 QString(), KFile::ExistingOnly, this);
	if (!url.isEmpty())
	{
		mEmailAttachList->addItem(url);
		mEmailAttachList->setCurrentIndex(mEmailAttachList->count() - 1);   // select the new item
		mEmailRemoveButton->setEnabled(true);
		mEmailAttachList->setEnabled(true);
	}
}

/******************************************************************************
 * Remove the currently selected attachment from the email.
 */
void EditAlarmDlg::slotRemoveAttachment()
{
	int item = mEmailAttachList->currentIndex();
	mEmailAttachList->removeItem(item);
	int count = mEmailAttachList->count();
	if (item >= count)
		mEmailAttachList->setCurrentIndex(count - 1);
	if (!count)
	{
		mEmailRemoveButton->setEnabled(false);
		mEmailAttachList->setEnabled(false);
	}
}

/******************************************************************************
*  Clean up the alarm text, and if it's a file, check whether it's valid.
*/
bool EditAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
	if (mMessageRadio->isChecked())
		result = mTextMessageEdit->toPlainText();
	else if (mEmailRadio->isChecked())
		result = mEmailMessageEdit->toPlainText();
	else if (mCommandRadio->isChecked())
	{
		if (mCmdTypeScript->isChecked())
			result = mCmdScriptEdit->toPlainText();
		else
			result = mCmdCommandEdit->text();
		result = result.trimmed();
	}
	else if (mFileRadio->isChecked())
	{
		QString alarmtext = mFileMessageEdit->text().trimmed();
		// Convert any relative file path to absolute
		// (using home directory as the default)
		enum Err { NONE = 0, BLANK, NONEXISTENT, DIRECTORY, UNREADABLE, NOT_TEXT_IMAGE };
		Err err = NONE;
		KUrl url;
		int i = alarmtext.indexOf(QLatin1Char('/'));
		if (i > 0  &&  alarmtext[i - 1] == QLatin1Char(':'))
		{
			url = alarmtext;
			url.cleanPath();
			alarmtext = url.prettyUrl();
			KIO::UDSEntry uds;
			if (!KIO::NetAccess::stat(url, uds, MainWindow::mainMainWindow()))
				err = NONEXISTENT;
			else
			{
				KFileItem fi(uds, url);
				if (fi.isDir())             err = DIRECTORY;
				else if (!fi.isReadable())  err = UNREADABLE;
			}
		}
		else if (alarmtext.isEmpty())
			err = BLANK;    // blank file name
		else
		{
			// It's a local file - convert to absolute path & check validity
			QFileInfo info(alarmtext);
			QDir::setCurrent(QDir::homePath());
			alarmtext = info.absoluteFilePath();
			url.setPath(alarmtext);
			alarmtext = QLatin1String("file:") + alarmtext;
			if (!err)
			{
				if      (info.isDir())        err = DIRECTORY;
				else if (!info.exists())      err = NONEXISTENT;
				else if (!info.isReadable())  err = UNREADABLE;
			}
		}
		if (!err)
		{
			switch (KAlarm::fileType(KFileItem(KFileItem::Unknown, KFileItem::Unknown, url).mimetype()))
			{
				case KAlarm::TextFormatted:
				case KAlarm::TextPlain:
				case KAlarm::TextApplication:
				case KAlarm::Image:
					break;
				default:
					err = NOT_TEXT_IMAGE;
					break;
			}
		}
		if (err  &&  showErrorMessage)
		{
			mFileMessageEdit->setFocus();
			QString errmsg;
			switch (err)
			{
				case BLANK:
					KMessageBox::sorry(const_cast<EditAlarmDlg*>(this), i18n("Please select a file to display"));
					return false;
				case NONEXISTENT:     errmsg = i18n("%1\nnot found", alarmtext);  break;
				case DIRECTORY:       errmsg = i18n("%1\nis a folder", alarmtext);  break;
				case UNREADABLE:      errmsg = i18n("%1\nis not readable", alarmtext);  break;
				case NOT_TEXT_IMAGE:  errmsg = i18n("%1\nappears not to be a text or image file", alarmtext);  break;
				case NONE:
				default:
					break;
			}
			if (KMessageBox::warningContinueCancel(const_cast<EditAlarmDlg*>(this), errmsg)
			    == KMessageBox::Cancel)
				return false;
		}
		result = alarmtext;
	}
	return true;
}


/*=============================================================================
= Class TextEdit
= A text edit field with a minimum height of 3 text lines.
= Provides KDE 2 compatibility.
=============================================================================*/
TextEdit::TextEdit(QWidget* parent)
	: QTextEdit(parent)
{
	QSize tsize = sizeHint();
	tsize.setHeight(fontMetrics().lineSpacing()*13/4 + 2*frameWidth());
	setMinimumSize(tsize);
}

void TextEdit::dragEnterEvent(QDragEnterEvent* e)
{
	if (KCal::ICalDrag::canDecode(e->mimeData()))
		e->ignore();   // don't accept "text/calendar" objects
	QTextEdit::dragEnterEvent(e);
}
