/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm message
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
#include <qdir.h>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "editdlg.moc"


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event)
	: KDialogBase(parent, name, true, caption, Ok|Cancel|Try, Ok, true)
{
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* topLayout = new QVBoxLayout(page, marginHint(), spacingHint());

	// Message label + multi-line editor

	messageTypeGroup = new QButtonGroup(i18n("Message"), page, "messageGroup");
	connect(messageTypeGroup, SIGNAL(clicked(int)), this, SLOT(slotMessageTypeClicked(int)));
	topLayout->addWidget(messageTypeGroup);
	QGridLayout* grid = new QGridLayout(messageTypeGroup, 3, 4, 2*KDialog::marginHint(), KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	// Message radio button has an ID of 0
	messageRadio = new QRadioButton(i18n("Text"), messageTypeGroup, "messageButton");
	messageRadio->setFixedSize(messageRadio->sizeHint());
	QWhatsThis::add(messageRadio,
	      i18n("The edit field below contains the alarm message text."));
	grid->addWidget(messageRadio, 1, 0, AlignLeft);
	grid->setColStretch(0, 1);

	// Command radio button has an ID of 1
	commandRadio = new QRadioButton(i18n("Command"), messageTypeGroup, "cmdButton");
	commandRadio->setFixedSize(commandRadio->sizeHint());
	QWhatsThis::add(commandRadio,
	      i18n("The edit field below contains a shell command to execute."));
	grid->addWidget(commandRadio, 1, 1, AlignLeft);
	grid->setColStretch(1, 1);

	// File radio button has an ID of 2
	fileRadio = new QRadioButton(i18n("File"), messageTypeGroup, "fileButton");
	fileRadio->setFixedSize(fileRadio->sizeHint());
	QWhatsThis::add(fileRadio,
	      i18n("The edit field below contains the name of a text file whose contents will be "
	           "displayed as the alarm message text."));
	grid->addWidget(fileRadio, 1, 2, AlignRight);

	// Browse button
	browseButton = new QPushButton(i18n("&Browse..."), messageTypeGroup);
	browseButton->setFixedSize(browseButton->sizeHint());
	QWhatsThis::add(browseButton,
	      i18n("Select a text file to display."));
	grid->addWidget(browseButton, 1, 3, AlignLeft);

	messageEdit = new QMultiLineEdit(messageTypeGroup);
	QSize size = messageEdit->sizeHint();
	size.setHeight(messageEdit->fontMetrics().lineSpacing()*13/4 + 2*messageEdit->frameWidth());
	messageEdit->setMinimumSize(size);
	messageEdit->setWrapPolicy(QMultiLineEdit::Anywhere);
	connect(messageEdit, SIGNAL(textChanged()), this, SLOT(slotMessageTextChanged()));
	grid->addMultiCellWidget(messageEdit, 2, 2, 0, 3);

	// Date and time entry
	timeWidget = new AlarmTimeWidget(i18n("Time"), 0, page, "timeGroup");
	topLayout->addWidget(timeWidget);

	// Repeating alarm

	QGroupBox* group = new QGroupBox(i18n("Repetition"), page, "repetitionGroup");
	topLayout->addWidget(group);
	grid = new QGridLayout(group, 3, 4, 2*KDialog::marginHint(), KDialog::spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);

	QLabel* lbl = new QLabel(i18n("Count:"), group);
	lbl->setFixedSize(lbl->sizeHint());
	grid->addWidget(lbl, 1, 0, AlignLeft);

	repeatCount = new QSpinBox(0, 9999, 1, group);
	repeatCount->setFixedSize(repeatCount->sizeHint());
	QWhatsThis::add(repeatCount,
	      i18n("Enter the number of times to repeat the alarm, after its initial display."));
	connect(repeatCount, SIGNAL(valueChanged(int)), this, SLOT(slotRepeatCountChanged(int)));
	grid->addWidget(repeatCount, 1, 1, AlignLeft);

	lbl = new QLabel(i18n("Interval:"), group);
	lbl->setFixedSize(lbl->sizeHint());
	grid->setColStretch(2, 1);
	grid->addWidget(lbl, 1, 2, AlignRight);

	repeatInterval = new TimeSpinBox(1, 99*60+59, group);
	repeatInterval->setValue(2399);
	size = repeatInterval->sizeHint();
	repeatInterval->setFixedSize(size);
	QWhatsThis::add(repeatInterval,
	      i18n("Enter the time (in hours and minutes) between repetitions of the alarm."));
	grid->addWidget(repeatInterval, 1, 3, AlignRight);

	// Repeat-at-login radio button has an ID of 1
	repeatAtLogin = new QCheckBox(i18n("Repeat at login"), group, "repeatAtLoginButton");
	repeatAtLogin->setFixedSize(repeatAtLogin->sizeHint());
	QWhatsThis::add(repeatAtLogin,
	      i18n("Repeat the alarm at every login until the specified time.\n"
	           "Note that it will also be repeated any time the alarm daemon is restarted."));
	grid->addWidget(repeatAtLogin, 2, 0, AlignLeft);

	// Late display checkbox - default = allow late display

	grid = new QGridLayout(1, 3);
	topLayout->addLayout(grid);

	lateCancel = new QCheckBox(page);
	lateCancel->setText(i18n("Cancel if late"));
	lateCancel->setFixedSize(lateCancel->sizeHint());
	lateCancel->setChecked(false);
	grid->addWidget(lateCancel, 0, 0, AlignLeft);
	QWhatsThis::add(lateCancel,
	      i18n("If checked, the message will be cancelled if it cannot be displayed within 1 "
	           "minute of the specified time. Possible reasons for non-display include your "
	           "being logged off, X not running, or the alarm daemon not running.\n\n"
	           "If unchecked, the message will be displayed at the first opportunity after "
	           "the specified time, regardless of how late it is."));

	// Beep checkbox - default = no beep

	beep = new QCheckBox(page);
	beep->setText(i18n("Beep"));
	beep->setFixedSize(beep->sizeHint());
	beep->setChecked(false);
	grid->addWidget(beep, 0, 1, AlignLeft);
	QWhatsThis::add(beep,
	      i18n("If checked, a beep will sound when the message is displayed."));

#ifdef SELECT_FONT
	// Font and colour choice drop-down list

	fontColour = new FontColourChooser(page, 0L, false, QStringList(), true, i18n("Font and background color"), false);
	size = fontColour->sizeHint();
	fontColour->setMinimumHeight(size.height() + 4);
	topLayout->addWidget(fontColour, 6);
	QWhatsThis::add(fontColour,
	      i18n("Choose the font and background color for the alarm message."));
#else
	// Colour choice drop-down list

	bgColourChoose = new ColourCombo(page);
	size = bgColourChoose->sizeHint();
	bgColourChoose->setMinimumHeight(size.height() + 4);
	grid->addWidget(bgColourChoose, 0, 2, AlignRight);
	QWhatsThis::add(bgColourChoose,
	      i18n("Choose the background color for the alarm message."));
#endif

	setButtonWhatsThis(Ok, i18n("Schedule the message for display at the specified time."));

	topLayout->activate();

	size = theApp()->readConfigWindowSize("EditDialog", minimumSize());
	resize(size);

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
		timeWidget->setDateTime(event->dateTime());
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
			case KAlarmAlarm::MESSAGE:
			default:
				radio = messageRadio;
				break;
		}
		messageEdit->setText(event->cleanText());
		messageTypeGroup->setButton(messageTypeGroup->id(radio));
		lateCancel->setChecked(event->lateCancel());
		beep->setChecked(event->beep());
		repeatCount->setValue(event->repeatCount());
		repeatInterval->setValue(event->repeatMinutes());
		repeatAtLogin->setChecked(event->repeatAtLogin());
	}
	else
	{
		// Set the values to their defaults
#ifdef SELECT_FONT
		fontColour->setColour(theApp()->settings()->defaultBgColour());
		fontColour->setFont(theApp()->settings()->messageFont());
#else
		bgColourChoose->setColour(theApp()->settings()->defaultBgColour());     // set colour before setting alarm type buttons
#endif
		timeWidget->setDateTime(QDateTime::currentDateTime().addSecs(60));
		messageEdit->setText(QString::null);
		messageTypeGroup->setButton(messageTypeGroup->id(messageRadio));
		repeatCount->setValue(0);
		repeatInterval->setValue(0);
		repeatAtLogin->setChecked(false);
	}

	slotMessageTypeClicked(-1);    // enable/disable things appropriately
	repeatInterval->setEnabled(repeatCount->value());
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
	event.set(alarmDateTime, alarmMessage, bgColourChoose->color(), getAlarmType(), getAlarmFlags(),
	          repeatCount->value(), repeatInterval->value());
}

/******************************************************************************
 * Get the currently specified alarm flag bits.
 */
int EditAlarmDlg::getAlarmFlags() const
{
	return (beep->isChecked()          ? KAlarmEvent::BEEP : 0)
	     | (lateCancel->isChecked()    ? KAlarmEvent::LATE_CANCEL : 0)
	     | (repeatAtLogin->isChecked() ? KAlarmEvent::REPEAT_AT_LOGIN : 0);
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
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right
*  hand edge of the list view.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
	theApp()->writeConfigWindowSize("EditDialog", re->size());
	KDialog::resizeEvent(re);
}


/******************************************************************************
*  Called when the OK button is clicked.
*  Set up the new alarm.
*/
void EditAlarmDlg::slotOk()
{
	if (timeWidget->getDateTime(alarmDateTime))
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
		KAlarmEvent event;
		event.set(QDateTime(), text, bgColourChoose->color(), getAlarmType(), getAlarmFlags());
		if (theApp()->execAlarm(event, event.firstAlarm(), false, false))
		{
			if (commandRadio->isOn())
				KMessageBox::information(this, i18n("Command executed:\n%1").arg(text));
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
			QFileInfo info(alarmtext);
			QDir::setCurrent(QDir::homeDirPath());
			alarmtext = info.absFilePath();
			url.setPath(alarmtext);
			alarmtext = QString::fromLatin1("file:") + alarmtext;
			if      (info.isDir())        err = DIRECTORY;
			else if (!info.exists())      err = NONEXISTENT;
			else if (!info.isReadable())  err = UNREADABLE;
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
	if (id == messageTypeGroup->id(browseButton))
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
	{
		// It's a multi-line edit mode
		QWhatsThis::add(messageEdit,
		      i18n("Enter the text of the alarm message. It may be multi-line."));
		setButtonWhatsThis(Try, i18n("Display the alarm message now"));
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
#ifndef SELECT_FONT
		bgColourChoose->setEnabled(true);
#endif
		beep->setEnabled(true);
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
			beep->setEnabled(true);
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
			beep->setEnabled(false);
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

/******************************************************************************
*  Called when the repeat count edit box value has changed.
*/
void EditAlarmDlg::slotRepeatCountChanged(int count)
{
	repeatInterval->setEnabled(count);
}
