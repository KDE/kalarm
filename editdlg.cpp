/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm or alarm template
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
#include <kabc/vcardconverter.h>
#include <kdebug.h>

#include <libkdepim/maillistdrag.h>
#include <libkdepim/kvcarddrag.h>

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
#include "mainwindow.h"
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

inline QString recurText(const KAEvent& event)
{
	return QString::fromLatin1("%1 / %2").arg(event.recurrenceText()).arg(event.repetitionText());
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_ConfirmAck()         { return i18n("Confirm acknowledgment"); }
QString EditAlarmDlg::i18n_k_ConfirmAck()       { return i18n("Confirm ac&knowledgment"); }
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
	: KDialogBase(parent, name, true, caption,
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
	mTabs->setMargin(marginHint() + marginKDE2);

	QVBox* mainPageBox = new QVBox(mTabs);
	mainPageBox->setSpacing(spacingHint());
	mTabs->addTab(mainPageBox, i18n("&Alarm"));
	mMainPageIndex = 0;
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Recurrence tab
	QVBox* recurTab = new QVBox(mTabs);
	mainPageBox->setSpacing(spacingHint());
	mTabs->addTab(recurTab, i18n("&Recurrence"));
	mRecurPageIndex = 1;
	mRecurrenceEdit = new RecurrenceEdit(readOnly, recurTab, "recurPage");
	connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));
	connect(mRecurrenceEdit, SIGNAL(frequencyChanged()), SLOT(slotRecurFrequencyChange()));

	// Alarm action

	mActionGroup = new QButtonGroup(i18n("Action"), mainPage, "actionGroup");
	connect(mActionGroup, SIGNAL(clicked(int)), SLOT(slotAlarmTypeClicked(int)));
	topLayout->addWidget(mActionGroup, 1);
	QGridLayout* grid = new QGridLayout(mActionGroup, 3, 5, marginKDE2 + marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	// Message radio button
	mMessageRadio = new RadioButton(i18n("Te&xt"), mActionGroup, "messageButton");
	mMessageRadio->setFixedSize(mMessageRadio->sizeHint());
	QWhatsThis::add(mMessageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(mMessageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	mFileRadio = new RadioButton(i18n("&File"), mActionGroup, "fileButton");
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
	initCommand(mActionGroup);
	initEmail(mActionGroup);
	mAlarmTypeStack = new QWidgetStack(mActionGroup);
	grid->addMultiCellWidget(mAlarmTypeStack, 2, 2, 0, 6);
	grid->setRowStretch(2, 1);
	mAlarmTypeStack->addWidget(mDisplayAlarmsFrame, 0);
	mAlarmTypeStack->addWidget(mCommandFrame, 1);
	mAlarmTypeStack->addWidget(mEmailFrame, 1);

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

	QBoxLayout* layout = new QHBoxLayout(topLayout);

	// Date and time entry
	if (mTemplate)
	{
		mTemplateTimeGroup = new ButtonGroup(i18n("Time"), mainPage, "templateGroup");
		connect(mTemplateTimeGroup, SIGNAL(buttonSet(int)), SLOT(slotTemplateTimeType(int)));
		layout->addWidget(mTemplateTimeGroup);
		QBoxLayout* vlayout = new QVBoxLayout(mTemplateTimeGroup, marginKDE2 + marginHint(), spacingHint());
		vlayout->addSpacing(fontMetrics().lineSpacing()/2);

		mTemplateDefaultTime = new RadioButton(i18n("&Default time"), mTemplateTimeGroup, "templateDefTimeButton");
		mTemplateDefaultTime->setFixedSize(mTemplateDefaultTime->sizeHint());
		mTemplateDefaultTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateDefaultTime,
		      i18n("Do not specify a start time for alarms based on this template. "
		           "The normal default start time will be used."));
		vlayout->addWidget(mTemplateDefaultTime, 0, Qt::AlignAuto);

		QHBox* box = new QHBox(mTemplateTimeGroup);
		box->setSpacing(spacingHint());
		vlayout->addWidget(box);
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

		mTemplateAnyTime = new RadioButton(i18n("An&y time"), mTemplateTimeGroup, "templateAnyTimeButton");
		mTemplateAnyTime->setFixedSize(mTemplateAnyTime->sizeHint());
		mTemplateAnyTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateAnyTime,
		      i18n("Set the '%1' option for alarms based on this template.").arg(i18n("Any time")));
		vlayout->addWidget(mTemplateAnyTime, 0, Qt::AlignAuto);

		layout->addStretch();
	}
	else
	{
		mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME, mainPage, "timeGroup");
		connect(mTimeWidget, SIGNAL(anyTimeToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		topLayout->addWidget(mTimeWidget);
	}

	// Recurrence type display
	layout = new QHBoxLayout(topLayout);
	QHBox* box = new QHBox(mainPage);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mRecurrenceText = new QLabel(box);
	QWhatsThis::add(box,
	      i18n("How often the alarm recurs.\nThe times shown are those configured in the Recurrence tab and in the Simple Repetition dialog."));
	box->setFixedHeight(box->sizeHint().height());
	layout->addWidget(box);
	layout->addSpacing(2*KDialog::spacingHint());

	// Simple repetition button
	mSimpleRepetition = new RepetitionButton(i18n("Simple Repetition"), true, mainPage);
	mSimpleRepetition->setFixedSize(mSimpleRepetition->sizeHint());
	connect(mSimpleRepetition, SIGNAL(needsInitialisation()), SLOT(slotSetSimpleRepetition()));
	connect(mSimpleRepetition, SIGNAL(changed()), SLOT(slotRecurFrequencyChange()));
	QWhatsThis::add(mSimpleRepetition, i18n("Set up a simple, or additional, alarm repetition"));
	layout->addWidget(mSimpleRepetition);
	layout->addStretch();

	// Late cancel selector - default = allow late display
	mLateCancel = new LateCancelSelector(true, mainPage);
	topLayout->addWidget(mLateCancel, 0, Qt::AlignAuto);

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	// Initialise the state of all controls according to the specified event, if any
	initialise(event);
	if (mTemplateName)
		mTemplateName->setFocus();

	// Save the initial state of all controls so that we can later tell if they have changed
	saveState(event);

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
	mTextMessageEdit->setWordWrap(QTextEdit::NoWrap);
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
	connect(mFileBrowseButton, SIGNAL(clicked()), SLOT(slotBrowseFile()));
	QToolTip::add(mFileBrowseButton, i18n("Choose a file"));
	QWhatsThis::add(mFileBrowseButton, i18n("Select a text or image file to display."));

	// Colour choice drop-down list
	QBoxLayout* layout = new QHBoxLayout(frameLayout);
	QHBox* box;
	mBgColourChoose = createBgColourChooser(&box, mDisplayAlarmsFrame);
	mBgColourChoose->setFixedSize(mBgColourChoose->sizeHint());
	connect(mBgColourChoose, SIGNAL(highlighted(const QColor&)), SLOT(slotBgColourSelected(const QColor&)));
	layout->addWidget(box);
	layout->addSpacing(2*KDialog::spacingHint());
	layout->addStretch();

	// Font and colour choice drop-down list
	mFontColourButton = new FontColourButton(mDisplayAlarmsFrame);
	mFontColourButton->setFixedSize(mFontColourButton->sizeHint());
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	layout->addWidget(mFontColourButton);

	// Sound checkbox and file selector
	mSoundPicker = new SoundPicker(mDisplayAlarmsFrame);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	frameLayout->addWidget(mSoundPicker, 0, Qt::AlignAuto);

	// Reminder
	static const QString reminderText = i18n("Enter how long in advance of the main alarm to display a reminder alarm.");
	mReminder = new Reminder(i18n("Rem&inder:"),
	                         i18n("Check to additionally display a reminder in advance of the main alarm time(s)."),
	                         QString("%1\n\n%2").arg(reminderText).arg(TimeSpinBox::shiftWhatsThis()),
	                         true, true, mDisplayAlarmsFrame);
	mReminder->setFixedSize(mReminder->sizeHint());
	frameLayout->addWidget(mReminder, 0, Qt::AlignAuto);

	// Acknowledgement confirmation required - default = no confirmation
	layout = new QHBoxLayout(frameLayout);
	mConfirmAck = createConfirmAckCheckbox(mDisplayAlarmsFrame);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	layout->addWidget(mConfirmAck);
	layout->addSpacing(2*KDialog::spacingHint());
	layout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(i18n("Special Actions..."), mDisplayAlarmsFrame);
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
	QBoxLayout* layout = new QVBoxLayout(mCommandFrame);

	mCommandMessageEdit = new LineEdit(LineEdit::Url, mCommandFrame);
	QWhatsThis::add(mCommandMessageEdit, i18n("Enter a shell command to execute."));
	layout->addWidget(mCommandMessageEdit);
	layout->addStretch();
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
	if (Preferences::instance()->emailFrom() == Preferences::MAIL_FROM_KMAIL)
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
			bool noTime = event->isTemplate() && event->usingDefaultTime();
			bool useTime = !event->mainDateTime().isDateOnly();
			int button = mTemplateTimeGroup->id(noTime ? mTemplateDefaultTime :
			                                    useTime ? mTemplateUseTime : mTemplateAnyTime);
			mTemplateTimeGroup->setButton(button);
			if (!noTime && useTime)
				mTemplateTime->setValue(event->mainDateTime().time());
		}
		else
		{
			if (event->isTemplate())
			{
				// Initialising from an alarm template: use current date
				QDateTime now = QDateTime::currentDateTime();
				if (event->usingDefaultTime())
					mTimeWidget->setDateTime(now.addSecs(60));
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
				mTimeWidget->setDateTime(!event->mainExpired() ? event->mainDateTime()
				                         : recurs ? DateTime() : event->deferDateTime());
			}
		}

		KAEvent::Action action = event->action();
		setAction(action, event->cleanText());
		if (action == KAEvent::EMAIL)
			mEmailAttachList->insertStringList(event->emailAttachments());

		mLateCancel->setMinutes(event->lateCancel(), event->startDateTime().isDateOnly(),
		                        TimePeriod::HOURS_MINUTES);
		mLateCancel->showAutoClose(action == KAEvent::MESSAGE || action == KAEvent::FILE);
		mLateCancel->setAutoClose(event->autoClose());
		mLateCancel->setFixedSize(mLateCancel->sizeHint());
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
		mSimpleRepetition->set(event->repeatInterval(), event->repeatCount());
		mRecurrenceText->setText(recurText(*event));
		mRecurrenceEdit->set(*event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		mSoundPicker->set(event->beep(), event->audioFile(), event->soundVolume(), event->repeatSound());
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
		Preferences* preferences = Preferences::instance();
		mFontColourButton->setDefaultFont();
		mFontColourButton->setBgColour(preferences->defaultBgColour());
		mFontColourButton->setFgColour(preferences->defaultFgColour());
		mBgColourChoose->setColour(preferences->defaultBgColour());     // set colour before setting alarm type buttons
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		if (mTemplate)
		{
			mTemplateTimeGroup->setButton(mTemplateTimeGroup->id(mTemplateDefaultTime));
			mTemplateTime->setValue(0);
		}
		else
			mTimeWidget->setDateTime(defaultTime);
		mActionGroup->setButton(mActionGroup->id(mMessageRadio));
		mLateCancel->setMinutes((preferences->defaultLateCancel() ? 1 : 0), false, TimePeriod::HOURS_MINUTES);
		mLateCancel->showAutoClose(true);
		mLateCancel->setAutoClose(preferences->defaultAutoClose());
		mLateCancel->setFixedSize(mLateCancel->sizeHint());
		mConfirmAck->setChecked(preferences->defaultConfirmAck());
		mReminder->setMinutes(0, false);
		mReminder->enableOnceOnly(false);
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(preferences->defaultPreAction(), preferences->defaultPostAction());
		mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		slotRecurFrequencyChange();      // update the Recurrence text
		mSoundPicker->set(preferences->defaultBeep(), preferences->defaultSoundFile(),
		                  preferences->defaultSoundVolume(), preferences->defaultSoundRepeat());
		mEmailBcc->setChecked(preferences->defaultEmailBcc());
	}

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
	mSimpleRepetition->setReadOnly(mReadOnly);

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
	mCommandMessageEdit->setReadOnly(mReadOnly);

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
	QRadioButton* radio;
	switch (action)
	{
		case KAEvent::FILE:
			radio = mFileRadio;
			mFileMessageEdit->setText(text);
			break;
		case KAEvent::COMMAND:
			radio = mCommandRadio;
			mCommandMessageEdit->setText(text);
			break;
		case KAEvent::EMAIL:
			radio = mEmailRadio;
			mEmailMessageEdit->setText(text);
			break;
		case KAEvent::MESSAGE:
		default:
			radio = mMessageRadio;
			mTextMessageEdit->setText(text);
			if (alarmText.isEmail())
			{
				// Set up email fields also, in case the user wants an email alarm
				mEmailToEdit->setText(alarmText.to());
				mEmailSubjectEdit->setText(alarmText.subject());
				mEmailMessageEdit->setText(alarmText.body());
			}
			break;
	}
	mActionGroup->setButton(mActionGroup->id(radio));
}

/******************************************************************************
 * Create a widget to choose the alarm message background colour.
 */
ColourCombo* EditAlarmDlg::createBgColourChooser(QHBox** box, QWidget* parent, const char* name)
{
	*box = new QHBox(parent);   // this is to control the QWhatsThis text display area
	QLabel* label = new QLabel(i18n("&Background color:"), *box);
	label->setFixedSize(label->sizeHint());
	ColourCombo* widget = new ColourCombo(*box, name);
	QSize size = widget->sizeHint();
	widget->setMinimumHeight(size.height() + 4);
	QToolTip::add(widget, i18n("Message color"));
	label->setBuddy(widget);
	(*box)->setFixedHeight((*box)->sizeHint().height());
	QWhatsThis::add(*box, i18n("Choose the background color for the alarm message."));
	return widget;
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
	mSavedEvent = 0;
	if (event)
		mSavedEvent = new KAEvent(*event);
	if (mTemplate)
	{
		mSavedTemplateName     = mTemplateName->text();
		mSavedTemplateTimeType = mTemplateTimeGroup->selected();
		mSavedTemplateTime     = mTemplateTime->time();
	}
	mSavedTypeRadio      = mActionGroup->selected();
	mSavedBeep           = mSoundPicker->beep();
	mSavedSoundFile      = mSoundPicker->file();
	mSavedSoundVolume    = mSoundPicker->volume();
	mSavedRepeatSound    = mSoundPicker->repeat();
	mSavedConfirmAck     = mConfirmAck->isChecked();
	mSavedFont           = mFontColourButton->font();
	mSavedFgColour       = mFontColourButton->fgColour();
	mSavedBgColour       = mBgColourChoose->color();
	mSavedReminder       = mReminder->minutes();
	mSavedOnceOnly       = mReminder->isOnceOnly();
	if (mSpecialActionsButton)
	{
		mSavedPreAction  = mSpecialActionsButton->preAction();
		mSavedPostAction = mSpecialActionsButton->postAction();
	}
	checkText(mSavedTextFileCommandMessage, false);
	if (mEmailFromList)
		mSavedEmailFrom = mEmailFromList->currentIdentityName();
	mSavedEmailTo        = mEmailToEdit->text();
	mSavedEmailSubject   = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0;  i < mEmailAttachList->count();  ++i)
		mSavedEmailAttach += mEmailAttachList->text(i);
	mSavedEmailBcc       = mEmailBcc->isChecked();
	if (mTimeWidget)
		mSavedDateTime = mTimeWidget->getDateTime(false, false);
	mSavedLateCancel     = mLateCancel->minutes();
	mSavedAutoClose      = mLateCancel->isAutoClose();
	mSavedRecurrenceType = mRecurrenceEdit->repeatType();
	mSavedRepeatInterval = mSimpleRepetition->interval();
	mSavedRepeatCount    = mSimpleRepetition->count();
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
		||  mSavedTemplateTime     != mTemplateTime->time())
			return true;
	}
	else
		if (mSavedDateTime != mTimeWidget->getDateTime(false, false))
			return true;
	if (mSavedTypeRadio        != mActionGroup->selected()
	||  mSavedLateCancel       != mLateCancel->minutes()
	||  textFileCommandMessage != mSavedTextFileCommandMessage
	||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType()
	||  mSavedRepeatInterval   != mSimpleRepetition->interval()
	||  mSavedRepeatCount      != mSimpleRepetition->count())
		return true;
	if (mMessageRadio->isOn()  ||  mFileRadio->isOn())
	{
		if (mSavedBeep       != mSoundPicker->beep()
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
		if (!mSavedBeep)
		{
			if (mSavedSoundFile != mSoundPicker->file())
				return true;
			if (!mSavedSoundFile.isEmpty())
			{
				if (mSavedRepeatSound != mSoundPicker->repeat()
				||  mSavedSoundVolume != mSoundPicker->volume())
					return true;
			}
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
			dt.setTime(mTemplateTime->time());
	}
	KAEvent::Action type = getAlarmType();
	event.set(dt, text, mBgColourChoose->color(), mFontColourButton->fgColour(), mFontColourButton->font(),
			  type, (trial ? 0 : mLateCancel->minutes()), getAlarmFlags());
	switch (type)
	{
		case KAEvent::MESSAGE:
		case KAEvent::FILE:
			event.setAudioFile(mSoundPicker->file(), mSoundPicker->volume());
			if (!trial)
				event.setReminder(mReminder->minutes(), mReminder->isOnceOnly());
			if (mSpecialActionsButton)
				event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction());
			break;
		case KAEvent::EMAIL:
		{
			QString from;
			if (mEmailFromList)
				from = mEmailFromList->currentIdentityName();
			event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
			break;
		}
		case KAEvent::COMMAND:
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
		if (mSimpleRepetition->count())
			event.setRepetition(mSimpleRepetition->interval(), mSimpleRepetition->count());
		if (mTemplate)
			event.setTemplate(mTemplateName->text(), mTemplateDefaultTime->isOn());
	}
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	bool displayAlarm = mMessageRadio->isOn() || mFileRadio->isOn();
	return (displayAlarm && mSoundPicker->beep()          ? KAEvent::BEEP : 0)
	     | (displayAlarm && mSoundPicker->repeat()        ? KAEvent::REPEAT_SOUND : 0)
	     | (displayAlarm && mConfirmAck->isChecked()      ? KAEvent::CONFIRM_ACK : 0)
	     | (displayAlarm && mLateCancel->isAutoClose()    ? KAEvent::AUTO_CLOSE : 0)
	     | (mEmailRadio->isOn() && mEmailBcc->isChecked() ? KAEvent::EMAIL_BCC : 0)
	     | (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN ? KAEvent::REPEAT_AT_LOGIN : 0)
	     | ((mTemplate ? mTemplateAnyTime->isOn() : mAlarmDateTime.isDateOnly()) ? KAEvent::ANY_TIME : 0)
	     | (mFontColourButton->defaultFont()              ? KAEvent::DEFAULT_FONT : 0);
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
		mAlarmDateTime = mTimeWidget->getDateTime(!(timedRecurrence || repeated), false, &errWidget);
		if (errWidget)
		{
			// It's more than just an existing deferral being changed, so the time matters
			mTabs->setCurrentPage(mMainPageIndex);
			errWidget->setFocus();
			mTimeWidget->getDateTime();   // display the error message now
			return;
		}
	}
	if (!checkEmailData())
		return;
	if (!mTemplate)
	{
		if (timedRecurrence)
		{
			QDateTime now = QDateTime::currentDateTime();
			if (mAlarmDateTime.date() < now.date()
			||  !mAlarmDateTime.isDateOnly() && mAlarmDateTime.time() < now.time())
			{
				// A timed recurrence has an entered start date which
				// has already expired, so we must adjust it.
				KAEvent event;
				getEvent(event);     // this may adjust mAlarmDateTime
				if ((mAlarmDateTime.date() < now.date()
				     ||  !mAlarmDateTime.isDateOnly() && mAlarmDateTime.time() < now.time())
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
					mTabs->setCurrentPage(mMainPageIndex);
					mReminder->setFocusOnCount();
					KMessageBox::sorry(this, i18n("Reminder period must be less than the recurrence interval, unless '%1' is checked."
					                             ).arg(Reminder::i18n_first_recurrence_only()));
					return;
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
					return;
				}
				// fall through to NO_RECUR
			case RecurrenceEdit::NO_RECUR:    // no restriction on repeat duration
				if (mSimpleRepetition->interval() % 1440
				&&  (mTemplate && mTemplateAnyTime->isOn()  ||  !mTemplate && mAlarmDateTime.isDateOnly()))
				{
					KMessageBox::sorry(this, i18n("Simple alarm repetition period must be in units of days or weeks for a date-only alarm"));
					mSimpleRepetition->activate();   // display the alarm repetition dialog again
					return;
				}
				break;
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
			if (mCommandRadio->isOn())
			{
				theApp()->commandMessage((ShellProcess*)proc, this);
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
				theApp()->commandMessage((ShellProcess*)proc, 0);
			}
			else if (mEmailRadio->isOn())
			{
				QString bcc;
				if (mEmailBcc->isChecked())
					bcc = i18n("\nBcc: %1").arg(Preferences::instance()->emailBccAddress());
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
	int repeatCount = mSimpleRepetition->count();
	DateTime start = mTimeWidget->getDateTime(!repeatCount, !mExpiredRecurrence);
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
	                       deferred, this, "deferDlg");
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
	slotAlarmTypeClicked(-1);
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
		mAlarmDateTime = mTimeWidget->getDateTime(false, false);
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
			mAlarmDateTime = mTimeWidget->getDateTime(false, false);
			mRecurrenceEdit->setEndDateTime(mAlarmDateTime.dateTime());
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
	bool dateOnly = mTemplate ? mTemplateAnyTime->isOn() : mTimeWidget->anyTime();
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
void EditAlarmDlg::slotAlarmTypeClicked(int)
{
	bool displayAlarm = false;
	QWidget* focus = 0;
	if (mMessageRadio->isOn())
	{
		mFileBox->hide();
		mFilePadding->hide();
		mTextMessageEdit->show();
		mFontColourButton->show();
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
		mAlarmTypeStack->raiseWidget(mDisplayAlarmsFrame);
		focus = mTextMessageEdit;
		displayAlarm = true;
	}
	else if (mFileRadio->isOn())
	{
		mTextMessageEdit->hide();
		mFileBox->show();
		mFilePadding->show();
		mFontColourButton->hide();
		setButtonWhatsThis(Try, i18n("Display the file now"));
		mAlarmTypeStack->raiseWidget(mDisplayAlarmsFrame);
		mFileMessageEdit->setNoSelect();
		focus = mFileMessageEdit;
		displayAlarm = true;
	}
	else if (mCommandRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Execute the specified command now"));
		mAlarmTypeStack->raiseWidget(mCommandFrame);
		mCommandMessageEdit->setNoSelect();
		focus = mCommandMessageEdit;
	}
	else if (mEmailRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
		mAlarmTypeStack->raiseWidget(mEmailFrame);
		mEmailToEdit->setNoSelect();
		focus = mEmailToEdit;
	}
	mLateCancel->showAutoClose(displayAlarm);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	if (focus)
		focus->setFocus();
}

/******************************************************************************
*  Called when one of the template time radio buttons is clicked,
*  to enable or disable the template time entry spin box.
*/
void EditAlarmDlg::slotTemplateTimeType(int)
{
	mTemplateTime->setEnabled(mTemplateUseTime->isOn());
}

/******************************************************************************
*  Called when the browse button is pressed to select a file to display.
*/
void EditAlarmDlg::slotBrowseFile()
{
	if (mFileDefaultDir.isEmpty())
		mFileDefaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(mFileDefaultDir, QString::null, this, i18n("Choose Text or Image File to Display"));
	if (!url.isEmpty())
	{
		mAlarmMessage = url.prettyURL();
		mFileMessageEdit->setText(mAlarmMessage);
		mFileDefaultDir = url.path();
	}
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
	if (mAttachDefaultDir.isEmpty())
		mAttachDefaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(mAttachDefaultDir, QString::null, this, i18n("Choose File to Attach"));
	if (!url.isEmpty())
	{
		mEmailAttachList->insertItem(url.prettyURL());
		mEmailAttachList->setCurrentItem(mEmailAttachList->count() - 1);   // select the new item
		mAttachDefaultDir = url.path();
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
		result = mCommandMessageEdit->text();
		result.stripWhiteSpace();
	}
	else if (mFileRadio->isOn())
	{
		QString alarmtext = mFileMessageEdit->text();
		// Convert any relative file path to absolute
		// (using home directory as the default)
		enum Err { NONE = 0, NONEXISTENT, DIRECTORY, UNREADABLE, NOT_TEXT_IMAGE, HTML };
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
		else
		{
			// It's a local file - convert to absolute path & check validity
			if (alarmtext.isEmpty())
				err = DIRECTORY;    // blank file name - need to get its path, for the error message
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
#if KDE_VERSION < 290
					err = HTML;
#endif
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
				case NONEXISTENT:     errmsg = i18n("%1\nnot found");  break;
				case DIRECTORY:       errmsg = i18n("%1\nis a folder");  break;
				case UNREADABLE:      errmsg = i18n("%1\nis not readable");  break;
				case NOT_TEXT_IMAGE:  errmsg = i18n("%1\nappears not to be a text or image file");  break;
#if KDE_VERSION < 290
				case HTML:            errmsg = i18n("%1\nHTML/XML files may not display correctly");  break;
#endif
				case NONE:
				default:
					break;
			}
			if (KMessageBox::warningContinueCancel(const_cast<EditAlarmDlg*>(this), errmsg.arg(alarmtext),
			                                       QString::null, KStdGuiItem::cont())    // explicit button text is for KDE2 compatibility
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
	: QTextEdit(parent, name)
{
	QSize tsize = sizeHint();
	tsize.setHeight(fontMetrics().lineSpacing()*13/4 + 2*frameWidth());
	setMinimumSize(tsize);
}


/*=============================================================================
= Class LineEdit
= Line edit which accepts drag and drop of text, URLs and/or email addresses.
* It has an option to prevent its contents being selected when it receives
= focus.
=============================================================================*/
LineEdit::LineEdit(Type type, QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  mType(type),
	  mNoSelect(false),
	  mSetCursorAtEnd(false)
{
	init();
}

LineEdit::LineEdit(QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  mType(Text),
	  mNoSelect(false),
	  mSetCursorAtEnd(false)
{
	init();
}

void LineEdit::init()
{
	if (mType == Url)
	{
		setCompletionMode(KGlobalSettings::CompletionShell);
		KURLCompletion* comp = new KURLCompletion(KURLCompletion::FileCompletion);
		comp->setReplaceHome(true);
		setCompletionObject(comp);
		setAutoDeleteCompletionObject(true);
	}
	else
		setCompletionMode(KGlobalSettings::CompletionNone);
}

/******************************************************************************
*  Called when the line edit receives focus.
*  If 'noSelect' is true, prevent the contents being selected.
*/
void LineEdit::focusInEvent(QFocusEvent* e)
{
	if (mNoSelect)
		QFocusEvent::setReason(QFocusEvent::Other);
	KLineEdit::focusInEvent(e);
	if (mNoSelect)
	{
		QFocusEvent::resetReason();
		mNoSelect = false;
	}
}

void LineEdit::setText(const QString& text)
{
	KLineEdit::setText(text);
	setCursorPosition(mSetCursorAtEnd ? text.length() : 0);
}

void LineEdit::dragEnterEvent(QDragEnterEvent* e)
{
	e->accept(QTextDrag::canDecode(e)
	       || KURLDrag::canDecode(e)
	       || mType != Url && KPIM::MailListDrag::canDecode(e)
	       || mType == Emails && KVCardDrag::canDecode(e));
}

void LineEdit::dropEvent(QDropEvent* e)
{
	QString     newText;
	QStringList newEmails;
	QString txt;
	KPIM::MailList mailList;
	KURL::List files;

	if (mType != Url
	&&  e->provides(KPIM::MailListDrag::format())
	&&  KPIM::MailListDrag::decode(e, mailList))
	{
		// KMail message(s) - ignore all but the first
		if (mailList.count())
		{
			if (mType == Emails)
				newText = mailList.first().from();
			else
				setText(mailList.first().subject());    // replace any existing text
#warning Set complete message if dragging onto a text alarm edit field
		}
	}
	// This must come before KURLDrag
	else if (mType == Emails
	&&  KVCardDrag::canDecode(e)  &&  KVCardDrag::decode(e, txt))
	{
		// KAddressBook entries
		KABC::VCardConverter converter;
		KABC::Addressee::List list = converter.parseVCards(txt);
		for (KABC::Addressee::List::Iterator it = list.begin();  it != list.end();  ++it)
		{
			QString em((*it).fullEmail());
			if (!em.isEmpty())
				newEmails.append(em);
		}
	}
	else if (KURLDrag::decode(e, files)  &&  files.count())
	{
		// URL(s)
		switch (mType)
		{
			case Url:
				// URL entry field - ignore all but the first dropped URL
				setText(files.first().prettyURL());    // replace any existing text
				break;
			case Emails:
			{
				// Email entry field - ignore all but mailto: URLs
				QString mailto = QString::fromLatin1("mailto");
				for (KURL::List::Iterator it = files.begin();  it != files.end();  ++it)
				{
					if ((*it).protocol() == mailto)
						newEmails.append((*it).path());
				}
				break;
			}
			case Text:
				newText = files.first().prettyURL();
				break;
		}
	}
	else if (QTextDrag::decode(e, txt))
	{
		// Plain text
		if (mType == Emails)
		{
			// Remove newlines from a list of email addresses, and allow an eventual mailto: protocol
			QString mailto = QString::fromLatin1("mailto:");
			newEmails = QStringList::split(QRegExp("[\r\n]+"), txt);
			for (QStringList::Iterator it = newEmails.begin();  it != newEmails.end();  ++it)
			{
				if ((*it).startsWith(mailto))
				{
					KURL url(*it);
					*it = url.path();
				}
			}
		}
		else
		{
			int newline = txt.find('\n');
			newText = (newline >= 0) ? txt.left(newline) : txt;
		}
	}

	if (newEmails.count())
	{
		newText = newEmails.join(",");
		int c = cursorPosition();
		if (c > 0)
			newText.prepend(",");
		if (c < static_cast<int>(text().length()))
			newText.append(",");
	}
	if (!newText.isEmpty())
		insert(newText);
}
