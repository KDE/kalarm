/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm or alarm template
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

#include <limits.h>

#include <qlayout.h>
#include <qpopupmenu.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qwidgetstack.h>
#include <qdragobject.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qtabwidget.h>
#include <qvalidator.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qdir.h>
#include <qstyle.h>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <kurldrag.h>
#include <kurlcompletion.h>
#include <kwin.h>
#include <kwinmodule.h>
#include <kstandarddirs.h>
#include <kstdguiitem.h>
#include <kabc/addresseedialog.h>
#include <kdebug.h>

#include <libkdepim/maillistdrag.h>
#include <libkdepim/kvcarddrag.h>
#include <libkcal/icaldrag.h>

#include "alarmcalendar.h"
#include "alarmtimewidget.h"
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
	PickAlarmFileRadio(const QString& text, QButtonGroup* parent, const char* name = 0)
		: PickFileRadio(text, parent, name) { }
	virtual QString pickFile()    // called when browse button is pressed to select a file to display
	{
		return KAlarm::browseFile(i18n("Choose Text or Image File to Display"), mDefaultDir, fileEdit()->text(),
		                          QString::null, KFile::ExistingOnly, parentWidget(), "pickAlarmFile");
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
	PickLogFileRadio(QPushButton* b, LineEdit* e, const QString& text, QButtonGroup* parent, const char* name = 0)
		: PickFileRadio(b, e, text, parent, name) { }
	virtual QString pickFile()    // called when browse button is pressed to select a log file
	{
		return KAlarm::browseFile(i18n("Choose Log File"), mDefaultDir, fileEdit()->text(), QString::null,
		                          KFile::LocalOnly, parentWidget(), "pickLogFile");
	}
    private:
	QString mDefaultDir;   // default directory for log file browse button
};

inline QString recurText(const KAEvent& event)
{
	QString r;
	if (event.repeatCount())
		r = QString::fromLatin1("%1 / %2").arg(event.recurrenceText()).arg(event.repetitionText());
	else
		r = event.recurrenceText();
	return i18n("&Recurrence - [%1]").arg(r);
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_ConfirmAck()         { return i18n("Confirm acknowledgment"); }
QString EditAlarmDlg::i18n_k_ConfirmAck()       { return i18n("Confirm ac&knowledgment"); }
QString EditAlarmDlg::i18n_SpecialActions()     { return i18n("Special Actions..."); }
QString EditAlarmDlg::i18n_ShowInKOrganizer()   { return i18n("Show in KOrganizer"); }
QString EditAlarmDlg::i18n_g_ShowInKOrganizer() { return i18n("Show in KOr&ganizer"); }
QString EditAlarmDlg::i18n_EnterScript()        { return i18n("Enter a script"); }
QString EditAlarmDlg::i18n_p_EnterScript()      { return i18n("Enter a scri&pt"); }
QString EditAlarmDlg::i18n_ExecInTermWindow()   { return i18n("Execute in terminal window"); }
QString EditAlarmDlg::i18n_w_ExecInTermWindow() { return i18n("Execute in terminal &window"); }
QString EditAlarmDlg::i18n_u_ExecInTermWindow() { return i18n("Exec&ute in terminal window"); }
QString EditAlarmDlg::i18n_g_LogToFile()        { return i18n("Lo&g to file"); }
QString EditAlarmDlg::i18n_CopyEmailToSelf()    { return i18n("Copy email to self"); }
QString EditAlarmDlg::i18n_e_CopyEmailToSelf()  { return i18n("Copy &email to self"); }
QString EditAlarmDlg::i18n_s_CopyEmailToSelf()  { return i18n("Copy email to &self"); }
QString EditAlarmDlg::i18n_EmailFrom()          { return i18n("'From' email address", "From:"); }
QString EditAlarmDlg::i18n_f_EmailFrom()        { return i18n("'From' email address", "&From:"); }
QString EditAlarmDlg::i18n_EmailTo()            { return i18n("Email addressee", "To:"); }
QString EditAlarmDlg::i18n_EmailSubject()       { return i18n("Email subject", "Subject:"); }
QString EditAlarmDlg::i18n_j_EmailSubject()     { return i18n("Email subject", "Sub&ject:"); }


/******************************************************************************
 * Constructor.
 * Parameters:
 *   Template = true to edit/create an alarm template
 *            = false to edit/create an alarm.
 *   event   != to initialise the dialogue to show the specified event's data.
 */
EditAlarmDlg::EditAlarmDlg(bool Template, const QString& caption, QWidget* parent, const char* name,
                           const KAEvent* event, bool readOnly)
	: KDialogBase(parent, (name ? name : Template ? "TemplEditDlg" : "EditDlg"), true, caption,
	              (readOnly ? Cancel|Try : Template ? Ok|Cancel|Try : Ok|Cancel|Try|Default),
	              (readOnly ? Cancel : Ok)),
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
	  mDeferGroupHeight(0),
	  mTemplate(Template),
	  mDesiredReadOnly(readOnly),
	  mReadOnly(readOnly),
	  mSavedEvent(0)
{
	setButtonText(Default, i18n("Load Template..."));
	QVBox* mainWidget = new QVBox(this);
	mainWidget->setSpacing(spacingHint());
	setMainWidget(mainWidget);
	if (mTemplate)
	{
		QHBox* box = new QHBox(mainWidget);
		box->setSpacing(spacingHint());
		QLabel* label = new QLabel(i18n("Template name:"), box);
		label->setFixedSize(label->sizeHint());
		mTemplateName = new QLineEdit(box);
		mTemplateName->setReadOnly(mReadOnly);
		label->setBuddy(mTemplateName);
		QWhatsThis::add(box, i18n("Enter the name of the alarm template"));
		box->setFixedHeight(box->sizeHint().height());
	}
	mTabs = new QTabWidget(mainWidget);
	mTabs->setMargin(marginHint());

	QVBox* mainPageBox = new QVBox(mTabs);
	mainPageBox->setSpacing(spacingHint());
	mTabs->addTab(mainPageBox, i18n("&Alarm"));
	mMainPageIndex = 0;
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, 0, spacingHint());

	// Recurrence tab
	QVBox* recurTab = new QVBox(mTabs);
	mainPageBox->setSpacing(spacingHint());
	mTabs->addTab(recurTab, QString::null);
	mRecurPageIndex = 1;
	mRecurrenceEdit = new RecurrenceEdit(readOnly, recurTab, "recurPage");
	connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));
	connect(mRecurrenceEdit, SIGNAL(frequencyChanged()), SLOT(slotRecurFrequencyChange()));
	connect(mRecurrenceEdit, SIGNAL(repeatNeedsInitialisation()), SLOT(slotSetSubRepetition()));

	// Alarm action

	mActionGroup = new ButtonGroup(i18n("Action"), mainPage, "actionGroup");
	connect(mActionGroup, SIGNAL(buttonSet(int)), SLOT(slotAlarmTypeChanged(int)));
	topLayout->addWidget(mActionGroup, 1);
	QBoxLayout* layout = new QVBoxLayout(mActionGroup, marginHint(), spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	QGridLayout* grid = new QGridLayout(layout, 1, 5);

	// Message radio button
	mMessageRadio = new RadioButton(i18n("Te&xt"), mActionGroup, "messageButton");
	mMessageRadio->setFixedSize(mMessageRadio->sizeHint());
	QWhatsThis::add(mMessageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(mMessageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	mFileRadio = new PickAlarmFileRadio(i18n("&File"), mActionGroup, "fileButton");
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	QWhatsThis::add(mFileRadio,
	      i18n("If checked, the alarm will display the contents of a text or image file."));
	grid->addWidget(mFileRadio, 1, 2);
	grid->setColStretch(3, 1);

	// Command radio button
	mCommandRadio = new RadioButton(i18n("Co&mmand"), mActionGroup, "cmdButton");
	mCommandRadio->setFixedSize(mCommandRadio->sizeHint());
	QWhatsThis::add(mCommandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(mCommandRadio, 1, 4);
	grid->setColStretch(5, 1);

	// Email radio button
	mEmailRadio = new RadioButton(i18n("&Email"), mActionGroup, "emailButton");
	mEmailRadio->setFixedSize(mEmailRadio->sizeHint());
	QWhatsThis::add(mEmailRadio,
	      i18n("If checked, the alarm will send an email."));
	grid->addWidget(mEmailRadio, 1, 6);

	initDisplayAlarms(mActionGroup);
	layout->addWidget(mDisplayAlarmsFrame);
	initCommand(mActionGroup);
	layout->addWidget(mCommandFrame);
	initEmail(mActionGroup);
	layout->addWidget(mEmailFrame);

	// Deferred date/time: visible only for a deferred recurring event.
	mDeferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), mainPage, "deferGroup");
	topLayout->addWidget(mDeferGroup);
	QLabel* label = new QLabel(i18n("Deferred to:"), mDeferGroup);
	label->setFixedSize(label->sizeHint());
	mDeferTimeLabel = new QLabel(mDeferGroup);

	mDeferChangeButton = new QPushButton(i18n("C&hange..."), mDeferGroup);
	mDeferChangeButton->setFixedSize(mDeferChangeButton->sizeHint());
	connect(mDeferChangeButton, SIGNAL(clicked()), SLOT(slotEditDeferral()));
	QWhatsThis::add(mDeferChangeButton, i18n("Change the alarm's deferred time, or cancel the deferral"));
	mDeferGroup->addSpace(0);

	layout = new QHBoxLayout(topLayout);

	// Date and time entry
	if (mTemplate)
	{
		mTemplateTimeGroup = new ButtonGroup(i18n("Time"), mainPage, "templateGroup");
		connect(mTemplateTimeGroup, SIGNAL(buttonSet(int)), SLOT(slotTemplateTimeType(int)));
		layout->addWidget(mTemplateTimeGroup);
		grid = new QGridLayout(mTemplateTimeGroup, 2, 2, marginHint(), spacingHint());
		grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
		// Get alignment to use in QGridLayout (AlignAuto doesn't work correctly there)
		int alignment = QApplication::reverseLayout() ? Qt::AlignRight : Qt::AlignLeft;

		mTemplateDefaultTime = new RadioButton(i18n("&Default time"), mTemplateTimeGroup, "templateDefTimeButton");
		mTemplateDefaultTime->setFixedSize(mTemplateDefaultTime->sizeHint());
		mTemplateDefaultTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateDefaultTime,
		      i18n("Do not specify a start time for alarms based on this template. "
		           "The normal default start time will be used."));
		grid->addWidget(mTemplateDefaultTime, 0, 0, alignment);

		QHBox* box = new QHBox(mTemplateTimeGroup);
		box->setSpacing(spacingHint());
		mTemplateUseTime = new RadioButton(i18n("Time:"), box, "templateTimeButton");
		mTemplateUseTime->setFixedSize(mTemplateUseTime->sizeHint());
		mTemplateUseTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateUseTime,
		      i18n("Specify a start time for alarms based on this template."));
		mTemplateTimeGroup->insert(mTemplateUseTime);
		mTemplateTime = new TimeEdit(box, "templateTimeEdit");
		mTemplateTime->setFixedSize(mTemplateTime->sizeHint());
		mTemplateTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateTime,
		      QString("%1\n\n%2").arg(i18n("Enter the start time for alarms based on this template."))
		                         .arg(TimeSpinBox::shiftWhatsThis()));
		box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 0, 1, alignment);

		mTemplateAnyTime = new RadioButton(i18n("An&y time"), mTemplateTimeGroup, "templateAnyTimeButton");
		mTemplateAnyTime->setFixedSize(mTemplateAnyTime->sizeHint());
		mTemplateAnyTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateAnyTime,
		      i18n("Set the '%1' option for alarms based on this template.").arg(i18n("Any time")));
		grid->addWidget(mTemplateAnyTime, 1, 0, alignment);

		box = new QHBox(mTemplateTimeGroup);
		box->setSpacing(spacingHint());
		mTemplateUseTimeAfter = new RadioButton(AlarmTimeWidget::i18n_w_TimeFromNow(), box, "templateFromNowButton");
		mTemplateUseTimeAfter->setFixedSize(mTemplateUseTimeAfter->sizeHint());
		mTemplateUseTimeAfter->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateUseTimeAfter,
		      i18n("Set alarms based on this template to start after the specified time "
		           "interval from when the alarm is created."));
		mTemplateTimeGroup->insert(mTemplateUseTimeAfter);
		mTemplateTimeAfter = new TimeSpinBox(1, maxDelayTime, box);
		mTemplateTimeAfter->setValue(1439);
		mTemplateTimeAfter->setFixedSize(mTemplateTimeAfter->sizeHint());
		mTemplateTimeAfter->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateTimeAfter,
		      QString("%1\n\n%2").arg(AlarmTimeWidget::i18n_TimeAfterPeriod())
		                         .arg(TimeSpinBox::shiftWhatsThis()));
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 1, 1, alignment);

		layout->addStretch();
	}
	else
	{
		mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME, mainPage, "timeGroup");
		connect(mTimeWidget, SIGNAL(anyTimeToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		topLayout->addWidget(mTimeWidget);
	}

	// Reminder
	static const QString reminderText = i18n("Enter how long in advance of the main alarm to display a reminder alarm.");
	mReminder = new Reminder(i18n("Rem&inder:"),
	                         i18n("Check to additionally display a reminder in advance of the main alarm time(s)."),
	                         QString("%1\n\n%2").arg(reminderText).arg(TimeSpinBox::shiftWhatsThis()),
	                         true, true, mainPage);
	mReminder->setFixedSize(mReminder->sizeHint());
	topLayout->addWidget(mReminder, 0, Qt::AlignAuto);

	// Late cancel selector - default = allow late display
	mLateCancel = new LateCancelSelector(true, mainPage);
	topLayout->addWidget(mLateCancel, 0, Qt::AlignAuto);

	// Acknowledgement confirmation required - default = no confirmation
	layout = new QHBoxLayout(topLayout, 0);
	mConfirmAck = createConfirmAckCheckbox(mainPage);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	layout->addWidget(mConfirmAck);
	layout->addSpacing(2*spacingHint());
	layout->addStretch();

	if (theApp()->korganizerEnabled())
	{
		// Show in KOrganizer checkbox
		mShowInKorganizer = new CheckBox(i18n_ShowInKOrganizer(), mainPage);
		mShowInKorganizer->setFixedSize(mShowInKorganizer->sizeHint());
		QWhatsThis::add(mShowInKorganizer, i18n("Check to copy the alarm into KOrganizer's calendar"));
		layout->addWidget(mShowInKorganizer);
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
	mDesktop = KWin::currentDesktop();
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
	mDisplayAlarmsFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* frameLayout = new QVBoxLayout(mDisplayAlarmsFrame, 0, spacingHint());

	// Text message edit box
	mTextMessageEdit = new TextEdit(mDisplayAlarmsFrame);
	mTextMessageEdit->setWordWrap(KTextEdit::NoWrap);
	QWhatsThis::add(mTextMessageEdit, i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new QHBox(mDisplayAlarmsFrame);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(LineEdit::Url, mFileBox);
	mFileMessageEdit->setAcceptDrops(true);
	QWhatsThis::add(mFileMessageEdit, i18n("Enter the name or URL of a text or image file to display."));

	// File browse button
	mFileBrowseButton = new QPushButton(mFileBox);
	mFileBrowseButton->setPixmap(SmallIcon("fileopen"));
	mFileBrowseButton->setFixedSize(mFileBrowseButton->sizeHint());
	QToolTip::add(mFileBrowseButton, i18n("Choose a file"));
	QWhatsThis::add(mFileBrowseButton, i18n("Select a text or image file to display."));
	mFileRadio->init(mFileBrowseButton, mFileMessageEdit);

	// Font and colour choice button and sample text
	mFontColourButton = new FontColourButton(mDisplayAlarmsFrame);
	mFontColourButton->setMaximumHeight(mFontColourButton->sizeHint().height());
	frameLayout->addWidget(mFontColourButton);

	QHBoxLayout* layout = new QHBoxLayout(frameLayout, 0, 0);
	mBgColourBox = new QHBox(mDisplayAlarmsFrame);
	mBgColourBox->setSpacing(spacingHint());
	layout->addWidget(mBgColourBox);
	layout->addStretch();
	QLabel* label = new QLabel(i18n("&Background color:"), mBgColourBox);
	mBgColourButton = new ColourCombo(mBgColourBox);
	label->setBuddy(mBgColourButton);
	QWhatsThis::add(mBgColourBox, i18n("Select the alarm message background color"));

	// Sound checkbox and file selector
	layout = new QHBoxLayout(frameLayout);
	mSoundPicker = new SoundPicker(mDisplayAlarmsFrame);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	layout->addWidget(mSoundPicker);
	layout->addSpacing(2*spacingHint());
	layout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(i18n_SpecialActions(), mDisplayAlarmsFrame);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		layout->addWidget(mSpecialActionsButton);
	}

	// Top-adjust the controls
	mFilePadding = new QHBox(mDisplayAlarmsFrame);
	frameLayout->addWidget(mFilePadding);
	frameLayout->setStretchFactor(mFilePadding, 1);
}

/******************************************************************************
 * Set up the command alarm dialog controls.
 */
void EditAlarmDlg::initCommand(QWidget* parent)
{
	mCommandFrame = new QFrame(parent);
	mCommandFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* frameLayout = new QVBoxLayout(mCommandFrame, 0, spacingHint());

	mCmdTypeScript = new CheckBox(i18n_p_EnterScript(), mCommandFrame);
	mCmdTypeScript->setFixedSize(mCmdTypeScript->sizeHint());
	connect(mCmdTypeScript, SIGNAL(toggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	QWhatsThis::add(mCmdTypeScript, i18n("Check to enter the contents of a script instead of a shell command line"));
	frameLayout->addWidget(mCmdTypeScript, 0, Qt::AlignAuto);

	mCmdCommandEdit = new LineEdit(LineEdit::Url, mCommandFrame);
	QWhatsThis::add(mCmdCommandEdit, i18n("Enter a shell command to execute."));
	frameLayout->addWidget(mCmdCommandEdit);

	mCmdScriptEdit = new TextEdit(mCommandFrame);
	QWhatsThis::add(mCmdScriptEdit, i18n("Enter the contents of a script to execute"));
	frameLayout->addWidget(mCmdScriptEdit);

	// What to do with command output

	mCmdOutputGroup = new ButtonGroup(i18n("Command Output"), mCommandFrame);
	frameLayout->addWidget(mCmdOutputGroup);
	QBoxLayout* layout = new QVBoxLayout(mCmdOutputGroup, marginHint(), spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);

	// Execute in terminal window
	RadioButton* button = new RadioButton(i18n_u_ExecInTermWindow(), mCmdOutputGroup, "execInTerm");
	button->setFixedSize(button->sizeHint());
	QWhatsThis::add(button, i18n("Check to execute the command in a terminal window"));
	mCmdOutputGroup->insert(button, EXEC_IN_TERMINAL);
	layout->addWidget(button, 0, Qt::AlignAuto);

	// Log file name edit box
	QHBox* box = new QHBox(mCmdOutputGroup);
	(new QWidget(box))->setFixedWidth(button->style().subRect(QStyle::SR_RadioButtonIndicator, button).width());   // indent the edit box
//	(new QWidget(box))->setFixedWidth(button->style().pixelMetric(QStyle::PM_ExclusiveIndicatorWidth));   // indent the edit box
	mCmdLogFileEdit = new LineEdit(LineEdit::Url, box);
	mCmdLogFileEdit->setAcceptDrops(true);
	QWhatsThis::add(mCmdLogFileEdit, i18n("Enter the name or path of the log file."));

	// Log file browse button.
	// The file browser dialogue is activated by the PickLogFileRadio class.
	QPushButton* browseButton = new QPushButton(box);
	browseButton->setPixmap(SmallIcon("fileopen"));
	browseButton->setFixedSize(browseButton->sizeHint());
	QToolTip::add(browseButton, i18n("Choose a file"));
	QWhatsThis::add(browseButton, i18n("Select a log file."));

	// Log output to file
	button = new PickLogFileRadio(browseButton, mCmdLogFileEdit, i18n_g_LogToFile(), mCmdOutputGroup, "cmdLog");
	button->setFixedSize(button->sizeHint());
	QWhatsThis::add(button,
	      i18n("Check to log the command output to a local file. The output will be appended to any existing contents of the file."));
	mCmdOutputGroup->insert(button, LOG_TO_FILE);
	layout->addWidget(button, 0, Qt::AlignAuto);
	layout->addWidget(box);

	// Discard output
	button = new RadioButton(i18n("Discard"), mCmdOutputGroup, "cmdDiscard");
	button->setFixedSize(button->sizeHint());
	QWhatsThis::add(button, i18n("Check to discard command output."));
	mCmdOutputGroup->insert(button, DISCARD_OUTPUT);
	layout->addWidget(button, 0, Qt::AlignAuto);

	// Top-adjust the controls
	mCmdPadding = new QHBox(mCommandFrame);
	frameLayout->addWidget(mCmdPadding);
	frameLayout->setStretchFactor(mCmdPadding, 1);
}

/******************************************************************************
 * Set up the email alarm dialog controls.
 */
void EditAlarmDlg::initEmail(QWidget* parent)
{
	mEmailFrame = new QFrame(parent);
	mEmailFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* layout = new QVBoxLayout(mEmailFrame, 0, spacingHint());
	QGridLayout* grid = new QGridLayout(layout, 3, 3, spacingHint());
	grid->setColStretch(1, 1);

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
		QWhatsThis::add(mEmailFromList,
		      i18n("Your email identity, used to identify you as the sender when sending email alarms."));
		grid->addMultiCellWidget(mEmailFromList, 0, 0, 1, 2);
	}

	// Email recipients
	QLabel* label = new QLabel(i18n_EmailTo(), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	mEmailToEdit = new LineEdit(LineEdit::Emails, mEmailFrame);
	mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
	QWhatsThis::add(mEmailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	grid->addWidget(mEmailToEdit, 1, 1);

	mEmailAddressButton = new QPushButton(mEmailFrame);
	mEmailAddressButton->setPixmap(SmallIcon("contents"));
	mEmailAddressButton->setFixedSize(mEmailAddressButton->sizeHint());
	connect(mEmailAddressButton, SIGNAL(clicked()), SLOT(openAddressBook()));
	QToolTip::add(mEmailAddressButton, i18n("Open address book"));
	QWhatsThis::add(mEmailAddressButton, i18n("Select email addresses from your address book."));
	grid->addWidget(mEmailAddressButton, 1, 2);

	// Email subject
	label = new QLabel(i18n_j_EmailSubject(), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 2, 0);

	mEmailSubjectEdit = new LineEdit(mEmailFrame);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	label->setBuddy(mEmailSubjectEdit);
	QWhatsThis::add(mEmailSubjectEdit, i18n("Enter the email subject."));
	grid->addMultiCellWidget(mEmailSubjectEdit, 2, 2, 1, 2);

	// Email body
	mEmailMessageEdit = new TextEdit(mEmailFrame);
	QWhatsThis::add(mEmailMessageEdit, i18n("Enter the email message."));
	layout->addWidget(mEmailMessageEdit);

	// Email attachments
	grid = new QGridLayout(layout, 2, 3, spacingHint());
	label = new QLabel(i18n("Attachment&s:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	mEmailAttachList = new QComboBox(true, mEmailFrame);
	mEmailAttachList->setMinimumSize(mEmailAttachList->sizeHint());
	mEmailAttachList->lineEdit()->setReadOnly(true);
QListBox* list = mEmailAttachList->listBox();
QRect rect = list->geometry();
list->setGeometry(rect.left() - 50, rect.top(), rect.width(), rect.height());
	label->setBuddy(mEmailAttachList);
	QWhatsThis::add(mEmailAttachList,
	      i18n("Files to send as attachments to the email."));
	grid->addWidget(mEmailAttachList, 0, 1);
	grid->setColStretch(1, 1);

	mEmailAddAttachButton = new QPushButton(i18n("Add..."), mEmailFrame);
	connect(mEmailAddAttachButton, SIGNAL(clicked()), SLOT(slotAddAttachment()));
	QWhatsThis::add(mEmailAddAttachButton, i18n("Add an attachment to the email."));
	grid->addWidget(mEmailAddAttachButton, 0, 2);

	mEmailRemoveButton = new QPushButton(i18n("Remo&ve"), mEmailFrame);
	connect(mEmailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
	QWhatsThis::add(mEmailRemoveButton, i18n("Remove the highlighted attachment from the email."));
	grid->addWidget(mEmailRemoveButton, 1, 2);

	// BCC email to sender
	mEmailBcc = new CheckBox(i18n_s_CopyEmailToSelf(), mEmailFrame);
	mEmailBcc->setFixedSize(mEmailBcc->sizeHint());
	QWhatsThis::add(mEmailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	grid->addMultiCellWidget(mEmailBcc, 1, 1, 0, 1, Qt::AlignAuto);
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
		mBgColourButton->setColour(event->bgColour());
		if (mTemplate)
		{
			// Editing a template
			int afterTime = event->isTemplate() ? event->templateAfterTime() : -1;
			bool noTime   = !afterTime;
			bool useTime  = !event->mainDateTime().isDateOnly();
			int button = mTemplateTimeGroup->id(noTime          ? mTemplateDefaultTime :
			                                    (afterTime > 0) ? mTemplateUseTimeAfter :
			                                    useTime         ? mTemplateUseTime : mTemplateAnyTime);
			mTemplateTimeGroup->setButton(button);
			mTemplateTimeAfter->setValue(afterTime > 0 ? afterTime : 1);
			if (!noTime && useTime)
				mTemplateTime->setValue(event->mainDateTime().time());
			else
				mTemplateTime->setValue(0);
		}
		else
		{
			if (event->isTemplate())
			{
				// Initialising from an alarm template: use current date
				QDateTime now = QDateTime::currentDateTime();
				int afterTime = event->templateAfterTime();
				if (afterTime >= 0)
				{
					mTimeWidget->setDateTime(now.addSecs(afterTime * 60));
					mTimeWidget->selectTimeFromNow();
				}
				else
				{
					QDate d = now.date();
					QTime t = event->startDateTime().time();
					bool dateOnly = event->startDateTime().isDateOnly();
					if (!dateOnly  &&  now.time() >= t)
						d = d.addDays(1);     // alarm time has already passed, so use tomorrow
					mTimeWidget->setDateTime(DateTime(QDateTime(d, t), dateOnly));
				}
			}
			else
			{
				mExpiredRecurrence = recurs && event->mainExpired();
				mTimeWidget->setDateTime(recurs || event->uidStatus() == KAEvent::EXPIRED ? event->startDateTime()
				                         : event->mainExpired() ? event->deferDateTime() : event->mainDateTime());
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
			mEmailAttachList->insertStringList(event->emailAttachments());

		mLateCancel->setMinutes(event->lateCancel(), event->startDateTime().isDateOnly(),
		                        TimePeriod::HOURS_MINUTES);
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
		mReminder->setMinutes(reminder, (mTimeWidget ? mTimeWidget->anyTime() : mTemplateAnyTime->isOn()));
		mReminder->setOnceOnly(event->reminderOnceOnly());
		mReminder->enableOnceOnly(event->recurs());
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(event->preAction(), event->postAction());
		mRecurrenceEdit->set(*event, (mTemplate || event->isTemplate()));   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		mTabs->setTabLabel(mTabs->page(mRecurPageIndex), recurText(*event));
		SoundPicker::Type soundType = event->speak()                ? SoundPicker::SPEAK
		                            : event->beep()                 ? SoundPicker::BEEP
		                            : !event->audioFile().isEmpty() ? SoundPicker::PLAY_FILE
		                            :                                 SoundPicker::NONE;
		mSoundPicker->set(soundType, event->audioFile(), event->soundVolume(),
		                  event->fadeVolume(), event->fadeSeconds(), event->repeatSound());
		CmdLogType logType = event->commandXterm()       ? EXEC_IN_TERMINAL
		                   : !event->logFile().isEmpty() ? LOG_TO_FILE
		                   :                               DISCARD_OUTPUT;
		if (logType == LOG_TO_FILE)
			mCmdLogFileEdit->setText(event->logFile());    // set file name before setting radio button
		mCmdOutputGroup->setButton(logType);
		mEmailToEdit->setText(event->emailAddresses(", "));
		mEmailSubjectEdit->setText(event->emailSubject());
		mEmailBcc->setChecked(event->emailBcc());
		if (mEmailFromList)
			mEmailFromList->setCurrentIdentity(event->emailFromId());
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
		mBgColourButton->setColour(Preferences::defaultBgColour());
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		if (mTemplate)
		{
			mTemplateTimeGroup->setButton(mTemplateTimeGroup->id(mTemplateDefaultTime));
			mTemplateTime->setValue(0);
			mTemplateTimeAfter->setValue(1);
		}
		else
			mTimeWidget->setDateTime(defaultTime);
		mActionGroup->setButton(mActionGroup->id(mMessageRadio));
		mLateCancel->setMinutes((Preferences::defaultLateCancel() ? 1 : 0), false, TimePeriod::HOURS_MINUTES);
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
	AlarmCalendar* cal = AlarmCalendar::templateCalendar();
	bool empty = cal->isOpen()  &&  !cal->events().count();
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
	if (mShowInKorganizer)
		mShowInKorganizer->setReadOnly(mReadOnly);

	// Message alarm controls
	mTextMessageEdit->setReadOnly(mReadOnly);
	mFileMessageEdit->setReadOnly(mReadOnly);
	mFontColourButton->setReadOnly(mReadOnly);
	mBgColourButton->setReadOnly(mReadOnly);
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
	for (int id = DISCARD_OUTPUT;  id < EXEC_IN_TERMINAL;  ++id)
		((RadioButton*)mCmdOutputGroup->find(id))->setReadOnly(mReadOnly);

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
				mCmdScriptEdit->setText(text);
			else
				mCmdCommandEdit->setText(text);
			break;
		case KAEvent::EMAIL:
			radio = mEmailRadio;
			mEmailMessageEdit->setText(text);
			break;
		case KAEvent::MESSAGE:
		default:
			radio = mMessageRadio;
			mTextMessageEdit->setText(text);
			mKMailSerialNumber = 0;
			if (alarmText.isEmail())
			{
				mKMailSerialNumber = alarmText.kmailSerialNumber();

				// Set up email fields also, in case the user wants an email alarm
				mEmailToEdit->setText(alarmText.to());
				mEmailSubjectEdit->setText(alarmText.subject());
				mEmailMessageEdit->setText(alarmText.body());
			}
			else if (alarmText.isScript())
			{
				// Set up command script field also, in case the user wants a command alarm
				mCmdScriptEdit->setText(text);
				mCmdTypeScript->setChecked(true);
			}
			break;
	}
	mActionGroup->setButton(mActionGroup->id(radio));
}

/******************************************************************************
 * Create an "acknowledgement confirmation required" checkbox.
 */
CheckBox* EditAlarmDlg::createConfirmAckCheckbox(QWidget* parent, const char* name)
{
	CheckBox* widget = new CheckBox(i18n_k_ConfirmAck(), parent, name);
	QWhatsThis::add(widget,
	      i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
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
		mSavedTemplateTimeType  = mTemplateTimeGroup->selected();
		mSavedTemplateTime      = mTemplateTime->time();
		mSavedTemplateAfterTime = mTemplateTimeAfter->value();
	}
	mSavedTypeRadio        = mActionGroup->selected();
	mSavedSoundType        = mSoundPicker->sound();
	mSavedSoundFile        = mSoundPicker->file();
	mSavedSoundVolume      = mSoundPicker->volume(mSavedSoundFadeVolume, mSavedSoundFadeSeconds);
	mSavedRepeatSound      = mSoundPicker->repeat();
	mSavedConfirmAck       = mConfirmAck->isChecked();
	mSavedFont             = mFontColourButton->font();
	mSavedFgColour         = mFontColourButton->fgColour();
	mSavedBgColour         = mFileRadio->isOn() ? mBgColourButton->colour() : mFontColourButton->bgColour();
	mSavedReminder         = mReminder->minutes();
	mSavedOnceOnly         = mReminder->isOnceOnly();
	if (mSpecialActionsButton)
	{
		mSavedPreAction  = mSpecialActionsButton->preAction();
		mSavedPostAction = mSpecialActionsButton->postAction();
	}
	checkText(mSavedTextFileCommandMessage, false);
	mSavedCmdScript        = mCmdTypeScript->isChecked();
	mSavedCmdOutputRadio   = mCmdOutputGroup->selected();
	mSavedCmdLogFile       = mCmdLogFileEdit->text();
	if (mEmailFromList)
		mSavedEmailFrom = mEmailFromList->currentIdentityName();
	mSavedEmailTo          = mEmailToEdit->text();
	mSavedEmailSubject     = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0;  i < mEmailAttachList->count();  ++i)
		mSavedEmailAttach += mEmailAttachList->text(i);
	mSavedEmailBcc         = mEmailBcc->isChecked();
	if (mTimeWidget)
		mSavedDateTime = mTimeWidget->getDateTime(0, false, false);
	mSavedLateCancel       = mLateCancel->minutes();
	mSavedAutoClose        = mLateCancel->isAutoClose();
	if (mShowInKorganizer)
		mSavedShowInKorganizer = mShowInKorganizer->isChecked();
	mSavedRecurrenceType   = mRecurrenceEdit->repeatType();
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
		||  mSavedTemplateTimeType != mTemplateTimeGroup->selected()
		||  mTemplateUseTime->isOn()  &&  mSavedTemplateTime != mTemplateTime->time()
		||  mTemplateUseTimeAfter->isOn()  &&  mSavedTemplateAfterTime != mTemplateTimeAfter->value())
			return true;
	}
	else
		if (mSavedDateTime != mTimeWidget->getDateTime(0, false, false))
			return true;
	if (mSavedTypeRadio        != mActionGroup->selected()
	||  mSavedLateCancel       != mLateCancel->minutes()
	||  mShowInKorganizer && mSavedShowInKorganizer != mShowInKorganizer->isChecked()
	||  textFileCommandMessage != mSavedTextFileCommandMessage
	||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType())
		return true;
	if (mMessageRadio->isOn()  ||  mFileRadio->isOn())
	{
		if (mSavedSoundType  != mSoundPicker->sound()
		||  mSavedConfirmAck != mConfirmAck->isChecked()
		||  mSavedFont       != mFontColourButton->font()
		||  mSavedFgColour   != mFontColourButton->fgColour()
		||  mSavedBgColour   != (mFileRadio->isOn() ? mBgColourButton->colour() : mFontColourButton->bgColour())
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
		if (mSavedSoundType == SoundPicker::PLAY_FILE)
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
	else if (mCommandRadio->isOn())
	{
		if (mSavedCmdScript      != mCmdTypeScript->isChecked()
		||  mSavedCmdOutputRadio != mCmdOutputGroup->selected())
			return true;
		if (mCmdOutputGroup->selectedId() == LOG_TO_FILE)
		{
			if (mSavedCmdLogFile != mCmdLogFileEdit->text())
				return true;
		}
	}
	else if (mEmailRadio->isOn())
	{
		QStringList emailAttach;
		for (int i = 0;  i < mEmailAttachList->count();  ++i)
			emailAttach += mEmailAttachList->text(i);
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
bool EditAlarmDlg::getEvent(KAEvent& event)
{
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
	QDateTime dt;
	if (!trial)
	{
		if (!mTemplate)
			dt = mAlarmDateTime.dateTime();
		else if (mTemplateUseTime->isOn())
			dt = QDateTime(QDate(2000,1,1), mTemplateTime->time());
	}
	KAEvent::Action type = getAlarmType();
	event.set(dt, text, (mFileRadio->isOn() ? mBgColourButton->colour() : mFontColourButton->bgColour()),
	          mFontColourButton->fgColour(), mFontColourButton->font(),
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
			event.setAudioFile(mSoundPicker->file(), volume, fadeVolume, fadeSecs);
			if (!trial)
				event.setReminder(mReminder->minutes(), mReminder->isOnceOnly());
			if (mSpecialActionsButton)
				event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction());
			break;
		}
		case KAEvent::EMAIL:
		{
			uint from = mEmailFromList ? mEmailFromList->currentIdentity() : 0;
			event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
			break;
		}
		case KAEvent::COMMAND:
			if (mCmdOutputGroup->selectedId() == LOG_TO_FILE)
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
			QDateTime now = QDateTime::currentDateTime();
			bool dateOnly = mAlarmDateTime.isDateOnly();
			if (dateOnly  &&  mAlarmDateTime.date() < now.date()
			||  !dateOnly  &&  mAlarmDateTime.rawDateTime() < now)
			{
				// A timed recurrence has an entered start date which has
				// already expired, so we must adjust the next repetition.
				event.setNextOccurrence(now);
			}
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
						if (remindTime > QDateTime::currentDateTime())
							deferral = false;    // ignore deferral if it's after next reminder
						else if (mDeferDateTime > remindTime)
							deferReminder = true;    // it's the reminder which is being deferred
					}
				}
				if (deferral)
					event.defer(mDeferDateTime, deferReminder, false);
			}
		}
		if (mTemplate)
		{
			int afterTime = mTemplateDefaultTime->isOn() ? 0
			              : mTemplateUseTimeAfter->isOn() ? mTemplateTimeAfter->value() : -1;
			event.setTemplate(mTemplateName->text(), afterTime);
		}
	}
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	bool displayAlarm = mMessageRadio->isOn() || mFileRadio->isOn();
	bool cmdAlarm     = mCommandRadio->isOn();
	bool emailAlarm   = mEmailRadio->isOn();
	return (displayAlarm && mSoundPicker->sound() == SoundPicker::BEEP           ? KAEvent::BEEP : 0)
	     | (displayAlarm && mSoundPicker->sound() == SoundPicker::SPEAK          ? KAEvent::SPEAK : 0)
	     | (displayAlarm && mSoundPicker->repeat()                               ? KAEvent::REPEAT_SOUND : 0)
	     | (displayAlarm && mConfirmAck->isChecked()                             ? KAEvent::CONFIRM_ACK : 0)
	     | (displayAlarm && mLateCancel->isAutoClose()                           ? KAEvent::AUTO_CLOSE : 0)
	     | (cmdAlarm     && mCmdTypeScript->isChecked()                          ? KAEvent::SCRIPT : 0)
	     | (cmdAlarm     && mCmdOutputGroup->selectedId() == EXEC_IN_TERMINAL    ? KAEvent::EXEC_IN_XTERM : 0)
	     | (emailAlarm   && mEmailBcc->isChecked()                               ? KAEvent::EMAIL_BCC : 0)
	     | (mShowInKorganizer && mShowInKorganizer->isChecked()                  ? KAEvent::COPY_KORGANIZER : 0)
	     | (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN            ? KAEvent::REPEAT_AT_LOGIN : 0)
	     | ((mTemplate ? mTemplateAnyTime->isOn() : mAlarmDateTime.isDateOnly()) ? KAEvent::ANY_TIME : 0)
	     | (mFontColourButton->defaultFont()                                     ? KAEvent::DEFAULT_FONT : 0);
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAEvent::Action EditAlarmDlg::getAlarmType() const
{
	return mFileRadio->isOn()    ? KAEvent::FILE
	     : mCommandRadio->isOn() ? KAEvent::COMMAND
	     : mEmailRadio->isOn()   ? KAEvent::EMAIL
	     :                         KAEvent::MESSAGE;
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
	KWin::setOnDesktop(winId(), mDesktop);    // ensure it displays on the desktop expected by the user
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
*  Called when the OK button is clicked.
*  Validate the input data.
*/
void EditAlarmDlg::slotOk()
{
	if (!stateChanged())
	{
		// No changes have been made except possibly to an existing deferral
		if (!mOnlyDeferred)
			reject();
		else
			accept();
		return;
	}
	RecurrenceEdit::RepeatType recurType = mRecurrenceEdit->repeatType();
	if (mTimeWidget
	&&  mTabs->currentPageIndex() == mRecurPageIndex  &&  recurType == RecurrenceEdit::AT_LOGIN)
		mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
	bool timedRecurrence = mRecurrenceEdit->isTimedRepeatType();    // does it recur other than at login?
	if (mTemplate)
	{
		// Check that the template name is not blank and is unique
		QString errmsg;
		QString name = mTemplateName->text();
		if (name.isEmpty())
			errmsg = i18n("You must enter a name for the alarm template");
		else if (name != mSavedTemplateName)
		{
			AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
			if (cal  &&  KAEvent::findTemplateName(*cal, name).valid())
				errmsg = i18n("Template name is already in use");
		}
		if (!errmsg.isEmpty())
		{
			mTemplateName->setFocus();
			KMessageBox::sorry(this, errmsg);
			return;
		}
	}
	else
	{
		QWidget* errWidget;
		mAlarmDateTime = mTimeWidget->getDateTime(0, !timedRecurrence, false, &errWidget);
		if (errWidget)
		{
			// It's more than just an existing deferral being changed, so the time matters
			mTabs->setCurrentPage(mMainPageIndex);
			errWidget->setFocus();
			mTimeWidget->getDateTime();   // display the error message now
			return;
		}
	}
	if (!checkCommandData()
	||  !checkEmailData())
		return;
	if (!mTemplate)
	{
		if (timedRecurrence)
		{
			QDateTime now = QDateTime::currentDateTime();
			if (mAlarmDateTime.date() < now.date()
			||  mAlarmDateTime.date() == now.date()
			    && !mAlarmDateTime.isDateOnly() && mAlarmDateTime.time() < now.time())
			{
				// A timed recurrence has an entered start date which
				// has already expired, so we must adjust it.
				KAEvent event;
				getEvent(event);     // this may adjust mAlarmDateTime
				if ((  mAlarmDateTime.date() < now.date()
				    || mAlarmDateTime.date() == now.date()
				       && !mAlarmDateTime.isDateOnly() && mAlarmDateTime.time() < now.time())
				&&  event.nextOccurrence(now, mAlarmDateTime, KAEvent::ALLOW_FOR_REPETITION) == KAEvent::NO_OCCURRENCE)
				{
					KMessageBox::sorry(this, i18n("Recurrence has already expired"));
					return;
				}
			}
		}
		QString errmsg;
		QWidget* errWidget = mRecurrenceEdit->checkData(mAlarmDateTime.dateTime(), errmsg);
		if (errWidget)
		{
			mTabs->setCurrentPage(mRecurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, errmsg);
			return;
		}
	}
	if (recurType != RecurrenceEdit::NO_RECUR)
	{
		KAEvent recurEvent;
		int longestRecurInterval = -1;
		int reminder = mReminder->minutes();
		if (reminder  &&  !mReminder->isOnceOnly())
		{
			mRecurrenceEdit->updateEvent(recurEvent, false);
			longestRecurInterval = recurEvent.longestRecurrenceInterval();
			if (longestRecurInterval  &&  reminder >= longestRecurInterval)
			{
				mTabs->setCurrentPage(mMainPageIndex);
				mReminder->setFocusOnCount();
				KMessageBox::sorry(this, i18n("Reminder period must be less than the recurrence interval, unless '%1' is checked."
				                             ).arg(Reminder::i18n_first_recurrence_only()));
				return;
			}
		}
		if (mRecurrenceEdit->subRepeatCount())
		{
			if (longestRecurInterval < 0)
			{
				mRecurrenceEdit->updateEvent(recurEvent, false);
				longestRecurInterval = recurEvent.longestRecurrenceInterval();
			}
			if (longestRecurInterval > 0
			&&  recurEvent.repeatInterval() * recurEvent.repeatCount() >= longestRecurInterval - reminder)
			{
				KMessageBox::sorry(this, i18n("The duration of a repetition within the recurrence must be less than the recurrence interval minus any reminder period"));
				mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
				return;
			}
			if (recurEvent.repeatInterval() % 1440
			&&  (mTemplate && mTemplateAnyTime->isOn()  ||  !mTemplate && mAlarmDateTime.isDateOnly()))
			{
				KMessageBox::sorry(this, i18n("For a repetition within the recurrence, its period must be in units of days or weeks for a date-only alarm"));
				mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
				return;
			}
		}
	}
	if (checkText(mAlarmMessage))
		accept();
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
		if (mEmailRadio->isOn())
		{
			if (!checkEmailData()
			||  KMessageBox::warningContinueCancel(this, i18n("Do you really want to send the email now to the specified recipient(s)?"),
			                                       i18n("Confirm Email"), i18n("&Send")) != KMessageBox::Continue)
				return;
		}
		KAEvent event;
		setEvent(event, text, true);
		void* proc = theApp()->execAlarm(event, event.firstAlarm(), false, false);
		if (proc)
		{
			if (mCommandRadio->isOn()  &&  mCmdOutputGroup->selectedId() != EXEC_IN_TERMINAL)
			{
				theApp()->commandMessage((ShellProcess*)proc, this);
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
				theApp()->commandMessage((ShellProcess*)proc, 0);
			}
			else if (mEmailRadio->isOn())
			{
				QString bcc;
				if (mEmailBcc->isChecked())
					bcc = i18n("\nBcc: %1").arg(Preferences::emailBccAddress());
				KMessageBox::information(this, i18n("Email sent to:\n%1%2").arg(mEmailAddresses.join("\n")).arg(bcc));
			}
		}
	}
}

/******************************************************************************
*  Called when the Cancel button is clicked.
*/
void EditAlarmDlg::slotCancel()
{
	reject();
}

/******************************************************************************
*  Called when the Load Template button is clicked.
*  Prompt to select a template and initialise the dialogue with its contents.
*/
void EditAlarmDlg::slotDefault()
{
	TemplatePickDlg dlg(this, "templPickDlg");
	if (dlg.exec() == QDialog::Accepted)
		initialise(dlg.selectedTemplate());
}

/******************************************************************************
 * Called when the Change deferral button is clicked.
 */
void EditAlarmDlg::slotEditDeferral()
{
	if (!mTimeWidget)
		return;
	bool limit = true;
	int repeatInterval;
	int repeatCount = mRecurrenceEdit->subRepeatCount(&repeatInterval);
	DateTime start = mTimeWidget->getDateTime(0, !repeatCount, !mExpiredRecurrence);
	if (!start.isValid())
	{
		if (!mExpiredRecurrence)
			return;
		limit = false;
	}
	QDateTime now = QDateTime::currentDateTime();
	if (limit)
	{
		if (repeatCount  &&  start < now)
		{
			// Sub-repetition - find the time of the next one
			repeatInterval *= 60;
			int repetition = (start.secsTo(now) + repeatInterval - 1) / repeatInterval;
			if (repetition > repeatCount)
			{
				mTimeWidget->getDateTime();    // output the appropriate error message
				return;
			}
			start = start.addSecs(repetition * repeatInterval);
		}
	}

	bool deferred = mDeferDateTime.isValid();
	DeferAlarmDlg deferDlg(i18n("Defer Alarm"), (deferred ? mDeferDateTime : DateTime(now.addSecs(60))),
	                       deferred, this, "EditDeferDlg");
	if (limit)
	{
		// Don't allow deferral past the next recurrence
		int reminder = mReminder->minutes();
		if (reminder)
		{
			DateTime remindTime = start.addMins(-reminder);
			if (QDateTime::currentDateTime() < remindTime)
				start = remindTime;
		}
		deferDlg.setLimit(start.addSecs(-60));
	}
	if (deferDlg.exec() == QDialog::Accepted)
	{
		mDeferDateTime = deferDlg.getDateTime();
		mDeferTimeLabel->setText(mDeferDateTime.isValid() ? mDeferDateTime.formatLocale() : QString::null);
	}
}

/******************************************************************************
*  Called when the main page is shown.
*  Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
	slotAlarmTypeChanged(-1);
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
	mRecurPageIndex = mTabs->currentPageIndex();
	if (!mReadOnly  &&  !mTemplate)
	{
		QDateTime now = QDateTime::currentDateTime();
		mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
		bool expired = (mAlarmDateTime.dateTime() < now);
		if (mRecurSetDefaultEndDate)
		{
			mRecurrenceEdit->setDefaultEndDate(expired ? now.date() : mAlarmDateTime.date());
			mRecurSetDefaultEndDate = false;
		}
		mRecurrenceEdit->setStartDate(mAlarmDateTime.date(), now.date());
		if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
			mRecurrenceEdit->setEndDateTime(expired ? now : mAlarmDateTime);
	}
	mRecurPageShown = true;
}

/******************************************************************************
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*  Enables/disables controls depending on at-login setting.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
	bool atLogin = (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN);
	if (!mTemplate)
	{
		bool recurs = (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR);
		if (mDeferGroup)
			mDeferGroup->setEnabled(recurs);
		mTimeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
		if (atLogin)
		{
			mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
			mRecurrenceEdit->setEndDateTime(mAlarmDateTime.dateTime());
		}
		mReminder->enableOnceOnly(recurs && !atLogin);
	}
	mReminder->setEnabled(!atLogin);
        mLateCancel->setEnabled(!atLogin);
        if (mShowInKorganizer)
                mShowInKorganizer->setEnabled(!atLogin);
	slotRecurFrequencyChange();
}

/******************************************************************************
*  Called when the recurrence frequency selection changes, or the sub-
*  repetition interval changes.
*  Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
	slotSetSubRepetition();
	KAEvent event;
	mRecurrenceEdit->updateEvent(event, false);
	mTabs->setTabLabel(mTabs->page(mRecurPageIndex), recurText(event));
}

/******************************************************************************
*  Called when the Repetition within Recurrence button has been pressed to
*  display the sub-repetition dialog.
*  Alarm repetition has the following restrictions:
*  1) Not allowed for a repeat-at-login alarm
*  2) For a date-only alarm, the repeat interval must be a whole number of days.
*  3) The overall repeat duration must be less than the recurrence interval.
*/
void EditAlarmDlg::slotSetSubRepetition()
{
	bool dateOnly = mTemplate ? mTemplateAnyTime->isOn() : mTimeWidget->anyTime();
	mRecurrenceEdit->setSubRepetition(mReminder->minutes(), dateOnly);
}

/******************************************************************************
*  Validate and convert command alarm data.
*/
bool EditAlarmDlg::checkCommandData()
{
	if (mCommandRadio->isOn()  &&  mCmdOutputGroup->selectedId() == LOG_TO_FILE)
	{
		// Validate the log file name
		QString file = mCmdLogFileEdit->text();
		QFileInfo info(file);
		QDir::setCurrent(QDir::homeDirPath());
		bool err = file.isEmpty()  ||  info.isDir();
		if (!err)
		{
			if (info.exists())
			{
				err = !info.isWritable();
			}
			else
			{
				QFileInfo dirinfo(info.dirPath(true));    // get absolute directory path
				err = (!dirinfo.isDir()  ||  !dirinfo.isWritable());
			}
		}
		if (err)
		{
			mTabs->setCurrentPage(mMainPageIndex);
			mCmdLogFileEdit->setFocus();
			KMessageBox::sorry(this, i18n("Log file must be the name or path of a local file, with write permission."));
			return false;
		}
		// Convert the log file to an absolute path
		mCmdLogFileEdit->setText(info.absFilePath());
	}
	return true;
}

/******************************************************************************
*  Convert the email addresses to a list, and validate them. Convert the email
*  attachments to a list.
*/
bool EditAlarmDlg::checkEmailData()
{
	if (mEmailRadio->isOn())
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
				KMessageBox::error(this, i18n("Invalid email address:\n%1").arg(bad));
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
		for (int i = 0;  i < mEmailAttachList->count();  ++i)
		{
			QString att = mEmailAttachList->text(i);
			switch (KAMail::checkAttachment(att))
			{
				case 1:
					mEmailAttachments.append(att);
					break;
				case 0:
					break;      // empty
				case -1:
					mEmailAttachList->setFocus();
					KMessageBox::error(this, i18n("Invalid email attachment:\n%1").arg(att));
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
void EditAlarmDlg::slotAlarmTypeChanged(int)
{
	bool displayAlarm = false;
	QWidget* focus = 0;
	if (mMessageRadio->isOn())
	{
		mFileBox->hide();
		mFilePadding->hide();
		mTextMessageEdit->show();
		mFontColourButton->show();
		mBgColourBox->hide();
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
	else if (mFileRadio->isOn())
	{
		mTextMessageEdit->hide();
		mFileBox->show();
		mFilePadding->show();
		mFontColourButton->hide();
		mBgColourBox->show();
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
	else if (mCommandRadio->isOn())
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
	else if (mEmailRadio->isOn())
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
void EditAlarmDlg::slotTemplateTimeType(int)
{
	mTemplateTime->setEnabled(mTemplateUseTime->isOn());
	mTemplateTimeAfter->setEnabled(mTemplateUseTimeAfter->isOn());
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
	QString addrs = mEmailToEdit->text().stripWhiteSpace();
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
	QString url = KAlarm::browseFile(i18n("Choose File to Attach"), mAttachDefaultDir, QString::null,
	                                 QString::null, KFile::ExistingOnly, this, "pickAttachFile");
	if (!url.isEmpty())
	{
		mEmailAttachList->insertItem(url);
		mEmailAttachList->setCurrentItem(mEmailAttachList->count() - 1);   // select the new item
		mEmailRemoveButton->setEnabled(true);
		mEmailAttachList->setEnabled(true);
	}
}

/******************************************************************************
 * Remove the currently selected attachment from the email.
 */
void EditAlarmDlg::slotRemoveAttachment()
{
	int item = mEmailAttachList->currentItem();
	mEmailAttachList->removeItem(item);
	int count = mEmailAttachList->count();
	if (item >= count)
		mEmailAttachList->setCurrentItem(count - 1);
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
	if (mMessageRadio->isOn())
		result = mTextMessageEdit->text();
	else if (mEmailRadio->isOn())
		result = mEmailMessageEdit->text();
	else if (mCommandRadio->isOn())
	{
		if (mCmdTypeScript->isChecked())
			result = mCmdScriptEdit->text();
		else
			result = mCmdCommandEdit->text();
		result = result.stripWhiteSpace();
	}
	else if (mFileRadio->isOn())
	{
		QString alarmtext = mFileMessageEdit->text().stripWhiteSpace();
		// Convert any relative file path to absolute
		// (using home directory as the default)
		enum Err { NONE = 0, BLANK, NONEXISTENT, DIRECTORY, UNREADABLE, NOT_TEXT_IMAGE };
		Err err = NONE;
		KURL url;
		int i = alarmtext.find(QString::fromLatin1("/"));
		if (i > 0  &&  alarmtext[i - 1] == ':')
		{
			url = alarmtext;
			url.cleanPath();
			alarmtext = url.prettyURL();
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
			QDir::setCurrent(QDir::homeDirPath());
			alarmtext = info.absFilePath();
			url.setPath(alarmtext);
			alarmtext = QString::fromLatin1("file:") + alarmtext;
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
				case NONEXISTENT:     errmsg = i18n("%1\nnot found");  break;
				case DIRECTORY:       errmsg = i18n("%1\nis a folder");  break;
				case UNREADABLE:      errmsg = i18n("%1\nis not readable");  break;
				case NOT_TEXT_IMAGE:  errmsg = i18n("%1\nappears not to be a text or image file");  break;
				case NONE:
				default:
					break;
			}
			if (KMessageBox::warningContinueCancel(const_cast<EditAlarmDlg*>(this), errmsg.arg(alarmtext))
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
TextEdit::TextEdit(QWidget* parent, const char* name)
	: KTextEdit(parent, name)
{
	QSize tsize = sizeHint();
	tsize.setHeight(fontMetrics().lineSpacing()*13/4 + 2*frameWidth());
	setMinimumSize(tsize);
}

void TextEdit::dragEnterEvent(QDragEnterEvent* e)
{
	if (KCal::ICalDrag::canDecode(e))
		e->accept(false);   // don't accept "text/calendar" objects
	KTextEdit::dragEnterEvent(e);
}
