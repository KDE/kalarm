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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
#include <kwinmodule.h>
#include <kstandarddirs.h>
#include <kstdguiitem.h>
#include <kabc/addresseedialog.h>
#include <kdebug.h>

#include <maillistdrag.h>

#include "alarmcalendar.h"
#include "alarmtimewidget.h"
#include "checkbox.h"
#include "colourcombo.h"
#include "combobox.h"
#include "deferdlg.h"
#include "fontcolourbutton.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#include "reminder.h"
#include "soundpicker.h"
#include "spinbox.h"
#include "templatepickdlg.h"
#include "timeperiod.h"
#include "timespinbox.h"
#include "editdlg.moc"
#include "editdlgprivate.moc"

using namespace KCal;

static const char EDIT_DIALOG_NAME[] = "EditDialog";


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
const QString EditAlarmDlg::i18n_ConfirmAck         = i18n("Confirm acknowledgment");
const QString EditAlarmDlg::i18n_k_ConfirmAck       = i18n("Confirm ac&knowledgment");
const QString EditAlarmDlg::i18n_CancelIfLate       = i18n("Cancel if late");
const QString EditAlarmDlg::i18n_n_CancelIfLate     = i18n("Ca&ncel if late");
const QString EditAlarmDlg::i18n_CopyEmailToSelf    = i18n("Copy email to self");
const QString EditAlarmDlg::i18n_e_CopyEmailToSelf  = i18n("Copy &email to self");
const QString EditAlarmDlg::i18n_s_CopyEmailToSelf  = i18n("Copy email to &self");


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
	  mReminderDeferral(false),
	  mReminderArchived(false),
	  mEmailRemoveButton(0),
	  mDeferGroup(0),
	  mTimeWidget(0),
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
	QTabWidget* tabs = new QTabWidget(mainWidget);
	tabs->setMargin(marginHint() + marginKDE2);

	QVBox* mainPageBox = new QVBox(tabs);
	mainPageBox->setSpacing(spacingHint());
	tabs->addTab(mainPageBox, i18n("&Alarm"));
	mMainPageIndex = 0;
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Recurrence tab
	QVBox* recurPage = new QVBox(tabs);
	mainPageBox->setSpacing(spacingHint());
	tabs->addTab(recurPage, i18n("&Recurrence"));
	mRecurPageIndex = 1;
	mRecurrenceEdit = new RecurrenceEdit(readOnly, recurPage, "recurPage");
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
	mDeferGroupHeight = mDeferGroup->height() + spacingHint();

	QBoxLayout* layout = new QHBoxLayout(topLayout);

	// Date and time entry
	if (mTemplate)
	{
		mTemplateTimeGroup = new ButtonGroup(i18n("Time"), mainPage, "templateGroup");
		connect(mTemplateTimeGroup, SIGNAL(buttonSet(int)), SLOT(slotTemplateTimeType(int)));
		layout->addWidget(mTemplateTimeGroup, 0, Qt::AlignLeft);
		QBoxLayout* vlayout = new QVBoxLayout(mTemplateTimeGroup, marginKDE2 + marginHint(), spacingHint());
		vlayout->addSpacing(fontMetrics().lineSpacing()/2);

		mTemplateDefaultTime = new RadioButton(i18n("&Default time"), mTemplateTimeGroup, "templateDefTimeButton");
		mTemplateDefaultTime->setFixedSize(mTemplateDefaultTime->sizeHint());
		mTemplateDefaultTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateDefaultTime,
		      i18n("Do not specify a start time for alarms based on this template. "
		           "The normal default start time will be used."));
		vlayout->addWidget(mTemplateDefaultTime, 0, Qt::AlignLeft);

		QHBox* box = new QHBox(mTemplateTimeGroup);
		box->setSpacing(spacingHint());
		vlayout->addWidget(box);
		mTemplateUseTime = new RadioButton(i18n("Time:"), box, "templateTimeButton");
		mTemplateUseTime->setFixedSize(mTemplateUseTime->sizeHint());
		mTemplateUseTime->setReadOnly(mReadOnly);
		QWhatsThis::add(mTemplateUseTime,
		      i18n("Specify a start time for alarms based on this template."));
		mTemplateTimeGroup->insert(mTemplateUseTime);
		mTemplateTime = new TimeSpinBox(box, "templateTimeEdit");
		mTemplateTime->setValue(1439);
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
		vlayout->addWidget(mTemplateAnyTime, 0, Qt::AlignLeft);
	}
	else
	{
		mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME | AlarmTimeWidget::NARROW,
		                                  mainPage, "timeGroup");
		connect(mTimeWidget, SIGNAL(anyTimeToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		layout->addWidget(mTimeWidget);
	}

	layout->addSpacing(marginHint() - spacingHint());
	QBoxLayout* vlayout = new QVBoxLayout(layout);
	layout->addStretch();

	// Recurrence type display
	QHBox* box = new QHBox(mainPage);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	label = new QLabel(i18n("Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mRecurrenceText = new QLabel(box);
	QWhatsThis::add(box,
	      i18n("How often the alarm recurs.\nThe alarm's recurrence is configured in the Recurrence tab."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box, 0, Qt::AlignLeft);

	// Late display checkbox - default = allow late display
	mLateCancel = createLateCancelCheckbox(mainPage);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	vlayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	// Initialise the state of all controls according to the specified event, if any
	initialise(event);
	if (mTemplateName)
		mTemplateName->setFocus();

	// Save the initial state of all controls so that we can later tell if they have changed
	saveState(event);
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
	frameLayout->addWidget(mSoundPicker, 0, Qt::AlignLeft);

	// Acknowledgement confirmation required - default = no confirmation
	mConfirmAck = createConfirmAckCheckbox(mDisplayAlarmsFrame);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	frameLayout->addWidget(mConfirmAck, 0, Qt::AlignLeft);

	// Reminder
	static const QString reminderText = i18n("Enter how long in advance of the main alarm to display a reminder alarm.");
	mReminder = new Reminder(i18n("Rem&inder:"),
	                         i18n("Check to additionally display a reminder in advance of the main alarm time(s)."),
	                         QString("%1\n\n%2").arg(reminderText).arg(TimeSpinBox::shiftWhatsThis()),
	                         true, mDisplayAlarmsFrame);
	mReminder->setFixedSize(mReminder->sizeHint());
	frameLayout->addWidget(mReminder, 0, Qt::AlignLeft);

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
	QGridLayout* grid = new QGridLayout(layout, 2, 3, spacingHint());
	grid->setColStretch(1, 1);

	// Email recipients
	QLabel* label = new QLabel(i18n("Email addressee", "To:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	mEmailToEdit = new LineEdit(LineEdit::EmailAddresses, mEmailFrame);
	mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
	QWhatsThis::add(mEmailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	grid->addWidget(mEmailToEdit, 0, 1);

	mEmailAddressButton = new QPushButton(mEmailFrame);
	mEmailAddressButton->setPixmap(SmallIcon("contents"));
	mEmailAddressButton->setFixedSize(mEmailAddressButton->sizeHint());
	connect(mEmailAddressButton, SIGNAL(clicked()), SLOT(openAddressBook()));
	QToolTip::add(mEmailAddressButton, i18n("Open address book"));
	QWhatsThis::add(mEmailAddressButton, i18n("Select email addresses from your address book."));
	grid->addWidget(mEmailAddressButton, 0, 2);

	// Email subject
	label = new QLabel(i18n("Email subject", "Sub&ject:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	mEmailSubjectEdit = new LineEdit(mEmailFrame);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	label->setBuddy(mEmailSubjectEdit);
	QWhatsThis::add(mEmailSubjectEdit, i18n("Enter the email subject."));
	grid->addMultiCellWidget(mEmailSubjectEdit, 1, 1, 1, 2);

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
	mEmailBcc = new CheckBox(i18n_s_CopyEmailToSelf, mEmailFrame);
	mEmailBcc->setFixedSize(mEmailBcc->sizeHint());
	QWhatsThis::add(mEmailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	grid->addMultiCellWidget(mEmailBcc, 1, 1, 0, 1, Qt::AlignLeft);
}

/******************************************************************************
 * Initialise the dialogue controls from the specified event.
 */
void EditAlarmDlg::initialise(const KAEvent* event)
{
	mReadOnly = mDesiredReadOnly;
	if (!mTemplate  &&  event  &&  event->action() == KAEvent::COMMAND  &&  theApp()->noShellAccess())
		mReadOnly = true;     // don't allow editing of existing command alarms in kiosk mode
	setReadOnly();

	bool deferGroupVisible = false;
	if (event)
	{
		// Set the values to those for the specified event
		if (mTemplate)
			mTemplateName->setText(event->templateName());
		bool recurs = event->recurs();
		deferGroupVisible = (recurs  &&  !mTemplate  &&  event->deferred());
		if (deferGroupVisible)
		{
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
				mTemplateTime->setTime(event->mainDateTime().time());
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
				mTimeWidget->setDateTime(!event->mainExpired() ? event->mainDateTime()
				                         : recurs ? DateTime() : event->deferDateTime());
		}

		setAction(event->action(), event->cleanText());
		if (event->action() == KAEvent::EMAIL)
			mEmailAttachList->insertStringList(event->emailAttachments());

		mLateCancel->setChecked(event->lateCancel());
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
		mRecurrenceText->setText(event->recurrenceText());
		mRecurrenceEdit->set(*event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		mSoundPicker->set(event->beep(), event->audioFile(), event->repeatSound());
		mEmailToEdit->setText(event->emailAddresses(", "));
		mEmailSubjectEdit->setText(event->emailSubject());
		mEmailBcc->setChecked(event->emailBcc());
	}
	else
	{
		// Set the values to their defaults
		if (theApp()->noShellAccess())
			mCommandRadio->setEnabled(false);    // don't allow shell commands in kiosk mode
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
		mLateCancel->setChecked(preferences->defaultLateCancel());
		mConfirmAck->setChecked(preferences->defaultConfirmAck());
		mReminder->setMinutes(0, false);
		mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		slotRecurFrequencyChange();      // update the Recurrence text
		mSoundPicker->set(preferences->defaultBeep(), preferences->defaultSoundFile(), preferences->defaultSoundRepeat());
		mEmailBcc->setChecked(preferences->defaultEmailBcc());
	}

	if (!deferGroupVisible)
		mDeferGroup->hide();
	int deferGroupHeight = deferGroupVisible ? mDeferGroupHeight : 0;
	QSize size = minimumSize();
	size.setHeight(size.height() - deferGroupHeight);
	mBasicSize = KAlarm::readConfigWindowSize(EDIT_DIALOG_NAME, size);
	size = mBasicSize;
	size.setHeight(size.height() + deferGroupHeight);
	resize(size);

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

	// Message alarm controls
	mTextMessageEdit->setReadOnly(mReadOnly);
	mFileMessageEdit->setReadOnly(mReadOnly);
	mBgColourChoose->setReadOnly(mReadOnly);
	mFontColourButton->setReadOnly(mReadOnly);
	mSoundPicker->setReadOnly(mReadOnly);
	mConfirmAck->setReadOnly(mReadOnly);
	mReminder->setReadOnly(mReadOnly);
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
void EditAlarmDlg::setAction(KAEvent::Action action, const QString& text)
{
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
	CheckBox* widget = new CheckBox(i18n_k_ConfirmAck, parent, name);
	QWhatsThis::add(widget,
	      i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
	return widget;
}

/******************************************************************************
 * Create a "cancel if late" checkbox.
 */
CheckBox* EditAlarmDlg::createLateCancelCheckbox(QWidget* parent, const char* name)
{
	CheckBox* widget = new CheckBox(i18n_n_CancelIfLate, parent, name);
	QWhatsThis::add(widget,
	      i18n("If checked, the alarm will be canceled if it cannot be triggered within 1 "
	           "minute of the specified time. Possible reasons for not triggering include your "
	           "being logged off, X not running, or the alarm daemon not running.\n\n"
	           "If unchecked, the alarm will be triggered at the first opportunity after "
	           "the specified time, regardless of how late it is."));
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
	mSavedRepeatSound    = mSoundPicker->repeat();
	mSavedConfirmAck     = mConfirmAck->isChecked();
	mSavedFont           = mFontColourButton->font();
	mSavedFgColour       = mFontColourButton->fgColour();
	mSavedBgColour       = mBgColourChoose->color();
	mSavedReminder       = mReminder->getMinutes();
	checkText(mSavedTextFileCommandMessage, false);
	mSavedEmailTo        = mEmailToEdit->text();
	mSavedEmailSubject   = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0;  i < mEmailAttachList->count();  ++i)
		mSavedEmailAttach += mEmailAttachList->text(i);
	mSavedEmailBcc       = mEmailBcc->isChecked();
	if (mTimeWidget)
		mSavedDateTime = mTimeWidget->getDateTime(false, false);
	mSavedLateCancel     = mLateCancel->isChecked();
	mSavedRecurrenceType = mRecurrenceEdit->repeatType();
}

/******************************************************************************
 * Check whether any of the controls other than the deferral time has changed
 * state since the dialog was first displayed.
 */
bool EditAlarmDlg::stateChanged() const
{
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
	||  mSavedLateCancel       != mLateCancel->isChecked()
	||  textFileCommandMessage != mSavedTextFileCommandMessage
	||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType())
		return true;
	if (mMessageRadio->isOn()  ||  mFileRadio->isOn())
	{
		if (mSavedBeep       != mSoundPicker->beep()
		||  mSavedConfirmAck != mConfirmAck->isChecked()
		||  mSavedFont       != mFontColourButton->font()
		||  mSavedFgColour   != mFontColourButton->fgColour()
		||  mSavedBgColour   != mBgColourChoose->color()
		||  mSavedReminder   != mReminder->getMinutes())
			return true;
		if (!mSavedBeep)
		{
			if (mSavedSoundFile != mSoundPicker->file()
			||  !mSavedSoundFile.isEmpty()  &&  mSavedRepeatSound != mSoundPicker->repeat())
				return true;
		}
	}
	else if (mEmailRadio->isOn())
	{
		QStringList emailAttach;
		for (int i = 0;  i < mEmailAttachList->count();  ++i)
			emailAttach += mEmailAttachList->text(i);
		if (mSavedEmailTo      != mEmailToEdit->text()
		||  mSavedEmailSubject != mEmailSubjectEdit->text()
		||  mSavedEmailAttach  != emailAttach
		||  mSavedEmailBcc     != mEmailBcc->isChecked())
			return true;
	}
	if (mSavedEvent->recurType() != KAEvent::NO_RECUR)
	{
		if (!mSavedEvent->recurrence())
			return true;
		KAEvent event;
		event.setTime(mSavedEvent->startDateTime().dateTime());
		mRecurrenceEdit->updateEvent(event);
		if (!event.recurrence())
			return true;
		event.recurrence()->setFloats(mSavedEvent->startDateTime().isDateOnly());
		if (*event.recurrence() != *mSavedEvent->recurrence()
		||  event.exceptionDates() != mSavedEvent->exceptionDates()
		||  event.exceptionDateTimes() != mSavedEvent->exceptionDateTimes())
			return true;
	}
	return false;
}

/******************************************************************************
 * Get the currently entered message data.
 * The data is returned in the supplied Event instance.
 */
void EditAlarmDlg::getEvent(KAEvent& event)
{
	if (!mSavedEvent  ||  stateChanged())
	{
		// It's a new event, or the edit controls have changed
		QDateTime dt;
		if (!mTemplate)
			dt = mAlarmDateTime.dateTime();
		else if (mTemplateUseTime->isOn())
			dt.setTime(mTemplateTime->time());
		event.set(dt, mAlarmMessage, mBgColourChoose->color(),
		          mFontColourButton->fgColour(), mFontColourButton->font(), getAlarmType(),
		          getAlarmFlags());
		event.setAudioFile(mSoundPicker->file());
		event.setEmail(mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
		event.setReminder(mReminder->getMinutes());
		if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
		{
			mRecurrenceEdit->updateEvent(event);
			mAlarmDateTime = event.startDateTime();
			if (mDeferDateTime.isValid()  &&  mDeferDateTime < mAlarmDateTime)
			{
				bool deferral = true;
				bool deferReminder = false;
				int reminder = mReminder->getMinutes();
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
			event.setTemplate(mTemplateName->text(), mTemplateDefaultTime->isOn());
	}
	else
	{
		// Only the deferral time may have changed
		event = *mSavedEvent;
		if (mSavedEvent->deferred())
		{
			if (mDeferDateTime.isValid())
				event.defer(mDeferDateTime, event.reminderDeferral(), false);
			else
				event.cancelDefer();
		}
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
	     | (mLateCancel->isChecked()                      ? KAEvent::LATE_CANCEL : 0)
	     | (displayAlarm && mConfirmAck->isChecked()      ? KAEvent::CONFIRM_ACK : 0)
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
*  Called when the dialog's size has changed.
*  Records the new size (adjusted to ignore the optional height of the deferred
*  time edit widget) in the config file.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
	{
		mBasicSize = re->size();
		mBasicSize.setHeight(mBasicSize.height() - (mDeferGroup->isVisible() ? mDeferGroupHeight : 0));
		KAlarm::writeConfigWindowSize(EDIT_DIALOG_NAME, mBasicSize);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Set up the new alarm.
*/
void EditAlarmDlg::slotOk()
{
	if (mTimeWidget
	&&  activePageIndex() == mRecurPageIndex  &&  mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
	bool timedRecurrence = mRecurrenceEdit->isTimedRepeatType();
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
		mAlarmDateTime = mTimeWidget->getDateTime(!timedRecurrence, false, &errWidget);
		if (errWidget)
		{
			showPage(mMainPageIndex);
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
				getEvent(event);
				if (event.nextOccurrence(now, mAlarmDateTime) == KAEvent::NO_OCCURRENCE)
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
			showPage(mRecurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, errmsg);
			return;
		}
	}
	if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
	{
		int reminder = mReminder->getMinutes();
		if (reminder)
		{
			KAEvent event;
			mRecurrenceEdit->updateEvent(event);
			int minutes = event.longestRecurrenceInterval();
			if (minutes  &&  reminder >= minutes)
			{
				showPage(mMainPageIndex);
				mReminder->setFocusOnCount();
				KMessageBox::sorry(this, i18n("Reminder period must be less than recurrence interval"));
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
		event.set(QDateTime(), text, mBgColourChoose->color(), mFontColourButton->fgColour(),
		          mFontColourButton->font(), getAlarmType(), getAlarmFlags());
		event.setAudioFile(mSoundPicker->file());
		event.setEmail(mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
		void* proc = theApp()->execAlarm(event, event.firstAlarm(), false, false);
		if (proc)
		{
			if (mCommandRadio->isOn())
			{
				theApp()->commandMessage((KProcess*)proc, this);
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
				theApp()->commandMessage((KProcess*)proc, 0);
			}
			else if (mEmailRadio->isOn())
			{
				QString bcc;
				if (mEmailBcc->isChecked())
					bcc = i18n("\nBcc: %1").arg(Preferences::instance()->emailAddress());
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
	DateTime start = mTimeWidget->getDateTime();
	if (!start.isValid())
		return;

	bool deferred = mDeferDateTime.isValid();
	DeferAlarmDlg deferDlg(i18n("Defer Alarm"), (deferred ? mDeferDateTime : DateTime(QDateTime::currentDateTime().addSecs(60))),
	                       deferred, this, "deferDlg");
	// Don't allow deferral past the next recurrence
	int reminder = mReminder->getMinutes();
	if (reminder)
	{
		DateTime remindTime = start.addMins(-reminder);
		if (QDateTime::currentDateTime() < remindTime)
			start = remindTime;
	}
	deferDlg.setLimit(start);
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
			mTimeWidget->setMinDate(false);
		else
			mTimeWidget->setMinDateToday();
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
	mRecurPageIndex = activePageIndex();
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
		if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		{
			mAlarmDateTime = mTimeWidget->getDateTime(false, false);
			mRecurrenceEdit->setEndDateTime(mAlarmDateTime.dateTime());
		}
	}
	slotRecurFrequencyChange();
}

/******************************************************************************
*  Called when the recurrence frequency selection changes.
*  Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
	KAEvent event;
	mRecurrenceEdit->updateEvent(event);
	mRecurrenceText->setText(event.recurrenceText());
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
*  Sets the advance reminder units to days if any time is checked.
*/
void EditAlarmDlg::slotAnyTimeToggled(bool anyTime)
{
	if (mReminder->isReminder())
	 	mReminder->setDateOnly(anyTime);
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
			if (!KIO::NetAccess::stat(url, uds))
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
= Line edit with option to prevent its contents being selected when it receives
= focus.
=============================================================================*/
LineEdit::LineEdit(Type type, QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  mNoSelect(false)
{
	init(type);
}

LineEdit::LineEdit(QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  mNoSelect(false)
{
	init(Basic);
}

void LineEdit::init(Type type)
{
	mEmailAddresses = (type == EmailAddresses);
	if (type == Url)
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

void LineEdit::dragEnterEvent(QDragEnterEvent* e)
{
	e->accept(QTextDrag::canDecode(e)
	       || KURLDrag::canDecode(e)
	       || KPIM::MailListDrag::canDecode(e));
}

void LineEdit::dropEvent(QDropEvent* e)
{
	QString text;
	KPIM::MailList mailList;
	KURL::List files;
	if (KURLDrag::decode(e, files)  &&  files.count())
		setText(files.first().prettyURL());
	else if (e->provides(KPIM::MailListDrag::format())
	&&  KPIM::MailListDrag::decode(e, mailList))
	{
		// KMail message(s). Ignore all but the first.
		if (mailList.count())
		{
			if (mEmailAddresses)
				setText(mailList.first().from());
			else
				setText(mailList.first().subject());
		}
	}
	else if (QTextDrag::decode(e, text))
	{
		if (mEmailAddresses)
		{
#warning "Allow drag of a list of email addresses as follows:"
			// Remove newlines from a list of email addresses, and allow an eventual mailto: protocol
			text.replace(QRegExp("\r?\n"), ", ");
			if (text.startsWith("mailto:"))
			{
				KURL url(text);
				text = url.path();
			}
		}
		else
		{
			int newline = text.find('\n');
			if (newline >= 0)
				text = text.left(newline);
		}
		setText(text);
	}
}
