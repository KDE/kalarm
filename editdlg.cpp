/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
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
#if KDE_VERSION >= 290
#include <kabc/addresseedialog.h>
#else
#include <kabapi.h>
#endif
#include <kdebug.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "alarmtimewidget.h"
#include "soundpicker.h"
#include "reminder.h"
#include "recurrenceedit.h"
#include "colourcombo.h"
#include "fontcolourbutton.h"
#include "kamail.h"
#include "deferdlg.h"
#include "radiobutton.h"
#include "checkbox.h"
#include "combobox.h"
#include "spinbox.h"
#include "timeperiod.h"
#include "editdlg.moc"
#include "editdlgprivate.moc"

using namespace KCal;


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event, bool readOnly)
	: KDialogBase(KDialogBase::Tabbed, caption, (readOnly ? Cancel|Try : Ok|Cancel|Try),
	              (readOnly ? Cancel : Ok), parent, name),
	  mRecurPageShown(false),
	  mRecurSetDefaultEndDate(true),
	  mReminderDeferral(false),
	  mReminderArchived(false),
	  mEmailRemoveButton(0),
	  mDeferGroup(0),
	  mReadOnly(readOnly),
	  mSavedEvent(0)
{
	if (event  &&  event->action() == KAlarmEvent::COMMAND  &&  theApp()->noShellAccess())
		mReadOnly = true;     // don't allow editing of existing command alarms in kiosk mode

	QVBox* mainPageBox = addVBoxPage(i18n("&Alarm"));
	mMainPageIndex = pageIndex(mainPageBox);
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Recurrence tab
	QVBox* recurPage = addVBoxPage(i18n("&Recurrence"));
	mRecurPageIndex = pageIndex(recurPage);
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
	mMessageRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mMessageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(mMessageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	mFileRadio = new RadioButton(i18n("&File"), mActionGroup, "fileButton");
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	mFileRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mFileRadio,
	      i18n("If checked, the alarm will display the contents of a text or image file."));
	grid->addWidget(mFileRadio, 1, 2);
	grid->setColStretch(3, 1);

	// Command radio button
	mCommandRadio = new RadioButton(i18n("Co&mmand"), mActionGroup, "cmdButton");
	mCommandRadio->setFixedSize(mCommandRadio->sizeHint());
	mCommandRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(mCommandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(mCommandRadio, 1, 4);
	grid->setColStretch(5, 1);

	// Email radio button
	mEmailRadio = new RadioButton(i18n("&Email"), mActionGroup, "emailButton");
	mEmailRadio->setFixedSize(mEmailRadio->sizeHint());
	mEmailRadio->setReadOnly(mReadOnly);
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

	bool recurs = event && event->recurs();
	if (recurs  &&  event->deferred())
	{
		// Deferred date/time for event without a time or recurring event
		mDeferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), mainPage, "deferGroup");
		topLayout->addWidget(mDeferGroup);
		QLabel* label = new QLabel(i18n("Deferred to:"), mDeferGroup);
		label->setFixedSize(label->sizeHint());

		mDeferDateTime = event->deferDateTime();
		mDeferTimeLabel = new QLabel(mDeferDateTime.formatLocale(), mDeferGroup);

		if (!mReadOnly)
		{
			QPushButton* button = new QPushButton(i18n("C&hange..."), mDeferGroup);
			button->setFixedSize(button->sizeHint());
			connect(button, SIGNAL(clicked()), SLOT(slotEditDeferral()));
			QWhatsThis::add(button, i18n("Change the alarm's deferred time, or cancel the deferral"));
		}
		mDeferGroup->addSpace(0);
	}

	QBoxLayout* layout = new QHBoxLayout(topLayout);

	// Date and time entry
	mTimeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME | AlarmTimeWidget::NARROW,
	                                  mainPage, "timeGroup");
	mTimeWidget->setReadOnly(mReadOnly);
	connect(mTimeWidget, SIGNAL(anyTimeToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
	layout->addWidget(mTimeWidget);

	layout->addSpacing(marginHint() - spacingHint());
	QBoxLayout* vlayout = new QVBoxLayout(layout);
	layout->addStretch();

	// Repetition type radio buttons
	QHBox* box = new QHBox(mainPage);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("Recurrence:"), box);
	label->setFixedSize(label->sizeHint());
	mRecurrenceText = new QLabel(box);
	QWhatsThis::add(box,
	      i18n("How often the alarm recurs.\nThe alarm's recurrence is configured in the Recurrence tab."));
	box->setFixedHeight(box->sizeHint().height());
	vlayout->addWidget(box, 0, Qt::AlignLeft);

	// Late display checkbox - default = allow late display
	mLateCancel = createLateCancelCheckbox(mReadOnly, mainPage);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	vlayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

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
		mTimeWidget->setDateTime(!event->mainExpired() ? event->mainDateTime()
		                         : recurs ? DateTime() : event->deferDateTime());

		setAction(event->action(), event->cleanText());
		if (event->action() == KAlarmEvent::EMAIL)
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
		mReminder->setMinutes(reminder, mTimeWidget->anyTime());
		mRecurrenceText->setText(event->recurrenceText());
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
		if (theApp()->noShellAccess())
			mCommandRadio->setEnabled(false);    // don't allow shell commands in kiosk mode
		Preferences* preferences = theApp()->preferences();
		mFontColourButton->setDefaultFont();
		mFontColourButton->setBgColour(preferences->defaultBgColour());
		mBgColourChoose->setColour(preferences->defaultBgColour());     // set colour before setting alarm type buttons
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		mTimeWidget->setDateTime(defaultTime);
		mActionGroup->setButton(mActionGroup->id(mMessageRadio));
		mLateCancel->setChecked(preferences->defaultLateCancel());
		mConfirmAck->setChecked(preferences->defaultConfirmAck());
		mReminder->setMinutes(0, false);
		mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
		slotRecurFrequencyChange();      // update the Recurrence text
		mSoundPicker->setFile(preferences->defaultSoundFile());
		mSoundPicker->setChecked(preferences->defaultBeep() || !preferences->defaultSoundFile().isEmpty());
		mEmailBcc->setChecked(preferences->defaultEmailBcc());
	}

	size = mBasicSize;
	size.setHeight(size.height() + mDeferGroupHeight);
	resize(size);

	bool enable = !!mEmailAttachList->count();
	mEmailAttachList->setEnabled(enable);
	if (mEmailRemoveButton)
		mEmailRemoveButton->setEnabled(enable);

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
	mTextMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(mTextMessageEdit, i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new QHBox(mDisplayAlarmsFrame);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(true, mFileBox);
	mFileMessageEdit->setReadOnly(mReadOnly);
	mFileMessageEdit->setAcceptDrops(true);
	QWhatsThis::add(mFileMessageEdit, i18n("Enter the name or URL of a text or image file to display."));

	// File browse button
	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(mFileBox);
		button->setPixmap(SmallIcon("fileopen"));
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotBrowseFile()));
		QWhatsThis::add(button, i18n("Select a text or image file to display."));
	}

	// Sound checkbox and file selector
	QBoxLayout* layout = new QHBoxLayout(frameLayout);
	mSoundPicker = new SoundPicker(mReadOnly, mDisplayAlarmsFrame);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	layout->addWidget(mSoundPicker);
	layout->addStretch();

	// Colour choice drop-down list
	mBgColourChoose = createBgColourChooser(mReadOnly, mDisplayAlarmsFrame);
	mBgColourChoose->setFixedSize(mBgColourChoose->sizeHint());
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
	if (mReadOnly)
		mFontColourButton->hide();
	mFontColourButton->setReadOnly(mReadOnly);
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	layout->addWidget(mFontColourButton);

	// Reminder
	mReminder = new Reminder(i18n("Rem&inder:"),
	                         i18n("Check to additionally display a reminder in advance of the main alarm time(s)."),
	                         i18n("Enter how long in advance of the main alarm to display a reminder alarm."),
	                         true, mDisplayAlarmsFrame);
	mReminder->setFixedSize(mReminder->sizeHint());
	mReminder->setReadOnly(mReadOnly);
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

	mCommandMessageEdit = new LineEdit(true, mCommandFrame);
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

	mEmailToEdit = new LineEdit(false, mEmailFrame);
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

	mEmailSubjectEdit = new LineEdit(false, mEmailFrame);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	mEmailSubjectEdit->setReadOnly(mReadOnly);
	label->setBuddy(mEmailSubjectEdit);
	QWhatsThis::add(mEmailSubjectEdit, i18n("Enter the email subject."));
	grid->addMultiCellWidget(mEmailSubjectEdit, 1, 1, 1, 2);

	// Email body
	mEmailMessageEdit = new TextEdit(mEmailFrame);
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
 * Set the dialog's action and the action's text.
 */
void EditAlarmDlg::setAction(KAlarmEvent::Action action, const QString& text)
{
	QRadioButton* radio;
	switch (action)
	{
		case KAlarmEvent::FILE:
			radio = mFileRadio;
			mFileMessageEdit->setText(text);
			break;
		case KAlarmEvent::COMMAND:
			radio = mCommandRadio;
			mCommandMessageEdit->setText(text);
			break;
		case KAlarmEvent::EMAIL:
			radio = mEmailRadio;
			mEmailMessageEdit->setText(text);
			break;
		case KAlarmEvent::MESSAGE:
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
 * Save the state of all controls.
 */
void EditAlarmDlg::saveState(const KAlarmEvent* event)
{
	mSavedEvent          = 0;
	if (event)
		mSavedEvent = new KAlarmEvent(*event);
	mSavedTypeRadio      = mActionGroup->selected();
	mSavedBeep           = mSoundPicker->beep();
	mSavedSoundFile      = mSoundPicker->file();
	mSavedConfirmAck     = mConfirmAck->isChecked();
	mSavedFont           = mFontColourButton->font();
	mSavedBgColour       = mBgColourChoose->color();
	mSavedReminder       = mReminder->getMinutes();
	checkText(mSavedTextFileCommandMessage, false);
	mSavedEmailTo        = mEmailToEdit->text();
	mSavedEmailSubject   = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0;  i < mEmailAttachList->count();  ++i)
		mSavedEmailAttach += mEmailAttachList->text(i);
	mSavedEmailBcc       = mEmailBcc->isChecked();
	mSavedDateTime       = mTimeWidget->getDateTime(false);
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
	if (mSavedTypeRadio        != mActionGroup->selected()
	||  mSavedDateTime         != mTimeWidget->getDateTime()
	||  mSavedLateCancel       != mLateCancel->isChecked()
	||  textFileCommandMessage != mSavedTextFileCommandMessage
	||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType())
		return true;
	if (mMessageRadio->isOn()  ||  mFileRadio->isOn())
	{
		if (mSavedBeep       != mSoundPicker->beep()
		||  mSavedSoundFile  != mSoundPicker->file()
		||  mSavedConfirmAck != mConfirmAck->isChecked()
		||  mSavedFont       != mFontColourButton->font()
		||  mSavedBgColour   != mBgColourChoose->color()
		||  mSavedReminder   != mReminder->getMinutes())
			return true;
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
	if (mSavedEvent->recurType() != KAlarmEvent::NO_RECUR)
	{
		if (!mSavedEvent->recurrence())
			return true;
		KAlarmEvent event;
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
void EditAlarmDlg::getEvent(KAlarmEvent& event)
{
	if (!mSavedEvent  ||  stateChanged())
	{
		// It's a new event, or the edit controls have changed
		event.set(mAlarmDateTime.dateTime(), mAlarmMessage, mBgColourChoose->color(),
		          mFontColourButton->font(), getAlarmType(), getAlarmFlags());
		event.setAudioFile(mSoundPicker->file());
		event.setEmail(mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
		event.setReminder(mReminder->getMinutes());
		if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
		{
			mRecurrenceEdit->updateEvent(event);
			mAlarmDateTime = event.startDateTime();
			if (mDeferDateTime.isValid()  &&  mDeferDateTime < mAlarmDateTime)
			{
				bool deferReminder = false;
				int reminder = mReminder->getMinutes();
				if (reminder)
				{
					DateTime remindTime = mAlarmDateTime.addMins(-reminder);
					if (mDeferDateTime > remindTime)
						deferReminder = true;
				}
				event.defer(mDeferDateTime, deferReminder, false);
			}
		}
	}
	else
	{
		// Only the deferral time has been changed
		event = *mSavedEvent;
		event.defer(mDeferDateTime, event.reminderDeferral(), false);
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
	     | (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mAlarmDateTime.isDateOnly()                   ? KAlarmEvent::ANY_TIME : 0)
	     | (mFontColourButton->defaultFont()              ? KAlarmEvent::DEFAULT_FONT : 0);
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
DateTime EditAlarmDlg::getDateTime()
{
	mAlarmDateTime = mTimeWidget->getDateTime();
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
	if (activePageIndex() == mRecurPageIndex  &&  mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
	QWidget* errWidget;
	mAlarmDateTime = mTimeWidget->getDateTime(false, &errWidget);
	if (errWidget)
	{
		showPage(mMainPageIndex);
		errWidget->setFocus();
		mTimeWidget->getDateTime();   // display the error message now
	}
	else if (checkEmailData())
	{
		QString errmsg;
		errWidget = mRecurrenceEdit->checkData(mAlarmDateTime.dateTime(), errmsg);
		if (errWidget)
		{
			showPage(mRecurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, errmsg);
			return;
		}
		if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
		{
			int reminder = mReminder->getMinutes();
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
					mReminder->setFocusOnCount();
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
					bcc = i18n("\nBcc: %1").arg(theApp()->preferences()->emailAddress());
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
	DateTime start = mTimeWidget->getDateTime();
	if (start.isValid())
	{
		bool deferred = mDeferDateTime.isValid();
		DeferAlarmDlg* deferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), (deferred ? mDeferDateTime : DateTime(QDateTime::currentDateTime().addSecs(60))),
		                                            deferred, this, "deferDlg");
		// Don't allow deferral past the next recurrence
		int reminder = mReminder->getMinutes();
		if (reminder)
		{
			DateTime remindTime = start.addMins(-reminder);
			if (QDateTime::currentDateTime() < remindTime)
				start = remindTime;
		}
		deferDlg->setLimit(start);
		if (deferDlg->exec() == QDialog::Accepted)
		{
			mDeferDateTime = deferDlg->getDateTime();
			mDeferTimeLabel->setText(mDeferDateTime.isValid() ? mDeferDateTime.formatLocale() : QString::null);
		}
	}
}

/******************************************************************************
*  Called when the main page is shown.
*  Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
	slotAlarmTypeClicked(-1);
	if (mRecurPageShown  &&  mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
}

/******************************************************************************
*  Called when the recurrence edit page is shown.
*  The first time, for a new alarm, the recurrence end date is set according to
*  the alarm start time.
*/
void EditAlarmDlg::slotShowRecurrenceEdit()
{
	mRecurPageIndex = activePageIndex();
	mAlarmDateTime  = mTimeWidget->getDateTime(false);
	if (mRecurSetDefaultEndDate)
	{
		QDateTime now = QDateTime::currentDateTime();
		mRecurrenceEdit->setDefaultEndDate(mAlarmDateTime.dateTime() >= now ? mAlarmDateTime.date() : now.date());
		mRecurSetDefaultEndDate = false;
	}
	mRecurrenceEdit->setStartDate(mAlarmDateTime.date());
	if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		mRecurrenceEdit->setEndDateTime(mAlarmDateTime);
	mRecurPageShown = true;
}

/******************************************************************************
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
	bool recurs = (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR);
	mTimeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
	if (mDeferGroup)
		mDeferGroup->setEnabled(recurs);
	if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
		mRecurrenceEdit->setEndDateTime(mAlarmDateTime.dateTime());
	slotRecurFrequencyChange();
}

/******************************************************************************
*  Called when the recurrence frequency selection changes.
*  Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
	KAlarmEvent event;
	mRecurrenceEdit->updateEvent(event);
	QString text = event.recurrenceText();
	mRecurrenceText->setText(text.isNull() ? i18n("None") : text);
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
		mFontColourButton->show();
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
		mAlarmTypeStack->raiseWidget(mDisplayAlarmsFrame);
		mTextMessageEdit->setFocus();
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
			switch (KAlarmApp::fileType(KFileItem(KFileItem::Unknown, KFileItem::Unknown, url).mimetype()))
			{
				case 1:
				case 3:
				case 4:   break;
				case 2:
#if KDE_VERSION < 290
					err = HTML;
#endif
					break;
				default:  err = NOT_TEXT_IMAGE;  break;
			}
		}
		if (err  &&  showErrorMessage)
		{
			mFileMessageEdit->setFocus();
			QString errmsg;
			switch (err)
			{
				case NONEXISTENT:     errmsg = i18n("%1\nnot found");  break;
				case DIRECTORY:       errmsg = i18n("%1\nis a directory");  break;
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
LineEdit::LineEdit(bool url, QWidget* parent, const char* name)
	: KLineEdit(parent, name),
	  noSelect(false)
{
	if (url)
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
	if (noSelect)
		QFocusEvent::setReason(QFocusEvent::Other);
	KLineEdit::focusInEvent(e);
	if (noSelect)
	{
		QFocusEvent::resetReason();
		noSelect = false;
	}
}

void LineEdit::dragEnterEvent(QDragEnterEvent* e)
{
	e->accept(QTextDrag::canDecode(e)
	       || KURLDrag::canDecode(e));
}

void LineEdit::dropEvent(QDropEvent* e)
{
	QString text;
	KURL::List files;
	if (KURLDrag::decode(e, files)  &&  files.count())
		setText(files.first().prettyURL());
	else if (QTextDrag::decode(e, text))
	{
		int newline = text.find('\n');
		if (newline >= 0)
			text = text.left(newline);
		setText(text);
	}
}
