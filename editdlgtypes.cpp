/*
 *  editdlgtypes.cpp  -  dialogs to create or edit alarm or alarm template types
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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
#include "colourbutton.h"
#include "emailidcombo.h"
#include "fontcolourbutton.h"
#include "functions.h"
#include "identities.h"
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
#include "sounddlg.h"
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
	: EditAlarmDlg(Template, KAEventData::MESSAGE, parent, getResource),
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
	connect(mTypeCombo, SIGNAL(currentIndexChanged(int)), SLOT(contentsChanged()));
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
	connect(mTextMessageEdit, SIGNAL(textChanged()), SLOT(contentsChanged()));
	frameLayout->addWidget(mTextMessageEdit);

	// File name edit box
	mFileBox = new KHBox(parent);
	mFileBox->setMargin(0);
	frameLayout->addWidget(mFileBox);
	mFileMessageEdit = new LineEdit(LineEdit::Url, mFileBox);
	mFileMessageEdit->setAcceptDrops(true);
	mFileMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or URL of a text or image file to display."));
	connect(mFileMessageEdit, SIGNAL(textChanged(const QString&)), SLOT(contentsChanged()));

	// File browse button
	mFileBrowseButton = new QPushButton(mFileBox);
	mFileBrowseButton->setIcon(SmallIcon("document-open"));
	int size = mFileBrowseButton->sizeHint().height();
	mFileBrowseButton->setFixedSize(size, size);
	mFileBrowseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
	mFileBrowseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a text or image file to display."));
	connect(mFileBrowseButton, SIGNAL(clicked()), SLOT(slotPickFile()));

	// Command type checkbox and edit box
	mCmdEdit = new CommandEdit(parent);
	connect(mCmdEdit, SIGNAL(scriptToggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	connect(mCmdEdit, SIGNAL(changed()), SLOT(contentsChanged()));
	frameLayout->addWidget(mCmdEdit);

	// Sound checkbox and file selector
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	frameLayout->addLayout(hlayout);
	mSoundPicker = new SoundPicker(parent);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	connect(mSoundPicker, SIGNAL(changed()), SLOT(contentsChanged()));
	hlayout->addWidget(mSoundPicker);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	// Font and colour choice button and sample text
	mFontColourButton = new FontColourButton(parent);
	mFontColourButton->setMaximumHeight(mFontColourButton->sizeHint().height() * 3/2);
	hlayout->addWidget(mFontColourButton);
	connect(mFontColourButton, SIGNAL(selected(const QColor&, const QColor&)), SLOT(setColours(const QColor&, const QColor&)));
	connect(mFontColourButton, SIGNAL(selected(const QColor&, const QColor&)), SLOT(contentsChanged()));

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(false, parent);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		connect(mSpecialActionsButton, SIGNAL(selected()), SLOT(contentsChanged()));
		frameLayout->addWidget(mSpecialActionsButton, 0, Qt::AlignRight);
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
		if (mAlarmType == KAEventData::MESSAGE  &&  event->kmailSerialNumber()
		&&  AlarmText::checkIfEmail(event->cleanText()))
			mKMailSerialNumber = event->kmailSerialNumber();
		lateCancel()->setAutoClose(event->autoClose());
		if (event->useDefaultFont())
			mFontColourButton->setDefaultFont();
		else
			mFontColourButton->setFont(event->font());
		mFontColourButton->setBgColour(event->bgColour());
		mFontColourButton->setFgColour(event->fgColour());
		setColours(event->fgColour(), event->bgColour());
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
			mSpecialActionsButton->setActions(event->preAction(), event->postAction(), event->cancelOnPreActionError());
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
		setColours(Preferences::defaultFgColour(), Preferences::defaultBgColour());
		mConfirmAck->setChecked(Preferences::defaultConfirmAck());
		reminder()->setMinutes(0, false);
		reminder()->enableOnceOnly(isTimedRecurrence());   // must be called after mRecurrenceEdit is set up
		if (mSpecialActionsButton)
			mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction(), Preferences::defaultCancelOnPreActionError());
		mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
		                  Preferences::defaultSoundVolume(), -1, 0, Preferences::defaultSoundRepeat());
	}
}

/******************************************************************************
* Called when the More/Less Options button is clicked.
* Show/hide the optional options.
*/
void EditDisplayAlarmDlg::type_showOptions(bool more)
{
	if (mSpecialActionsButton)
	{
		if (more)
		{
			switch (mTypeCombo->currentIndex())
			{
				case tFILE:
				case tTEXT:
					mSpecialActionsButton->show();
					break;
				default:
					break;
			}
		}
		else
			mSpecialActionsButton->hide();
	}
}

/******************************************************************************
* Called when the font/color button has been clicked.
* Set the colors in the message text entry control.
*/
void EditDisplayAlarmDlg::setColours(const QColor& fgColour, const QColor& bgColour)
{
	QPalette pal = mTextMessageEdit->palette();
	pal.setColor(mTextMessageEdit->backgroundRole(), bgColour);
	pal.setColor(QPalette::Text, fgColour);
	mTextMessageEdit->setPalette(pal);
	pal = mTextMessageEdit->viewport()->palette();
	pal.setColor(mTextMessageEdit->viewport()->backgroundRole(), bgColour);
	pal.setColor(QPalette::Text, fgColour);
	mTextMessageEdit->viewport()->setPalette(pal);
	// Change the color of existing text
	QTextCursor cursor = mTextMessageEdit->textCursor();
	mTextMessageEdit->selectAll();
	mTextMessageEdit->setTextColor(fgColour);
	mTextMessageEdit->setTextCursor(cursor);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditDisplayAlarmDlg::setAction(KAEventData::Action action, const AlarmText& alarmText)
{
	QString text = alarmText.displayText();
	switch (action)
	{
		case KAEventData::MESSAGE:
			mTypeCombo->setCurrentIndex(tTEXT);
			mTextMessageEdit->setPlainText(text);
			mKMailSerialNumber = alarmText.isEmail() ? alarmText.kmailSerialNumber() : 0;
			break;
		case KAEventData::FILE:
			mTypeCombo->setCurrentIndex(tFILE);
			mFileMessageEdit->setText(text);
			break;
		case KAEventData::COMMAND:
			mTypeCombo->setCurrentIndex(tCOMMAND);
			mCmdEdit->setText(alarmText);
			break;
		default:
			Q_ASSERT(0);
			break;
	}
}

/******************************************************************************
* Initialise various values in the New Alarm dialogue.
*/
void EditDisplayAlarmDlg::setBgColour(const QColor& colour)
{
	mFontColourButton->setBgColour(colour);
	setColours(mFontColourButton->fgColour(), colour);
}
void EditDisplayAlarmDlg::setFgColour(const QColor& colour)
{
	mFontColourButton->setFgColour(colour);
	setColours(colour, mFontColourButton->bgColour());
}
void EditDisplayAlarmDlg::setConfirmAck(bool confirm)
{
	mConfirmAck->setChecked(confirm);
}
void EditDisplayAlarmDlg::setAutoClose(bool close)
{
	lateCancel()->setAutoClose(close);
}
void EditDisplayAlarmDlg::setAudio(Preferences::SoundType type, const QString& file, float volume, bool repeat)
{
	mSoundPicker->set(type, file, volume, -1, 0, repeat);
}
void EditDisplayAlarmDlg::setReminder(int minutes)
{
	bool onceOnly = (minutes < 0);
	if (onceOnly)
		minutes = -minutes;
	reminder()->setMinutes(minutes, dateOnly());
	reminder()->setOnceOnly(onceOnly);
	reminder()->enableOnceOnly(isTimedRecurrence());
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
		mFileBrowseButton->hide();
	else
		mFileBrowseButton->show();
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
		mSavedPreAction       = mSpecialActionsButton->preAction();
		mSavedPostAction      = mSpecialActionsButton->postAction();
		mSavedPreActionCancel = mSpecialActionsButton->cancelOnError();
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
		if (mSavedPreAction       != mSpecialActionsButton->preAction()
		||  mSavedPostAction      != mSpecialActionsButton->postAction()
		||  mSavedPreActionCancel != mSpecialActionsButton->cancelOnError())
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
	KAEventData::Action type;
	switch (mTypeCombo->currentIndex())
	{
		case tFILE:     type = KAEventData::FILE; break;
		case tCOMMAND:  type = KAEventData::COMMAND; break;
		default:
		case tTEXT:     type = KAEventData::MESSAGE; break;
	}
	event.set(dt, text, mFontColourButton->bgColour(), mFontColourButton->fgColour(), mFontColourButton->font(),
	          type, lateCancel, getAlarmFlags());
	if (type == KAEventData::MESSAGE)
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
		event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction(), mSpecialActionsButton->cancelOnError());
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
			mSoundPicker->showSpeak(true);
			if (mSpecialActionsButton  &&  showingMore())
				mSpecialActionsButton->show();
			setButtonWhatsThis(Try, i18nc("@info:whatsthis", "Display the alarm message now"));
			focus = mTextMessageEdit;
			break;
		case tFILE:    // file contents
			mTextMessageEdit->hide();
			mFileBox->show();
			mFilePadding->show();
			mCmdEdit->hide();
			mSoundPicker->showSpeak(false);
			if (mSpecialActionsButton  &&  showingMore())
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
	if (!file.isEmpty())
	{
		mFileMessageEdit->setText(KAlarm::pathOrUrl(file));
		contentsChanged();
	}
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
			KUrl url;
			KAlarm::FileErr err = KAlarm::checkFileExists(alarmtext, url);
			if (err == KAlarm::FileErr_None)
			{
				switch (KAlarm::fileType(KFileItem(KFileItem::Unknown, KFileItem::Unknown, url).mimeTypePtr()))
				{
					case KAlarm::TextFormatted:
					case KAlarm::TextPlain:
					case KAlarm::TextApplication:
					case KAlarm::Image:
						break;
					default:
						err = KAlarm::FileErr_NotTextImage;
						break;
				}
			}
			if (err != KAlarm::FileErr_None  &&  showErrorMessage)
			{
				mFileMessageEdit->setFocus();
				if (!KAlarm::showFileErrMessage(alarmtext, err, KAlarm::FileErr_BlankDisplay, const_cast<EditDisplayAlarmDlg*>(this)))
					return false;
			}
			result = alarmtext;
			break;
		}
		case tCOMMAND:
			result = mCmdEdit->text(const_cast<EditDisplayAlarmDlg*>(this), showErrorMessage);
			if (result.isEmpty())
				return false;
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
	: EditAlarmDlg(Template, KAEventData::COMMAND, parent, getResource)
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
	connect(mCmdEdit, SIGNAL(changed()), SLOT(contentsChanged()));
	frameLayout->addWidget(mCmdEdit);

	// What to do with command output

	mCmdOutputBox = new QGroupBox(i18nc("@title:group", "Command Output"), parent);
	frameLayout->addWidget(mCmdOutputBox);
	QVBoxLayout* vlayout = new QVBoxLayout(mCmdOutputBox);
	vlayout->setMargin(marginHint());
	vlayout->setSpacing(spacingHint());
	mCmdOutputGroup = new ButtonGroup(mCmdOutputBox);
	connect(mCmdOutputGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(contentsChanged()));

	// Execute in terminal window
	mCmdExecInTerm = new RadioButton(i18n_radio_ExecInTermWindow(), mCmdOutputBox);
	mCmdExecInTerm->setFixedSize(mCmdExecInTerm->sizeHint());
	mCmdExecInTerm->setWhatsThis(i18nc("@info:whatsthis", "Check to execute the command in a terminal window"));
	mCmdOutputGroup->addButton(mCmdExecInTerm, Preferences::Log_Terminal);
	vlayout->addWidget(mCmdExecInTerm, 0, Qt::AlignLeft);

	// Log file name edit box
	KHBox* box = new KHBox(mCmdOutputBox);
	box->setMargin(0);
	(new QWidget(box))->setFixedWidth(mCmdExecInTerm->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth));   // indent the edit box
	mCmdLogFileEdit = new LineEdit(LineEdit::Url, box);
	mCmdLogFileEdit->setAcceptDrops(true);
	mCmdLogFileEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or path of the log file."));
	connect(mCmdLogFileEdit, SIGNAL(textChanged(const QString&)), SLOT(contentsChanged()));

	// Log file browse button.
	// The file browser dialog is activated by the PickLogFileRadio class.
	QPushButton* browseButton = new QPushButton(box);
	browseButton->setIcon(SmallIcon("document-open"));
	int size = browseButton->sizeHint().height();
	browseButton->setFixedSize(size, size);
	browseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
	browseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a log file."));

	// Log output to file
	mCmdLogToFile = new PickLogFileRadio(browseButton, mCmdLogFileEdit, i18nc("@option:radio", "Log to file"), mCmdOutputGroup, mCmdOutputBox);
	mCmdLogToFile->setFixedSize(mCmdLogToFile->sizeHint());
	mCmdLogToFile->setWhatsThis(i18nc("@info:whatsthis", "Check to log the command output to a local file. The output will be appended to any existing contents of the file."));
	connect(mCmdLogToFile, SIGNAL(fileChanged()), SLOT(contentsChanged()));
	mCmdOutputGroup->addButton(mCmdLogToFile, Preferences::Log_File);
	vlayout->addWidget(mCmdLogToFile, 0, Qt::AlignLeft);
	vlayout->addWidget(box);

	// Discard output
	mCmdDiscardOutput = new RadioButton(i18nc("@option:radio", "Discard"), mCmdOutputBox);
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
* Called when the More/Less Options button is clicked.
* Show/hide the optional options.
*/
void EditCommandAlarmDlg::type_showOptions(bool more)
{
	if (more)
		mCmdOutputBox->show();
	else
		mCmdOutputBox->hide();
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditCommandAlarmDlg::setAction(KAEventData::Action action, const AlarmText& alarmText)
{
	Q_ASSERT(action == KAEventData::COMMAND);
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
	event.set(dt, text, QColor(), QColor(), QFont(), KAEventData::COMMAND, lateCancel, getAlarmFlags());
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
	result = mCmdEdit->text(const_cast<EditCommandAlarmDlg*>(this), showErrorMessage);
	if (result.isEmpty())
		return false;
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
	: EditAlarmDlg(Template, KAEventData::EMAIL, parent, getResource),
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

		mEmailFromList = new EmailIdCombo(Identities::identityManager(), parent);
		mEmailFromList->setMinimumSize(mEmailFromList->sizeHint());
		label->setBuddy(mEmailFromList);
		mEmailFromList->setWhatsThis(i18nc("@info:whatsthis", "Your email identity, used to identify you as the sender when sending email alarms."));
		connect(mEmailFromList, SIGNAL(identityChanged(uint)), SLOT(contentsChanged()));
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
	connect(mEmailToEdit, SIGNAL(textChanged(const QString&)), SLOT(contentsChanged()));
	grid->addWidget(mEmailToEdit, 1, 1);

	mEmailAddressButton = new QPushButton(parent);
	mEmailAddressButton->setIcon(SmallIcon("help-contents"));
	int size = mEmailAddressButton->sizeHint().height();
	mEmailAddressButton->setFixedSize(size, size);
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
	connect(mEmailSubjectEdit, SIGNAL(textChanged(const QString&)), SLOT(contentsChanged()));
	grid->addWidget(mEmailSubjectEdit, 2, 1, 1, 2);

	// Email body
	mEmailMessageEdit = new TextEdit(parent);
	mEmailMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the email message."));
	connect(mEmailMessageEdit, SIGNAL(textChanged()), SLOT(contentsChanged()));
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
	connect(mEmailBcc, SIGNAL(toggled(bool)), SLOT(contentsChanged()));
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
	attachmentEnable();
}

/******************************************************************************
* Enable/disable controls depending on whether any attachments are entered.
*/
void EditEmailAlarmDlg::attachmentEnable()
{
	bool enable = mEmailAttachList->count();
	mEmailAttachList->setEnabled(enable);
	if (mEmailRemoveButton)
		mEmailRemoveButton->setEnabled(enable);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditEmailAlarmDlg::setAction(KAEventData::Action action, const AlarmText& alarmText)
{
	Q_ASSERT(action == KAEventData::EMAIL);
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
* Initialise various values in the New Alarm dialogue.
*/
void EditEmailAlarmDlg::setEmailFields(uint fromID, const EmailAddressList& addresses,
                                       const QString& subject, const QStringList& attachments)
{
	if (fromID)
		mEmailFromList->setCurrentIdentity(fromID);
	if (!addresses.isEmpty())
		mEmailToEdit->setText(addresses.join(", "));
	if (!subject.isEmpty())
		mEmailSubjectEdit->setText(subject);
	if (!attachments.isEmpty())
	{
		mEmailAttachList->addItems(attachments);
		attachmentEnable();
	}
}
void EditEmailAlarmDlg::setBcc(bool bcc)
{
	mEmailBcc->setChecked(bcc);
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
	if ((mEmailFromList  &&  mSavedEmailFrom != mEmailFromList->currentIdentityName())
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
	event.set(dt, text, QColor(), QColor(), QFont(), KAEventData::EMAIL, lateCancel, getAlarmFlags());
	uint from = mEmailFromList ? mEmailFromList->currentIdentity() : 0;
	event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
int EditEmailAlarmDlg::getAlarmFlags() const
{
	return EditAlarmDlg::getAlarmFlags()
	     | (mEmailBcc->isChecked() ? KAEventData::EMAIL_BCC : 0);
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
	QString to = mEmailAddresses.join("<nl/>");
	to.replace('<', "&lt;");
	to.replace('>', "&gt;");
	if (mEmailBcc->isChecked())
		msg = "<qt>" + i18nc("@info", "Email sent to:<nl/>%1<nl/>Bcc: <email>%2</email>",
		            to, Preferences::emailBccAddress()) + "</qt>";
	else
		msg = "<qt>" + i18nc("@info", "Email sent to:<nl/>%1", to) + "</qt>";
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
		contentsChanged();
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
	contentsChanged();
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
= Class EditAudioAlarmDlg
= Dialog to edit audio alarms with no display window.
=============================================================================*/

/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditAudioAlarmDlg::EditAudioAlarmDlg(bool Template, bool newAlarm, QWidget* parent, GetResourceType getResource)
	: EditAlarmDlg(Template, KAEventData::AUDIO, parent, getResource)
{
	kDebug() << "New";
	init(0, newAlarm);
}

EditAudioAlarmDlg::EditAudioAlarmDlg(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                                     GetResourceType getResource, bool readOnly)
	: EditAlarmDlg(Template, event, parent, getResource, readOnly)
{
	kDebug() << "Event.id()";
	init(event, newAlarm);
}

/******************************************************************************
* Return the window caption.
*/
QString EditAudioAlarmDlg::type_caption(bool newAlarm) const
{
	return isTemplate() ? (newAlarm ? i18nc("@title:window", "New Audio Alarm Template") : i18nc("@title:window", "Edit Audio Alarm Template"))
	                    : (newAlarm ? i18nc("@title:window", "New Audio Alarm") : i18nc("@title:window", "Edit Audio Alarm"));
}

/******************************************************************************
* Set up the dialog controls common to display alarms.
*/
void EditAudioAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
	// File name edit box
	mSoundConfig = new SoundWidget(false, true, parent);
	connect(mSoundConfig, SIGNAL(changed()), SLOT(contentsChanged()));
	frameLayout->addWidget(mSoundConfig);

	// Top-adjust the controls
	mPadding = new KHBox(parent);
	mPadding->setMargin(0);
	frameLayout->addWidget(mPadding);
	frameLayout->setStretchFactor(mPadding, 1);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditAudioAlarmDlg::type_initValues(const KAEvent* event)
{
	if (event)
	{
		mSoundConfig->set(event->audioFile(), event->soundVolume(), event->fadeVolume(), event->fadeSeconds(),
		                  (event->flags() & KAEvent::REPEAT_SOUND));
	}
	else
	{
		// Set the values to their defaults
		mSoundConfig->set(Preferences::defaultSoundFile(), Preferences::defaultSoundVolume(), -1, 0, false);
	}
}

/******************************************************************************
* Initialise various values in the New Alarm dialogue.
*/
void EditAudioAlarmDlg::setAudio(const QString& file, float volume)
{
	mSoundConfig->set(file, volume, -1, 0, false);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditAudioAlarmDlg::setAction(KAEventData::Action action, const AlarmText&)
{
	Q_ASSERT(action == KAEventData::AUDIO);
}

/******************************************************************************
* Set the read-only status of all non-template controls.
*/
void EditAudioAlarmDlg::setReadOnly(bool readOnly)
{
	mSoundConfig->setReadOnly(readOnly);
	EditAlarmDlg::setReadOnly(readOnly);
}

/******************************************************************************
* Save the state of all controls.
*/
void EditAudioAlarmDlg::saveState(const KAEvent* event)
{
	EditAlarmDlg::saveState(event);
	mSavedFile   = mSoundConfig->fileName();
	mSavedRepeat = mSoundConfig->getVolume(mSavedVolume, mSavedFadeVolume, mSavedFadeSeconds);
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any controls have changed, or if it's a new event.
*       = false if no controls have changed.
*/
bool EditAudioAlarmDlg::type_stateChanged() const
{
        if (mSavedFile != mSoundConfig->fileName())
                return true;
        if (!mSavedFile.isEmpty())
        {
                float volume, fadeVolume;
                int   fadeSecs;
		if (mSavedRepeat      != mSoundConfig->getVolume(volume, fadeVolume, fadeSecs)
                ||  mSavedVolume      != volume
                ||  mSavedFadeVolume  != fadeVolume
                ||  mSavedFadeSeconds != fadeSecs)
                        return true;
	}
	return false;
}

/******************************************************************************
* Extract the data in the dialog specific to the alarm type and set up a
* KAEvent from it.
*/
void EditAudioAlarmDlg::type_setEvent(KAEvent& event, const KDateTime& dt, const QString& text, int lateCancel, bool trial)
{
	Q_UNUSED(text);
	Q_UNUSED(trial);
	event.set(dt, QString(), QColor(), QColor(), QFont(), KAEventData::AUDIO, lateCancel, getAlarmFlags());
	float volume, fadeVolume;
	int   fadeSecs;
	mSoundConfig->getVolume(volume, fadeVolume, fadeSecs);
	event.setAudioFile(mSoundConfig->file(false).prettyUrl(), volume, fadeVolume, fadeSecs);
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
int EditAudioAlarmDlg::getAlarmFlags() const
{
	return EditAlarmDlg::getAlarmFlags()
	     | (mSoundConfig->getRepeat() ? KAEvent::REPEAT_SOUND : 0);
}

/******************************************************************************
* Check whether the file name is valid.
*/
bool EditAudioAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
	result = mSoundConfig->file(showErrorMessage).pathOrUrl();
	return !result.isEmpty();
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
	mTypeScript->setWhatsThis(i18nc("@info:whatsthis", "Check to enter the contents of a script instead of a shell command line"));
	connect(mTypeScript, SIGNAL(toggled(bool)), SLOT(slotCmdScriptToggled(bool)));
	connect(mTypeScript, SIGNAL(toggled(bool)), SIGNAL(changed()));
	vlayout->addWidget(mTypeScript, 0, Qt::AlignLeft);

	mCommandEdit = new LineEdit(LineEdit::Url, this);
	mCommandEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter a shell command to execute."));
	connect(mCommandEdit, SIGNAL(textChanged(const QString&)), SIGNAL(changed()));
	vlayout->addWidget(mCommandEdit);

	mScriptEdit = new TextEdit(this);
	mScriptEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the contents of a script to execute"));
	connect(mScriptEdit, SIGNAL(textChanged()), SIGNAL(changed()));
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
		mCommandEdit->setText(KAlarm::pathOrUrl(text));
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
* Return the alarm text.
* If 'showErrorMessage' is true and the text is empty, an error message is
* displayed.
*/
QString CommandEdit::text(EditAlarmDlg* dlg, bool showErrorMessage) const
{
	QString result = text();
	if (showErrorMessage  &&  result.isEmpty())
		KMessageBox::sorry(dlg, i18nc("@info", "Please enter a command or script to execute"));
	return result;
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
