/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <limits.h>

#include <qlayout.h>
#include <qpopupmenu.h>
#include <qlineedit.h>
#include <qmultilineedit.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qwidgetstack.h>
#include <qlabel.h>
#include <qmessagebox.h>
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
#include <kwinmodule.h>
#include <kstandarddirs.h>
#include <kstdguiitem.h>
#if KDE_VERSION >= 290
#include <kabc/addresseedialog.h>
#else
#include <kabapi.h>
#endif
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "soundpicker.h"
#include "recurrenceedit.h"
#include "colourcombo.h"
#include "fontcolourbutton.h"
#include "kamail.h"
#include "deferdlg.h"
#include "buttongroup.h"
#include "radiobutton.h"
#include "checkbox.h"
#include "combobox.h"
#include "spinbox.h"
#include "timeperiod.h"
#include "editdlg.moc"

using namespace KCal;


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event, bool readOnly)
	: KDialogBase(KDialogBase::Tabbed, caption, (readOnly ? Cancel|Try : Ok|Cancel|Try),
	              (readOnly ? Cancel : Ok), parent, name),
	  mRecurSetEndDate(true),
	  mEmailRemoveButton(0),
	  mDeferGroup(0),
	  mReadOnly(readOnly)
{
	QVBox* mainPageBox = addVBoxPage(i18n("&Alarm"));
	mMainPageIndex = pageIndex(mainPageBox);
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Recurrence tab
	QVBox* recurPage = addVBoxPage(i18n("&Recurrence"));
	mRecurPageIndex = pageIndex(recurPage);
	mRecurTabStack = new QWidgetStack(recurPage);

	mRecurrenceEdit = new RecurrenceEdit(readOnly, mRecurTabStack, "recurPage");
	mRecurTabStack->addWidget(mRecurrenceEdit, 0);
	connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));

	mRecurDisabled = new QLabel(i18n("The alarm does not recur.\nEnable recurrence in the Alarm tab."), mRecurTabStack);
	mRecurDisabled->setAlignment(Qt::AlignCenter);
	mRecurTabStack->addWidget(mRecurDisabled, 1);

	// Alarm action

	QButtonGroup* actionGroup = new QButtonGroup(i18n("Action"), mainPage, "actionGroup");
	connect(actionGroup, SIGNAL(clicked(int)), SLOT(slotAlarmTypeClicked(int)));
	topLayout->addWidget(actionGroup, 1);
	QGridLayout* grid = new QGridLayout(actionGroup, 3, 5, marginKDE2 + marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	// Message radio button
	mMessageRadio = new RadioButton(i18n("Te&xt"), actionGroup, "messageButton");
	mMessageRadio->setFixedSize(mMessageRadio->sizeHint());
	mMessageRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mMessageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(mMessageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	mFileRadio = new RadioButton(i18n("&File"), actionGroup, "fileButton");
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	mFileRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mFileRadio,
	      i18n("If checked, the alarm will display the contents of a text file."));
	grid->addWidget(mFileRadio, 1, 2);
	grid->setColStretch(3, 1);

	// Command radio button
	mCommandRadio = new RadioButton(i18n("Co&mmand"), actionGroup, "cmdButton");
	mCommandRadio->setFixedSize(mCommandRadio->sizeHint());
	mCommandRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mCommandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(mCommandRadio, 1, 4);
	grid->setColStretch(5, 1);

	// Email radio button
	mEmailRadio = new RadioButton(i18n("&Email"), actionGroup, "emailButton");
	mEmailRadio->setFixedSize(mEmailRadio->sizeHint());
	mEmailRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mEmailRadio,
	      i18n("If checked, the alarm will send an email."));
	grid->addWidget(mEmailRadio, 1, 6);

	initDisplayAlarms(actionGroup);
	initCommand(actionGroup);
	initEmail(actionGroup);
	mAlarmTypeStack = new QWidgetStack(actionGroup);
	grid->addMultiCellWidget(mAlarmTypeStack, 2, 2, 0, 6);
	grid->setRowStretch(2, 1);
	mAlarmTypeStack->addWidget(mDisplayAlarmsFrame, 0);
	mAlarmTypeStack->addWidget(mCommandFrame, 1);
	mAlarmTypeStack->addWidget(mEmailFrame, 1);

	bool recurs = event && event->recurs();
	if (recurs  &&  event->deferred())
	{
		// Deferred date/time for event without a time or recurring event
		mDeferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), mainPage, "deferGroup");
		topLayout->addWidget(mDeferGroup);
		QLabel* label = new QLabel(i18n("Deferred to:"), mDeferGroup);
		label->setFixedSize(label->sizeHint());

		mDeferDateTime = event->deferDateTime();
		mDeferTimeLabel = new QLabel(KGlobal::locale()->formatDateTime(mDeferDateTime), mDeferGroup);

		if (!mReadOnly)
		{
			QPushButton* button = new QPushButton(i18n("C&hange..."), mDeferGroup);
			button->setFixedSize(button->sizeHint());
			connect(button, SIGNAL(clicked()), SLOT(slotEditDeferral()));
			QWhatsThis::add(button, i18n("Change the alarm's deferred time, or cancel the deferral"));
		}
		mDeferGroup->addSpace(0);
	}

	grid = new QGridLayout(topLayout, 3, 2, spacingHint());

	// Date and time entry

	mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME | AlarmTimeWidget::NARROW,
	                                  mainPage, "timeGroup");
	mTimeWidget->setReadOnly(mReadOnly);
	connect(mTimeWidget, SIGNAL(anyTimeToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
	grid->addMultiCellWidget(mTimeWidget, 0, 2, 0, 0);

	// Repetition type radio buttons

	ButtonGroup* repeatGroup = new ButtonGroup(1, Qt::Horizontal, i18n("Repetition"), mainPage);
	connect(repeatGroup, SIGNAL(buttonSet(int)), SLOT(slotRepeatClicked(int)));
	grid->addWidget(repeatGroup, 0, 1);
	grid->setRowStretch(1, 1);

	mNoRepeatRadio = new RadioButton(i18n("No re&petition"), repeatGroup);
	mNoRepeatRadio->setFixedSize(mNoRepeatRadio->sizeHint());
	mNoRepeatRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mNoRepeatRadio,
	      i18n("Trigger the alarm once only"));

	mRepeatAtLoginRadio = new RadioButton(i18n("Repeat at lo&gin"), repeatGroup, "repeatAtLoginButton");
	mRepeatAtLoginRadio->setFixedSize(mRepeatAtLoginRadio->sizeHint());
	mRepeatAtLoginRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mRepeatAtLoginRadio,
	      i18n("Repeat the alarm at every login until the specified time.\n"
	           "Note that it will also be repeated any time the alarm daemon is restarted."));

	mRecurRadio = new RadioButton(i18n("Rec&ur"), repeatGroup);
	mRecurRadio->setFixedSize(mRecurRadio->sizeHint());
	mRecurRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mRecurRadio, i18n("Regularly repeat the alarm"));

	// Late display checkbox - default = allow late display
	mLateCancel = createLateCancelCheckbox(mReadOnly, mainPage);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	grid->addWidget(mLateCancel, 1, 1, Qt::AlignLeft);
	grid->setColStretch(1, 1);

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	topLayout->activate();

	mDeferGroupHeight = mDeferGroup ? mDeferGroup->height() + spacingHint() : 0;
	QSize size = minimumSize();
	size.setHeight(size.height() - mDeferGroupHeight);
	mBasicSize = theApp()->readConfigWindowSize("EditDialog", size);
	resize(mBasicSize);

	// Set up initial values
	if (event)
	{
		// Set the values to those for the specified event
		if (event->defaultFont())
			mFontColourButton->setDefaultFont();
		else
			mFontColourButton->setFont(event->font());
		mFontColourButton->setBgColour(event->bgColour());
		mBgColourChoose->setColour(event->bgColour());     // set colour before setting alarm type buttons
		mTimeWidget->setDateTime((!event->mainExpired() ? event->mainDateTime() : recurs ? QDateTime() : event->deferDateTime()),
		                         (event->anyTime() && !event->deferred()));

		QRadioButton* radio;
		switch (event->action())
		{
			case KAlarmEvent::FILE:
				radio = mFileRadio;
				mFileMessageEdit->setText(event->cleanText());
				break;
			case KAlarmEvent::COMMAND:
				radio = mCommandRadio;
				mCommandMessageEdit->setText(event->cleanText());
				break;
			case KAlarmEvent::EMAIL:
				radio = mEmailRadio;
				mEmailMessageEdit->setText(event->cleanText());
				mEmailAttachList->insertStringList(event->emailAttachments());
				break;
			case KAlarmEvent::MESSAGE:
			default:
				radio = mMessageRadio;
				mTextMessageEdit->setText(event->cleanText());
				break;
		}
		actionGroup->setButton(actionGroup->id(radio));

		if (recurs)
		{
			radio = mRecurRadio;
			mRecurSetEndDate = false;
		}
		else if (event->repeatAtLogin())
			radio = mRepeatAtLoginRadio;
		else
			radio = mNoRepeatRadio;
		repeatGroup->setButton(repeatGroup->id(radio));

		mLateCancel->setChecked(event->lateCancel());
		mConfirmAck->setChecked(event->confirmAck());
		int reminder = event->reminder();
		if (!reminder  &&  event->reminderDeferral()  &&  !recurs)
			reminder = event->reminderDeferral();
		if (!reminder  &&  event->reminderArchived()  &&  recurs)
			reminder = event->reminderArchived();
		setReminder(reminder);
		mRecurrenceEdit->set(*event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		mSoundPicker->setFile(event->audioFile());
		mSoundPicker->setChecked(event->beep() || !event->audioFile().isEmpty());
		mEmailToEdit->setText(event->emailAddresses(", "));
		mEmailSubjectEdit->setText(event->emailSubject());
		mEmailBcc->setChecked(event->emailBcc());
	}
	else
	{
		// Set the values to their defaults
		Settings* settings = theApp()->settings();
		mFontColourButton->setFont(settings->messageFont());
		mFontColourButton->setBgColour(settings->defaultBgColour());
		mBgColourChoose->setColour(settings->defaultBgColour());     // set colour before setting alarm type buttons
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		mTimeWidget->setDateTime(defaultTime, false);
		actionGroup->setButton(actionGroup->id(mMessageRadio));
		repeatGroup->setButton(repeatGroup->id(mNoRepeatRadio));
		mLateCancel->setChecked(settings->defaultLateCancel());
		mConfirmAck->setChecked(settings->defaultConfirmAck());
		setReminder(0);
		mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		mSoundPicker->setChecked(settings->defaultBeep());
		mEmailBcc->setChecked(settings->defaultEmailBcc());
	}

	size = mBasicSize;
	size.setHeight(size.height() + mDeferGroupHeight);
	resize(size);

	bool enable = !!mEmailAttachList->count();
	mEmailAttachList->setEnabled(enable);
	if (mEmailRemoveButton)
		mEmailRemoveButton->setEnabled(enable);
}

EditAlarmDlg::~EditAlarmDlg()
{
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
	mTextMessageEdit = new QMultiLineEdit(mDisplayAlarmsFrame);
	QSize tsize = mTextMessageEdit->sizeHint();
	tsize.setHeight(mTextMessageEdit->fontMetrics().lineSpacing()*13/4 + 2*mTextMessageEdit->frameWidth());
	mTextMessageEdit->setMinimumSize(tsize);
	mTextMessageEdit->setWordWrap(QMultiLineEdit::NoWrap);
	mTextMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(mTextMessageEdit, i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new QHBox(mDisplayAlarmsFrame);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(mFileBox);
	mFileMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(mFileMessageEdit, i18n("Enter the name of a text file, or a URL, to display."));

	// File browse button
	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(mFileBox);
		button->setPixmap(SmallIcon("fileopen"));
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotBrowseFile()));
		QWhatsThis::add(button, i18n("Select a text file to display."));
	}

	// Sound checkbox and file selector
	QBoxLayout* layout = new QHBoxLayout(frameLayout);
	mSoundPicker = new SoundPicker(mReadOnly, mDisplayAlarmsFrame);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	layout->addWidget(mSoundPicker);
	layout->addStretch();

	// Colour choice drop-down list
	mBgColourChoose = createBgColourChooser(mReadOnly, mDisplayAlarmsFrame);
	connect(mBgColourChoose, SIGNAL(highlighted(const QColor&)), SLOT(slotBgColourSelected(const QColor&)));
	layout->addWidget(mBgColourChoose);

	// Acknowledgement confirmation required - default = no confirmation
	layout = new QHBoxLayout(frameLayout);
	mConfirmAck = createConfirmAckCheckbox(mReadOnly, mDisplayAlarmsFrame);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	layout->addWidget(mConfirmAck);
	layout->addStretch();

	// Font and colour choice drop-down list
	mFontColourButton = new FontColourButton(mDisplayAlarmsFrame);
	mFontColourButton->setFixedSize(mFontColourButton->sizeHint());
	mFontColourButton->setReadOnly(mReadOnly);
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	layout->addWidget(mFontColourButton);

	// Reminder
	layout = new QHBoxLayout(frameLayout, spacingHint());
	mReminder = new CheckBox(i18n("Rem&inder:"), mDisplayAlarmsFrame);
	mReminder->setFixedSize(mReminder->sizeHint());
	mReminder->setReadOnly(mReadOnly);
	connect(mReminder, SIGNAL(toggled(bool)), SLOT(slotReminderToggled(bool)));
	QWhatsThis::add(mReminder,
	      i18n("Check to additionally display a reminder in advance of the main alarm time(s)."));
	layout->addWidget(mReminder);

	QHBox* box = new QHBox(mDisplayAlarmsFrame);    // to group widgets for QWhatsThis text
	box->setSpacing(spacingHint());
	layout->addWidget(box);
	mReminderCount = new TimePeriod(box);
	mReminderCount->setHourMinRange(1, 100*60-1);    // max 99H59M
	mReminderCount->setUnitRange(1, 9999);
	mReminderCount->setUnitSteps(1, 10);
	mReminderCount->setFixedSize(mReminderCount->sizeHint());
	mReminderCount->setSelectOnStep(false);
	mReminderCount->setReadOnly(mReadOnly);
	mReminder->setFocusWidget(mReminderCount);

	mReminderUnits = new ComboBox(false, box);
	mReminderUnits->insertItem(i18n("hours/minutes"), REMIND_HOURS_MINUTES);
	mReminderUnits->insertItem(i18n("days"), REMIND_DAYS);
	mReminderUnits->insertItem(i18n("weeks"), REMIND_WEEKS);
	mReminderUnits->setFixedSize(mReminderUnits->sizeHint());
	mReminderUnits->setReadOnly(mReadOnly);
	connect(mReminderUnits, SIGNAL(activated(int)), SLOT(slotReminderUnitsSelected(int)));
	new QLabel(i18n("in advance"), box);
	QWhatsThis::add(box,
	      i18n("Enter how long in advance of the main alarm to display a reminder alarm."));
	layout->addWidget(new QWidget(mDisplayAlarmsFrame));   // left adjust the reminder controls

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

	mCommandMessageEdit = new LineEdit(mCommandFrame);
	mCommandMessageEdit->setReadOnly(mReadOnly);
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
	QLabel* label = new QLabel(i18n("To:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	mEmailToEdit = new LineEdit(mEmailFrame);
	mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
	mEmailToEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(mEmailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	grid->addWidget(mEmailToEdit, 0, 1);

	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(mEmailFrame);
		button->setPixmap(SmallIcon("contents"));
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(openAddressBook()));
		QToolTip::add(button, i18n("Open address book"));
		QWhatsThis::add(button, i18n("Select email addresses from your address book."));
		grid->addWidget(button, 0, 2);
	}

	// Email subject
	label = new QLabel(i18n("Sub&ject:"), mEmailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	mEmailSubjectEdit = new QLineEdit(mEmailFrame);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	mEmailSubjectEdit->setReadOnly(mReadOnly);
	label->setBuddy(mEmailSubjectEdit);
	QWhatsThis::add(mEmailSubjectEdit, i18n("Enter the email subject."));
	grid->addMultiCellWidget(mEmailSubjectEdit, 1, 1, 1, 2);

	// Email body
	mEmailMessageEdit = new QMultiLineEdit(mEmailFrame);
	QSize size = mEmailMessageEdit->sizeHint();
	size.setHeight(mEmailMessageEdit->fontMetrics().lineSpacing()*13/4 + 2*mEmailMessageEdit->frameWidth());
	mEmailMessageEdit->setMinimumSize(size);
	mEmailMessageEdit->setReadOnly(mReadOnly);
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

	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(i18n("Add..."), mEmailFrame);
		connect(button, SIGNAL(clicked()), SLOT(slotAddAttachment()));
		QWhatsThis::add(button, i18n("Add an attachment to the email."));
		grid->addWidget(button, 0, 2);

		mEmailRemoveButton = new QPushButton(i18n("Remo&ve"), mEmailFrame);
		connect(mEmailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
		QWhatsThis::add(mEmailRemoveButton, i18n("Remove the highlighted attachment from the email."));
		grid->addWidget(mEmailRemoveButton, 1, 2);
	}

	// BCC email to sender
	mEmailBcc = new CheckBox(i18n("Copy email to &self"), mEmailFrame);
	mEmailBcc->setFixedSize(mEmailBcc->sizeHint());
	mEmailBcc->setReadOnly(mReadOnly);
	QWhatsThis::add(mEmailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	grid->addMultiCellWidget(mEmailBcc, 1, 1, 0, 1, Qt::AlignLeft);
}

/******************************************************************************
 * Create a widget to choose the alarm message background colour.
 */
ColourCombo* EditAlarmDlg::createBgColourChooser(bool readOnly, QWidget* parent, const char* name)
{
	ColourCombo* widget = new ColourCombo(parent, name);
	QSize size = widget->sizeHint();
	widget->setMinimumHeight(size.height() + 4);
	widget->setReadOnly(readOnly);
	QToolTip::add(widget, i18n("Message color"));
	QWhatsThis::add(widget, i18n("Choose the background color for the alarm message."));
	return widget;
}

/******************************************************************************
 * Create an "acknowledgement confirmation required" checkbox.
 */
CheckBox* EditAlarmDlg::createConfirmAckCheckbox(bool readOnly, QWidget* parent, const char* name)
{
	CheckBox* widget = new CheckBox(i18n("Confirm ac&knowledgement"), parent, name);
	widget->setReadOnly(readOnly);
	QWhatsThis::add(widget,
	      i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
	return widget;
}

/******************************************************************************
 * Create an "cancel if late" checkbox.
 */
CheckBox* EditAlarmDlg::createLateCancelCheckbox(bool readOnly, QWidget* parent, const char* name)
{
	CheckBox* widget = new CheckBox(i18n("Ca&ncel if late"), parent, name);
	widget->setReadOnly(readOnly);
	QWhatsThis::add(widget,
	      i18n("If checked, the alarm will be canceled if it cannot be triggered within 1 "
	           "minute of the specified time. Possible reasons for not triggering include your "
	           "being logged off, X not running, or the alarm daemon not running.\n\n"
	           "If unchecked, the alarm will be triggered at the first opportunity after "
	           "the specified time, regardless of how late it is."));
	return widget;
}


/******************************************************************************
 * Get the currently entered message data.
 * The data is returned in the supplied Event instance.
 */
void EditAlarmDlg::getEvent(KAlarmEvent& event)
{
	event.set(mAlarmDateTime, mAlarmMessage, mBgColourChoose->color(), mFontColourButton->font(),
	          getAlarmType(), getAlarmFlags());
	event.setAudioFile(mSoundPicker->file());
	event.setEmail(mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
	event.setReminder(getReminderMinutes());
	if (mRecurRadio->isOn())
	{
		mRecurrenceEdit->updateEvent(event);
		if (mDeferDateTime.isValid()  &&  mDeferDateTime < mAlarmDateTime)
		{
			bool deferReminder = false;
			int reminder = getReminderMinutes();
			if (reminder)
			{
				QDateTime remindTime = mAlarmDateTime.addSecs(-reminder * 60);
				if (mDeferDateTime > remindTime)
					deferReminder = true;
			}
			event.defer(mDeferDateTime, deferReminder, false);
		}
	}
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	bool displayAlarm = mMessageRadio->isOn() || mFileRadio->isOn();
	return (displayAlarm && mSoundPicker->beep()          ? KAlarmEvent::BEEP : 0)
	     | (mLateCancel->isChecked()                      ? KAlarmEvent::LATE_CANCEL : 0)
	     | (displayAlarm && mConfirmAck->isChecked()      ? KAlarmEvent::CONFIRM_ACK : 0)
	     | (mEmailRadio->isOn() && mEmailBcc->isChecked() ? KAlarmEvent::EMAIL_BCC : 0)
	     | (mRepeatAtLoginRadio->isChecked()              ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mAlarmAnyTime                                 ? KAlarmEvent::ANY_TIME : 0)
	     | (mFontColourButton->defaultFont()              ? KAlarmEvent::DEFAULT_FONT : 0);
}

/******************************************************************************
 * Get the specified number of minutes in advance of the main alarm the
 * reminder is to be.
 */
int EditAlarmDlg::getReminderMinutes() const
{
	if (!mReminder->isChecked())
		return 0;
	int warning = mReminderCount->value();
	switch (mReminderUnits->currentItem())
	{
		case REMIND_HOURS_MINUTES:
			break;
		case REMIND_DAYS:
			warning *= 24*60;
			break;
		case REMIND_WEEKS:
			warning *= 7*24*60;
			break;
	}
	return warning;
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAlarmEvent::Action EditAlarmDlg::getAlarmType() const
{
	return mFileRadio->isOn()    ? KAlarmEvent::FILE
	     : mCommandRadio->isOn() ? KAlarmEvent::COMMAND
	     : mEmailRadio->isOn()   ? KAlarmEvent::EMAIL
	     :                         KAlarmEvent::MESSAGE;
}

/******************************************************************************
*  Return the alarm's start date and time.
*/
QDateTime EditAlarmDlg::getDateTime(bool* anyTime)
{
	mTimeWidget->getDateTime(mAlarmDateTime, mAlarmAnyTime);
	if (anyTime)
		*anyTime = mAlarmAnyTime;
	return mAlarmDateTime;
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
		mBasicSize.setHeight(mBasicSize.height() - mDeferGroupHeight);
		theApp()->writeConfigWindowSize("EditDialog", mBasicSize);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Set up the new alarm.
*/
void EditAlarmDlg::slotOk()
{
	QWidget* errWidget = mTimeWidget->getDateTime(mAlarmDateTime, mAlarmAnyTime, false);
	if (errWidget)
	{
		showPage(mMainPageIndex);
		errWidget->setFocus();
		mTimeWidget->getDateTime(mAlarmDateTime, mAlarmAnyTime);   // display the error message now
	}
	else if (checkEmailData())
	{
		bool noTime;
		errWidget = mRecurrenceEdit->checkData(mAlarmDateTime, noTime);
		if (errWidget)
		{
			showPage(mRecurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, (noTime ? i18n("End date is earlier than start date")
			                                 : i18n("End date/time is earlier than start date/time")));
			return;
		}
		if (mRecurRadio->isOn())
		{
			int reminder = getReminderMinutes();
			if (reminder)
			{
				KAlarmEvent event;
				mRecurrenceEdit->updateEvent(event);
				int minutes = event.recurInterval();
				switch (event.recurType())
				{
					case KAlarmEvent::NO_RECUR:     minutes = 0;  break;
					case KAlarmEvent::MINUTELY:     break;
					case KAlarmEvent::DAILY:        minutes *= 1440;  break;
					case KAlarmEvent::WEEKLY:       minutes *= 7*1440;  break;
					case KAlarmEvent::MONTHLY_DAY:
					case KAlarmEvent::MONTHLY_POS:  minutes *= 28*1440;  break;
					case KAlarmEvent::ANNUAL_DATE:
					case KAlarmEvent::ANNUAL_POS:
					case KAlarmEvent::ANNUAL_DAY:   minutes *= 365*1440;  break;
				}
				if (minutes  &&  reminder >= minutes)
				{
					showPage(mMainPageIndex);
					mReminderCount->setFocus();
					KMessageBox::sorry(this, i18n("Reminder period must be less than recurrence interval"));
					return;
				}
			}
		}
		if (checkText(mAlarmMessage))
			accept();
	}
}

/******************************************************************************
*  Called when the Try button is clicked.
*  Display the alarm immediately for the user to check its configuration.
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
		KAlarmEvent event;
		event.set(QDateTime(), text, mBgColourChoose->color(), mFontColourButton->font(), getAlarmType(), getAlarmFlags());
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
					bcc = i18n("\nBcc: %1").arg(theApp()->settings()->emailAddress());
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
 * Called when the Change deferral button is clicked.
 */
void EditAlarmDlg::slotEditDeferral()
{
	bool anyTime;
	QDateTime start;
	if (!mTimeWidget->getDateTime(start, anyTime))
	{
		bool deferred = mDeferDateTime.isValid();
		DeferAlarmDlg* deferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), (deferred ? mDeferDateTime : QDateTime::currentDateTime().addSecs(60)),
		                                            deferred, this, "deferDlg");
		// Don't allow deferral past the next recurrence
		int reminder = getReminderMinutes();
		if (reminder)
		{
			QDateTime remindTime = start.addSecs(-reminder * 60);
			if (QDateTime::currentDateTime() < remindTime)
				start = remindTime;
		}
		deferDlg->setLimit(start);
		if (deferDlg->exec() == QDialog::Accepted)
		{
			mDeferDateTime = deferDlg->getDateTime();
			mDeferTimeLabel->setText(mDeferDateTime.isValid() ? KGlobal::locale()->formatDateTime(mDeferDateTime) : QString::null);
		}
	}
#warning "If only the deferral time is changed, ensure that event ID is retained"
}

/******************************************************************************
*  Called when the main page is shown.
*  Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
	slotAlarmTypeClicked(-1);
}

/******************************************************************************
*  Called when the recurrence edit page is shown.
*  The first time, for a new alarm, the recurrence end date is set according to
*  the alarm start time.
*/
void EditAlarmDlg::slotShowRecurrenceEdit()
{
	mRecurPageIndex = activePageIndex();
	mTimeWidget->getDateTime(mAlarmDateTime, mAlarmAnyTime, false);
	if (mRecurSetEndDate)
	{
		QDateTime now = QDateTime::currentDateTime();
		mRecurrenceEdit->setEndDate(mAlarmDateTime >= now ? mAlarmDateTime.date() : now.date());
		mRecurSetEndDate = false;
	}
	mRecurrenceEdit->setStartDate(mAlarmDateTime.date());
}

/******************************************************************************
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
	bool recurs = mRecurRadio->isOn();
	mTimeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
	if (mDeferGroup)
		mDeferGroup->setEnabled(recurs);
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
*  Called when one of the message type radio buttons is clicked.
*/
void EditAlarmDlg::slotAlarmTypeClicked(int)
{
	if (mMessageRadio->isOn())
	{
		mFileBox->hide();
		mFilePadding->hide();
		mTextMessageEdit->show();
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
		mAlarmTypeStack->raiseWidget(mDisplayAlarmsFrame);
		mTextMessageEdit->setFocus();
	}
	else if (mFileRadio->isOn())
	{
		mTextMessageEdit->hide();
		mFileBox->show();
		mFilePadding->show();
		setButtonWhatsThis(Try, i18n("Display the text file now"));
		mAlarmTypeStack->raiseWidget(mDisplayAlarmsFrame);
		mFileMessageEdit->setNoSelect();
		mFileMessageEdit->setFocus();
	}
	else if (mCommandRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Execute the specified command now"));
		mAlarmTypeStack->raiseWidget(mCommandFrame);
		mCommandMessageEdit->setNoSelect();
		mCommandMessageEdit->setFocus();
	}
	else if (mEmailRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
		mAlarmTypeStack->raiseWidget(mEmailFrame);
		mEmailToEdit->setNoSelect();
		mEmailToEdit->setFocus();
	}
}

/******************************************************************************
*  Called when one of the repetition radio buttons is clicked.
*/
void EditAlarmDlg::slotRepeatClicked(int)
{
	bool on = mRecurRadio->isOn();
	if (on)
		mRecurTabStack->raiseWidget(mRecurrenceEdit);
	else
		mRecurTabStack->raiseWidget(mRecurDisabled);
	mRecurrenceEdit->setEnabled(on);
}

/******************************************************************************
*  Called when the browse button is pressed to select a file to display.
*/
void EditAlarmDlg::slotBrowseFile()
{
	if (mFileDefaultDir.isEmpty())
		mFileDefaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(mFileDefaultDir, QString::null, this, i18n("Choose Text File to Display"));
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
	if (mReminder->isChecked())
	 	setReminderAnyTime(getReminderMinutes(), anyTime);
}

/******************************************************************************
*  Set the advance reminder units to days if "Any time" is checked.
*/
EditAlarmDlg::ReminderUnits EditAlarmDlg::setReminderAnyTime(int reminderMinutes, bool anyTime)
{
	mReminderUnits->setEnabled(!anyTime);
	ReminderUnits units = static_cast<ReminderUnits>(mReminderUnits->currentItem());
	if (anyTime  &&  units == REMIND_HOURS_MINUTES)
	{
		// Set units to days and round up the warning period
		units = REMIND_DAYS;
		mReminderUnits->setCurrentItem(REMIND_DAYS);
		mReminderUnits->setEnabled(false);
		mReminderCount->showUnit();
		mReminderCount->setUnitValue((reminderMinutes + 1439) / 1440);
	}
	return units;
}

/******************************************************************************
*  Initialise the "Advance reminder" controls.
*/
void EditAlarmDlg::setReminder(int minutes)
{
	bool on = !!minutes;
	mReminder->setChecked(on);
	mReminderCount->setEnabled(on);
	mReminderUnits->setEnabled(on);
	int item;
	if (minutes)
	{
		int count = minutes;
		if (minutes % (24*60))
			item = REMIND_HOURS_MINUTES;
		else if (minutes % (7*24*60))
		{
			item = REMIND_DAYS;
			count = minutes / (24*60);
		}
		else
		{
			item = REMIND_WEEKS;
			count = minutes / (7*24*60);
		}
		mReminderUnits->setCurrentItem(item);
		if (item == REMIND_HOURS_MINUTES)
			mReminderCount->setHourMinValue(count);
		else
			mReminderCount->setUnitValue(count);
		item = setReminderAnyTime(minutes, mTimeWidget->anyTime());
	}
	else
	{
		item = theApp()->settings()->defaultReminderUnits();
		mReminderUnits->setCurrentItem(item);
	}
	mReminderCount->showHourMin(item == REMIND_HOURS_MINUTES);
}

/******************************************************************************
*  Called when the Reminder checkbox is toggled.
*/
void EditAlarmDlg::slotReminderToggled(bool on)
{
	mReminderUnits->setEnabled(on);
	mReminderCount->setEnabled(on);
	if (on  &&  mTimeWidget->anyTime())
	 	setReminderAnyTime(getReminderMinutes(), true);
}

/******************************************************************************
*  Called when a new item is made current in the reminder units combo box.
*/
void EditAlarmDlg::slotReminderUnitsSelected(int index)
{
	mReminderCount->showHourMin(index == REMIND_HOURS_MINUTES);
}

/******************************************************************************
 * Get a selection from the Address Book.
 */
void EditAlarmDlg::openAddressBook()
{
#if KDE_VERSION >= 290
	KABC::Addressee a = KABC::AddresseeDialog::getAddressee(this);
	if (a.isEmpty())
		return;
	Person person(a.realName(), a.preferredEmail());
#else
	KabAPI addrDialog(this);
	if (addrDialog.init() != AddressBook::NoError)
	{
		KMessageBox::error(this, i18n("Unable to open address book"));
		return;
	}
	if (!addrDialog.exec())
		return;
	KabKey key;
	AddressBook::Entry entry;
	if (addrDialog.getEntry(entry, key) != AddressBook::NoError)
	{
		KMessageBox::sorry(this, i18n("Error getting entry from address book"));
		return;
	}
	Person person;
	QString name;
	addrDialog.addressbook()->literalName(entry, name);
	person.setName(name);
	if (!entry.emails.isEmpty())
		person.setEmail(entry.emails.first());   // use first email address
#endif
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
bool EditAlarmDlg::checkText(QString& result)
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
		enum Err { NONE = 0, NONEXISTENT, DIRECTORY, UNREADABLE, NOT_TEXT, HTML };
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
			switch (KAlarmApp::isTextFile(url))
			{
				case 1:   break;
				case 2:   err = HTML;  break;
				default:  err = NOT_TEXT;  break;
			}
		}
		if (err)
		{
			mFileMessageEdit->setFocus();
			QString errmsg;
			switch (err)
			{
				case NONEXISTENT:  errmsg = i18n("%1\nnot found");  break;
				case DIRECTORY:    errmsg = i18n("%1\nis a directory");  break;
				case UNREADABLE:   errmsg = i18n("%1\nis not readable");  break;
				case NOT_TEXT:     errmsg = i18n("%1\nappears not to be a text file");  break;
				case HTML:         errmsg = i18n("%1\nis an html/xml file");  break;
				case NONE:
				default:
					break;
			}
			if (KMessageBox::warningContinueCancel(this, errmsg.arg(alarmtext), QString::null, KStdGuiItem::cont())    // explicit button text is for KDE2 compatibility
			    == KMessageBox::Cancel)
				return false;
		}
		result = alarmtext;
	}
	return true;
}


void LineEdit::focusInEvent(QFocusEvent* e)
{
	if (noSelect)
		QFocusEvent::setReason(QFocusEvent::Other);
	QLineEdit::focusInEvent(e);
	if (noSelect)
	{
		QFocusEvent::resetReason();
		noSelect = false;
	}
}
