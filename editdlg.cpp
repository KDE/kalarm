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
 */

#include "kalarm.h"

#include <limits.h>

#include <qlayout.h>
#include <qpopupmenu.h>
#include <qpushbutton.h>
#include <qmultilinedit.h>
#include <qbuttongroup.h>
#include <qgroupbox.h>
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
#include "deferdlg.h"
#include "editdlg.moc"


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event)
	: KDialogBase(parent, name, true, caption, Ok|Cancel|Try, Ok, true),
	  deferGroup(0),
	  shown(false)
{
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* topLayout = new QVBoxLayout(page, marginKDE2, spacingHint());

	// Message label + multi-line editor

	actionGroup = new QButtonGroup(i18n("Action"), page, "actionGroup");
	connect(actionGroup, SIGNAL(clicked(int)), this, SLOT(slotMessageTypeClicked(int)));
	topLayout->addWidget(actionGroup);
	QGridLayout* grid = new QGridLayout(actionGroup, 3, 4, marginKDE2 + marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	int gridRow = 1;

	// Message radio button
	messageRadio = new QRadioButton(i18n("Text"), actionGroup, "messageButton");
	messageRadio->setFixedSize(messageRadio->sizeHint());
	QWhatsThis::add(messageRadio,
	      i18n("If checked, the alarm will display a text message."));
	grid->addWidget(messageRadio, gridRow, 0, AlignLeft);
	grid->setColStretch(0, 1);

	// File radio button
	QBoxLayout* layout = new QHBoxLayout(grid, spacingHint());
	fileRadio = new QRadioButton(i18n("File"), actionGroup, "fileButton");
	fileRadio->setFixedSize(fileRadio->sizeHint());
	QWhatsThis::add(fileRadio,
	      i18n("If checked, the alarm will display the contents of a text file."));
	layout->addWidget(fileRadio);

	// Browse button
	browseButton = new QPushButton(actionGroup);
	browseButton->setPixmap(SmallIcon("fileopen"));
	browseButton->setFixedSize(browseButton->sizeHint());
	QWhatsThis::add(browseButton, i18n("Select a text file to display."));
	layout->addWidget(browseButton);
	grid->addLayout(layout, gridRow, 1);
	grid->setColStretch(1, 1);

	// Command radio button
	commandRadio = new QRadioButton(i18n("Command"), actionGroup, "cmdButton");
	commandRadio->setFixedSize(commandRadio->sizeHint());
	QWhatsThis::add(commandRadio,
	      i18n("If checked, the alarm will execute a shell command."));
	grid->addWidget(commandRadio, gridRow, 2, AlignRight);
	grid->setColStretch(2, 1);

#ifdef KALARM_EMAIL
	// Email radio button
	emailRadio = new QRadioButton(i18n("Email"), actionGroup, "emailButton");
	emailRadio->setFixedSize(emailRadio->sizeHint());
	QWhatsThis::add(emailRadio,
	      i18n("If checked, the alarm will send an email."));
	grid->addWidget(emailRadio, gridRow, 3, AlignRight);

	// Email recipients
	++gridRow;
	emailFrame = new QFrame(actionGroup);
	emailFrame->setFrameStyle(QFrame::NoFrame);
	QGridLayout* subGrid = new QGridLayout(emailFrame, 2, 2, 0, spacingHint());
	QLabel* label = new QLabel(i18n("To:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	subGrid->addWidget(label, 0, 0);

	emailToEdit = new QLineEdit(emailFrame);
	emailToEdit->setMinimumSize(emailToEdit->sizeHint());
	QWhatsThis::add(emailToEdit,
	      i18n("Enter the addresses of the email recipients. Separate multiple addresses by "
	           "commas or semicolons."));
	subGrid->addWidget(emailToEdit, 0, 1);

	// Email subject
	label = new QLabel(i18n("Subject:"), emailFrame);
	label->setFixedSize(label->sizeHint());
	subGrid->addWidget(label, 1, 0);

	emailSubjectEdit = new QLineEdit(emailFrame);
	emailSubjectEdit->setMinimumSize(emailSubjectEdit->sizeHint());
	QWhatsThis::add(emailSubjectEdit, i18n("Enter the email subject."));
	subGrid->addWidget(emailSubjectEdit, 1, 1);
	grid->addMultiCellWidget(emailFrame, gridRow, gridRow, 0, 3);
#endif

	++gridRow;
	messageEdit = new QMultiLineEdit(actionGroup);
	QSize size = messageEdit->sizeHint();
	size.setHeight(messageEdit->fontMetrics().lineSpacing()*13/4 + 2*messageEdit->frameWidth());
	messageEdit->setMinimumSize(size);
	connect(messageEdit, SIGNAL(textChanged()), this, SLOT(slotMessageTextChanged()));
	grid->addMultiCellWidget(messageEdit, gridRow, gridRow, 0, 3);

	if (event  &&  event->recurs() != KAlarmEvent::NO_RECUR  &&  event->deferred())
	{
		// Recurring event's deferred date/time
		deferGroup = new QGroupBox(1, Qt::Vertical, i18n("Deferred Alarm"), page, "deferGroup");
		topLayout->addWidget(deferGroup);
		QLabel* label = new QLabel(i18n("Deferred to:"), deferGroup);
		label->setFixedSize(label->sizeHint());

		deferDateTime = event->deferDateTime();
		deferTimeLabel = new QLabel(KGlobal::locale()->formatDateTime(deferDateTime), deferGroup);

		QPushButton* button = new QPushButton(i18n("&Change..."), deferGroup);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), this, SLOT(slotEditDeferral()));
		QWhatsThis::add(button, i18n("Change the alarm's deferred time, or cancel the deferral"));
		deferGroup->addSpace(0);
	}

	// Date and time entry

	timeWidget = new AlarmTimeWidget(i18n("Time"), AlarmTimeWidget::AT_TIME, 0, page, "timeGroup");
	topLayout->addWidget(timeWidget);

	// Repeating alarm

	recurrenceEdit = new RecurrenceEdit(i18n("Repetition"), page);
	recurrenceEdit->setMinimumSize(recurrenceEdit->sizeHint());
	connect(recurrenceEdit, SIGNAL(typeChanged(int)), this, SLOT(slotRepeatTypeChange(int)));
	connect(recurrenceEdit, SIGNAL(resized(QSize,QSize)), this, SLOT(slotRecurrenceResized(QSize,QSize)));
	topLayout->addWidget(recurrenceEdit);

//	QHBoxLayout* layout = new QHBoxLayout(topLayout);
	grid = new QGridLayout(topLayout, 2, 2, spacingHint());

	// Late display checkbox - default = allow late display

	lateCancel = new QCheckBox(i18n("Cancel if late"), page);
	lateCancel->setFixedSize(lateCancel->sizeHint());
	QWhatsThis::add(lateCancel,
	      i18n("If checked, the alarm will be canceled if it cannot be triggered within 1 "
	           "minute of the specified time. Possible reasons for not triggering include your "
	           "being logged off, X not running, or the alarm daemon not running.\n\n"
	           "If unchecked, the alarm will be triggered at the first opportunity after "
	           "the specified time, regardless of how late it is."));
	grid->addWidget(lateCancel, 0, 0, AlignLeft);
	grid->setColStretch(0, 1);

	// Sound checkbox & sound picker button - default = no sound

	QFrame* frame = new QFrame(page);
	frame->setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* slayout = new QHBoxLayout(frame, 0, spacingHint());
	sound = new QCheckBox(i18n("Sound"), frame);
	sound->setFixedSize(sound->sizeHint());
	connect(sound, SIGNAL(toggled(bool)), this, SLOT(slotSoundToggled(bool)));
	QWhatsThis::add(sound,
	      i18n("If checked, a sound will be played when the message is displayed. Click the "
	           "button on the right to select the sound."));
	slayout->addWidget(sound);

	soundPicker = new QPushButton(frame);
	soundPicker->setPixmap(SmallIcon("playsound"));
	soundPicker->setFixedSize(soundPicker->sizeHint());
	soundPicker->setToggleButton(true);
	connect(soundPicker, SIGNAL(clicked()), SLOT(slotPickSound()));
	QWhatsThis::add(soundPicker,
	      i18n("Select a sound file to play when the message is displayed. If no sound file is "
	           "selected, a beep will sound."));
	slayout->addWidget(soundPicker);
	grid->addWidget(frame, 0, 1, AlignRight);

	// Acknowledgement confirmation required - default = no confirmation

	confirmAck = new QCheckBox(i18n("Confirm acknowledgement"), page);
	confirmAck->setFixedSize(confirmAck->sizeHint());
	QWhatsThis::add(confirmAck,
	      i18n("Check to be prompted for confirmation when you acknowledge the alarm."));
	grid->addWidget(confirmAck, 1, 0, AlignLeft);

#ifdef SELECT_FONT
	// Font and colour choice drop-down list

	fontButton = new QPushButton(i18n("Font && Color..."), page);
	fontColour = new FontColourChooser(page, 0L, false, QStringList(), true, i18n("Font and background color"), false);
	size = fontColour->sizeHint();
	fontColour->setMinimumHeight(size.height() + 4);
	QWhatsThis::add(fontColour,
	      i18n("Choose the font and background color for the alarm message."));
	grid->addWidget(fontColour, 1, 1, AlignLeft);
#else
	// Colour choice drop-down list

	bgColourChoose = new ColourCombo(page);
	size = bgColourChoose->sizeHint();
	bgColourChoose->setMinimumHeight(size.height() + 4);
	QToolTip::add(bgColourChoose, i18n("Message color"));
	QWhatsThis::add(bgColourChoose,
	      i18n("Choose the background color for the alarm message."));
	grid->addWidget(bgColourChoose, 1, 1, AlignLeft);
#endif

#ifdef KALARM_EMAIL
	// BCC email to sender
	emailBcc = new QCheckBox(i18n("Copy email to self"), page);
	emailBcc->setFixedSize(emailBcc->sizeHint());
	QWhatsThis::add(emailBcc,
	      i18n("If checked, the email will be blind copied to you."));
	grid->addWidget(emailBcc, 2, 0, AlignLeft);
#endif

	setButtonWhatsThis(Ok, i18n("Schedule the alarm at the specified time."));

	topLayout->activate();

	deferGroupHeight = deferGroup ? deferGroup->height() + spacingHint() : 0;
	size = minimumSize();
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
#else
		bgColourChoose->setColour(event->colour());     // set colour before setting alarm type buttons
#endif
		timeWidget->setDateTime(event->mainDateTime(), event->anyTime());
		QRadioButton* radio;
		singleLineOnly = false;       // ensure the text isn't changed erroneously
		switch (event->type())
		{
			case KAlarmAlarm::FILE:
				radio = fileRadio;
				break;
			case KAlarmAlarm::COMMAND:
				radio = commandRadio;
				break;
#ifdef KALARM_EMAIL
			case KAlarmAlarm::EMAIL:
				radio = emailRadio;
				break;
#endif
			case KAlarmAlarm::MESSAGE:
			default:
				radio = messageRadio;
				break;
		}
		messageEdit->setText(event->cleanText());
		actionGroup->setButton(actionGroup->id(radio));
		lateCancel->setChecked(event->lateCancel());
		confirmAck->setChecked(event->confirmAck());
		recurrenceEdit->set(*event, event->repeatAtLogin());   // must be called after timeWidget is set up, to ensure correct date-only enabling
		soundFile = event->audioFile();
		sound->setChecked(event->beep() || !soundFile.isEmpty());
#ifdef KALARM_EMAIL
		emailToEdit->setText(event->emailAddressees());
		emailSubjectEdit->setText(event->emailSubject());
		emailBcc->setChecked(event->emailBcc());
#endif
	}
	else
	{
		// Set the values to their defaults
		Settings* settings = theApp()->settings();
#ifdef SELECT_FONT
		fontColour->setColour(settings->defaultBgColour());
		fontColour->setFont(settings->messageFont());
#else
		bgColourChoose->setColour(settings->defaultBgColour());     // set colour before setting alarm type buttons
#endif
		QDateTime defaultTime = QDateTime::currentDateTime().addSecs(60);
		timeWidget->setDateTime(defaultTime, false);
		singleLineOnly = false;
		messageEdit->setText(QString::null);
		actionGroup->setButton(actionGroup->id(messageRadio));
		lateCancel->setChecked(settings->defaultLateCancel());
		confirmAck->setChecked(settings->defaultConfirmAck());
		recurrenceEdit->setDefaults(defaultTime);   // must be called after timeWidget is set up, to ensure correct date-only enabling
		sound->setChecked(settings->defaultBeep());
#ifdef KALARM_EMAIL
		emailBcc->setChecked(settings->defaultEmailBcc());
#endif
	}

	size = basicSize;
	size.setHeight(size.height() + deferGroupHeight);
	if (recurrenceEdit  &&  !recurrenceEdit->isSmallSize())
		size.setHeight(size.height() + recurrenceEdit->heightVariation());
	resize(size);

	slotMessageTypeClicked(-1);    // enable/disable things appropriately
	messageEdit->setFocus();
}


EditAlarmDlg::~EditAlarmDlg()
{
}


/******************************************************************************
 * Get the currently entered message data.
 * The data is returned in the supplied Event instance.
 */
void EditAlarmDlg::getEvent(KAlarmEvent& event)
{
	event.set(alarmDateTime, alarmMessage, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
	event.setAudioFile(soundFile);
	recurrenceEdit->writeEvent(event);

	RecurrenceEdit::RepeatType type = recurrenceEdit->getRepeatType();
	if (type != RecurrenceEdit::NONE  &&  type != RecurrenceEdit::AT_LOGIN
	&&  deferDateTime.isValid()
	&&  deferDateTime < alarmDateTime)
		event.defer(deferDateTime);
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	return (sound->isChecked() && soundFile.isEmpty() ? KAlarmEvent::BEEP : 0)
	     | (lateCancel->isChecked()                   ? KAlarmEvent::LATE_CANCEL : 0)
	     | (confirmAck->isChecked()                   ? KAlarmEvent::CONFIRM_ACK : 0)
#ifdef KALARM_EMAIL
	     | (emailBcc->isChecked()                     ? KAlarmEvent::EMAIL_BCC : 0)
#endif
	     | (recurrenceEdit->repeatAtLogin()           ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (alarmAnyTime                              ? KAlarmEvent::ANY_TIME : 0);
}

/******************************************************************************
 * Get the currently selected alarm type.
 */
KAlarmAlarm::Type EditAlarmDlg::getAlarmType() const
{
	return fileRadio->isOn()    ? KAlarmAlarm::FILE
	     : commandRadio->isOn() ? KAlarmAlarm::COMMAND
#ifdef KALARM_EMAIL
	     : emailRadio->isOn()   ? KAlarmAlarm::EMAIL
#endif
	     :                        KAlarmAlarm::MESSAGE;
}


/******************************************************************************
*  Called when the window is about to be displayed.
*  The first time, it is moved up if necessary so that if the recurrence edit
*  widget later enlarges, it will all be above the bottom of the screen.
*/
void EditAlarmDlg::showEvent(QShowEvent* se)
{
	if (!shown  &&  recurrenceEdit  &&  recurrenceEdit->isSmallSize())
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
	}
	shown = true;
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size (adjusted to ignore the optional heights of the
*  deferred time and recurrence edit widgets) in the config file.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
	{
		basicSize = re->size();
		basicSize.setHeight(basicSize.height() - deferGroupHeight);
		if (recurrenceEdit  &&  !recurrenceEdit->isSmallSize())
			basicSize.setHeight(basicSize.height() - recurrenceEdit->heightVariation());
		theApp()->writeConfigWindowSize("EditDialog", basicSize);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
 * Called when the recurrence edit widget has been resized.
 */
void EditAlarmDlg::slotRecurrenceResized(QSize old, QSize New)
{
	if (recurrenceEdit  &&  old.height() != New.height())
	{
		int newheight = basicSize.height() + deferGroupHeight;
		if (New.height() > recurrenceEdit->noRecurHeight())
			newheight += recurrenceEdit->heightVariation();
		setMinimumHeight(newheight);
		resize(width(), newheight);
	}
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
		QString prefix = KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
		QString fileName(KFileDialog::getOpenFileName(prefix, i18n("*.wav|Wav Files"), 0));
		if (!fileName.isEmpty())
		{
			soundFile = fileName;
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

/******************************************************************************
*  Called when the OK button is clicked.
*  Set up the new alarm.
*/
void EditAlarmDlg::slotOk()
{
	if (timeWidget->getDateTime(alarmDateTime, alarmAnyTime)
	&&  recurrenceEdit->checkData(alarmDateTime))
	{
		if (checkText(alarmMessage))
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
			if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to send the email now to the specified recipient(s)?"),
			                                       i18n("Confirm Email"), i18n("Send")) != KMessageBox::Continue)
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
				KMessageBox::information(this, i18n("Email sent to:\n%1").arg(text));
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
*  Called when the repetition type selection changes.
*  Enables/disables date-only alarms as appropriate.
*/
void EditAlarmDlg::slotRepeatTypeChange(int repeatType)
{
	timeWidget->enableAnyTime(repeatType != RecurrenceEdit::SUBDAILY);
	if (deferGroup)
		deferGroup->setEnabled(repeatType != RecurrenceEdit::NONE && repeatType != RecurrenceEdit::AT_LOGIN);
}

/******************************************************************************
*  Clean up the alarm text, and if it's a file, check whether it's valid.
*/
bool EditAlarmDlg::checkText(QString& result)
{
	QString alarmtext = getMessageText();
	if (fileRadio->isOn())
	{
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
			messageEdit->setFocus();
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
			if (KMessageBox::warningContinueCancel(this, errmsg.arg(alarmtext), QString::null,
			                                       i18n("Continue")) == KMessageBox::Cancel)
				return false;
		}
	}
	else
		alarmtext.stripWhiteSpace();
	result = alarmtext;
	return true;
}

/******************************************************************************
*  Called when one of the message type radio buttons is clicked, or
*  the browse button is pressed to select a file to display.
*/
void EditAlarmDlg::slotMessageTypeClicked(int id)
{
	if (id == actionGroup->id(browseButton))
	{
		// Browse button has been clicked
		static QString defaultDir;
		if (defaultDir.isEmpty())
			defaultDir = QDir::homeDirPath();
		KURL url = KFileDialog::getOpenURL(defaultDir, QString::null, this, i18n("Choose Text File to Display"));
		if (!url.isEmpty())
		{
			alarmMessage = url.prettyURL();
			messageEdit->setText(alarmMessage);
			defaultDir = url.path();
		}
	}
	else if (messageRadio->isOn())
#ifdef KALARM_EMAIL
	else if (messageRadio->isOn()  ||  emailRadio->isOn())
#endif
	{
		// It's a multi-line edit mode
		if (messageRadio->isOn())
		{
			QWhatsThis::add(messageEdit,
			      i18n("Enter the text of the alarm message. It may be multi-line."));
			setButtonWhatsThis(Try, i18n("Display the alarm message now"));
#ifndef SELECT_FONT
			bgColourChoose->setEnabled(true);
#endif
			enableMessageControls(true);
		}
#ifdef KALARM_EMAIL
		else if (emailRadio->isOn())
		{
			QWhatsThis::add(messageEdit,
			      i18n("Enter the email message."));
			setButtonWhatsThis(Try, i18n("Send the email to the specified addressees now"));
			browseButton->setEnabled(false);
#ifndef SELECT_FONT
			bgColourChoose->setEnabled(false);
#endif
			enableEmailControls(true);
		}
#endif
		singleLineOnly = false;
		if (!multiLineText.isEmpty())
		{
			// The edit text has not changed since previously switching to a
			// single line edit mode, so restore the old text.
			messageEdit->setText(multiLineText);
			multiLineText = QString::null;
		}
		messageEdit->setWordWrap(QMultiLineEdit::NoWrap);
		browseButton->setEnabled(false);
	}
	else
	{
		// It's a single-line edit mode
		if (fileRadio->isOn())
		{
			QWhatsThis::add(messageEdit,
		         i18n("Enter the name of a text file, or a URL, to display."));
			setButtonWhatsThis(Try, i18n("Display the text file now"));
			browseButton->setEnabled(true);
#ifndef SELECT_FONT
			bgColourChoose->setEnabled(true);
#endif
			enableMessageControls(true);
		}
		else if (commandRadio->isOn())
		{
			QWhatsThis::add(messageEdit,
			      i18n("Enter a shell command to execute."));
			setButtonWhatsThis(Try, i18n("Execute the specified command now"));
			browseButton->setEnabled(false);
#ifndef SELECT_FONT
			bgColourChoose->setEnabled(false);
#endif
			enableMessageControls(false);
#ifdef KALARM_EMAIL
			enableEmailControls(false);
#endif
		}
		singleLineOnly = true;
		QString text = messageEdit->text();
		int newline = text.find('\n');
		if (newline >= 0)
		{
			// Existing text contains multiple lines. Save it so that it
			// can be restored if the user switches straight back to a
			// multi-line edit mode without touching the text.
			messageEdit->setText(text.left(newline));
			messageEdit->setWordWrap(QMultiLineEdit::WidgetWidth);
			multiLineText = text;
		}
		else
			messageEdit->setWordWrap(QMultiLineEdit::WidgetWidth);
	}
}

/******************************************************************************
 * Enable/disable the controls appropriate only to displayed alarms.
 * These are the 'Sound' checkbox and sound picker button, and the 'Confirm
 * acknowledgement' checkbox.
 */
void EditAlarmDlg::enableMessageControls(bool enable)
{
	confirmAck->setEnabled(enable);
	sound->setEnabled(enable);
	slotSoundToggled(enable && sound->isChecked());
#ifdef KALARM_EMAIL
	if (enable)
		enableEmailControls(false);
#endif
}

#ifdef KALARM_EMAIL
/******************************************************************************
 * Enable/disable the controls appropriate only to email alarms.
 * These are the Addressees and Subject edit fields, and the blind copy checkbox.
 */
void EditAlarmDlg::enableEmailControls(bool enable)
{
	emailBcc->setEnabled(enable);
	if (enable)
	{
		emailFrame->show();
		emailFrame->setFixedHeight(emailFrame->sizeHint());
		enableMessageControls(false);
	}
	else
	{
		emailFrame->hide();
		emailFrame->setFixedHeight(0);
	}
}
#endif

/******************************************************************************
*  Called when the text in the message edit field changes.
*  If multiple lines are not allowed, excess lines or newlines are removed.
*/
void EditAlarmDlg::slotMessageTextChanged()
{
	getMessageText();
	if (!multiLineText.isEmpty())
	{
		// Now that the edit text has been changed, scrap the saved
		// multi-line text from the previous message mode
		multiLineText = QString::null;
	}
}

/******************************************************************************
*  Removes excess lines or newlines if multiple lines are not allowed.
*/
QString EditAlarmDlg::getMessageText()
{
	if (singleLineOnly)
	{
		QString text = messageEdit->text();
		int newline = text.find('\n');
		if (newline >= 0)
			messageEdit->setText(text.remove(newline, 1));
		return text;
	}
	return messageEdit->text();
}
