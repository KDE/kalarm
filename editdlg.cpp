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
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qmultilinedit.h>
#include <qhbox.h>
#include <qgroupbox.h>
#include <qwidgetstack.h>
#include <qlabel.h>
#include <qmsgbox.h>
#include <qvalidator.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qdir.h>

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
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "recurrenceedit.h"
#include "colourcombo.h"
#include "deferdlg.h"
#include "buttongroup.h"
#include "editdlg.moc"


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event)
	: KDialogBase(KDialogBase::Tabbed, caption, Ok|Cancel|Try, Ok, parent, name),
	  deferGroup(0L),
	  shown(false)
{
	mainPage  = addPage(i18n("&Alarm"));

	// Recurrence tab
	recurPage = addPage(i18n("&Recurrence"));
	recurPageIndex = pageIndex(recurPage);
	QBoxLayout* layout = new QVBoxLayout(recurPage);
	recurTabStack = new QWidgetStack(recurPage);
	layout->addWidget(recurTabStack);

	recurFrame = new QFrame(recurTabStack);
	recurTabStack->addWidget(recurFrame, 0);
	recurrenceEdit = new RecurrenceEdit(recurFrame, "recurPage");
	connect(recurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));

	recurDisabled = new QLabel(i18n("The alarm does not recur.\nEnable recurrence in the Alarm tab."), recurTabStack);
	recurDisabled->setAlignment(Qt::AlignCenter);
	recurTabStack->addWidget(recurDisabled, 1);

	QVBoxLayout* topLayout = new QVBoxLayout(mainPage, marginKDE2, spacingHint());

	// Alarm action

	QButtonGroup* actionGroup = new QButtonGroup(i18n("Action"), mainPage, "actionGroup");
	connect(actionGroup, SIGNAL(clicked(int)), SLOT(slotAlarmTypeClicked(int)));
	topLayout->addWidget(actionGroup, 1);
	QGridLayout* grid = new QGridLayout(actionGroup, 3, 5, marginKDE2 + marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	// Message radio button
	messageRadio = new QRadioButton(i18n("Te&xt"), actionGroup, "messageButton");
	messageRadio->setFixedSize(messageRadio->sizeHint());
	QWhatsThis::add(messageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(messageRadio, 1, 0);
	grid->setColStretch(1, 1);

	// File radio button
	fileRadio = new QRadioButton(i18n("&File"), actionGroup, "fileButton");
	fileRadio->setFixedSize(fileRadio->sizeHint());
	QWhatsThis::add(fileRadio,
	      i18n("If checked, the alarm will display the contents of a text file."));
	grid->addWidget(fileRadio, 1, 2);
	grid->setColStretch(3, 1);

	// Command radio button
	commandRadio = new QRadioButton(i18n("Co&mmand"), actionGroup, "cmdButton");
	commandRadio->setFixedSize(commandRadio->sizeHint());
	QWhatsThis::add(commandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(commandRadio, 1, 4);

#ifdef KALARM_EMAIL
	// Email radio button
	emailRadio = new QRadioButton(i18n("&Email"), actionGroup, "emailButton");
	emailRadio->setFixedSize(emailRadio->sizeHint());
	QWhatsThis::add(emailRadio,
	      i18n("If checked, the alarm will send an email."));
	grid->addWidget(emailRadio, 1, 6);
#endif

	initDisplayAlarms(actionGroup);
	initCommand(actionGroup);
	alarmTypeStack = new QWidgetStack(actionGroup);
	grid->addMultiCellWidget(alarmTypeStack, 2, 2, 0, 6);
	grid->setRowStretch(2, 1);
	alarmTypeStack->addWidget(displayAlarmsFrame, 0);
	alarmTypeStack->addWidget(commandFrame, 1);

	if (event  &&  event->recurs() != KAlarmEvent::NO_RECUR  &&  event->deferred())
	{
		// Recurring event's deferred date/time
		deferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), mainPage, "deferGroup");
		topLayout->addWidget(deferGroup);
		QLabel* label = new QLabel(i18n("Deferred to:"), deferGroup);
		label->setFixedSize(label->sizeHint());

		deferDateTime = event->deferDateTime();
		deferTimeLabel = new QLabel(KGlobal::locale()->formatDateTime(deferDateTime), deferGroup);

		QPushButton* button = new QPushButton(i18n("C&hange..."), deferGroup);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(slotEditDeferral()));
		QWhatsThis::add(button, i18n("Change the alarm's deferred time, or cancel the deferral"));
		deferGroup->addSpace(0);
	}

	grid = new QGridLayout(topLayout, 3, 2, spacingHint());

	// Date and time entry

	timeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME | AlarmTimeWidget::NARROW,
	                                 mainPage, "timeGroup");
	grid->addMultiCellWidget(timeWidget, 0, 2, 0, 0);

	// Repetition type radio buttons

	ButtonGroup* repeatGroup = new ButtonGroup(1, Qt::Horizontal, i18n("Repetition"), mainPage);
	connect(repeatGroup, SIGNAL(buttonSet(int)), SLOT(slotRepeatClicked(int)));
	grid->addWidget(repeatGroup, 0, 1);
	grid->setRowStretch(1, 1);

	noRepeatRadio = new QRadioButton(i18n("&No repetition"), repeatGroup);
	noRepeatRadio->setFixedSize(noRepeatRadio->sizeHint());
	QWhatsThis::add(noRepeatRadio,
	      i18n("Trigger the alarm once only"));

	repeatAtLoginRadio = new QRadioButton(i18n("Repeat at lo&gin"), repeatGroup, "repeatAtLoginButton");
	repeatAtLoginRadio->setFixedSize(repeatAtLoginRadio->sizeHint());
	QWhatsThis::add(repeatAtLoginRadio,
	      i18n("Repeat the alarm at every login until the specified time.\n"
	           "Note that it will also be repeated any time the alarm daemon is restarted."));

	recurRadio = new QRadioButton(i18n("Rec&ur"), repeatGroup);
	recurRadio->setFixedSize(recurRadio->sizeHint());
	QWhatsThis::add(recurRadio, i18n("Regularly repeat the alarm"));

#if KDE_VERSION < 290
	repeatGroup->addWidget(noRepeatRadio);
	repeatGroup->addWidget(repeatAtLoginRadio);
	repeatGroup->addWidget(recurRadio);
#endif

	// Late display checkbox - default = allow late display
	lateCancel = new QCheckBox(i18n("Cancel &if late"), mainPage);
	lateCancel->setFixedSize(lateCancel->sizeHint());
	QWhatsThis::add(lateCancel,
	      i18n("If checked, the alarm will be canceled if it cannot be triggered within 1 "
	           "minute of the specified time. Possible reasons for not triggering include your "
	           "being logged off, X not running, or the alarm daemon not running.\n\n"
	           "If unchecked, the alarm will be triggered at the first opportunity after "
	           "the specified time, regardless of how late it is."));
	grid->addWidget(lateCancel, 1, 1);
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
		switch (event->type())
		{
			case KAlarmAlarm::FILE:
				radio = fileRadio;
				fileMessageEdit->setText(event->cleanText());
				break;
			case KAlarmAlarm::COMMAND:
				radio = commandRadio;
				commandMessageEdit->setText(event->cleanText());
				break;
			case KAlarmAlarm::MESSAGE:
			default:
				radio = messageRadio;
				textMessageEdit->setText(event->cleanText());
				break;
		}
		actionGroup->setButton(actionGroup->id(radio));

		if (event->recurs() != KAlarmEvent::NO_RECUR)
		{
			radio = recurRadio;
			recurrenceEdit->set(*event);
		}
		else if (event->repeatAtLogin())
			radio = repeatAtLoginRadio;
		else
			radio = noRepeatRadio;
		repeatGroup->setButton(repeatGroup->id(radio));

		lateCancel->setChecked(event->lateCancel());
		confirmAck->setChecked(event->confirmAck());
		recurrenceEdit->set(*event);   // must be called after timeWidget is set up, to ensure correct date-only enabling
		soundFile = event->audioFile();
		sound->setChecked(event->beep() || !soundFile.isEmpty());
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
		sound->setChecked(settings->defaultBeep());
	}

	size = basicSize;
	size.setHeight(size.height() + deferGroupHeight);
	resize(size);

	slotAlarmTypeClicked(-1);    // enable/disable things appropriately
	slotSoundToggled(sound->isChecked());
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
	QWhatsThis::add(textMessageEdit, i18n("Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(textMessageEdit);

	// File name edit box
	fileBox = new QHBox(displayAlarmsFrame);
	frameLayout->addWidget(fileBox);
	fileMessageEdit = new QLineEdit(fileBox);
	QWhatsThis::add(fileMessageEdit, i18n("Enter the name of a text file, or a URL, to display."));

	// File browse button
	fileBrowseButton = new QPushButton(fileBox);
	fileBrowseButton->setPixmap(SmallIcon("fileopen"));
	fileBrowseButton->setFixedSize(fileBrowseButton->sizeHint());
	connect(fileBrowseButton, SIGNAL(clicked()), SLOT(slotBrowseFile()));
	QWhatsThis::add(fileBrowseButton, i18n("Select a text file to display."));

	// Sound checkbox
	QBoxLayout* layout = new QHBoxLayout(frameLayout);
	QFrame* frame = new QFrame(displayAlarmsFrame);
	frame->setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* soundLayout = new QHBoxLayout(frame, 0, spacingHint());
	sound = new QCheckBox(i18n("&Sound"), frame);
	sound->setFixedSize(sound->sizeHint());
	connect(sound, SIGNAL(toggled(bool)), SLOT(slotSoundToggled(bool)));
	QWhatsThis::add(sound,
	      i18n("If checked, a sound will be played when the message is displayed. Click the "
	           "button on the right to select the sound."));
	soundLayout->addWidget(sound);

	// Sound picker button
	soundPicker = new QPushButton(frame);
	soundPicker->setPixmap(SmallIcon("playsound"));
	soundPicker->setFixedSize(soundPicker->sizeHint());
	soundPicker->setToggleButton(true);
	connect(soundPicker, SIGNAL(clicked()), SLOT(slotPickSound()));
	QWhatsThis::add(soundPicker,
	      i18n("Select a sound file to play when the message is displayed. If no sound file is "
	           "selected, a beep will sound."));
	soundLayout->addWidget(soundPicker);
	soundLayout->addStretch();
	layout->addWidget(frame);
	layout->addStretch();

#ifdef SELECT_FONT
	// Font and colour choice drop-down list
	fontButton = new QPushButton(i18n("Font && Co&lor..."), displayAlarmsFrame);
	fontColour = new FontColourChooser(displayAlarmsFrame, 0L, false, QStringList(), true, i18n("Font and background color"), false);
	size = fontColour->sizeHint();
	fontColour->setMinimumHeight(size.height() + 4);
	QWhatsThis::add(fontColour,
	      i18n("Choose the font and background color for the alarm message."));
	layout->addWidget(fontColour);
#endif
	// Colour choice drop-down list
	bgColourChoose = new ColourCombo(displayAlarmsFrame);
	QSize size = bgColourChoose->sizeHint();
	bgColourChoose->setMinimumHeight(size.height() + 4);
	QToolTip::add(bgColourChoose, i18n("Message color"));
	QWhatsThis::add(bgColourChoose,
	      i18n("Choose the background color for the alarm message."));
	layout->addWidget(bgColourChoose);

	// Acknowledgement confirmation required - default = no confirmation
	confirmAck = new QCheckBox(i18n("Confirm ac&knowledgement"), displayAlarmsFrame);
	confirmAck->setFixedSize(confirmAck->sizeHint());
	QWhatsThis::add(confirmAck,
	      i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
	frameLayout->addWidget(confirmAck);

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

	commandMessageEdit = new QLineEdit(commandFrame);
	QWhatsThis::add(commandMessageEdit, i18n("Enter a shell command to execute."));
	layout->addWidget(commandMessageEdit);
	layout->addStretch();
}

#ifdef KALARM_EMAIL
/******************************************************************************
 * Set up the email alarm dialog controls.
 */
void EditAlarmDlg::initEmail(QWidget* parent)
{
	emailFrame = new QFrame(parent);
	emailFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* layout = new QVBoxLayout(emailFrame, 0, spacingHint());
	QGridLayout* grid = new QGridLayout(layout, 2, 2, spacingHint());

	// Email recipients
	QLabel* label = new QLabel(i18n("To:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	emailToEdit = new QLineEdit(emailFrame);
	emailToEdit->setMinimumSize(emailToEdit->sizeHint());
	QWhatsThis::add(emailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	grid->addWidget(emailToEdit, 0, 1);

	// Email subject
	label = new QLabel(i18n("Sub&ject:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	emailSubjectEdit = new QLineEdit(emailFrame);
	emailSubjectEdit->setMinimumSize(emailSubjectEdit->sizeHint());
	label->setBuddy(emailSubjectEdit);
	QWhatsThis::add(emailSubjectEdit, i18n("Enter the email subject."));
	grid->addWidget(emailSubjectEdit, 1, 1);

	// Email body
	emailMessageEdit = new QMultiLineEdit(emailFrame);
	QSize size = emailMessageEdit->sizeHint();
	size.setHeight(emailMessageEdit->fontMetrics().lineSpacing()*13/4 + 2*emailMessageEdit->frameWidth());
	emailMessageEdit->setMinimumSize(size);
	QWhatsThis::add(emailMessageEdit, i18n("Enter the email message."));
	layout->addWidget(emailMessageEdit);

	// Email attachments
	QBoxLayout* attLayout = new QHBoxLayout(layout);
	label = new QLabel(i18n("Attachment&s:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	attLayout->addWidget(label);
	attLayout->addStretch();

	emailAttachList = new QComboBox(emailFrame);
	emailAttachList->setMinimumSize(emailAttachList->sizeHint());
	label->setBuddy(emailAttachList);
	QWhatsThis::add(emailAttachList,
	      i18n("Files to send as attachments to the email."));
	attLayout->addWidget(emailAttachList);

	QPushButton* button = new QPushButton(i18n("Add..."), emailFrame);
	connect(button, SIGNAL(clicked()), SLOT(slotAddAttachment()));
	QWhatsThis::add(button, i18n("Add an attachment to the email."));
	attLayout->addWidget(button);

	emailRemoveButton = new QPushButton(i18n("Remo&ve"), emailFrame);
	connect(emailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
	QWhatsThis::add(emailRemoveButton, i18n("Remove the highlighted attachment from the email."));
	attLayout->addWidget(emailRemoveButton);

	// BCC email to sender
	emailBcc = new QCheckBox(i18n("Co&py email to self"), emailFrame);
	emailBcc->setFixedSize(emailBcc->sizeHint());
	QWhatsThis::add(emailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	layout->addWidget(emailBcc);
}
#endif


/******************************************************************************
 * Get the currently entered message data.
 * The data is returned in the supplied Event instance.
 */
void EditAlarmDlg::getEvent(KAlarmEvent& event)
{
	event.set(alarmDateTime, alarmMessage, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
	event.setAudioFile(soundFile);
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
	return (displayAlarm && sound->isChecked() && soundFile.isEmpty() ? KAlarmEvent::BEEP : 0)
	     | (lateCancel->isChecked()                                   ? KAlarmEvent::LATE_CANCEL : 0)
	     | (displayAlarm && confirmAck->isChecked()                   ? KAlarmEvent::CONFIRM_ACK : 0)
	     | (repeatAtLoginRadio->isChecked()                           ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (alarmAnyTime                                              ? KAlarmEvent::ANY_TIME : 0);
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAlarmAlarm::Type EditAlarmDlg::getAlarmType() const
{
	return fileRadio->isOn()    ? KAlarmAlarm::FILE
	     : commandRadio->isOn() ? KAlarmAlarm::COMMAND
	     :                        KAlarmAlarm::MESSAGE;
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
*  Called when the window is about to be displayed.
*  The first time, it is moved up if necessary so that if the recurrence edit
*  widget later enlarges, it will all be above the bottom of the screen.
*/
void EditAlarmDlg::showEvent(QShowEvent* se)
{
/*	if (!shown  &&  recurrenceEdit  &&  recurrenceEdit->isSmallSize())
	{
		// We don't know the window's frame size yet, since it hasn't been drawn.
		// So use the parent's frame thickness as a guide.
		QRect workArea = KWinModule().workArea();
		int highest = workArea.top();
		int frameHeight = parentWidget()->frameSize().height() - parentWidget()->size().height();
		int top = mapToGlobal(QPoint(0, 0)).y();
		int bottom = top + height() + recurrenceEdit->heightVariation() + frameHeight;
		int shift = bottom - workArea.bottom();
		if (shift > 0  &&  top > highest)
		{
			// Move the window upwards if possible
			if (shift > top - highest)
				shift = top - highest;
			QRect rect = geometry();
			rect.setTop(rect.top() - shift);
			rect.setBottom(rect.bottom() - shift);
			setGeometry(rect);
		}
	}*/
//?????what about deferred time height?
	shown = true;
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
	if (timeWidget->getDateTime(alarmDateTime, alarmAnyTime))
	{
		bool noTime;
		if (!recurrenceEdit->checkData(alarmDateTime, noTime))
		{
			showPage(recurPageIndex);
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
#ifdef KALARM_EMAIL
		if (emailRadio->isOn())
		{
			if (!checkEmailAddresses()
			||  KMessageBox::warningContinueCancel(this, i18n("Do you really want to send the email now to the specified recipient(s)?"),
			                                       i18n("Confirm Email"), i18n("&Send")) != KMessageBox::Continue)
				return;
		}
#endif
		KAlarmEvent event;
		event.set(QDateTime(), text, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
		event.setAudioFile(soundFile);
		if (theApp()->execAlarm(event, event.firstAlarm(), false, false))
		{
			if (commandRadio->isOn())
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
#ifdef KALARM_EMAIL
			else if (emailRadio->isOn())
				KMessageBox::information(this, i18n("Email sent to:\n%1").arg(emailAddresses.join("\n")));
#endif
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
	if (timeWidget->getDateTime(start, anyTime))
	{
		bool deferred = deferDateTime.isValid();
		DeferAlarmDlg* deferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), (deferred ? deferDateTime : QDateTime::currentDateTime().addSecs(60)),
		                                            start, deferred, this, "deferDlg");
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
	timeWidget->enableAnyTime(!recurs || recurrenceEdit->repeatType() != RecurrenceEdit::SUBDAILY);
	if (deferGroup)
		deferGroup->setEnabled(recurs);
}

#ifdef KALARM_EMAIL
/******************************************************************************
*  Convert the email addresses to a list, and validate them.
*/
bool EditAlarmDlg::checkEmailAddresses()
{
	if (emailRadio->isOn())
	{
		QString addrs = emailToEdit->text();
		if (addrs.isEmpty())
			emailAddresses.clear();
		else
		{
			QString bad = KAMail::convertEmailAddresses(addrs, emailAddresses);
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
	}
	return true;
}
#endif

/******************************************************************************
*  Called when one of the message type radio buttons is clicked.
*/
void EditAlarmDlg::slotAlarmTypeClicked(int id)
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
		fileMessageEdit->setFocus();
	}
	else if (commandRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Execute the specified command now"));
		alarmTypeStack->raiseWidget(commandFrame);
		commandMessageEdit->setFocus();
	}
#ifdef KALARM_EMAIL
	else if (emailRadio->isOn())
	{
		setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
		alarmTypeStack->raiseWidget(emailFrame);
		emailToEdit->setFocus();
	}
#endif
}

/******************************************************************************
*  Called when one of the repetition radio buttons is clicked.
*/
void EditAlarmDlg::slotRepeatClicked(int)
{
	bool on = recurRadio->isOn();
	if (on)
		recurTabStack->raiseWidget(recurFrame);
	else
		recurTabStack->raiseWidget(recurDisabled);
	recurFrame->setEnabled(on);
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
 * Called when the sound checkbox is toggled.
 */
void EditAlarmDlg::slotSoundToggled(bool on)
{
	soundPicker->setEnabled(on);
	setSoundPicker();
}

/******************************************************************************
 * Called when the sound picker button is clicked.
 */
void EditAlarmDlg::slotPickSound()
{
	if (soundPicker->isOn())
	{
		if (soundDefaultDir.isEmpty())
			soundDefaultDir = KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
		KURL url = KFileDialog::getOpenURL(soundDefaultDir, i18n("*.wav|Wav Files"), 0, i18n("Choose a Sound File"));
		if (!url.isEmpty())
		{
			soundFile = url.prettyURL();
			soundDefaultDir = url.path();
			setSoundPicker();
		}
		else if (soundFile.isEmpty())
			soundPicker->setOn(false);
	}
	else
	{
		soundFile = "";
		setSoundPicker();
	}
}

/******************************************************************************
 * Set the sound picker button according to whether a sound file is selected.
 */
void EditAlarmDlg::setSoundPicker()
{
	QToolTip::remove(soundPicker);
	if (soundPicker->isEnabled())
	{
		bool beep = soundFile.isEmpty();
		if (beep)
			QToolTip::add(soundPicker, i18n("Beep"));
		else
			QToolTip::add(soundPicker, i18n("Play '%1'").arg(soundFile));
		soundPicker->setOn(!beep);
	}
}

#ifdef KALARM_EMAIL
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
		attachDefaultDir = url.path();
		emailRemoveButton->setEnabled(true);
	}
}
#endif

/******************************************************************************
*  Clean up the alarm text, and if it's a file, check whether it's valid.
*/
bool EditAlarmDlg::checkText(QString& result)
{
	if (messageRadio->isOn())
		result = textMessageEdit->text();
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
			if (KMessageBox::warningContinueCancel(this, errmsg.arg(alarmtext)) == KMessageBox::Cancel)
				return false;
		}
		result = alarmtext;
	}
	return true;
}
