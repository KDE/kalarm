/*
*  editdlgtypes.cpp  -  dialogs to create or edit alarm or alarm template types
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
#include "editdlgtypes.moc"
#include "editdlgprivate.h"

#include "buttongroup.h"
#include "checkbox.h"
#include "colourcombo.h"
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
#include "reminder.h"
#include "shellprocess.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "templatepickdlg.h"
#include "timespinbox.h"

#include <QLabel>
#include <QDir>
#include <QStyle>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>

#include <klocale.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <khbox.h>
#include <kabc/addresseedialog.h>
#include <kdebug.h>

#include <kcal/icaldrag.h>

using namespace KCal;

enum { tTEXT, tFILE, tCOMMAND };  // order of mTypeCombo items


/*=============================================================================
= Class PickLogFileRadio
=============================================================================*/
class PickLogFileRadio : public PickFileRadio
{
    public:
	PickLogFileRadio(QPushButton* b, LineEdit* e, const QString& text, ButtonGroup* group, QWidget* parent)
		: PickFileRadio(b, e, text, group, parent) { }
	virtual QString pickFile()    // called when browse button is pressed to select a log file
	{
		return KAlarm::browseFile(i18nc("@title:window", "Choose Log File"), mDefaultDir, fileEdit()->text(), QString(),
		                          KFile::LocalOnly, parentWidget());
	}
    private:
	QString mDefaultDir;   // default directory for log file browse button
};


/*=============================================================================
= Class EditDisplayAlarmDlg
= Dialog to edit display alarms.
=============================================================================*/

QString EditDisplayAlarmDlg::i18n_chk_ConfirmAck()    { return i18nc("@option:check", "Confirm acknowledgment"); }


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditDisplayAlarmDlg::EditDisplayAlarmDlg(bool Template, bool newAlarm, QWidget* parent, GetResourceType getResource)
	: EditAlarmDlg(Template, KAEvent::MESSAGE, parent, getResource),
	  mSpecialActionsButton(0),
	  mReminderDeferral(false),
	  mReminderArchived(false)
{
	kDebug() << "New";
	init(0, newAlarm);
}

EditDisplayAlarmDlg::EditDisplayAlarmDlg(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                                         GetResourceType getResource, bool readOnly)
	: EditAlarmDlg(Template, event, parent, getResource, readOnly),
	  mSpecialActionsButton(0),
	  mReminderDeferral(false),
	  mReminderArchived(false)
{
	kDebug() << "Event.id()";
	init(event, newAlarm);
}

/******************************************************************************
* Return the window caption.
*/
QString EditDisplayAlarmDlg::type_caption(bool newAlarm) const
{
	return isTemplate() ? (newAlarm ? i18nc("@title:window", "New Display Alarm Template") : i18nc("@title:window", "Edit Display Alarm Template"))
	                    : (newAlarm ? i18nc("@title:window", "New Display Alarm") : i18nc("@title:window", "Edit Display Alarm"));
}

/******************************************************************************
* Set up the dialog controls common to display alarms.
*/
void EditDisplayAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
	// Display type combo box
	KHBox* box = new KHBox(parent);    // to group widgets for QWhatsThis text
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	QLabel* label = new QLabel(i18nc("@label:listbox", "Display type:"), box);
	label->setFixedSize(label->sizeHint());
	mTypeCombo = new ComboBox(box);
	QString textItem    = i18nc("@item:inlistbox", "Text message");
	QString fileItem    = i18nc("@item:inlistbox", "File contents");
	QString commandItem = i18nc("@item:inlistbox", "Command output");
	mTypeCombo->addItem(textItem);     // index = tTEXT
	mTypeCombo->addItem(fileItem);     // index = tFILE
	mTypeCombo->addItem(commandItem);  // index = tCOMMAND
	mTypeCombo->setFixedSize(mTypeCombo->sizeHint());
	mTypeCombo->setCurrentIndex(-1);    // ensure slotAlarmTypeChanged() is called when index is set
	connect(mTypeCombo, SIGNAL(currentIndexChanged(int)), SLOT(slotAlarmTypeChanged(int)));
	label->setBuddy(mTypeCombo);
	box->setWhatsThis(i18nc("@info:whatsthis", "<para>Select what the alarm should display:"
	      "<list><item><interface>%1</interface>: the alarm will display the text message you type in.</item>"
	      "<item><interface>%2</interface>: the alarm will display the contents of a text or image file.</item>"
	      "<item><interface>%3</interface>: the alarm will display the output from a command.</item></list></para>",
	      textItem, fileItem, commandItem));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the control
	frameLayout->addWidget(box);

	// Text message edit box
	mTextMessageEdit = new TextEdit(parent);
	mTextMessageEdit->setLineWrapMode(KTextEdit::NoWrap);
	mTextMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the text of the alarm message. It may be multi-line."));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new KHBox(parent);
	mFileBox->setMargin(0);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(LineEdit::Url, mFileBox);
	mFileMessageEdit->setAcceptDrops(true);
	mFileMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or URL of a text or image file to display."));

	// File browse button
	mFileBrowseButton = new QPushButton(mFileBox);
	mFileBrowseButton->setIcon(SmallIcon("document-open"));
	mFileBrowseButton->setFixedSize(mFileBrowseButton->sizeHint());
	mFileBrowseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
	mFileBrowseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a text or image file to display."));
	connect(mFileBrowseButton, SIGNAL(clicked()), SLOT(slotPickFile()));

	// Command type checkbox and edit box
	mCmdEdit = new CommandEdit(parent);
	frameLayout->addWidget(mCmdEdit);
	connect(mCmdEdit, SIGNAL(scriptToggled(bool)), SLOT(slotCmdScriptToggled(bool)));

	// Font and colour choice button and sample text
	mFontColourButton = new FontColourButton(parent);
	mFontColourButton->setMaximumHeight(mFontColourButton->sizeHint().height() * 3/2);
	frameLayout->addWidget(mFontColourButton);

	// Sound checkbox and file selector
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	frameLayout->addLayout(hlayout);
	mSoundPicker = new SoundPicker(parent);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	hlayout->addWidget(mSoundPicker);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(parent);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		hlayout->addWidget(mSpecialActionsButton);
	}

	// Top-adjust the controls
	mFilePadding = new KHBox(parent);
	mFilePadding->setMargin(0);
	frameLayout->addWidget(mFilePadding);
	frameLayout->setStretchFactor(mFilePadding, 1);
}

/******************************************************************************
* Create a reminder control.
*/
Reminder* EditDisplayAlarmDlg::createReminder(QWidget* parent)
{
	static const QString reminderText = i18nc("@info:whatsthis", "Enter how long in advance of the main alarm to display a reminder alarm.");
	return new Reminder(i18nc("@info:whatsthis", "Check to additionally display a reminder in advance of the main alarm time(s)."),
	                    i18nc("@info:whatsthis", "<para>Enter how long in advance of the main alarm to display a reminder alarm.</para><para>%1</para>", TimeSpinBox::shiftWhatsThis()),
	                    true, true, parent);
}

/******************************************************************************
* Create an "acknowledgement confirmation required" checkbox.
*/
CheckBox* EditDisplayAlarmDlg::createConfirmAckCheckbox(QWidget* parent)
{
	CheckBox* confirmAck = new CheckBox(i18n_chk_ConfirmAck(), parent);
	confirmAck->setWhatsThis(i18nc("@info:whatsthis", "Check to be prompted for confirmation when you acknowledge the alarm."));
	return confirmAck;
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditDisplayAlarmDlg::type_initValues(const KAEvent* event)
{
	mKMailSerialNumber = 0;
	lateCancel()->showAutoClose(true);
	if (event)
	{
		if (mAlarmType == KAEvent::MESSAGE  &&  event->kmailSerialNumber()
		&&  AlarmText::checkIfEmail(event->cleanText()))
			mKMailSerialNumber = event->kmailSerialNumber();
		lateCancel()->setAutoClose(event->autoClose());
		if (event->defaultFont())
			mFontColourButton->setDefaultFont();
		else
			mFontColourButton->setFont(event->font());
		mFontColourButton->setBgColour(event->bgColour());
		mFontColourButton->setFgColour(event->fgColour());
		mConfirmAck->setChecked(event->confirmAck());
		bool recurs = event->recurs();
		int reminderMins = event->reminder();
		if (!reminderMins  &&  event->reminderDeferral()  &&  !recurs)
		{
			reminderMins = event->reminderDeferral();
			mReminderDeferral = true;
		}
		if (!reminderMins  &&  event->reminderArchived()  &&  recurs)
		{
			reminderMins = event->reminderArchived();
			mReminderArchived = true;
		}
		reminder()->setMinutes(reminderMins, dateOnly());
		reminder()->setOnceOnly(event->reminderOnceOnly());
		reminder()->enableOnceOnly(recurs);
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(event->preAction(), event->postAction());
		Preferences::SoundType soundType = event->speak()                ? Preferences::Sound_Speak
		                                 : event->beep()                 ? Preferences::Sound_Beep
		                                 : !event->audioFile().isEmpty() ? Preferences::Sound_File
		                                 :                                 Preferences::Sound_None;
		mSoundPicker->set(soundType, event->audioFile(), event->soundVolume(),
		                  event->fadeVolume(), event->fadeSeconds(), event->repeatSound());
	}
	else
	{
		// Set the values to their defaults
		if (!ShellProcess::authorised())
		{
			// Don't allow shell commands in kiosk mode
			if (mSpecialActionsButton)
				mSpecialActionsButton->setEnabled(false);
		}
		lateCancel()->setAutoClose(Preferences::defaultAutoClose());
		mTypeCombo->setCurrentIndex(0);
		mFontColourButton->setDefaultFont();
		mFontColourButton->setBgColour(Preferences::defaultBgColour());
		mFontColourButton->setFgColour(Preferences::defaultFgColour());
		mConfirmAck->setChecked(Preferences::defaultConfirmAck());
		reminder()->setMinutes(0, false);
		reminder()->enableOnceOnly(isTimedRecurrence());   // must be called after mRecurrenceEdit is set up
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction());
		mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
		                  Preferences::defaultSoundVolume(), -1, 0, Preferences::defaultSoundRepeat());
	}

}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditDisplayAlarmDlg::setAction(KAEvent::Action action, const AlarmText& alarmText)
{
	QString text = alarmText.displayText();
	switch (action)
	{
		case KAEvent::MESSAGE:
			mTypeCombo->setCurrentIndex(tTEXT);
			mTextMessageEdit->setPlainText(text);
			mKMailSerialNumber = alarmText.isEmail() ? alarmText.kmailSerialNumber() : 0;
			break;
		case KAEvent::FILE:
			mTypeCombo->setCurrentIndex(tFILE);
			mFileMessageEdit->setText(text);
			break;
		case KAEvent::COMMAND:
			mTypeCombo->setCurrentIndex(tCOMMAND);
			mCmdEdit->setText(alarmText);
			break;
		default:
			Q_ASSERT(0);
			break;
	}
}

/******************************************************************************
* Set the read-only status of all non-template controls.
*/
void EditDisplayAlarmDlg::setReadOnly(bool readOnly)
{
	mTypeCombo->setReadOnly(readOnly);
	mTextMessageEdit->setReadOnly(readOnly);
	mFileMessageEdit->setReadOnly(readOnly);
	mCmdEdit->setReadOnly(readOnly);
	mFontColourButton->setReadOnly(readOnly);
	mSoundPicker->setReadOnly(readOnly);
	mConfirmAck->setReadOnly(readOnly);
	reminder()->setReadOnly(readOnly);
	if (mSpecialActionsButton)
		mSpecialActionsButton->setReadOnly(readOnly);
	if (readOnly)
	{
		mFileBrowseButton->hide();
		mFontColourButton->hide();
	}
	else
	{
		mFileBrowseButton->show();
		mFontColourButton->show();
	}
	EditAlarmDlg::setReadOnly(readOnly);
}

/******************************************************************************
* Save the state of all controls.
*/
void EditDisplayAlarmDlg::saveState(const KAEvent* event)
{
	EditAlarmDlg::saveState(event);
	mSavedType        = mTypeCombo->currentIndex();
	mSavedCmdScript   = mCmdEdit->isScript();
	mSavedSoundType   = mSoundPicker->sound();
	mSavedSoundFile   = mSoundPicker->file();
	mSavedSoundVolume = mSoundPicker->volume(mSavedSoundFadeVolume, mSavedSoundFadeSeconds);
	mSavedRepeatSound = mSoundPicker->repeat();
	mSavedConfirmAck  = mConfirmAck->isChecked();
	mSavedFont        = mFontColourButton->font();
	mSavedFgColour    = mFontColourButton->fgColour();
	mSavedBgColour    = mFontColourButton->bgColour();
	mSavedReminder    = reminder()->minutes();
	mSavedOnceOnly    = reminder()->isOnceOnly();
	mSavedAutoClose   = lateCancel()->isAutoClose();
	if (mSpecialActionsButton)
	{
		mSavedPreAction  = mSpecialActionsButton->preAction();
		mSavedPostAction = mSpecialActionsButton->postAction();
	}
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any controls have changed, or if it's a new event.
*       = false if no controls have changed.
*/
bool EditDisplayAlarmDlg::type_stateChanged() const
{
	if (mSavedType       != mTypeCombo->currentIndex()
	||  mSavedCmdScript  != mCmdEdit->isScript()
	||  mSavedSoundType  != mSoundPicker->sound()
	||  mSavedConfirmAck != mConfirmAck->isChecked()
	||  mSavedFont       != mFontColourButton->font()
	||  mSavedFgColour   != mFontColourButton->fgColour()
	||  mSavedBgColour   != mFontColourButton->bgColour()
	||  mSavedReminder   != reminder()->minutes()
	||  mSavedOnceOnly   != reminder()->isOnceOnly()
	||  mSavedAutoClose  != lateCancel()->isAutoClose())
		return true;
	if (mSpecialActionsButton)
	{
		if (mSavedPreAction  != mSpecialActionsButton->preAction()
		||  mSavedPostAction != mSpecialActionsButton->postAction())
			return true;
	}
	if (mSavedSoundType == Preferences::Sound_File)
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
	return false;
}

/******************************************************************************
* Extract the data in the dialog specific to the alarm type and set up a
* KAEvent from it.
*/
void EditDisplayAlarmDlg::type_setEvent(KAEvent& event, const KDateTime& dt, const QString& text, int lateCancel, bool trial)
{
	KAEvent::Action type;
	switch (mTypeCombo->currentIndex())
	{
		case tFILE:     type = KAEvent::FILE; break;
		case tCOMMAND:  type = KAEvent::COMMAND; break;
		default:
		case tTEXT:     type = KAEvent::MESSAGE; break;
	}
	event.set(dt, text, mFontColourButton->bgColour(), mFontColourButton->fgColour(),
	          mFontColourButton->font(), type, lateCancel, getAlarmFlags());
	if (type == KAEvent::MESSAGE)
	{
		if (AlarmText::checkIfEmail(text))
			event.setKMailSerialNumber(mKMailSerialNumber);
	}
	float fadeVolume;
	int   fadeSecs;
	float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
	event.setAudioFile(mSoundPicker->file().prettyUrl(), volume, fadeVolume, fadeSecs);
	if (!trial)
		event.setReminder(reminder()->minutes(), reminder()->isOnceOnly());
	if (mSpecialActionsButton  &&  mSpecialActionsButton->isEnabled())
		event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction());
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
int EditDisplayAlarmDlg::getAlarmFlags() const
{
	bool cmd = (mTypeCombo->currentIndex() == tCOMMAND);
	return EditAlarmDlg::getAlarmFlags()
	     | (mSoundPicker->sound() == Preferences::Sound_Beep  ? KAEvent::BEEP : 0)
	     | (mSoundPicker->sound() == Preferences::Sound_Speak ? KAEvent::SPEAK : 0)
	     | (mSoundPicker->repeat()                            ? KAEvent::REPEAT_SOUND : 0)
	     | (mConfirmAck->isChecked()                          ? KAEvent::CONFIRM_ACK : 0)
	     | (lateCancel()->isAutoClose()                       ? KAEvent::AUTO_CLOSE : 0)
	     | (mFontColourButton->defaultFont()                  ? KAEvent::DEFAULT_FONT : 0)
	     | (cmd                                               ? KAEvent::DISPLAY_COMMAND : 0)
	     | (cmd && mCmdEdit->isScript()                       ? KAEvent::SCRIPT : 0);
}

/******************************************************************************
*  Called when one of the alarm display type combo box is changed, to display
*  the appropriate set of controls for that action type.
*/
void EditDisplayAlarmDlg::slotAlarmTypeChanged(int index)
{
	QWidget* focus = 0;
	switch (index)
	{
		case tTEXT:    // text message
			mFileBox->hide();
			mFilePadding->hide();
			mCmdEdit->hide();
			mTextMessageEdit->show();
			mFontColourButton->show();
			mSoundPicker->showSpeak(true);
			if (mSpecialActionsButton)
				mSpecialActionsButton->show();
			setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Display the alarm message now"));
			focus = mTextMessageEdit;
			break;
		case tFILE:    // file contents
			mTextMessageEdit->hide();
			mFileBox->show();
			mFilePadding->show();
			mCmdEdit->hide();
			mFontColourButton->hide();
			mSoundPicker->showSpeak(false);
			if (mSpecialActionsButton)
				mSpecialActionsButton->show();
			setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Display the file now"));
			mFileMessageEdit->setNoSelect();
			focus = mFileMessageEdit;
			break;
		case tCOMMAND:    // command output
			mTextMessageEdit->hide();
			mFileBox->hide();
			slotCmdScriptToggled(mCmdEdit->isScript());  // show/hide mFilePadding
			mCmdEdit->show();
			mFontColourButton->show();
			mSoundPicker->showSpeak(true);
			if (mSpecialActionsButton)
				mSpecialActionsButton->hide();
			setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Display the command output now"));
			focus = mCmdEdit;
			break;
	}
	if (focus)
		focus->setFocus();
}

/******************************************************************************
* Called when the file browse button is pressed to select a file to display.
*/
void EditDisplayAlarmDlg::slotPickFile()
{
	static QString defaultDir;   // default directory for file browse button
	QString file = KAlarm::browseFile(i18nc("@title:window", "Choose Text or Image File to Display"),
	                                  defaultDir, mFileMessageEdit->text(), QString(), KFile::ExistingOnly, this);
	mFileMessageEdit->setText(file);
}

/******************************************************************************
* Called when one of the command type radio buttons is clicked,
* to display the appropriate edit field.
*/
void EditDisplayAlarmDlg::slotCmdScriptToggled(bool on)
{
	if (on)
		mFilePadding->hide();
	else
		mFilePadding->show();
}

/******************************************************************************
* Clean up the alarm text, and if it's a file, check whether it's valid.
*/
bool EditDisplayAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
	switch (mTypeCombo->currentIndex())
	{
		case tTEXT:
			result = mTextMessageEdit->toPlainText();
			break;

		case tFILE:
		{
			QString alarmtext = mFileMessageEdit->text().trimmed();
			// Convert any relative file path to absolute
			// (using home directory as the default)
			enum Err { NONE = 0, BLANK, NONEXISTENT, DIRECTORY, UNREADABLE, NOT_TEXT_IMAGE };
			Err err = NONE;
			KUrl url;
			int i = alarmtext.indexOf(QLatin1Char('/'));
			if (i > 0  &&  alarmtext[i - 1] == QLatin1Char(':'))
			{
				url = alarmtext;
				url.cleanPath();
				alarmtext = url.prettyUrl();
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
				QDir::setCurrent(QDir::homePath());
				alarmtext = info.absoluteFilePath();
				url.setPath(alarmtext);
				alarmtext = QLatin1String("file:") + alarmtext;
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
						KMessageBox::sorry(const_cast<EditDisplayAlarmDlg*>(this), i18nc("@info", "Please select a file to display"));
						return false;
					case NONEXISTENT:     errmsg = i18nc("@info", "<filename>%1</filename> not found", alarmtext);  break;
					case DIRECTORY:       errmsg = i18nc("@info", "<filename>%1</filename> is a folder", alarmtext);  break;
					case UNREADABLE:      errmsg = i18nc("@info", "<filename>%1</filename> is not readable", alarmtext);  break;
					case NOT_TEXT_IMAGE:  errmsg = i18nc("@info", "<filename>%1</filename> appears not to be a text or image file", alarmtext);  break;
					case NONE:
					default:
						break;
				}
				if (KMessageBox::warningContinueCancel(const_cast<EditDisplayAlarmDlg*>(this), errmsg)
				    == KMessageBox::Cancel)
					return false;
			}
			result = alarmtext;
			break;
		}
		case tCOMMAND:
			result = mCmdEdit->text();
			break;
	}
	return true;
}


/*=============================================================================
= Class EditCommandAlarmDlg
= Dialog to edit command alarms.
=============================================================================*/

QString EditCommandAlarmDlg::i18n_chk_EnterScript()        { return i18nc("@option:check", "Enter a script"); }
QString EditCommandAlarmDlg::i18n_radio_ExecInTermWindow() { return i18nc("@option:radio", "Execute in terminal window"); }
QString EditCommandAlarmDlg::i18n_chk_ExecInTermWindow()   { return i18nc("@option:check", "Execute in terminal window"); }


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditCommandAlarmDlg::EditCommandAlarmDlg(bool Template, bool newAlarm, QWidget* parent, GetResourceType getResource)
	: EditAlarmDlg(Template, KAEvent::COMMAND, parent, getResource)
{
	kDebug() << "New";
	init(0, newAlarm);
}

EditCommandAlarmDlg::EditCommandAlarmDlg(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                                         GetResourceType getResource, bool readOnly)
	: EditAlarmDlg(Template, event, parent, getResource, readOnly)
{
	kDebug() << "Event.id()";
	init(event, newAlarm);
}

/******************************************************************************
* Return the window caption.
*/
QString EditCommandAlarmDlg::type_caption(bool newAlarm) const
{
	return isTemplate() ? (newAlarm ? i18nc("@title:window", "New Command Alarm Template") : i18nc("@title:window", "Edit Command Alarm Template"))
	                    : (newAlarm ? i18nc("@title:window", "New Command Alarm") : i18nc("@title:window", "Edit Command Alarm"));
}

/******************************************************************************
* Set up the command alarm dialog controls.
*/
void EditCommandAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
	setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Execute the specified command now"));

	mCmdEdit = new CommandEdit(parent);
	connect(mCmdEdit, SIGNAL(scriptToggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	frameLayout->addWidget(mCmdEdit);

	// What to do with command output

	QGroupBox* cmdOutputBox = new QGroupBox(i18nc("@title:group", "Command Output"), parent);
	frameLayout->addWidget(cmdOutputBox);
	QVBoxLayout* vlayout = new QVBoxLayout(cmdOutputBox);
	vlayout->setMargin(marginHint());
	vlayout->setSpacing(spacingHint());
	mCmdOutputGroup = new ButtonGroup(cmdOutputBox);

	// Execute in terminal window
	mCmdExecInTerm = new RadioButton(i18n_radio_ExecInTermWindow(), cmdOutputBox);
	mCmdExecInTerm->setFixedSize(mCmdExecInTerm->sizeHint());
	mCmdExecInTerm->setWhatsThis(i18nc("@info:whatsthis", "Check to execute the command in a terminal window"));
	mCmdOutputGroup->addButton(mCmdExecInTerm, Preferences::Log_Terminal);
	vlayout->addWidget(mCmdExecInTerm, 0, Qt::AlignLeft);

	// Log file name edit box
	KHBox* box = new KHBox(cmdOutputBox);
	box->setMargin(0);
	(new QWidget(box))->setFixedWidth(mCmdExecInTerm->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth));   // indent the edit box
	mCmdLogFileEdit = new LineEdit(LineEdit::Url, box);
	mCmdLogFileEdit->setAcceptDrops(true);
	mCmdLogFileEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or path of the log file."));

	// Log file browse button.
	// The file browser dialog is activated by the PickLogFileRadio class.
	QPushButton* browseButton = new QPushButton(box);
	browseButton->setIcon(SmallIcon("document-open"));
	browseButton->setFixedSize(browseButton->sizeHint());
	browseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
	browseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a log file."));

	// Log output to file
	mCmdLogToFile = new PickLogFileRadio(browseButton, mCmdLogFileEdit, i18nc("@option:radio", "Log to file"), mCmdOutputGroup, cmdOutputBox);
	mCmdLogToFile->setFixedSize(mCmdLogToFile->sizeHint());
	mCmdLogToFile->setWhatsThis(i18nc("@info:whatsthis", "Check to log the command output to a local file. The output will be appended to any existing contents of the file."));
	mCmdOutputGroup->addButton(mCmdLogToFile, Preferences::Log_File);
	vlayout->addWidget(mCmdLogToFile, 0, Qt::AlignLeft);
	vlayout->addWidget(box);

	// Discard output
	mCmdDiscardOutput = new RadioButton(i18nc("@option:radio", "Discard"), cmdOutputBox);
	mCmdDiscardOutput->setFixedSize(mCmdDiscardOutput->sizeHint());
	mCmdDiscardOutput->setWhatsThis(i18nc("@info:whatsthis", "Check to discard command output."));
	mCmdOutputGroup->addButton(mCmdDiscardOutput, Preferences::Log_Discard);
	vlayout->addWidget(mCmdDiscardOutput, 0, Qt::AlignLeft);

	// Top-adjust the controls
	mCmdPadding = new KHBox(parent);
	mCmdPadding->setMargin(0);
	frameLayout->addWidget(mCmdPadding);
	frameLayout->setStretchFactor(mCmdPadding, 1);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditCommandAlarmDlg::type_initValues(const KAEvent* event)
{
	if (event)
	{
		// Set the values to those for the specified event
		RadioButton* logType = event->commandXterm()       ? mCmdExecInTerm
		                     : !event->logFile().isEmpty() ? mCmdLogToFile
		                     :                               mCmdDiscardOutput;
		if (logType == mCmdLogToFile)
			mCmdLogFileEdit->setText(event->logFile());    // set file name before setting radio button
		logType->setChecked(true);
	}
	else
	{
		// Set the values to their defaults
		mCmdEdit->setScript(Preferences::defaultCmdScript());
		mCmdLogFileEdit->setText(Preferences::defaultCmdLogFile());    // set file name before setting radio button
		mCmdOutputGroup->setButton(Preferences::defaultCmdLogType());
	}
	slotCmdScriptToggled(mCmdEdit->isScript());
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditCommandAlarmDlg::setAction(KAEvent::Action action, const AlarmText& alarmText)
{
	Q_ASSERT(action == KAEvent::COMMAND);
	mCmdEdit->setText(alarmText);
}

/******************************************************************************
* Set the read-only status of all non-template controls.
*/
void EditCommandAlarmDlg::setReadOnly(bool readOnly)
{
	if (!isTemplate()  &&  !ShellProcess::authorised())
		readOnly = true;     // don't allow editing of existing command alarms in kiosk mode
	mCmdEdit->setReadOnly(readOnly);
	mCmdExecInTerm->setReadOnly(readOnly);
	mCmdLogToFile->setReadOnly(readOnly);
	mCmdDiscardOutput->setReadOnly(readOnly);
	EditAlarmDlg::setReadOnly(readOnly);
}

/******************************************************************************
* Save the state of all controls.
*/
void EditCommandAlarmDlg::saveState(const KAEvent* event)
{
	EditAlarmDlg::saveState(event);
	mSavedCmdScript      = mCmdEdit->isScript();
	mSavedCmdOutputRadio = mCmdOutputGroup->checkedButton();
	mSavedCmdLogFile     = mCmdLogFileEdit->text();
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any controls have changed, or if it's a new event.
*       = false if no controls have changed.
*/
bool EditCommandAlarmDlg::type_stateChanged() const
{
	if (mSavedCmdScript      != mCmdEdit->isScript()
	||  mSavedCmdOutputRadio != mCmdOutputGroup->checkedButton())
		return true;
	if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
	{
		if (mSavedCmdLogFile != mCmdLogFileEdit->text())
			return true;
	}
	return false;
}

/******************************************************************************
* Extract the data in the dialog specific to the alarm type and set up a
* KAEvent from it.
*/
void EditCommandAlarmDlg::type_setEvent(KAEvent& event, const KDateTime& dt, const QString& text, int lateCancel, bool trial)
{
	Q_UNUSED(trial);
	event.set(dt, text, QColor(), QColor(), QFont(), KAEvent::COMMAND, lateCancel, getAlarmFlags());
	if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
		event.setLogFile(mCmdLogFileEdit->text());
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
int EditCommandAlarmDlg::getAlarmFlags() const
{
	return EditAlarmDlg::getAlarmFlags()
	     | (mCmdEdit->isScript()                               ? KAEvent::SCRIPT : 0)
	     | (mCmdOutputGroup->checkedButton() == mCmdExecInTerm ? KAEvent::EXEC_IN_XTERM : 0);
}

/******************************************************************************
* Validate and convert command alarm data.
*/
bool EditCommandAlarmDlg::type_validate(bool trial)
{
	Q_UNUSED(trial);
	if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
	{
		// Validate the log file name
		QString file = mCmdLogFileEdit->text();
		QFileInfo info(file);
		QDir::setCurrent(QDir::homePath());
		bool err = file.isEmpty()  ||  info.isDir();
		if (!err)
		{
			if (info.exists())
			{
				err = !info.isWritable();
			}
			else
			{
				QFileInfo dirinfo(info.absolutePath());    // get absolute directory path
				err = (!dirinfo.isDir()  ||  !dirinfo.isWritable());
			}
		}
		if (err)
		{
			showMainPage();
			mCmdLogFileEdit->setFocus();
			KMessageBox::sorry(this, i18nc("@info", "Log file must be the name or path of a local file, with write permission."));
			return false;
		}
		// Convert the log file to an absolute path
		mCmdLogFileEdit->setText(info.absoluteFilePath());
	}
	return true;
}

/******************************************************************************
* Tell the user the result of the Try action.
*/
void EditCommandAlarmDlg::type_trySuccessMessage(ShellProcess* proc, const QString& text)
{
	if (mCmdOutputGroup->checkedButton() != mCmdExecInTerm)
	{
		theApp()->commandMessage(proc, this);
		KMessageBox::information(this, i18nc("@info", "Command executed: <icode>%1</icode>", text));
		theApp()->commandMessage(proc, 0);
	}
}

/******************************************************************************
* Called when one of the command type radio buttons is clicked,
* to display the appropriate edit field.
*/
void EditCommandAlarmDlg::slotCmdScriptToggled(bool on)
{
	if (on)
		mCmdPadding->hide();
	else
		mCmdPadding->show();
}

/******************************************************************************
* Clean up the alarm text.
*/
bool EditCommandAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
	Q_UNUSED(showErrorMessage);
	result = mCmdEdit->text();
	return true;
}


/*=============================================================================
= Class EditEmailAlarmDlg
= Dialog to edit email alarms.
=============================================================================*/

QString EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf()    { return i18nc("@option:check", "Copy email to self"); }


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditEmailAlarmDlg::EditEmailAlarmDlg(bool Template, bool newAlarm, QWidget* parent, GetResourceType getResource)
	: EditAlarmDlg(Template, KAEvent::EMAIL, parent, getResource),
	  mEmailRemoveButton(0)
{
	kDebug() << "New";
	init(0, newAlarm);
}

EditEmailAlarmDlg::EditEmailAlarmDlg(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                                     GetResourceType getResource, bool readOnly)
	: EditAlarmDlg(Template, event, parent, getResource, readOnly),
	  mEmailRemoveButton(0)
{
	kDebug() << "Event.id()";
	init(event, newAlarm);
}

/******************************************************************************
* Return the window caption.
*/
QString EditEmailAlarmDlg::type_caption(bool newAlarm) const
{
	return isTemplate() ? (newAlarm ? i18nc("@title:window", "New Email Alarm Template") : i18nc("@title:window", "Edit Email Alarm Template"))
	                    : (newAlarm ? i18nc("@title:window", "New Email Alarm") : i18nc("@title:window", "Edit Email Alarm"));
}

/******************************************************************************
* Set up the email alarm dialog controls.
*/
void EditEmailAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
	setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Send the email to the specified addressees now"));

	QGridLayout* grid = new QGridLayout();
	grid->setMargin(0);
	grid->setColumnStretch(1, 1);
	frameLayout->addLayout(grid);

	mEmailFromList = 0;
	if (Preferences::emailFrom() == Preferences::MAIL_FROM_KMAIL)
	{
		// Email sender identity
		QLabel* label = new QLabel(i18nc("@label:listbox 'From' email address", "From:"), parent);
		label->setFixedSize(label->sizeHint());
		grid->addWidget(label, 0, 0);

		mEmailFromList = new EmailIdCombo(KAMail::identityManager(), parent);
		mEmailFromList->setMinimumSize(mEmailFromList->sizeHint());
		label->setBuddy(mEmailFromList);
		mEmailFromList->setWhatsThis(i18nc("@info:whatsthis", "Your email identity, used to identify you as the sender when sending email alarms."));
		grid->addWidget(mEmailFromList, 0, 1, 1, 2);
	}

	// Email recipients
	QLabel* label = new QLabel(i18nc("@label:textbox Email addressee", "To:"), parent);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);

	mEmailToEdit = new LineEdit(LineEdit::Emails, parent);
	mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
	mEmailToEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the addresses of the email recipients. Separate multiple addresses by "
	                                "commas or semicolons."));
	grid->addWidget(mEmailToEdit, 1, 1);

	mEmailAddressButton = new QPushButton(parent);
	mEmailAddressButton->setIcon(SmallIcon("help-contents"));
	mEmailAddressButton->setFixedSize(mEmailAddressButton->sizeHint());
	connect(mEmailAddressButton, SIGNAL(clicked()), SLOT(openAddressBook()));
	mEmailAddressButton->setToolTip(i18nc("@info:tooltip", "Open address book"));
	mEmailAddressButton->setWhatsThis(i18nc("@info:whatsthis", "Select email addresses from your address book."));
	grid->addWidget(mEmailAddressButton, 1, 2);

	// Email subject
	label = new QLabel(i18nc("@label:textbox Email subject", "Subject:"), parent);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 2, 0);

	mEmailSubjectEdit = new LineEdit(parent);
	mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
	label->setBuddy(mEmailSubjectEdit);
	mEmailSubjectEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the email subject."));
	grid->addWidget(mEmailSubjectEdit, 2, 1, 1, 2);

	// Email body
	mEmailMessageEdit = new TextEdit(parent);
	mEmailMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the email message."));
	frameLayout->addWidget(mEmailMessageEdit);

	// Email attachments
	grid = new QGridLayout();
	grid->setMargin(0);
	frameLayout->addLayout(grid);
	label = new QLabel(i18nc("@label:listbox", "Attachments:"), parent);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);

	mEmailAttachList = new KComboBox(parent);
	mEmailAttachList->setEditable(true);
	mEmailAttachList->setMinimumSize(mEmailAttachList->sizeHint());
	if (mEmailAttachList->lineEdit())
		mEmailAttachList->lineEdit()->setReadOnly(true);
//Q3ListBox* list = mEmailAttachList->listBox();
//QRect rect = list->geometry();
//list->setGeometry(rect.left() - 50, rect.top(), rect.width(), rect.height());
	label->setBuddy(mEmailAttachList);
	mEmailAttachList->setWhatsThis(i18nc("@info:whatsthis", "Files to send as attachments to the email."));
	grid->addWidget(mEmailAttachList, 0, 1);
	grid->setColumnStretch(1, 1);

	mEmailAddAttachButton = new QPushButton(i18nc("@action:button", "Add..."), parent);
	connect(mEmailAddAttachButton, SIGNAL(clicked()), SLOT(slotAddAttachment()));
	mEmailAddAttachButton->setWhatsThis(i18nc("@info:whatsthis", "Add an attachment to the email."));
	grid->addWidget(mEmailAddAttachButton, 0, 2);

	mEmailRemoveButton = new QPushButton(i18nc("@action:button", "Remove"), parent);
	connect(mEmailRemoveButton, SIGNAL(clicked()), SLOT(slotRemoveAttachment()));
	mEmailRemoveButton->setWhatsThis(i18nc("@info:whatsthis", "Remove the highlighted attachment from the email."));
	grid->addWidget(mEmailRemoveButton, 1, 2);

	// BCC email to sender
	mEmailBcc = new CheckBox(i18n_chk_CopyEmailToSelf(), parent);
	mEmailBcc->setFixedSize(mEmailBcc->sizeHint());
	mEmailBcc->setWhatsThis(i18nc("@info:whatsthis", "If checked, the email will be blind copied to you."));
	grid->addWidget(mEmailBcc, 1, 0, 1, 2, Qt::AlignLeft);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditEmailAlarmDlg::type_initValues(const KAEvent* event)
{
	if (event)
	{
		// Set the values to those for the specified event
		mEmailAttachList->addItems(event->emailAttachments());
		mEmailToEdit->setText(event->emailAddresses(", "));
		mEmailSubjectEdit->setText(event->emailSubject());
		mEmailBcc->setChecked(event->emailBcc());
		if (mEmailFromList)
			mEmailFromList->setCurrentIdentity(event->emailFromId());
	}
	else
	{
		// Set the values to their defaults
		mEmailBcc->setChecked(Preferences::defaultEmailBcc());
	}
	bool enable = !!mEmailAttachList->count();
	mEmailAttachList->setEnabled(enable);
	if (mEmailRemoveButton)
		mEmailRemoveButton->setEnabled(enable);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditEmailAlarmDlg::setAction(KAEvent::Action action, const AlarmText& alarmText)
{
	Q_ASSERT(action == KAEvent::EMAIL);
	if (alarmText.isEmail())
	{
		mEmailToEdit->setText(alarmText.to());
		mEmailSubjectEdit->setText(alarmText.subject());
		mEmailMessageEdit->setPlainText(alarmText.body());
	}
	else
		mEmailMessageEdit->setPlainText(alarmText.displayText());
}

/******************************************************************************
* Set the read-only status of all non-template controls.
*/
void EditEmailAlarmDlg::setReadOnly(bool readOnly)
{
	mEmailToEdit->setReadOnly(readOnly);
	mEmailSubjectEdit->setReadOnly(readOnly);
	mEmailMessageEdit->setReadOnly(readOnly);
	mEmailBcc->setReadOnly(readOnly);
	if (mEmailFromList)
		mEmailFromList->setReadOnly(readOnly);
	if (readOnly)
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
	EditAlarmDlg::setReadOnly(readOnly);
}

/******************************************************************************
* Save the state of all controls.
*/
void EditEmailAlarmDlg::saveState(const KAEvent* event)
{
	EditAlarmDlg::saveState(event);
	if (mEmailFromList)
		mSavedEmailFrom = mEmailFromList->currentIdentityName();
	mSavedEmailTo      = mEmailToEdit->text();
	mSavedEmailSubject = mEmailSubjectEdit->text();
	mSavedEmailAttach.clear();
	for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
		mSavedEmailAttach += mEmailAttachList->itemText(i);
	mSavedEmailBcc     = mEmailBcc->isChecked();
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any controls have changed, or if it's a new event.
*       = false if no controls have changed.
*/
bool EditEmailAlarmDlg::type_stateChanged() const
{
	QStringList emailAttach;
	for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
		emailAttach += mEmailAttachList->itemText(i);
	if (mEmailFromList  &&  mSavedEmailFrom != mEmailFromList->currentIdentityName()
	||  mSavedEmailTo      != mEmailToEdit->text()
	||  mSavedEmailSubject != mEmailSubjectEdit->text()
	||  mSavedEmailAttach  != emailAttach
	||  mSavedEmailBcc     != mEmailBcc->isChecked())
		return true;
	return false;
}

/******************************************************************************
* Extract the data in the dialog specific to the alarm type and set up a
* KAEvent from it.
*/
void EditEmailAlarmDlg::type_setEvent(KAEvent& event, const KDateTime& dt, const QString& text, int lateCancel, bool trial)
{
	Q_UNUSED(trial);
	event.set(dt, text, QColor(), QColor(), QFont(), KAEvent::EMAIL, lateCancel, getAlarmFlags());
	uint from = mEmailFromList ? mEmailFromList->currentIdentity() : 0;
	event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
int EditEmailAlarmDlg::getAlarmFlags() const
{
	return EditAlarmDlg::getAlarmFlags()
	     | (mEmailBcc->isChecked() ? KAEvent::EMAIL_BCC : 0);
}

/******************************************************************************
* Convert the email addresses to a list, and validate them. Convert the email
* attachments to a list.
*/
bool EditEmailAlarmDlg::type_validate(bool trial)
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
			KMessageBox::error(this, i18nc("@info", "Invalid email address: <email>%1</email>", bad));
			return false;
		}
	}
	if (mEmailAddresses.isEmpty())
	{
		mEmailToEdit->setFocus();
		KMessageBox::error(this, i18nc("@info", "No email address specified"));
		return false;
	}

	mEmailAttachments.clear();
	for (int i = 0, end = mEmailAttachList->count();  i < end;  ++i)
	{
		QString att = mEmailAttachList->itemText(i);
		switch (KAMail::checkAttachment(att))
		{
			case 1:
				mEmailAttachments.append(att);
				break;
			case 0:
				break;      // empty
			case -1:
				mEmailAttachList->setFocus();
				KMessageBox::error(this, i18nc("@info", "Invalid email attachment: <filename>%1</filename>", att));
				return false;
		}
	}
	if (trial  &&  KMessageBox::warningContinueCancel(this, i18nc("@info", "Do you really want to send the email now to the specified recipient(s)?"),
	                                                  i18nc("@action:button", "Confirm Email"), KGuiItem(i18nc("@action:button", "Send"))) != KMessageBox::Continue)
		return false;
	return true;
}

/******************************************************************************
* Tell the user the result of the Try action.
*/
void EditEmailAlarmDlg::type_trySuccessMessage(ShellProcess*, const QString&)
{
	QString msg;
	if (mEmailBcc->isChecked())
		msg = i18nc("@info", "Email sent to:<nl/>%1<nl/>Bcc: <email>%2</email>",
		            mEmailAddresses.join("<nl/>"), Preferences::emailBccAddress());
	else
		msg = i18nc("@info", "Email sent to:<nl/>%1", mEmailAddresses.join("<nl/>"));
	KMessageBox::information(this, msg);
}

/******************************************************************************
* Get a selection from the Address Book.
*/
void EditEmailAlarmDlg::openAddressBook()
{
	KABC::Addressee a = KABC::AddresseeDialog::getAddressee(this);
	if (a.isEmpty())
		return;
	Person person(a.realName(), a.preferredEmail());
	QString addrs = mEmailToEdit->text().trimmed();
	if (!addrs.isEmpty())
		addrs += ", ";
	addrs += person.fullName();
	mEmailToEdit->setText(addrs);
}

/******************************************************************************
* Select a file to attach to the email.
*/
void EditEmailAlarmDlg::slotAddAttachment()
{
	QString url = KAlarm::browseFile(i18nc("@title:window", "Choose File to Attach"), mAttachDefaultDir, QString(),
	                                 QString(), KFile::ExistingOnly, this);
	if (!url.isEmpty())
	{
		mEmailAttachList->addItem(url);
		mEmailAttachList->setCurrentIndex(mEmailAttachList->count() - 1);   // select the new item
		mEmailRemoveButton->setEnabled(true);
		mEmailAttachList->setEnabled(true);
	}
}

/******************************************************************************
* Remove the currently selected attachment from the email.
*/
void EditEmailAlarmDlg::slotRemoveAttachment()
{
	int item = mEmailAttachList->currentIndex();
	mEmailAttachList->removeItem(item);
	int count = mEmailAttachList->count();
	if (item >= count)
		mEmailAttachList->setCurrentIndex(count - 1);
	if (!count)
	{
		mEmailRemoveButton->setEnabled(false);
		mEmailAttachList->setEnabled(false);
	}
}

/******************************************************************************
* Clean up the alarm text.
*/
bool EditEmailAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
	Q_UNUSED(showErrorMessage);
	result = mEmailMessageEdit->toPlainText();
	return true;
}


/*=============================================================================
= Class CommandEdit
= A widget to allow entry of a command or a command script.
=============================================================================*/
CommandEdit::CommandEdit(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* vlayout = new QVBoxLayout(this);
	vlayout->setMargin(0);
	vlayout->setSpacing(KDialog::spacingHint());
	mTypeScript = new CheckBox(EditCommandAlarmDlg::i18n_chk_EnterScript(), this);
	mTypeScript->setFixedSize(mTypeScript->sizeHint());
	connect(mTypeScript, SIGNAL(toggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	mTypeScript->setWhatsThis(i18nc("@info:whatsthis", "Check to enter the contents of a script instead of a shell command line"));
	vlayout->addWidget(mTypeScript, 0, Qt::AlignLeft);

	mCommandEdit = new LineEdit(LineEdit::Url, this);
	mCommandEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter a shell command to execute."));
	vlayout->addWidget(mCommandEdit);

	mScriptEdit = new TextEdit(this);
	mScriptEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the contents of a script to execute"));
	vlayout->addWidget(mScriptEdit);

	slotCmdScriptToggled(mTypeScript->isChecked());
}

/******************************************************************************
* Initialise the widget controls from the specified event.
*/
void CommandEdit::setScript(bool script)
{
	mTypeScript->setChecked(script);
}

bool CommandEdit::isScript() const
{
	return mTypeScript->isChecked();
}

/******************************************************************************
* Set the widget's text.
*/
void CommandEdit::setText(const AlarmText& alarmText)
{
	QString text = alarmText.displayText();
	bool script = alarmText.isScript();
	mTypeScript->setChecked(script);
	if (script)
		mScriptEdit->setPlainText(text);
	else
		mCommandEdit->setText(text);
}

/******************************************************************************
* Return the widget's text.
*/
QString CommandEdit::text() const
{
	QString result;
	if (mTypeScript->isChecked())
		result = mScriptEdit->toPlainText();
	else
		result = mCommandEdit->text();
	return result.trimmed();
}

/******************************************************************************
* Set the read-only status of all controls.
*/
void CommandEdit::setReadOnly(bool readOnly)
{
	mTypeScript->setReadOnly(readOnly);
	mCommandEdit->setReadOnly(readOnly);
	mScriptEdit->setReadOnly(readOnly);
}

/******************************************************************************
* Called when one of the command type radio buttons is clicked,
* to display the appropriate edit field.
*/
void CommandEdit::slotCmdScriptToggled(bool on)
{
	if (on)
	{
		mCommandEdit->hide();
		mScriptEdit->show();
		mScriptEdit->setFocus();
	}
	else
	{
		mScriptEdit->hide();
		mCommandEdit->show();
		mCommandEdit->setFocus();
	}
	emit scriptToggled(on);
}

/******************************************************************************
* Returns the minimum size of the widget.
*/
QSize CommandEdit::minimumSizeHint() const
{
	QSize t(mTypeScript->minimumSizeHint());
	QSize s(mCommandEdit->minimumSizeHint().expandedTo(mScriptEdit->minimumSizeHint()));
	s.setHeight(s.height() + KDialog::spacingHint() + t.height());
	if (s.width() < t.width())
		s.setWidth(t.width());
	return s;
}



/*=============================================================================
= Class TextEdit
= A text edit field with a minimum height of 3 text lines.
=============================================================================*/
TextEdit::TextEdit(QWidget* parent)
	: KTextEdit(parent)
{
	QSize tsize = sizeHint();
	tsize.setHeight(fontMetrics().lineSpacing()*13/4 + 2*frameWidth());
	setMinimumSize(tsize);
}

void TextEdit::dragEnterEvent(QDragEnterEvent* e)
{
	if (KCal::ICalDrag::canDecode(e->mimeData()))
		e->ignore();   // don't accept "text/calendar" objects
	KTextEdit::dragEnterEvent(e);
}
