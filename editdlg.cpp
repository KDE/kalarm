/*
 *  editdlg.cpp  -  dialogue to create or modify an alarm message
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
#include <kmessagebox.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "editdlg.h"
#include "editdlg.moc"


EditAlarmDlg::EditAlarmDlg(const QString& caption, QWidget* parent, const char* name,
	                        const KAlarmEvent* event)
	: KDialogBase(parent, name, true, caption, Ok|Cancel, Ok, true)
{
	QGroupBox* group;
	QVBoxLayout* layout;
	QGridLayout* grid;
	QLabel* lbl;

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* topLayout = new QVBoxLayout(page, marginHint(), spacingHint());

	// Message label + multi-line editor

	group = new QGroupBox(i18n("Message"), page, "messageGroup");
	topLayout->addWidget(group);
	layout = new QVBoxLayout(group, KDialog::spacingHint(), 0);
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	grid = new QGridLayout(group, 2, 4, KDialog::spacingHint());
	layout->addLayout(grid);
	// To have better control over the button layout, don't use a QButtonGroup
// QGridLayout* grid = new QGridLayout(1, 4);
// topLayout->addLayout(grid);

	// Message radio button has an ID of 0
	messageRadio = new QRadioButton(i18n("Text"), group, "messageButton");
	messageRadio->setFixedSize(messageRadio->sizeHint());
	connect(messageRadio, SIGNAL(toggled(bool)), this, SLOT(slotMessageToggled(bool)));
	QWhatsThis::add(messageRadio,
	      i18n("The edit field below contains the alarm message text."));
	grid->addWidget(messageRadio, 0, 0, AlignLeft);
	grid->setColStretch(0, 1);

	// File radio button has an ID of 1
	fileRadio = new QRadioButton(i18n("File"), group, "fileButton");
	fileRadio->setFixedSize(fileRadio->sizeHint());
	connect(fileRadio, SIGNAL(toggled(bool)), this, SLOT(slotFileToggled(bool)));
	QWhatsThis::add(fileRadio,
	      i18n("The edit field below contains the name of a text file whose contents will be "
	           "displayed as the alarm message text."));
	grid->addWidget(fileRadio, 0, 2, AlignRight);

	// Browse button
	browseButton = new QPushButton(i18n("&Browse..."), group);
	browseButton->setFixedSize(browseButton->sizeHint());
	connect(browseButton, SIGNAL(clicked()), this, SLOT(slotBrowse()));
	QWhatsThis::add(browseButton,
	      i18n("Select a text file to display."));
	grid->addWidget(browseButton, 0, 3, AlignLeft);

	messageEdit = new QMultiLineEdit(group);
	QSize size = messageEdit->sizeHint();
	size.setHeight(messageEdit->fontMetrics().lineSpacing()*13/4 + 2*messageEdit->frameWidth());
	messageEdit->setMinimumSize(size);
	messageEdit->setWrapPolicy(QMultiLineEdit::Anywhere);
	connect(messageEdit, SIGNAL(textChanged()), this, SLOT(slotMessageTextChanged()));
	grid->addMultiCellWidget(messageEdit, 1, 1, 0, 3);

	// Date and time entry
	timeWidget = new AlarmTimeWidget(i18n("Time"), false, page, "timeGroup");
	topLayout->addWidget(timeWidget);

	// Repeating alarm

	group = new QGroupBox(i18n("Repetition"), page, "repetitionGroup");
	topLayout->addWidget(group);
	layout = new QVBoxLayout(group, KDialog::spacingHint(), KDialog::spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	grid = new QGridLayout(group, 2, 4, KDialog::spacingHint());
	layout->addLayout(grid);

	lbl = new QLabel(group);
	lbl->setText(i18n("Count:"));
	lbl->setFixedSize(lbl->sizeHint());
	grid->addWidget(lbl, 0, 0, AlignLeft);

	repeatCount = new QSpinBox(0, 9999, 1, group);
	repeatCount->setFixedSize(repeatCount->sizeHint());
	QWhatsThis::add(repeatCount,
	      i18n("Enter the number of times to repeat the alarm, after its initial display."));
	connect(repeatCount, SIGNAL(valueChanged(int)), this, SLOT(slotRepeatCountChanged(int)));
	grid->addWidget(repeatCount, 0, 1, AlignLeft);

	lbl = new QLabel(group);
	lbl->setText(i18n("Interval:"));
	lbl->setFixedSize(lbl->sizeHint());
	grid->setColStretch(2, 1);
	grid->addWidget(lbl, 0, 2, AlignRight);

	repeatInterval = new TimeSpinBox(1, 99*60+59, group);
	repeatInterval->setValue(2399);
	size = repeatInterval->sizeHint();
	repeatInterval->setFixedSize(size);
	QWhatsThis::add(repeatInterval,
	      i18n("Enter the time (in hours and minutes) between repetitions of the alarm."));
	grid->addWidget(repeatInterval, 0, 3, AlignRight);

	// Repeat-at-login radio button has an ID of 1
	repeatAtLogin = new QCheckBox(i18n("Repeat at login"), group, "repeatAtLoginButton");
	repeatAtLogin->setFixedSize(repeatAtLogin->sizeHint());
	QWhatsThis::add(repeatAtLogin,
	      i18n("Repeat the alarm at every login until the specified time.\n"
	           "Note that it will also be repeated any time the alarm daemon is restarted."));
	grid->addWidget(repeatAtLogin, 1, 0, AlignLeft);

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
// grid->setColStretch(2, 1);
// topLayout->addWidget(bgColourChoose, 6);
	QWhatsThis::add(bgColourChoose,
	      i18n("Choose the background color for the alarm message."));
#endif

	setButtonText(Ok, i18n("&Set Alarm"));
	setButtonWhatsThis(Ok, i18n("Schedule the message for display at the specified time."));

	topLayout->activate();

	size = theApp()->readConfigWindowSize("EditDialog", minimumSize());
	resize(size);

	// Set up initial values
	if (event)
	{
		// Set the values to those for the specified event
		timeWidget->setDateTime(event->dateTime());
		bool fileMessageType = event->messageIsFileName();
		messageRadio->setChecked(!fileMessageType);
		fileRadio->setChecked(!fileMessageType);    // toggle the button to ensure things are set up correctly
		fileRadio->setChecked(fileMessageType);
		browseButton->setEnabled(fileMessageType);
		if (fileMessageType)
			messageEdit->setText(event->fileName());
		else
			messageEdit->setText(event->message());
		lateCancel->setChecked(event->lateCancel());
		beep->setChecked(event->beep());
		repeatCount->setValue(event->repeatCount());
		repeatInterval->setValue(event->repeatMinutes());
		repeatAtLogin->setChecked(event->repeatAtLogin());
#ifdef SELECT_FONT
		fontColour->setColour(event->colour());
		fontColour->setFont(?);
#else
		bgColourChoose->setColour(event->colour());
#endif
	}
	else
	{
		// Set the values to their defaults
		timeWidget->setDateTime(QDateTime::currentDateTime().addSecs(60));
		messageRadio->setChecked(false);    // toggle the button to ensure things are set up correctly
		messageRadio->setChecked(true);
//		fileRadio->setChecked(false);
		browseButton->setEnabled(false);
		messageEdit->setText(QString::null);
		repeatCount->setValue(0);
		repeatInterval->setValue(0);
		repeatAtLogin->setChecked(false);
#ifdef SELECT_FONT
		fontColour->setColour(theApp()->generalSettings()->defaultBgColour());
		fontColour->setFont(theApp()->generalSettings()->messageFont());
#else
		bgColourChoose->setColour(theApp()->generalSettings()->defaultBgColour());
#endif
	}

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
	int flags = (beep->isChecked()          ? KAlarmEvent::BEEP : 0)
	          | (lateCancel->isChecked()    ? KAlarmEvent::LATE_CANCEL : 0)
	          | (repeatAtLogin->isChecked() ? KAlarmEvent::REPEAT_AT_LOGIN : 0);
	event.set(alarmDateTime, alarmMessage, bgColourChoose->color(), fileRadio->isOn(),
	          flags, repeatCount->value(), repeatInterval->value());
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


void EditAlarmDlg::slotOk()
{
	if (timeWidget->getDateTime(alarmDateTime))
	{
		if (fileRadio->isOn())
		{
			// Convert any relative file path to absolute
			// (using home directory as the default)
			alarmMessage = messageEdit->text();
			int i = alarmMessage.find(QString::fromLatin1("/"));
			if (i > 0  &&  alarmMessage[i - 1] == ':')
			{
				KURL url(alarmMessage);
				url.cleanPath();
				alarmMessage = url.prettyURL();
			}
			else
			{
				// It's a local file - convert to absolute path & check validity
				QFileInfo info(alarmMessage);
				QDir::setCurrent(QDir::homeDirPath());
				alarmMessage = info.absFilePath();
				QString errmsg;
				if      (info.isDir())        errmsg = i18n("\nis a directory");
				else if (!info.exists())      errmsg = i18n("\nnot found");
				else if (!info.isReadable())  errmsg = i18n("\nis not readable");
				if (!errmsg.isEmpty())
				{
					messageEdit->setFocus();
					if (KMessageBox::warningContinueCancel(this, alarmMessage + errmsg, QString::null,
					                                       i18n("Continue")) == Cancel)
						return;
				}
				alarmMessage = QString::fromLatin1("file:") + alarmMessage;
			}
		}
		else
			alarmMessage = messageEdit->text();
		if (messageRadio->isOn())
			alarmMessage.stripWhiteSpace();
		accept();
	}
}

void EditAlarmDlg::slotCancel()
{
	reject();
}


/******************************************************************************
*  Called when the browse button is pressed to select a file to display.
*/
void EditAlarmDlg::slotBrowse()
{
	static QString defaultDir;
	if (defaultDir.isEmpty())
		defaultDir = QDir::homeDirPath();
	KURL url = KFileDialog::getOpenURL(defaultDir, QString::null, this, i18n("Choose Text file to Display"));
	if (!url.isEmpty())
	{
		alarmMessage = url.prettyURL();
		messageEdit->setText(alarmMessage);
		defaultDir = url.path();
	}
}

void EditAlarmDlg::slotMessageToggled(bool on)
{
	if (on  &&  fileRadio->isOn()
	||  !on  &&  !fileRadio->isOn())
		fileRadio->setChecked(!on);
	if (on)
	{
		QWhatsThis::add(messageEdit,
		      i18n("Enter the text of the alarm message. It may be multi-line."));
		messageEdit->setWordWrap(QMultiLineEdit::NoWrap);
	}
}

void EditAlarmDlg::slotFileToggled(bool on)
{
	if (on  &&  messageRadio->isOn()
	||  !on  &&  !messageRadio->isOn())
		messageRadio->setChecked(!on);
	browseButton->setEnabled(on);
	if (on)
	{
		QWhatsThis::add(messageEdit,
		      i18n("Enter the name of a text file, or a URL, to display."));
		if (!messageEdit->text().contains('\n'))
			messageEdit->setWordWrap(QMultiLineEdit::WidgetWidth);
	}
}

void EditAlarmDlg::slotMessageTextChanged()
{
	if (fileRadio->isOn())
	{
		QString text = messageEdit->text();
		int newline = text.find('\n');
		if (newline >= 0)
		{
			messageEdit->setText(text.left(newline));
			messageEdit->setWordWrap(QMultiLineEdit::WidgetWidth);
		}
	}
}

/******************************************************************************
*  Called when the repeat count edit box value has changed.
*/
void EditAlarmDlg::slotRepeatCountChanged(int count)
{
	repeatInterval->setEnabled(count);
}
