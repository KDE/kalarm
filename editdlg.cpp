/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
#include "kamail.h"
#include "deferdlg.h"
#include "buttongroup.h"
#include "radiobutton.h"
#include "checkbox.h"
#include "pushbutton.h"
#include "editdlg.moc"


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event, bool readOnly)
	: KDialogBase(KDialogBase::Tabbed, caption, (readOnly ? Cancel|Try : Ok|Cancel|Try),
	              (readOnly ? Cancel : Ok), parent, name),
	  recurSetEndDate(true),
	  emailRemoveButton(0),
	  deferGroup(0),
	  mReadOnly(readOnly)
{
	QVBox* mainPageBox = addVBoxPage(i18n("&Alarm"));
	mainPageIndex = pageIndex(mainPageBox);
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Recurrence tab
	QVBox* recurPage = addVBoxPage(i18n("&Recurrence"));
	recurPageIndex = pageIndex(recurPage);
	recurTabStack = new QWidgetStack(recurPage);

	recurrenceEdit = new RecurrenceEdit(readOnly, recurTabStack, "recurPage");
	recurTabStack->addWidget(recurrenceEdit, 0);
	connect(recurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(recurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));

	recurDisabled = new QLabel(i18n("The alarm does not recur.\nEnable recurrence in the Alarm tab."), recurTabStack);
	recurDisabled->setAlignment(Qt::AlignCenter);
	recurTabStack->addWidget(recurDisabled, 1);

	// Alarm action

	QButtonGroup* actionGroup = new QButtonGroup(i18n("Action"), mainPage, "actionGroup");
	connect(actionGroup, SIGNAL(clicked(int)), SLOT(slotAlarmTypeClicked(int)));
	topLayout->addWidget(actionGroup, 1);
	QGridLayout* grid = new QGridLayout(actionGroup, 3, 5, marginKDE2 + marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	// Message radio button
	messageRadio = new RadioButton(i18n("Te&xt"), actionGroup, "messageButton");
	messageRadio->setFixedSize(messageRadio->sizeHint());
	messageRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(messageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(messageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	fileRadio = new RadioButton(i18n("&File"), actionGroup, "fileButton");
	fileRadio->setFixedSize(fileRadio->sizeHint());
	fileRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(fileRadio,
	      i18n("If checked, the alarm will display the contents of a text file."));
	grid->addWidget(fileRadio, 1, 2);
	grid->setColStretch(3, 1);

	// Command radio button
	commandRadio = new RadioButton(i18n("Co&mmand"), actionGroup, "cmdButton");
	commandRadio->setFixedSize(commandRadio->sizeHint());
	commandRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(commandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(commandRadio, 1, 4);
	grid->setColStretch(5, 1);

	// Email radio button
	emailRadio = new RadioButton(i18n("&Email"), actionGroup, "emailButton");
	emailRadio->setFixedSize(emailRadio->sizeHint());
	emailRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(emailRadio,
	      i18n("If checked, the alarm will send an email."));
	grid->addWidget(emailRadio, 1, 6);

	initDisplayAlarms(actionGroup);
	initCommand(actionGroup);
	initEmail(actionGroup);
	alarmTypeStack = new QWidgetStack(actionGroup);
	grid->addMultiCellWidget(alarmTypeStack, 2, 2, 0, 6);
	grid->setRowStretch(2, 1);
	alarmTypeStack->addWidget(displayAlarmsFrame, 0);
	alarmTypeStack->addWidget(commandFrame, 1);
	alarmTypeStack->addWidget(emailFrame, 1);

	if (event  &&  event->recurs() != KAlarmEvent::NO_RECUR  &&  event->deferred())
	{
		// Recurring event's deferred date/time
		deferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), mainPage, "deferGroup");
		topLayout->addWidget(deferGroup);
		QLabel* label = new QLabel(i18n("Deferred to:"), deferGroup);
		label->setFixedSize(label->sizeHint());

		deferDateTime = event->deferDateTime();
		deferTimeLabel = new QLabel(KGlobal::locale()->formatDateTime(deferDateTime), deferGroup);

		if (!mReadOnly)
		{
			QPushButton* button = new QPushButton(i18n("C&hange..."), deferGroup);
			button->setFixedSize(button->sizeHint());
			connect(button, SIGNAL(clicked()), SLOT(slotEditDeferral()));
			QWhatsThis::add(button, i18n("Change the alarm's deferred time, or cancel the deferral"));
		}
		deferGroup->addSpace(0);
	}

	grid = new QGridLayout(topLayout, 3, 2, spacingHint());

	// Date and time entry

	timeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME | AlarmTimeWidget::NARROW,
	                                 mainPage, "timeGroup");
	timeWidget->setReadOnly(mReadOnly);
	grid->addMultiCellWidget(timeWidget, 0, 2, 0, 0);

	// Repetition type radio buttons

	ButtonGroup* repeatGroup = new ButtonGroup(1, Qt::Horizontal, i18n("Repetition"), mainPage);
	connect(repeatGroup, SIGNAL(buttonSet(int)), SLOT(slotRepeatClicked(int)));
	grid->addWidget(repeatGroup, 0, 1);
	grid->setRowStretch(1, 1);

	noRepeatRadio = new RadioButton(i18n("&No repetition"), repeatGroup);
	noRepeatRadio->setFixedSize(noRepeatRadio->sizeHint());
	noRepeatRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(noRepeatRadio,
	      i18n("Trigger the alarm once only"));

	repeatAtLoginRadio = new RadioButton(i18n("Repeat at lo&gin"), repeatGroup, "repeatAtLoginButton");
	repeatAtLoginRadio->setFixedSize(repeatAtLoginRadio->sizeHint());
	repeatAtLoginRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(repeatAtLoginRadio,
	      i18n("Repeat the alarm at every login until the specified time.\n"
	           "Note that it will also be repeated any time the alarm daemon is restarted."));

	recurRadio = new RadioButton(i18n("Rec&ur"), repeatGroup);
	recurRadio->setFixedSize(recurRadio->sizeHint());
	recurRadio->setReadOnly(mReadOnly);
	QWhatsThis::add(recurRadio, i18n("Regularly repeat the alarm"));

	// Late display checkbox - default = allow late display
	lateCancel = createLateCancelCheckbox(mReadOnly, mainPage);
	lateCancel->setFixedSize(lateCancel->sizeHint());
	grid->addWidget(lateCancel, 1, 1, Qt::AlignLeft);
	grid->setColStretch(1, 1);

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	topLayout->activate();

	deferGroupHeight = deferGroup ? deferGroup->height() + spacingHint() : 0;
	QSize size = minimumSize();
	size.setHeight(size.height() - deferGroupHeight);
	basicSize = theApp()->readConfigWindowSize("EditDialog", size);
	resize(basicSize);

	// Set up initial values
	if (event)
	{
		// Set the values to those for the specified event
#ifdef SELECT_FONT
		fontColour->setColour(event->colour());
		fontColour->setFont(?);
#endif
		bgColourChoose->setColour(event->colour());     // set colour before setting alarm type buttons
		timeWidget->setDateTime(event->mainDateTime(), event->anyTime());

		QRadioButton* radio;
		switch (event->action())
		{
			case KAlarmEvent::FILE:
				radio = fileRadio;
				fileMessageEdit->setText(event->cleanText());
				break;
			case KAlarmEvent::COMMAND:
				radio = commandRadio;
				commandMessageEdit->setText(event->cleanText());
				break;
			case KAlarmEvent::EMAIL:
				radio = emailRadio;
				emailMessageEdit->setText(event->cleanText());
				emailAttachList->insertStringList(event->emailAttachments());
				break;
			case KAlarmEvent::MESSAGE:
			default:
				radio = messageRadio;
				textMessageEdit->setText(event->cleanText());
				break;
		}
		actionGroup->setButton(actionGroup->id(radio));

		if (event->recurs() != KAlarmEvent::NO_RECUR)
		{
			radio = recurRadio;
			recurSetEndDate = false;
		}
		else if (event->repeatAtLogin())
			radio = repeatAtLoginRadio;
		else
			radio = noRepeatRadio;
		repeatGroup->setButton(repeatGroup->id(radio));

		lateCancel->setChecked(event->lateCancel());
		confirmAck->setChecked(event->confirmAck());
		recurrenceEdit->set(*event);   // must be called after timeWidget is set up, to ensure correct date-only enabling
		soundPicker->setFile(event->audioFile());
		soundPicker->setChecked(event->beep() || !event->audioFile().isEmpty());
		emailToEdit->setText(event->emailAddresses(", "));
		emailSubjectEdit->setText(event->emailSubject());
		emailBcc->setChecked(event->emailBcc());
	}
	else
	{
		// Set the values to their defaults
		Settings* settings = theApp()->settings();
#ifdef SELECT_FONT
		fontColour->setColour(settings->defaultBgColour());
		fontColour->setFont(settings->messageFont());
#endif
		bgColourChoose->setColour(settings->defaultBgColour());     // set colour before setting alarm type buttons
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		timeWidget->setDateTime(defaultTime, false);
		actionGroup->setButton(actionGroup->id(messageRadio));
		repeatGroup->setButton(repeatGroup->id(noRepeatRadio));
		lateCancel->setChecked(settings->defaultLateCancel());
		confirmAck->setChecked(settings->defaultConfirmAck());
		recurrenceEdit->setDefaults(defaultTime);   // must be called after timeWidget is set up, to ensure correct date-only enabling
		soundPicker->setChecked(settings->defaultBeep());
		emailBcc->setChecked(settings->defaultEmailBcc());
	}

	size = basicSize;
	size.setHeight(size.height() + deferGroupHeight);
	resize(size);

	bool enable = !!emailAttachList->count();
	emailAttachList->setEnabled(enable);
	if (emailRemoveButton)
		emailRemoveButton->setEnabled(enable);
}

EditAlarmDlg::~EditAlarmDlg()
{
}

/******************************************************************************
 * Set up the dialog controls common to display alarms.
 */
void EditAlarmDlg::initDisplayAlarms(QWidget* parent)
{
	displayAlarmsFrame = new QFrame(parent);
	displayAlarmsFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* frameLayout = new QVBoxLayout(displayAlarmsFrame, 0, spacingHint());

	// Text message edit box
	textMessageEdit = new QMultiLineEdit(displayAlarmsFrame);
	QSize tsize = textMessageEdit->sizeHint();
	tsize.setHeight(textMessageEdit->fontMetrics().lineSpacing()*13/4 + 2*textMessageEdit->frameWidth());
	textMessageEdit->setMinimumSize(tsize);
	textMessageEdit->setWordWrap(QMultiLineEdit::NoWrap);
	textMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(textMessageEdit, i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(textMessageEdit);

	// File name edit box
	fileBox = new QHBox(displayAlarmsFrame);
	frameLayout->addWidget(fileBox);
	fileMessageEdit = new LineEdit(fileBox);
	fileMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(fileMessageEdit, i18n("Enter the name of a text file, or a URL, to display."));

	// File browse button
	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(fileBox);
		button->setPixmap(SmallIcon("fileopen"));
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotBrowseFile()));
		QWhatsThis::add(button, i18n("Select a text file to display."));
	}

	// Sound checkbox and file selector
	QBoxLayout* layout = new QHBoxLayout(frameLayout);
	soundPicker = new SoundPicker(mReadOnly, displayAlarmsFrame);
	soundPicker->setFixedSize(soundPicker->sizeHint());
	layout->addWidget(soundPicker);
	layout->addStretch();

#ifdef SELECT_FONT
	// Font and colour choice drop-down list
	fontButton = new QPushButton(i18n("Font && Co&lor..."), displayAlarmsFrame);
	fontColour = new FontColourChooser(displayAlarmsFrame, 0, false, QStringList(), true, i18n("Font and background color"), false);
	size = fontColour->sizeHint();
	fontColour->setMinimumHeight(size.height() + 4);
	QWhatsThis::add(fontColour,
	      i18n("Choose the font and background color for the alarm message."));
	layout->addWidget(fontColour);
	if (mReadOnly)
		new ReadOnlyCover(fontButton, this);
#endif

	// Colour choice drop-down list
	bgColourChoose = createBgColourChooser(mReadOnly, displayAlarmsFrame);
	layout->addWidget(bgColourChoose);

	// Acknowledgement confirmation required - default = no confirmation
	confirmAck = createConfirmAckCheckbox(mReadOnly, displayAlarmsFrame);
	confirmAck->setFixedSize(confirmAck->sizeHint());
	frameLayout->addWidget(confirmAck, 0, Qt::AlignLeft);

	filePadding = new QHBox(displayAlarmsFrame);
	frameLayout->addWidget(filePadding);
	frameLayout->setStretchFactor(filePadding, 1);
}

/******************************************************************************
 * Set up the command alarm dialog controls.
 */
void EditAlarmDlg::initCommand(QWidget* parent)
{
	commandFrame = new QFrame(parent);
	commandFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* layout = new QVBoxLayout(commandFrame);

	commandMessageEdit = new LineEdit(commandFrame);
	commandMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(commandMessageEdit, i18n("Enter a shell command to execute."));
	layout->addWidget(commandMessageEdit);
	layout->addStretch();
}

/******************************************************************************
 * Set up the email alarm dialog controls.
 */
void EditAlarmDlg::initEmail(QWidget* parent)
{
	emailFrame = new QFrame(parent);
	emailFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* layout = new QVBoxLayout(emailFrame, 0, spacingHint());
	QGridLayout* grid = new QGridLayout(layout, 2, 3, spacingHint());
	grid->setColStretch(1, 1);

	// Email recipients
	QLabel* label = new QLabel(i18n("To:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	emailToEdit = new LineEdit(emailFrame);
	emailToEdit->setMinimumSize(emailToEdit->sizeHint());
	emailToEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(emailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	grid->addWidget(emailToEdit, 0, 1);

	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(emailFrame);
		button->setPixmap(SmallIcon("contents"));
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(openAddressBook()));
		QToolTip::add(button, i18n("Open address book"));
		QWhatsThis::add(button, i18n("Select email addresses from your address book."));
		grid->addWidget(button, 0, 2);
	}

	// Email subject
	label = new QLabel(i18n("Sub&ject:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	emailSubjectEdit = new QLineEdit(emailFrame);
	emailSubjectEdit->setMinimumSize(emailSubjectEdit->sizeHint());
	emailSubjectEdit->setReadOnly(mReadOnly);
	label->setBuddy(emailSubjectEdit);
	QWhatsThis::add(emailSubjectEdit, i18n("Enter the email subject."));
	grid->addMultiCellWidget(emailSubjectEdit, 1, 1, 1, 2);

	// Email body
	emailMessageEdit = new QMultiLineEdit(emailFrame);
	QSize size = emailMessageEdit->sizeHint();
	size.setHeight(emailMessageEdit->fontMetrics().lineSpacing()*13/4 + 2*emailMessageEdit->frameWidth());
	emailMessageEdit->setMinimumSize(size);
	emailMessageEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(emailMessageEdit, i18n("Enter the email message."));
	layout->addWidget(emailMessageEdit);

	// Email attachments
	grid = new QGridLayout(layout, 2, 3, spacingHint());
	label = new QLabel(i18n("Attachment&s:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	emailAttachList = new QComboBox(true, emailFrame);
	emailAttachList->setMinimumSize(emailAttachList->sizeHint());
	emailAttachList->lineEdit()->setReadOnly(true);
QListBox* list = emailAttachList->listBox();
QRect rect = list->geometry();
list->setGeometry(rect.left() - 50, rect.top(), rect.width(), rect.height());
	label->setBuddy(emailAttachList);
	QWhatsThis::add(emailAttachList,
	      i18n("Files to send as attachments to the email."));
	grid->addWidget(emailAttachList, 0, 1);
	grid->setColStretch(1, 1);

	if (!mReadOnly)
	{
		QPushButton* button = new QPushButton(i18n("Add..."), emailFrame);
		connect(button, SIGNAL(clicked()), SLOT(slotAddAttachment()));
		QWhatsThis::add(button, i18n("Add an attachment to the email."));
		grid->addWidget(button, 0, 2);

		emailRemoveButton = new QPushButton(i18n("Remo&ve"), emailFrame);
		connect(emailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
		QWhatsThis::add(emailRemoveButton, i18n("Remove the highlighted attachment from the email."));
		grid->addWidget(emailRemoveButton, 1, 2);
	}

	// BCC email to sender
	emailBcc = new CheckBox(i18n("Co&py email to self"), emailFrame);
	emailBcc->setFixedSize(emailBcc->sizeHint());
	emailBcc->setReadOnly(mReadOnly);
	QWhatsThis::add(emailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	grid->addMultiCellWidget(emailBcc, 1, 1, 0, 1, Qt::AlignLeft);
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
	CheckBox* widget = new CheckBox(i18n("Cancel &if late"), parent, name);
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
	event.set(alarmDateTime, alarmMessage, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
	event.setAudioFile(soundPicker->file());
	event.setEmail(emailAddresses, emailSubjectEdit->text(), emailAttachments);
	if (recurRadio->isOn())
	{
		recurrenceEdit->updateEvent(event);
		if (deferDateTime.isValid()  &&  deferDateTime < alarmDateTime)
			event.defer(deferDateTime);
	}
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	bool displayAlarm = messageRadio->isOn() || fileRadio->isOn();
	return (displayAlarm && soundPicker->beep()         ? KAlarmEvent::BEEP : 0)
	     | (lateCancel->isChecked()                     ? KAlarmEvent::LATE_CANCEL : 0)
	     | (displayAlarm && confirmAck->isChecked()     ? KAlarmEvent::CONFIRM_ACK : 0)
	     | (emailRadio->isOn() && emailBcc->isChecked() ? KAlarmEvent::EMAIL_BCC : 0)
	     | (repeatAtLoginRadio->isChecked()             ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (alarmAnyTime                                ? KAlarmEvent::ANY_TIME : 0);
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAlarmEvent::Action EditAlarmDlg::getAlarmType() const
{
	return fileRadio->isOn()    ? KAlarmEvent::FILE
	     : commandRadio->isOn() ? KAlarmEvent::COMMAND
	     : emailRadio->isOn()   ? KAlarmEvent::EMAIL
	     :                        KAlarmEvent::MESSAGE;
}

/******************************************************************************
*  Return the alarm's start date and time.
*/
QDateTime EditAlarmDlg::getDateTime(bool* anyTime)
{
	timeWidget->getDateTime(alarmDateTime, alarmAnyTime);
	if (anyTime)
		*anyTime = alarmAnyTime;
	return alarmDateTime;
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
		basicSize = re->size();
		basicSize.setHeight(basicSize.height() - deferGroupHeight);
		theApp()->writeConfigWindowSize("EditDialog", basicSize);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Set up the new alarm.
*/
void EditAlarmDlg::slotOk()
{
	QWidget* errWidget = timeWidget->getDateTime(alarmDateTime, alarmAnyTime, false);
	if (errWidget)
	{
		showPage(mainPageIndex);
		errWidget->setFocus();
		timeWidget->getDateTime(alarmDateTime, alarmAnyTime);   // display the error message now
	}
	else if (checkEmailData())
	{
		bool noTime;
		errWidget = recurrenceEdit->checkData(alarmDateTime, noTime);
		if (errWidget)
		{
			showPage(recurPageIndex);
			errWidget->setFocus();
			KMessageBox::sorry(this, (noTime ? i18n("End date is earlier than start date")
			                                 : i18n("End date/time is earlier than start date/time")));
		}
		else if (checkText(alarmMessage))
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
		if (emailRadio->isOn())
		{
			if (!checkEmailData()
			||  KMessageBox::warningContinueCancel(this, i18n("Do you really want to send the email now to the specified recipient(s)?"),
			                                       i18n("Confirm Email"), i18n("&Send")) != KMessageBox::Continue)
				return;
		}
		KAlarmEvent event;
		event.set(QDateTime(), text, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
		event.setAudioFile(soundPicker->file());
		event.setEmail(emailAddresses, emailSubjectEdit->text(), emailAttachments);
		void* proc = theApp()->execAlarm(event, event.firstAlarm(), false, false);
		if (proc)
		{
			if (commandRadio->isOn())
			{
				theApp()->commandMessage((KProcess*)proc, this);
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
				theApp()->commandMessage((KProcess*)proc, 0);
			}
			else if (emailRadio->isOn())
			{
				QString bcc;
				if (emailBcc->isChecked())
					bcc = i18n("\nBcc: %1").arg(theApp()->settings()->emailAddress());
				KMessageBox::information(this, i18n("Email sent to:\n%1%2").arg(emailAddresses.join("\n")).arg(bcc));
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
	if (!timeWidget->getDateTime(start, anyTime))
	{
		bool deferred = deferDateTime.isValid();
		DeferAlarmDlg* deferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), (deferred ? deferDateTime : QDateTime::currentDateTime().addSecs(60)),
		                                            deferred, this, "deferDlg");
		deferDlg->setLimit(start);
		if (deferDlg->exec() == QDialog::Accepted)
		{
			deferDateTime = deferDlg->getDateTime();
			deferTimeLabel->setText(deferDateTime.isValid() ? KGlobal::locale()->formatDateTime(deferDateTime) : QString::null);
		}
	}
}

/******************************************************************************
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
	bool recurs = recurRadio->isOn();
	timeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
	if (deferGroup)
		deferGroup->setEnabled(recurs);
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
	recurPageIndex = activePageIndex();
	timeWidget->getDateTime(alarmDateTime, alarmAnyTime, false);
	if (recurSetEndDate)
	{
		QDateTime now = QDateTime::currentDateTime();
		recurrenceEdit->setEndDate(alarmDateTime >= now ? alarmDateTime.date() : now.date());
		recurSetEndDate = false;
	}
	recurrenceEdit->setStartDate(alarmDateTime.date());
}

/******************************************************************************
*  Convert the email addresses to a list, and validate them. Convert the email
*  attachments to a list.
*/
bool EditAlarmDlg::checkEmailData()
{
	if (emailRadio->isOn())
	{
		QString addrs = emailToEdit->text();
		if (addrs.isEmpty())
			emailAddresses.clear();
		else
		{
			QString bad = KAMail::convertAddresses(addrs, emailAddresses);
			if (!bad.isEmpty())
			{
				emailToEdit->setFocus();
				KMessageBox::error(this, i18n("Invalid email address:\n%1").arg(bad));
				return false;
			}
		}
		if (emailAddresses.isEmpty())
		{
			emailToEdit->setFocus();
			KMessageBox::error(this, i18n("No email address specified"));
			return false;
		}

		emailAttachments.clear();
		for (int i = 0;  i < emailAttachList->count();  ++i)
		{
			QString att = emailAttachList->text(i);
			switch (KAMail::checkAttachment(att))
			{
				case 1:
					emailAttachments.append(att);
					break;
				case 0:
					break;      // empty
				case -1:
					emailAttachList->setFocus();
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
	if (messageRadio->isOn())
	{
		fileBox->hide();
		filePadding->hide();
		textMessageEdit->show();
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
		alarmTypeStack->raiseWidget(displayAlarmsFrame);
		textMessageEdit->setFocus();
	}
	else if (fileRadio->isOn())
	{
		textMessageEdit->hide();
		fileBox->show();
		filePadding->show();
		setButtonWhatsThis(Try, i18n("Display the text file now"));
		alarmTypeStack->raiseWidget(displayAlarmsFrame);
		fileMessageEdit->setNoSelect();
		fileMessageEdit->setFocus();
	}
	else if (commandRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Execute the specified command now"));
		alarmTypeStack->raiseWidget(commandFrame);
		commandMessageEdit->setNoSelect();
		commandMessageEdit->setFocus();
	}
	else if (emailRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
		alarmTypeStack->raiseWidget(emailFrame);
		emailToEdit->setNoSelect();
		emailToEdit->setFocus();
	}
}

/******************************************************************************
*  Called when one of the repetition radio buttons is clicked.
*/
void EditAlarmDlg::slotRepeatClicked(int)
{
	bool on = recurRadio->isOn();
	if (on)
		recurTabStack->raiseWidget(recurrenceEdit);
	else
		recurTabStack->raiseWidget(recurDisabled);
	recurrenceEdit->setEnabled(on);
}

/******************************************************************************
*  Called when the browse button is pressed to select a file to display.
*/
void EditAlarmDlg::slotBrowseFile()
{
	if (fileDefaultDir.isEmpty())
		fileDefaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(fileDefaultDir, QString::null, this, i18n("Choose Text File to Display"));
	if (!url.isEmpty())
	{
		alarmMessage = url.prettyURL();
		fileMessageEdit->setText(alarmMessage);
		fileDefaultDir = url.path();
	}
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
	QString addrs = emailToEdit->text().stripWhiteSpace();
	if (!addrs.isEmpty())
		addrs += ", ";
	addrs += person.fullName();
	emailToEdit->setText(addrs);
}

/******************************************************************************
 * Select a file to attach to the email.
 */
void EditAlarmDlg::slotAddAttachment()
{
	if (attachDefaultDir.isEmpty())
		attachDefaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(attachDefaultDir, QString::null, this, i18n("Choose File to Attach"));
	if (!url.isEmpty())
	{
		emailAttachList->insertItem(url.prettyURL());
		emailAttachList->setCurrentItem(emailAttachList->count() - 1);   // select the new item
		attachDefaultDir = url.path();
		emailRemoveButton->setEnabled(true);
		emailAttachList->setEnabled(true);
	}
}

/******************************************************************************
 * Remove the currently selected attachment from the email.
 */
void EditAlarmDlg::slotRemoveAttachment()
{
	int item = emailAttachList->currentItem();
	emailAttachList->removeItem(item);
	int count = emailAttachList->count();
	if (item >= count)
		emailAttachList->setCurrentItem(count - 1);
	if (!count)
	{
		emailRemoveButton->setEnabled(false);
		emailAttachList->setEnabled(false);
	}
}

/******************************************************************************
*  Clean up the alarm text, and if it's a file, check whether it's valid.
*/
bool EditAlarmDlg::checkText(QString& result)
{
	if (messageRadio->isOn())
		result = textMessageEdit->text();
	else if (emailRadio->isOn())
		result = emailMessageEdit->text();
	else if (commandRadio->isOn())
	{
		result = commandMessageEdit->text();
		result.stripWhiteSpace();
	}
	else if (fileRadio->isOn())
	{
		QString alarmtext = fileMessageEdit->text();
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
			fileMessageEdit->setFocus();
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
			if (KMessageBox::warningContinueCancel(this, errmsg.arg(alarmtext), QString::null, KStdGuiItem::cont().text())    // explicit button text is for KDE2 compatibility
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
