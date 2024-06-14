/*
 *  editdlgtypes.cpp  -  dialogs to create or edit alarm or alarm template types
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "editdlgtypes.h"
#include "editdlg_p.h"

#include "audioplayer.h"
#include "emailidcombo.h"
#include "fontcolourbutton.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "latecancel.h"
#include "mainwindow.h"
#include "messagewindow.h"
#include "pickfileradio.h"
#include "reminder.h"
#include "soundpicker.h"
#include "sounddlg.h"
#include "specialactions.h"
#include "lib/buttongroup.h"
#include "lib/checkbox.h"
#include "lib/dragdrop.h"
#include "lib/file.h"
#include "lib/lineedit.h"
#include "lib/messagebox.h"
#include "lib/radiobutton.h"
#include "lib/shellprocess.h"
#include "lib/timespinbox.h"
#include "kalarmcalendar/identities.h"
#include "akonadiplugin/akonadiplugin.h"
#include "kalarm_debug.h"

#include <KCalUtils/ICalDrag>
#include <KCalendarCore/Person>

#include <KLocalizedString>
#include <KFileItem>

#include <QComboBox>
#include <QLabel>
#include <QDir>
#include <QStyle>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QStandardItemModel>

using namespace KAlarmCal;
using namespace KCalendarCore;

enum { tTEXT, tFILE, tCOMMAND };  // order of mTypeCombo items
enum { dWINDOW, dNOTIFY };        // order of mDisplayMethodCombo items


/*=============================================================================
= Class PickLogFileRadio
=============================================================================*/
class PickLogFileRadio : public PickFileRadio
{
    public:
        PickLogFileRadio(QPushButton* b, LineEdit* e, const QString& text, ButtonGroup* group, QWidget* parent)
            : PickFileRadio(b, e, text, group, parent) { }
        bool pickFile(QString& file) override    // called when browse button is pressed to select a log file
        {
            return File::browseFile(file, i18nc("@title:window", "Choose Log File"), mDefaultDir, fileEdit()->text(),
                                    false, parentWidget());
        }
    private:
        QString mDefaultDir;   // default directory for log file browse button
};


/*=============================================================================
= Class EditDisplayAlarmDlg
= Dialog to edit display alarms.
=============================================================================*/

QString EditDisplayAlarmDlg::i18n_lbl_DisplayMethod() { return i18nc("@label:listbox How to display alarms", "Display method:"); }
QString EditDisplayAlarmDlg::i18n_combo_Window()      { return i18nc("@item:inlistbox", "Window"); }
QString EditDisplayAlarmDlg::i18n_combo_Notify()      { return i18nc("@item:inlistbox", "Notification"); }
QString EditDisplayAlarmDlg::i18n_chk_ConfirmAck()    { return i18nc("@option:check", "Confirm acknowledgment"); }

/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditDisplayAlarmDlg::EditDisplayAlarmDlg(bool Template, QWidget* parent, GetResourceType getResource)
    : EditAlarmDlg(Template, KAEvent::SubAction::Message, parent, getResource)
{
    qCDebug(KALARM_LOG) << "EditDisplayAlarmDlg: New";
    init(KAEvent());
}

EditDisplayAlarmDlg::EditDisplayAlarmDlg(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                         GetResourceType getResource, bool readOnly)
    : EditAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly)
{
    qCDebug(KALARM_LOG) << "EditDisplayAlarmDlg: Event.id()";
    init(event);
}

EditDisplayAlarmDlg::~EditDisplayAlarmDlg()
{
    // Delete KTextEdit while parent exists to prevent crash if spell checking is enabled.
    delete mTextMessageEdit;
    mTextMessageEdit = nullptr;
}

/******************************************************************************
* Return the window caption.
*/
QString EditDisplayAlarmDlg::type_caption() const
{
    return isTemplate() ? (isNewAlarm() ? i18nc("@title:window", "New Display Alarm Template") : i18nc("@title:window", "Edit Display Alarm Template"))
                        : (isNewAlarm() ? i18nc("@title:window", "New Display Alarm") : i18nc("@title:window", "Edit Display Alarm"));
}

/******************************************************************************
* Set up the dialog controls common to display alarms.
*/
void EditDisplayAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
    // Display type combo box
    QWidget* box = new QWidget(parent);    // to group widgets for QWhatsThis text

    auto boxHLayout = new QHBoxLayout(box);
    boxHLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(i18nc("@label:listbox", "Display type:"), box);
    boxHLayout->addWidget(label);
    mTypeCombo = new ComboBox(box);
    boxHLayout->addWidget(mTypeCombo);
    const QString textItem    = i18nc("@item:inlistbox", "Text message");
    const QString fileItem    = i18nc("@item:inlistbox", "File contents");
    const QString commandItem = i18nc("@item:inlistbox", "Command output");
    mTypeCombo->addItem(textItem);     // index = tTEXT
    mTypeCombo->addItem(fileItem);     // index = tFILE
    mTypeCombo->addItem(commandItem);  // index = tCOMMAND
    mTypeCombo->setCurrentIndex(-1);    // ensure slotAlarmTypeChanged() is called when index is set
    if (!ShellProcess::authorised())
    {
        // User not authorised to issue shell commands - disable Command Output option
        auto model = qobject_cast<QStandardItemModel*>(mTypeCombo->model());
        if (model)
        {
            QModelIndex index = model->index(2, mTypeCombo->modelColumn(), mTypeCombo->rootModelIndex());
            QStandardItem* item = model->itemFromIndex(index);
            if (item)
                item->setEnabled(false);
        }
    }
    connect(mTypeCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &EditDisplayAlarmDlg::slotAlarmTypeChanged);
    connect(mTypeCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &EditDisplayAlarmDlg::contentsChanged);
    label->setBuddy(mTypeCombo);
    box->setWhatsThis(xi18nc("@info:whatsthis", "<para>Select what the alarm should display:"
                             "<list><item><interface>%1</interface>: the alarm will display the text message you type in.</item>"
                             "<item><interface>%2</interface>: the alarm will display the contents of a text or image file.</item>"
                             "<item><interface>%3</interface>: the alarm will display the output from a command.</item></list></para>",
                             textItem, fileItem, commandItem));
    auto hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addLayout(hlayout);
    hlayout->addWidget(box);
    hlayout->addStretch();    // left adjust the control

    // Text message edit box
    mTextMessageEdit = new TextEdit(parent);
    mTextMessageEdit->setLineWrapMode(KTextEdit::NoWrap);
    mTextMessageEdit->enableEmailDrop();         // allow drag-and-drop of emails onto this widget
    mTextMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the text of the alarm message. It may be multi-line."));
    connect(mTextMessageEdit, &TextEdit::textChanged, this, &EditDisplayAlarmDlg::contentsChanged);
    frameLayout->addWidget(mTextMessageEdit);

    // File name edit box
    mFileBox = new QWidget(parent);
    frameLayout->addWidget(mFileBox);
    auto fileBoxHLayout = new QHBoxLayout(mFileBox);
    fileBoxHLayout->setContentsMargins(0, 0, 0, 0);
    fileBoxHLayout->setSpacing(0);
    mFileMessageEdit = new LineEdit(LineEdit::Type::Url, mFileBox);
    fileBoxHLayout->addWidget(mFileMessageEdit);
    mFileMessageEdit->setAcceptDrops(true);
    mFileMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or URL of a text or image file to display."));
    connect(mFileMessageEdit, &LineEdit::textChanged, this, &EditDisplayAlarmDlg::contentsChanged);

    // File browse button
    mFileBrowseButton = new QPushButton(mFileBox);
    fileBoxHLayout->addWidget(mFileBrowseButton);
    mFileBrowseButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    mFileBrowseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
    mFileBrowseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a text or image file to display."));
    connect(mFileBrowseButton, &QPushButton::clicked, this, &EditDisplayAlarmDlg::slotPickFile);

    // Command type checkbox and edit box
    mCmdEdit = new CommandEdit(parent);
    connect(mCmdEdit, &CommandEdit::scriptToggled, this, &EditDisplayAlarmDlg::slotCmdScriptToggled);
    connect(mCmdEdit, &CommandEdit::changed, this, &EditDisplayAlarmDlg::contentsChanged);
    frameLayout->addWidget(mCmdEdit);

    // Sound checkbox and file selector
    hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addLayout(hlayout);
    mSoundPicker = new SoundPicker(parent);
    connect(mSoundPicker, &SoundPicker::changed, this, &EditDisplayAlarmDlg::contentsChanged);
    hlayout->addWidget(mSoundPicker);
    hlayout->addSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hlayout->addStretch();

    // Font and colour choice button and sample text
    mFontColourButton = new FontColourButton(parent);
    mFontColourButton->setMaximumHeight(mFontColourButton->sizeHint().height() * 3/2);
    hlayout->addWidget(mFontColourButton);
    connect(mFontColourButton, &FontColourButton::selected, this, &EditDisplayAlarmDlg::setColours);
    connect(mFontColourButton, &FontColourButton::selected, this, &EditDisplayAlarmDlg::contentsChanged);

    // Display method selector
    hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addLayout(hlayout);
    mDisplayMethodBox = new QWidget(parent);    // to group widgets for QWhatsThis text
    boxHLayout = new QHBoxLayout(mDisplayMethodBox);
    boxHLayout->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18n_lbl_DisplayMethod(), mDisplayMethodBox);
    boxHLayout->addWidget(label);
    mDisplayMethodCombo = new ComboBox(mDisplayMethodBox);
    boxHLayout->addWidget(mDisplayMethodCombo);
    const QString windowItem = i18n_combo_Window();
    const QString notifyItem = i18n_combo_Notify();
    mDisplayMethodCombo->addItem(windowItem);     // index = dWINDOW
    mDisplayMethodCombo->addItem(notifyItem);     // index = dNOTIFY
    connect(mDisplayMethodCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &EditDisplayAlarmDlg::slotDisplayMethodChanged);
    connect(mDisplayMethodCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &EditDisplayAlarmDlg::contentsChanged);
    label->setBuddy(mDisplayMethodCombo);
    mDisplayMethodBox->setWhatsThis(i18nc("@info:whatsthis", "Select whether to display the alarm in a window or by the notification system."));
    hlayout->addWidget(mDisplayMethodBox);
    hlayout->addSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hlayout->addStretch();

    if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
    {
        // Special actions button
        mSpecialActionsButton = new SpecialActionsButton(false, parent);
        connect(mSpecialActionsButton, &SpecialActionsButton::selected, this, &EditDisplayAlarmDlg::contentsChanged);
        hlayout->addWidget(mSpecialActionsButton);
    }

    // Top-adjust the controls
    mFilePadding = new QWidget(parent);
    hlayout = new QHBoxLayout(mFilePadding);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(0);
    frameLayout->addWidget(mFilePadding);
    frameLayout->setStretchFactor(mFilePadding, 1);
}

/******************************************************************************
* Create a reminder control.
*/
Reminder* EditDisplayAlarmDlg::createReminder(QWidget* parent)
{
    return new Reminder(i18nc("@info:whatsthis", "Check to additionally display a reminder in advance of or after the main alarm time(s)."),
                        xi18nc("@info:whatsthis", "<para>Enter how long in advance of or after the main alarm to display a reminder alarm.</para><para>%1</para>", TimeSpinBox::shiftWhatsThis()),
                        i18nc("@info:whatsthis", "Select whether the reminder should be triggered before or after the main alarm"),
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
void EditDisplayAlarmDlg::type_initValues(const KAEvent& event)
{
    mTryButton->setToolTip(i18nc("@info:tooltip", "Display the alarm now"));
    mEmailId = -1;
    lateCancel()->showAutoClose(true);
    if (event.isValid())
    {
        if (mAlarmType == KAEvent::SubAction::Message  &&  event.emailId()
        &&  AlarmText::checkIfEmail(event.cleanText()))
            mEmailId = event.emailId();
        lateCancel()->setAutoClose(event.autoClose());
        mFontColourButton->setFont(event.font(), event.useDefaultFont());
        mFontColourButton->setBgColour(event.bgColour());
        mFontColourButton->setFgColour(event.fgColour());
        setColours(event.fgColour(), event.bgColour());
        mDisplayMethodCombo->setCurrentIndex(event.notify() ? dNOTIFY : dWINDOW);
        mConfirmAck->setChecked(event.confirmAck());
        const bool recurs = event.recurs();
        int reminderMins = event.reminderMinutes();
        if (reminderMins > 0  &&  !event.reminderActive())
            reminderMins = 0;   // don't show advance reminder which has already passed
        if (!reminderMins)
        {
            if (event.reminderDeferral()  &&  !recurs)
            {
                reminderMins = event.deferDateTime().minsTo(event.mainDateTime());
                mReminderDeferral = true;
            }
            else if (event.reminderMinutes()  &&  recurs)
            {
                reminderMins = event.reminderMinutes();
                mReminderArchived = true;
            }
        }
        reminder()->setMinutes(reminderMins, dateOnly());
        reminder()->setOnceOnly(event.reminderOnceOnly());
        reminder()->enableOnceOnly(recurs);
        if (mSpecialActionsButton)
            mSpecialActionsButton->setActions(event.preAction(), event.postAction(), event.extraActionOptions());
        Preferences::SoundType soundType = event.speak()                ? Preferences::Sound_Speak
                                         : event.beep()                 ? Preferences::Sound_Beep
                                         : !event.audioFile().isEmpty() ? Preferences::Sound_File
                                         :                                Preferences::Sound_None;
        mSoundPicker->set(soundType, event.audioFile(), event.soundVolume(),
                          event.fadeVolume(), event.fadeSeconds(), event.repeatSoundPause());
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
        mFontColourButton->setFont(Preferences::messageFont(), true);
        mFontColourButton->setBgColour(Preferences::defaultBgColour());
        mFontColourButton->setFgColour(Preferences::defaultFgColour());
        setColours(Preferences::defaultFgColour(), Preferences::defaultBgColour());
        mDisplayMethodCombo->setCurrentIndex(Preferences::defaultDisplayMethod() == Preferences::Display_Window ? dWINDOW : dNOTIFY);
        mConfirmAck->setChecked(Preferences::defaultConfirmAck());
        reminder()->setMinutes(0, false);
        reminder()->enableOnceOnly(isTimedRecurrence());   // must be called after mRecurrenceEdit is set up
        if (mSpecialActionsButton)
        {
            KAEvent::ExtraActionOptions opts({});
            if (Preferences::defaultExecPreActionOnDeferral())
                opts |= KAEvent::ExecPreActOnDeferral;
            if (Preferences::defaultCancelOnPreActionError())
                opts |= KAEvent::CancelOnPreActError;
            if (Preferences::defaultDontShowPreActionError())
                opts |= KAEvent::DontShowPreActError;
            mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction(), opts);
        }
        mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
                          Preferences::defaultSoundVolume(), -1, 0, (Preferences::defaultSoundRepeat() ? 0 : -1));
    }
    slotDisplayMethodChanged(mDisplayMethodCombo->currentIndex());
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
            mDisplayMethodBox->show();
            mSpecialActionsButton->show();
        }
        else
        {
            mDisplayMethodBox->hide();
            mSpecialActionsButton->hide();
        }
    }
}

/******************************************************************************
* Called when the font/color button has been clicked.
* Set the colors in the message text entry control.
*/
void EditDisplayAlarmDlg::setColours(const QColor& fgColour, const QColor& bgColour)
{
    QColor fg(fgColour);
    if (mDisplayMethodCombo->currentIndex() == dNOTIFY)
    {
        const QPalette pal = mFileMessageEdit->palette();
        mTextMessageEdit->setPalette(pal);
        mTextMessageEdit->viewport()->setPalette(pal);
        fg = pal.color(QPalette::Text);
    }
    else
    {
        QPalette pal = mTextMessageEdit->palette();
        pal.setColor(mTextMessageEdit->backgroundRole(), bgColour);
        pal.setColor(QPalette::Text, fgColour);
        mTextMessageEdit->setPalette(pal);
        pal = mTextMessageEdit->viewport()->palette();
        pal.setColor(mTextMessageEdit->viewport()->backgroundRole(), bgColour);
        pal.setColor(QPalette::Text, fgColour);
        mTextMessageEdit->viewport()->setPalette(pal);
    }
    // Change the color of existing text
    const QTextCursor cursor = mTextMessageEdit->textCursor();
    mTextMessageEdit->selectAll();
    mTextMessageEdit->setTextColor(fg);
    mTextMessageEdit->setTextCursor(cursor);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditDisplayAlarmDlg::setAction(KAEvent::SubAction action, const AlarmText& alarmText)
{
    const QString text = alarmText.displayText();
    switch (action)
    {
        case KAEvent::SubAction::Message:
            mTypeCombo->setCurrentIndex(tTEXT);
            mTextMessageEdit->setPlainText(text);
            mEmailId = alarmText.isEmail() ? alarmText.emailId() : -1;
            break;
        case KAEvent::SubAction::File:
            mTypeCombo->setCurrentIndex(tFILE);
            mFileMessageEdit->setText(text);
            break;
        case KAEvent::SubAction::Command:
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
void EditDisplayAlarmDlg::setNotify(bool notify)
{
    mDisplayMethodCombo->setCurrentIndex(notify ? dNOTIFY : dWINDOW);
}
void EditDisplayAlarmDlg::setConfirmAck(bool confirm)
{
    mConfirmAck->setChecked(confirm);
}
void EditDisplayAlarmDlg::setAutoClose(bool close)
{
    lateCancel()->setAutoClose(close);
}
void EditDisplayAlarmDlg::setAudio(Preferences::SoundType type, const QString& file, float volume, int repeatPause)
{
    mSoundPicker->set(type, file, volume, -1, 0, repeatPause);
}
void EditDisplayAlarmDlg::setReminder(int minutes, bool onceOnly)
{
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
    mDisplayMethodCombo->setReadOnly(readOnly);
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
    mSavedType          = mTypeCombo->currentIndex();
    mSavedCmdScript     = mCmdEdit->isScript();
    mSavedSoundType     = mSoundPicker->sound();
    mSavedSoundFile     = mSoundPicker->file();
    mSavedSoundVolume   = mSoundPicker->volume(mSavedSoundFadeVolume, mSavedSoundFadeSeconds);
    mSavedRepeatPause   = mSoundPicker->repeatPause();
    mSavedDisplayMethod = mDisplayMethodCombo->currentIndex();
    mSavedConfirmAck    = mConfirmAck->isChecked();
    mSavedFont          = mFontColourButton->font();
    mSavedFgColour      = mFontColourButton->fgColour();
    mSavedBgColour      = mFontColourButton->bgColour();
    mSavedReminder      = reminder()->minutes();
    mSavedOnceOnly      = reminder()->isOnceOnly();
    mSavedAutoClose     = lateCancel()->isAutoClose();
    if (mSpecialActionsButton)
    {
        mSavedPreAction        = mSpecialActionsButton->preAction();
        mSavedPostAction       = mSpecialActionsButton->postAction();
        mSavedPreActionOptions = mSpecialActionsButton->options();
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
    if (mSavedType          != mTypeCombo->currentIndex()
    ||  mSavedCmdScript     != mCmdEdit->isScript()
    ||  mSavedSoundType     != mSoundPicker->sound()
    ||  mSavedDisplayMethod != mDisplayMethodCombo->currentIndex()
    ||  mSavedReminder      != reminder()->minutes()
    ||  mSavedOnceOnly      != reminder()->isOnceOnly())
        return true;
    if (mDisplayMethodCombo->currentIndex() == dWINDOW)
    {
        if (mSavedConfirmAck != mConfirmAck->isChecked()
        ||  mSavedFont       != mFontColourButton->font()
        ||  mSavedFgColour   != mFontColourButton->fgColour()
        ||  mSavedBgColour   != mFontColourButton->bgColour()
        ||  mSavedAutoClose  != lateCancel()->isAutoClose())
            return true;
    }
    if (mSpecialActionsButton)
    {
        if (mSavedPreAction        != mSpecialActionsButton->preAction()
        ||  mSavedPostAction       != mSpecialActionsButton->postAction()
        ||  mSavedPreActionOptions != mSpecialActionsButton->options())
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
            if (mSavedRepeatPause != mSoundPicker->repeatPause()
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
void EditDisplayAlarmDlg::type_setEvent(KAEvent& event, const KADateTime& dt, const QString& name, const QString& text, int lateCancel, bool trial)
{
    KAEvent::SubAction type;
    switch (mTypeCombo->currentIndex())
    {
        case tFILE:     type = KAEvent::SubAction::File; break;
        case tCOMMAND:  type = KAEvent::SubAction::Command; break;
        default:
        case tTEXT:     type = KAEvent::SubAction::Message; break;
    }
    QColor fgColour, bgColour;
    QFont font;
    if (mDisplayMethodCombo->currentIndex() == dNOTIFY)
    {
        bgColour = Preferences::defaultBgColour();
        fgColour = Preferences::defaultFgColour();
    }
    else
    {
        bgColour = mFontColourButton->bgColour();
        fgColour = mFontColourButton->fgColour();
        font     = mFontColourButton->font();
    }
    event = KAEvent(dt, name, text, bgColour, fgColour, font, type, lateCancel, getAlarmFlags());
    if (type == KAEvent::SubAction::Message)
    {
        if (AlarmText::checkIfEmail(text))
            event.setEmailId(mEmailId);
    }
    float fadeVolume;
    int   fadeSecs;
    const float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
    const int   repeatPause = mSoundPicker->repeatPause();
    event.setAudioFile(mSoundPicker->file().toDisplayString(), volume, fadeVolume, fadeSecs, repeatPause);
    if (!trial  &&  reminder()->isEnabled())
        event.setReminder(reminder()->minutes(), reminder()->isOnceOnly());
    if (mSpecialActionsButton  &&  mSpecialActionsButton->isEnabled())
        event.setActions(mSpecialActionsButton->preAction(), mSpecialActionsButton->postAction(),
                         mSpecialActionsButton->options());
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditDisplayAlarmDlg::getAlarmFlags() const
{
    const bool cmd = (mTypeCombo->currentIndex() == tCOMMAND);
    KAEvent::Flags flags = EditAlarmDlg::getAlarmFlags();
    if (mSoundPicker->sound() == Preferences::Sound_Beep)  flags |= KAEvent::BEEP;
    if (mSoundPicker->sound() == Preferences::Sound_Speak) flags |= KAEvent::SPEAK;
    if (mSoundPicker->repeatPause() >= 0)                  flags |= KAEvent::REPEAT_SOUND;
    if (cmd)                                               flags |= KAEvent::DISPLAY_COMMAND;
    if (cmd && mCmdEdit->isScript())                       flags |= KAEvent::SCRIPT;
    if (mDisplayMethodCombo->currentIndex() == dNOTIFY)
    {
                                                           flags |= KAEvent::NOTIFY;
                                                           flags |= KAEvent::DEFAULT_FONT;
    }
    else
    {
        if (mFontColourButton->defaultFont())              flags |= KAEvent::DEFAULT_FONT;
        if (mConfirmAck->isChecked())                      flags |= KAEvent::CONFIRM_ACK;
        if (lateCancel()->isAutoClose())                   flags |= KAEvent::AUTO_CLOSE;
    }
    return flags;
}

/******************************************************************************
* Called when the alarm display type combo box is changed, to display the
* appropriate set of controls for that action type.
*/
void EditDisplayAlarmDlg::slotAlarmTypeChanged(int index)
{
    QWidget* focus = nullptr;
    switch (index)
    {
        case tTEXT:    // text message
            mFileBox->hide();
            mFilePadding->hide();
            mCmdEdit->hide();
            mTextMessageEdit->show();
            mSoundPicker->showSpeak(true);
            mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Display the alarm message now"));
            focus = mTextMessageEdit;
            break;
        case tFILE:    // file contents
            mTextMessageEdit->hide();
            mFileBox->show();
            mFilePadding->show();
            mCmdEdit->hide();
            mSoundPicker->showSpeak(false);
            mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Display the file now"));
            mFileMessageEdit->setNoSelect();
            focus = mFileMessageEdit;
            break;
        case tCOMMAND:    // command output
            mTextMessageEdit->hide();
            mFileBox->hide();
            slotCmdScriptToggled(mCmdEdit->isScript());  // show/hide mFilePadding
            mCmdEdit->show();
            mSoundPicker->showSpeak(true);
            mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Display the command output now"));
            focus = mCmdEdit;
            break;
    }
    if (focus)
        focus->setFocus();
}

/******************************************************************************
* Called when the display method combo box is changed, to enable/disable the
* appropriate set of controls for that display method.
*/
void EditDisplayAlarmDlg::slotDisplayMethodChanged(int index)
{
    const bool enable = (index == dWINDOW);
    mConfirmAck->setVisible(enable);
    mFontColourButton->setVisible(enable);
    mSoundPicker->showFile(enable);
    // Because notifications automatically time out after 10 seconds,
    // auto-close would always occur after a notification closes.
    lateCancel()->showAutoClose(enable);
    // Set the text message edit box colours according to the display method.
    setColours(mFontColourButton->fgColour(), mFontColourButton->bgColour());
}

/******************************************************************************
* Called when the file browse button is pressed to select a file to display.
*/
void EditDisplayAlarmDlg::slotPickFile()
{
    static QString defaultDir;   // default directory for file browse button
    QString file;
    if (File::browseFile(file, i18nc("@title:window", "Choose Text or Image File to Display"),
                         defaultDir, mFileMessageEdit->text(), true, this))
    {
        if (!file.isEmpty())
        {
            mFileMessageEdit->setText(File::pathOrUrl(file));
            contentsChanged();
        }
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
            QString fileName = mFileMessageEdit->text().trimmed();
            QUrl url;
            File::Error err = File::checkFileExists(fileName, url, MainWindow::mainMainWindow());
            if (err == File::Error::None)
            {
                KFileItem fi(url);
                switch (File::fileType(fi.currentMimeType()))
                {
                    case File::Type::TextFormatted:
                    case File::Type::TextPlain:
                    case File::Type::TextApplication:
                    case File::Type::Image:
                        break;
                    default:
                        err = File::Error::NotTextImage;
                        break;
                }
            }
            if (err != File::Error::None  &&  showErrorMessage)
            {
                mFileMessageEdit->setFocus();
                if (!File::showFileErrMessage(fileName, err, File::Error::BlankDisplay, const_cast<EditDisplayAlarmDlg*>(this)))
                    return false;
            }
            result = fileName;
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
EditCommandAlarmDlg::EditCommandAlarmDlg(bool Template, QWidget* parent, GetResourceType getResource)
    : EditAlarmDlg(Template, KAEvent::SubAction::Command, parent, getResource)
{
    qCDebug(KALARM_LOG) << "EditCommandAlarmDlg: New";
    init(KAEvent());
}

EditCommandAlarmDlg::EditCommandAlarmDlg(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                         GetResourceType getResource, bool readOnly)
    : EditAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly)
{
    qCDebug(KALARM_LOG) << "EditCommandAlarmDlg: Event.id()";
    init(event);
}

/******************************************************************************
* Return the window caption.
*/
QString EditCommandAlarmDlg::type_caption() const
{
    return isTemplate() ? (isNewAlarm() ? i18nc("@title:window", "New Command Alarm Template") : i18nc("@title:window", "Edit Command Alarm Template"))
                        : (isNewAlarm() ? i18nc("@title:window", "New Command Alarm") : i18nc("@title:window", "Edit Command Alarm"));
}

/******************************************************************************
* Set up the command alarm dialog controls.
*/
void EditCommandAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
    mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Execute the specified command now"));
    mTryButton->setToolTip(i18nc("@info:tooltip", "Execute the specified command now"));

    mCmdEdit = new CommandEdit(parent);
    connect(mCmdEdit, &CommandEdit::scriptToggled, this, &EditCommandAlarmDlg::slotCmdScriptToggled);
    connect(mCmdEdit, &CommandEdit::changed, this, &EditCommandAlarmDlg::contentsChanged);
    frameLayout->addWidget(mCmdEdit);

    mCmdDontShowError = new CheckBox(i18nc("@option:check", "Do not notify errors"), parent);
    mCmdDontShowError->setWhatsThis(i18nc("@info:whatsthis", "Do not show error message if the command fails."));
    frameLayout->addWidget(mCmdDontShowError, 0, Qt::AlignLeft);
    connect(mCmdDontShowError, &CheckBox::toggled, this, &EditCommandAlarmDlg::contentsChanged);

    // What to do with command output

    mCmdOutputBox = new QGroupBox(i18nc("@title:group", "Command Output"), parent);
    frameLayout->addWidget(mCmdOutputBox);
    auto vlayout = new QVBoxLayout(mCmdOutputBox);
    mCmdOutputGroup = new ButtonGroup(mCmdOutputBox);
    connect(mCmdOutputGroup, &ButtonGroup::buttonSet, this, &EditCommandAlarmDlg::contentsChanged);

    // Execute in terminal window
    mCmdExecInTerm = new RadioButton(i18n_radio_ExecInTermWindow(), mCmdOutputBox);
    mCmdExecInTerm->setWhatsThis(i18nc("@info:whatsthis", "Check to execute the command in a terminal window"));
    mCmdOutputGroup->addButton(mCmdExecInTerm, Preferences::Log_Terminal);
    vlayout->addWidget(mCmdExecInTerm, 0, Qt::AlignLeft);

    // Log file name edit box
    QWidget* box = new QWidget(mCmdOutputBox);
    auto boxHLayout = new QHBoxLayout(box);
    boxHLayout->setContentsMargins(0, 0, 0, 0);
    boxHLayout->setSpacing(0);
    (new QWidget(box))->setFixedWidth(mCmdExecInTerm->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth));   // indent the edit box
    mCmdLogFileEdit = new LineEdit(LineEdit::Type::Url, box);
    boxHLayout->addWidget(mCmdLogFileEdit);
    mCmdLogFileEdit->setAcceptDrops(true);
    mCmdLogFileEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or path of the log file."));
    connect(mCmdLogFileEdit, &LineEdit::textChanged, this, &EditCommandAlarmDlg::contentsChanged);

    // Log file browse button.
    // The file browser dialog is activated by the PickLogFileRadio class.
    auto browseButton = new QPushButton(box);
    boxHLayout->addWidget(browseButton);
    browseButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    browseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
    browseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a log file."));

    // Log output to file
    mCmdLogToFile = new PickLogFileRadio(browseButton, mCmdLogFileEdit, i18nc("@option:radio", "Log to file"), mCmdOutputGroup, mCmdOutputBox);
    mCmdLogToFile->setWhatsThis(i18nc("@info:whatsthis", "Check to log the command output to a local file. The output will be appended to any existing contents of the file."));
    connect(mCmdLogToFile, &PickLogFileRadio::fileChanged, this, &EditCommandAlarmDlg::contentsChanged);
    mCmdOutputGroup->addButton(mCmdLogToFile, Preferences::Log_File);
    vlayout->addWidget(mCmdLogToFile, 0, Qt::AlignLeft);
    vlayout->addWidget(box);

    // Discard output
    mCmdDiscardOutput = new RadioButton(i18nc("@option:radio", "Discard"), mCmdOutputBox);
    mCmdDiscardOutput->setWhatsThis(i18nc("@info:whatsthis", "Check to discard command output."));
    mCmdOutputGroup->addButton(mCmdDiscardOutput, Preferences::Log_Discard);
    vlayout->addWidget(mCmdDiscardOutput, 0, Qt::AlignLeft);

    // Top-adjust the controls
    mCmdPadding = new QWidget(parent);
    auto hlayout = new QHBoxLayout(mCmdPadding);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(0);
    frameLayout->addWidget(mCmdPadding);
    frameLayout->setStretchFactor(mCmdPadding, 1);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditCommandAlarmDlg::type_initValues(const KAEvent& event)
{
    if (event.isValid())
    {
        // Set the values to those for the specified event
        RadioButton* logType = event.commandXterm()       ? mCmdExecInTerm
                             : !event.logFile().isEmpty() ? mCmdLogToFile
                             :                               mCmdDiscardOutput;
        if (logType == mCmdLogToFile)
            mCmdLogFileEdit->setText(event.logFile());    // set file name before setting radio button
        logType->setChecked(true);
        mCmdDontShowError->setChecked(event.commandHideError());
    }
    else
    {
        // Set the values to their defaults
        mCmdEdit->setScript(Preferences::defaultCmdScript());
        mCmdLogFileEdit->setText(Preferences::defaultCmdLogFile());    // set file name before setting radio button
        mCmdOutputGroup->setButton(Preferences::defaultCmdLogType());
        mCmdDontShowError->setChecked(false);
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
void EditCommandAlarmDlg::setAction(KAEvent::SubAction action, const AlarmText& alarmText)
{
    Q_UNUSED(action);
    Q_ASSERT(action == KAEvent::SubAction::Command);
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
    mCmdDontShowError->setReadOnly(readOnly);
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
    mSavedCmdScript        = mCmdEdit->isScript();
    mSavedCmdDontShowError = mCmdDontShowError->isChecked();
    mSavedCmdOutputRadio   = mCmdOutputGroup->checkedButton();
    mSavedCmdLogFile       = mCmdLogFileEdit->text();
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any controls have changed, or if it's a new event.
*       = false if no controls have changed.
*/
bool EditCommandAlarmDlg::type_stateChanged() const
{
    if (mSavedCmdScript        != mCmdEdit->isScript()
    ||  mSavedCmdOutputRadio   != mCmdOutputGroup->checkedButton()
    ||  mSavedCmdDontShowError != mCmdDontShowError->isChecked())
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
void EditCommandAlarmDlg::type_setEvent(KAEvent& event, const KADateTime& dt, const QString& name, const QString& text, int lateCancel, bool trial)
{
    Q_UNUSED(trial);
    event = KAEvent(dt, name, text, QColor(), QColor(), QFont(), KAEvent::SubAction::Command, lateCancel, getAlarmFlags());
    if (mCmdOutputGroup->checkedButton() == mCmdLogToFile)
        event.setLogFile(mCmdLogFileEdit->text());
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditCommandAlarmDlg::getAlarmFlags() const
{
    KAEvent::Flags flags = EditAlarmDlg::getAlarmFlags();
    if (mCmdEdit->isScript())                               flags |= KAEvent::SCRIPT;
    if (mCmdOutputGroup->checkedButton() == mCmdExecInTerm) flags |= KAEvent::EXEC_IN_XTERM;
    if (mCmdDontShowError->isChecked())                     flags |= KAEvent::DONT_SHOW_ERROR;
    return flags;
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
        const QString file = mCmdLogFileEdit->text();
        const QFileInfo info(file);
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
                const QFileInfo dirinfo(info.absolutePath());    // get absolute directory path
                err = (!dirinfo.isDir()  ||  !dirinfo.isWritable());
            }
        }
        if (err)
        {
            showMainPage();
            mCmdLogFileEdit->setFocus();
            KAMessageBox::error(this, i18nc("@info", "Log file must be the name or path of a local file, with write permission."));
            return false;
        }
        // Convert the log file to an absolute path
        mCmdLogFileEdit->setText(info.absoluteFilePath());
    }
    else if (mCmdOutputGroup->checkedButton() == mCmdExecInTerm)
    {
        if (Preferences::cmdXTermCommand().isEmpty())
        {
            if (KAMessageBox::warningContinueCancel(this, xi18nc("@info", "<para>No terminal is selected for command alarms.</para>"
                                                                 "<para>Please set it in the <application>KAlarm</application> Configuration dialog.</para>"))
                    != KMessageBox::Continue)
                return false;
        }
    }
    return true;
}

/******************************************************************************
* Called when the Try action has been executed.
* Tell the user the result of the Try action.
*/
void EditCommandAlarmDlg::type_executedTry(const QString& text, void* result)
{
    auto* proc = (ShellProcess*)result;
    if (proc  &&  proc != (void*)-1
    &&  mCmdOutputGroup->checkedButton() != mCmdExecInTerm)
    {
        theApp()->commandMessage(proc, this);
        KAMessageBox::information(this, xi18nc("@info", "Command executed: <icode>%1</icode>", text));
        theApp()->commandMessage(proc, nullptr);
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
EditEmailAlarmDlg::EditEmailAlarmDlg(bool Template, QWidget* parent, GetResourceType getResource)
    : EditAlarmDlg(Template, KAEvent::SubAction::Email, parent, getResource)
{
    qCDebug(KALARM_LOG) << "EditEmailAlarmDlg: New";
    init(KAEvent());
}

EditEmailAlarmDlg::EditEmailAlarmDlg(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                     GetResourceType getResource, bool readOnly)
    : EditAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly)
{
    qCDebug(KALARM_LOG) << "EditEmailAlarmDlg: Event.id()";
    init(event);
}

EditEmailAlarmDlg::~EditEmailAlarmDlg()
{
    // Delete KTextEdit while parent exists to prevent crash if spell checking is enabled.
    delete mEmailMessageEdit;
    mEmailMessageEdit = nullptr;
}

/******************************************************************************
* Return the window caption.
*/
QString EditEmailAlarmDlg::type_caption() const
{
    return isTemplate() ? (isNewAlarm() ? i18nc("@title:window", "New Email Alarm Template") : i18nc("@title:window", "Edit Email Alarm Template"))
                        : (isNewAlarm() ? i18nc("@title:window", "New Email Alarm") : i18nc("@title:window", "Edit Email Alarm"));
}

/******************************************************************************
* Set up the email alarm dialog controls.
*/
void EditEmailAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
    mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Send the email to the specified addressees now"));
    mTryButton->setToolTip(i18nc("@info:tooltip", "Send the email now"));

    auto grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    frameLayout->addLayout(grid);

    mEmailFromList = nullptr;
    if (Preferences::emailFrom() == Preferences::MAIL_FROM_KMAIL)
    {
        // Email sender identity
        QLabel* label = new QLabel(i18nc("@label:listbox 'From' email address", "From:"), parent);
        grid->addWidget(label, 0, 0);

        mEmailFromList = new EmailIdCombo(Identities::identityManager(), parent);
        mEmailFromList->setMinimumSize(mEmailFromList->sizeHint());
        label->setBuddy(mEmailFromList);
        mEmailFromList->setWhatsThis(i18nc("@info:whatsthis", "Your email identity, used to identify you as the sender when sending email alarms."));
        connect(mEmailFromList, &EmailIdCombo::identityChanged, this, &EditEmailAlarmDlg::contentsChanged);
        grid->addWidget(mEmailFromList, 0, 1, 1, 2);
    }

    // Email recipients
    QLabel* label = new QLabel(i18nc("@label:textbox Email addressee", "To:"), parent);
    grid->addWidget(label, 1, 0);

    mEmailToEdit = new LineEdit(LineEdit::Type::Emails, parent);
    mEmailToEdit->setMinimumSize(mEmailToEdit->sizeHint());
    mEmailToEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the addresses of the email recipients. Separate multiple addresses by "
                                    "commas or semicolons."));
    connect(mEmailToEdit, &LineEdit::textChanged, this, &EditEmailAlarmDlg::contentsChanged);

    if (Preferences::useAkonadi())
    {
        grid->addWidget(mEmailToEdit, 1, 1);

        mEmailAddressButton = new QPushButton(parent);
        mEmailAddressButton->setIcon(QIcon::fromTheme(QStringLiteral("help-contents")));
        connect(mEmailAddressButton, &QPushButton::clicked, this, &EditEmailAlarmDlg::openAddressBook);
        mEmailAddressButton->setToolTip(i18nc("@info:tooltip", "Open address book"));
        mEmailAddressButton->setWhatsThis(i18nc("@info:whatsthis", "Select email addresses from your address book."));
        grid->addWidget(mEmailAddressButton, 1, 2);
    }
    else
        grid->addWidget(mEmailToEdit, 1, 1, 1, 2);

    // Email subject
    label = new QLabel(i18nc("@label:textbox Email subject", "Subject:"), parent);
    grid->addWidget(label, 2, 0);

    mEmailSubjectEdit = new LineEdit(parent);
    mEmailSubjectEdit->setMinimumSize(mEmailSubjectEdit->sizeHint());
    label->setBuddy(mEmailSubjectEdit);
    mEmailSubjectEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the email subject."));
    connect(mEmailSubjectEdit, &LineEdit::textChanged, this, &EditEmailAlarmDlg::contentsChanged);
    grid->addWidget(mEmailSubjectEdit, 2, 1, 1, 2);

    // Email body
    mEmailMessageEdit = new TextEdit(parent);
    mEmailMessageEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the email message."));
    connect(mEmailMessageEdit, &TextEdit::textChanged, this, &EditEmailAlarmDlg::contentsChanged);
    frameLayout->addWidget(mEmailMessageEdit);

    // Email attachments
    grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    frameLayout->addLayout(grid);
    label = new QLabel(i18nc("@label:listbox", "Attachments:"), parent);
    grid->addWidget(label, 0, 0);

    mEmailAttachList = new QComboBox(parent);
    mEmailAttachList->setEditable(true);
    mEmailAttachList->setMinimumSize(mEmailAttachList->sizeHint());
    if (mEmailAttachList->lineEdit())
        mEmailAttachList->lineEdit()->setReadOnly(true);
    label->setBuddy(mEmailAttachList);
    mEmailAttachList->setWhatsThis(i18nc("@info:whatsthis", "Files to send as attachments to the email."));
    grid->addWidget(mEmailAttachList, 0, 1);
    grid->setColumnStretch(1, 1);

    mEmailAddAttachButton = new QPushButton(i18nc("@action:button", "Add..."), parent);
    connect(mEmailAddAttachButton, &QPushButton::clicked, this, &EditEmailAlarmDlg::slotAddAttachment);
    mEmailAddAttachButton->setWhatsThis(i18nc("@info:whatsthis", "Add an attachment to the email."));
    grid->addWidget(mEmailAddAttachButton, 0, 2);

    mEmailRemoveButton = new QPushButton(i18nc("@action:button", "Remove"), parent);
    connect(mEmailRemoveButton, &QPushButton::clicked, this, &EditEmailAlarmDlg::slotRemoveAttachment);
    mEmailRemoveButton->setWhatsThis(i18nc("@info:whatsthis", "Remove the highlighted attachment from the email."));
    grid->addWidget(mEmailRemoveButton, 1, 2);

    // BCC email to sender
    mEmailBcc = new CheckBox(i18n_chk_CopyEmailToSelf(), parent);
    mEmailBcc->setWhatsThis(i18nc("@info:whatsthis", "If checked, the email will be blind copied to you."));
    connect(mEmailBcc, &CheckBox::toggled, this, &EditEmailAlarmDlg::contentsChanged);
    grid->addWidget(mEmailBcc, 1, 0, 1, 2, Qt::AlignLeft);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditEmailAlarmDlg::type_initValues(const KAEvent& event)
{
    if (event.isValid())
    {
        // Set the values to those for the specified event
        mEmailAttachList->addItems(event.emailAttachments());
        mEmailToEdit->setText(event.emailAddresses(QStringLiteral(", ")));
        mEmailSubjectEdit->setText(event.emailSubject());
        mEmailBcc->setChecked(event.emailBcc());
        if (mEmailFromList)
            mEmailFromList->setCurrentIdentity(event.emailFromId());
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
    const bool enable = mEmailAttachList->count();
    mEmailAttachList->setEnabled(enable);
    if (mEmailRemoveButton)
        mEmailRemoveButton->setEnabled(enable);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditEmailAlarmDlg::setAction(KAEvent::SubAction action, const AlarmText& alarmText)
{
    Q_UNUSED(action);
    Q_ASSERT(action == KAEvent::SubAction::Email);
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
void EditEmailAlarmDlg::setEmailFields(uint fromID, const KCalendarCore::Person::List& addresses,
                                       const QString& subject, const QStringList& attachments)
{
    if (fromID && mEmailFromList)
        mEmailFromList->setCurrentIdentity(fromID);
    if (!addresses.isEmpty())
        mEmailToEdit->setText(KAEvent::joinEmailAddresses(addresses, QStringLiteral(", ")));
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
        if (mEmailAddressButton)
            mEmailAddressButton->hide();
        mEmailAddAttachButton->hide();
        mEmailRemoveButton->hide();
    }
    else
    {
        if (mEmailAddressButton)
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
    int count = mEmailAttachList->count();
    QStringList emailAttach;
    emailAttach.reserve(count);
    for (int i = 0;  i < count;  ++i)
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
void EditEmailAlarmDlg::type_setEvent(KAEvent& event, const KADateTime& dt, const QString& name, const QString& text, int lateCancel, bool trial)
{
    Q_UNUSED(trial);
    event = KAEvent(dt, name, text, QColor(), QColor(), QFont(), KAEvent::SubAction::Email, lateCancel, getAlarmFlags());
    const uint from = mEmailFromList ? mEmailFromList->currentIdentity() : 0;
    event.setEmail(from, mEmailAddresses, mEmailSubjectEdit->text(), mEmailAttachments);
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditEmailAlarmDlg::getAlarmFlags() const
{
    KAEvent::Flags flags = EditAlarmDlg::getAlarmFlags();
    if (mEmailBcc->isChecked()) flags |= KAEvent::EMAIL_BCC;
    return flags;
}

/******************************************************************************
* Convert the email addresses to a list, and validate them. Convert the email
* attachments to a list.
*/
bool EditEmailAlarmDlg::type_validate(bool trial)
{
    const QString addrs = mEmailToEdit->text();
    if (addrs.isEmpty())
        mEmailAddresses.clear();
    else
    {
        const QString bad = KAMail::convertAddresses(addrs, mEmailAddresses);
        if (!bad.isEmpty())
        {
            mEmailToEdit->setFocus();
            KAMessageBox::error(this, xi18nc("@info", "Invalid email address: <email>%1</email>", bad));
            return false;
        }
    }
    if (mEmailAddresses.isEmpty())
    {
        mEmailToEdit->setFocus();
        KAMessageBox::error(this, i18nc("@info", "No email address specified"));
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
                KAMessageBox::error(this, xi18nc("@info", "Invalid email attachment: <filename>%1</filename>", att));
                return false;
        }
    }
    if (trial  &&  KAMessageBox::warningContinueCancel(this, i18nc("@info", "Do you really want to send the email now to the specified recipient(s)?"),
                                                       i18nc("@action:button", "Confirm Email"), KGuiItem(i18nc("@action:button", "Send"))) != KMessageBox::Continue)
        return false;
    return true;
}

/******************************************************************************
* Called when the Try action is about to be executed.
*/
void EditEmailAlarmDlg::type_aboutToTry()
{
    // Disconnect any previous connections, to prevent multiple messages being output
    disconnect(theApp(), &KAlarmApp::execAlarmSuccess, this, &EditEmailAlarmDlg::slotTrySuccess);
    connect(theApp(), &KAlarmApp::execAlarmSuccess, this, &EditEmailAlarmDlg::slotTrySuccess);
}

/******************************************************************************
* Tell the user the result of the Try action.
*/
void EditEmailAlarmDlg::slotTrySuccess()
{
    disconnect(theApp(), &KAlarmApp::execAlarmSuccess, this, &EditEmailAlarmDlg::slotTrySuccess);
    QString msg;
    QString to = KAEvent::joinEmailAddresses(mEmailAddresses, QStringLiteral("<nl/>"));
    to.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    to.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    if (mEmailBcc->isChecked())
        msg = QLatin1String("<qt>") + xi18nc("@info", "Email sent to:<nl/>%1<nl/>Bcc: <email>%2</email>",
                    to, Preferences::emailBccAddress()) + QLatin1String("</qt>");
    else
        msg = QLatin1String("<qt>") + xi18nc("@info", "Email sent to:<nl/>%1", to) + QLatin1String("</qt>");
    KAMessageBox::information(this, msg);
}

/******************************************************************************
* Get a selection from the Address Book.
*/
void EditEmailAlarmDlg::openAddressBook()
{
    AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
    if (akonadiPlugin)
    {
        Person person;
        if (!akonadiPlugin->getAddressBookSelection(person, this))
            return;
        QString addrs = mEmailToEdit->text().trimmed();
        if (!addrs.isEmpty())
            addrs += QLatin1String(", ");
        addrs += person.fullName();
        mEmailToEdit->setText(addrs);
    }
}

/******************************************************************************
* Select a file to attach to the email.
*/
void EditEmailAlarmDlg::slotAddAttachment()
{
    QString file;
    if (File::browseFile(file, i18nc("@title:window", "Choose File to Attach"),
                         mAttachDefaultDir, QString(), true, this))
    {
        if (!file.isEmpty())
        {
            mEmailAttachList->addItem(file);
            mEmailAttachList->setCurrentIndex(mEmailAttachList->count() - 1);   // select the new item
            mEmailRemoveButton->setEnabled(true);
            mEmailAttachList->setEnabled(true);
            contentsChanged();
        }
    }
}

/******************************************************************************
* Remove the currently selected attachment from the email.
*/
void EditEmailAlarmDlg::slotRemoveAttachment()
{
    const int item = mEmailAttachList->currentIndex();
    mEmailAttachList->removeItem(item);
    const int count = mEmailAttachList->count();
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
EditAudioAlarmDlg::EditAudioAlarmDlg(bool Template, QWidget* parent, GetResourceType getResource)
    : EditAlarmDlg(Template, KAEvent::SubAction::Audio, parent, getResource)
{
    qCDebug(KALARM_LOG) << "EditAudioAlarmDlg: New";
    init(KAEvent());
}

EditAudioAlarmDlg::EditAudioAlarmDlg(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                     GetResourceType getResource, bool readOnly)
    : EditAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly)
{
    qCDebug(KALARM_LOG) << "EditAudioAlarmDlg: Event.id()";
    init(event);
    mTryButton->setEnabled(!MessageDisplay::isAudioPlaying());
    connect(theApp(), &KAlarmApp::audioPlaying, this, &EditAudioAlarmDlg::slotAudioPlaying);
}

EditAudioAlarmDlg::~EditAudioAlarmDlg()
{
    if (mMessageWindow)
        MessageDisplay::stopAudio();
}

/******************************************************************************
* Return the window caption.
*/
QString EditAudioAlarmDlg::type_caption() const
{
    return isTemplate() ? (isNewAlarm() ? i18nc("@title:window", "New Audio Alarm Template") : i18nc("@title:window", "Edit Audio Alarm Template"))
                        : (isNewAlarm() ? i18nc("@title:window", "New Audio Alarm") : i18nc("@title:window", "Edit Audio Alarm"));
}

/******************************************************************************
* Set up the dialog controls common to display alarms.
*/
void EditAudioAlarmDlg::type_init(QWidget* parent, QVBoxLayout* frameLayout)
{
    mTryButton->setWhatsThis(i18nc("@info:whatsthis", "Play the audio file now"));
    mTryButton->setToolTip(i18nc("@info:tooltip", "Play the audio file now"));
    // File name edit box
    const QString repWhatsThis = i18nc("@info:whatsthis", "If checked, the sound file will be played repeatedly until %1 is clicked.", KAlarm::i18n_act_StopPlay());
    mSoundConfig = new SoundWidget(false, repWhatsThis, parent);
    if (isTemplate())
        mSoundConfig->setAllowEmptyFile();
    connect(mSoundConfig, &SoundWidget::changed, this, &EditAudioAlarmDlg::contentsChanged);
    frameLayout->addWidget(mSoundConfig);

    // Top-adjust the controls
    mPadding = new QWidget(parent);
    auto hlayout = new QHBoxLayout(mPadding);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(0);
    frameLayout->addWidget(mPadding);
    frameLayout->setStretchFactor(mPadding, 1);
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditAudioAlarmDlg::type_initValues(const KAEvent& event)
{
    if (event.isValid())
    {
        mSoundConfig->set(event.audioFile(), event.soundVolume(), event.fadeVolume(), event.fadeSeconds(),
                          (event.flags() & KAEvent::REPEAT_SOUND) ? event.repeatSoundPause() : -1);
    }
    else
    {
        // Set the values to their defaults
        mSoundConfig->set(Preferences::defaultSoundFile(), Preferences::defaultSoundVolume(),
                          -1, 0, (Preferences::defaultSoundRepeat() ? 0 : -1));
    }
}

/******************************************************************************
* Initialise various values in the New Alarm dialogue.
*/
void EditAudioAlarmDlg::setAudio(const QString& file, float volume)
{
    mSoundConfig->set(file, volume);
}

/******************************************************************************
* Set the dialog's action and the action's text.
*/
void EditAudioAlarmDlg::setAction(KAEvent::SubAction action, const AlarmText& alarmText)
{
    Q_UNUSED(action);
    Q_ASSERT(action == KAEvent::SubAction::Audio);
    mSoundConfig->set(alarmText.displayText(), Preferences::defaultSoundVolume());
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
    mSoundConfig->getVolume(mSavedVolume, mSavedFadeVolume, mSavedFadeSeconds);
    mSavedRepeatPause = mSoundConfig->repeatPause();
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
    if (!mSavedFile.isEmpty()  ||  isTemplate())
    {
        float volume, fadeVolume;
        int   fadeSecs;
        mSoundConfig->getVolume(volume, fadeVolume, fadeSecs);
        if (mSavedRepeatPause != mSoundConfig->repeatPause()
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
void EditAudioAlarmDlg::type_setEvent(KAEvent& event, const KADateTime& dt, const QString& name, const QString& text, int lateCancel, bool trial)
{
    Q_UNUSED(text);
    Q_UNUSED(trial);
    event = KAEvent(dt, name, QString(), QColor(), QColor(), QFont(), KAEvent::SubAction::Audio, lateCancel, getAlarmFlags());
    float volume, fadeVolume;
    int   fadeSecs;
    mSoundConfig->getVolume(volume, fadeVolume, fadeSecs);
    const int repeatPause = mSoundConfig->repeatPause();
    QUrl url;
    mSoundConfig->file(url, false);
    event.setAudioFile(url.toString(), volume, fadeVolume, fadeSecs, repeatPause, isTemplate());
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditAudioAlarmDlg::getAlarmFlags() const
{
    KAEvent::Flags flags = EditAlarmDlg::getAlarmFlags();
    if (mSoundConfig->repeatPause() >= 0) flags |= KAEvent::REPEAT_SOUND;
    return flags;
}

/******************************************************************************
* Check whether the file name is valid.
*/
bool EditAudioAlarmDlg::checkText(QString& result, bool showErrorMessage) const
{
    QUrl url;
    if (!mSoundConfig->file(url, showErrorMessage))
    {
        result.clear();
        return false;
    }
    result = url.isLocalFile() ? url.toLocalFile() : url.toString();
    return true;
}

/******************************************************************************
* Called when the Try button is clicked.
* If the audio file is currently playing (as a result of previously clicking
* the Try button), cancel playback. Otherwise, play the audio file.
*/
void EditAudioAlarmDlg::slotTry()
{
    if (!MessageDisplay::isAudioPlaying())
        EditAlarmDlg::slotTry();   // play the audio file
    else if (mMessageWindow)
    {
        MessageDisplay::stopAudio();
        mMessageWindow = nullptr;
    }
}

/******************************************************************************
* Called when the Try action has been executed.
*/
void EditAudioAlarmDlg::type_executedTry(const QString&, void* result)
{
    mMessageWindow = (MessageWindow*)result;    // note which MessageWindow controls the audio playback
    if (mMessageWindow)
    {
        slotAudioPlaying(true);
        connect(mMessageWindow, &QObject::destroyed, this, &EditAudioAlarmDlg::audioWinDestroyed);
    }
}

/******************************************************************************
* Called when audio playing starts or stops.
* Enable/disable/toggle the Try button.
*/
void EditAudioAlarmDlg::slotAudioPlaying(bool playing)
{
    if (!playing)
    {
        // Nothing is playing, so enable the Try button
        mTryButton->setEnabled(true);
        mTryButton->setCheckable(false);
        mTryButton->setChecked(false);
        mMessageWindow = nullptr;
        const QString errmsg = AudioPlayer::popError();
        if (!errmsg.isEmpty())
            KAMessageBox::error(this, errmsg);
    }
    else if (mMessageWindow)
    {
        // The test sound file is playing, so enable the Try button and depress it
        mTryButton->setEnabled(true);
        mTryButton->setCheckable(true);
        mTryButton->setChecked(true);
    }
    else
    {
        // An alarm is playing, so disable the Try button
        mTryButton->setEnabled(false);
        mTryButton->setCheckable(false);
        mTryButton->setChecked(false);
    }
}


/*=============================================================================
= Class CommandEdit
= A widget to allow entry of a command or a command script.
=============================================================================*/
CommandEdit::CommandEdit(QWidget* parent)
    : QWidget(parent)
{
    auto vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0, 0, 0, 0);
    mTypeScript = new CheckBox(EditCommandAlarmDlg::i18n_chk_EnterScript(), this);
    mTypeScript->setWhatsThis(i18nc("@info:whatsthis", "Check to enter the contents of a script instead of a shell command line"));
    connect(mTypeScript, &CheckBox::toggled, this, &CommandEdit::slotCmdScriptToggled);
    connect(mTypeScript, &CheckBox::toggled, this, &CommandEdit::changed);
    vlayout->addWidget(mTypeScript, 0, Qt::AlignLeft);

    mCommandEdit = new LineEdit(LineEdit::Type::Url, this);
    mCommandEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter a shell command to execute."));
    connect(mCommandEdit, &LineEdit::textChanged, this, &CommandEdit::changed);
    vlayout->addWidget(mCommandEdit);

    mScriptEdit = new TextEdit(this);
    mScriptEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the contents of a script to execute"));
    connect(mScriptEdit, &TextEdit::textChanged, this, &CommandEdit::changed);
    vlayout->addWidget(mScriptEdit);

    slotCmdScriptToggled(mTypeScript->isChecked());
}

CommandEdit::~CommandEdit()
{
    // Delete KTextEdit while parent exists to prevent crash if spell checking is enabled.
    delete mScriptEdit;
    mScriptEdit = nullptr;
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
    const QString text = alarmText.displayText();
    const bool script = alarmText.isScript();
    mTypeScript->setChecked(script);
    if (script)
        mScriptEdit->setPlainText(text);
    else
        mCommandEdit->setText(File::pathOrUrl(text));
}

/******************************************************************************
* Return the widget's text.
* If it's a command line, it must not start with environment variable
* specifications.
* If 'showErrorMessage' is true and the text is empty, an error message is
* displayed.
* Reply = text, or empty if a command line starting with environment vars.
*/
QString CommandEdit::text(EditAlarmDlg* dlg, bool showErrorMessage) const
{
    QString result;
    if (mTypeScript->isChecked())
        result = mScriptEdit->toPlainText().trimmed();
    else
    {
        result = mCommandEdit->text().trimmed();
        QString params = result;
        const QString cmd = ShellProcess::splitCommandLine(params);
        if (cmd.contains(QLatin1Char('=')))
        {
            if (showErrorMessage)
                KAMessageBox::error(dlg, xi18nc("@info", "<para>The command cannot set environment variables:</para><para><icode>%1</icode></para>", cmd));
            return {};
        }
    }
    if (showErrorMessage  &&  result.isEmpty())
        KAMessageBox::error(dlg, i18nc("@info", "Please enter a command or script to execute"));
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
    Q_EMIT scriptToggled(on);
}

/******************************************************************************
* Returns the minimum size of the widget.
*/
QSize CommandEdit::minimumSizeHint() const
{
    const QSize t(mTypeScript->minimumSizeHint());
    QSize s(mCommandEdit->minimumSizeHint().expandedTo(mScriptEdit->minimumSizeHint()));
    s.setHeight(s.height() + style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) + t.height());
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
    QSize tsize = TextEdit::minimumSizeHint();   // avoid calling virtual method from constructor
    tsize.setHeight(fontMetrics().lineSpacing()*13/4 + 2*frameWidth());
    setMinimumSize(tsize);
}

void TextEdit::enableEmailDrop()
{
    mEmailDrop = true;
    setAcceptDrops(true);   // allow drag-and-drop onto this widget
}

void TextEdit::dragEnterEvent(QDragEnterEvent* e)
{
    if (KCalUtils::ICalDrag::canDecode(e->mimeData()))
    {
        e->ignore();   // don't accept "text/calendar" objects
        return;
    }
    if (mEmailDrop  &&  DragDrop::mayHaveRFC822(e->mimeData()))
    {
        e->acceptProposedAction();
        return;
    }
    KTextEdit::dragEnterEvent(e);
}

void TextEdit::dragMoveEvent(QDragMoveEvent* e)
{
    if (mEmailDrop  &&  DragDrop::mayHaveRFC822(e->mimeData()))
    {
        e->acceptProposedAction();
        return;
    }
    KTextEdit::dragMoveEvent(e);
}

/******************************************************************************
* Called when an object is dropped on the widget.
*/
void TextEdit::dropEvent(QDropEvent* e)
{
    const QMimeData* data = e->mimeData();
    if (mEmailDrop)
    {
        AlarmText alarmText;
        bool haveEmail = false;
        if (DragDrop::dropRFC822(data, alarmText))
        {
            // Email message(s). Ignore all but the first.
            qCDebug(KALARM_LOG) << "TextEdit::dropEvent: email";
            haveEmail = true;
        }
        else
        {
            QUrl url;
            if (KAlarm::dropAkonadiEmail(data, url, alarmText))
            {
                // It's an email held in Akonadi
                qCDebug(KALARM_LOG) << "TextEdit::dropEvent: Akonadi email";
                haveEmail = true;
            }
        }
        if (haveEmail)
        {
            if (!alarmText.isEmpty())
                setPlainText(alarmText.displayText());
            return;
        }
    }
    QString text;
    if (DragDrop::dropPlainText(data, text))
    {
        setPlainText(text);
        return;
    }
    KTextEdit::dropEvent(e);
}

#include "moc_editdlgtypes.cpp"

// vim: et sw=4:
