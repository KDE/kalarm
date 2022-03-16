/*
 *  editdlgtypes.h  -  dialogues to create or edit alarm or alarm template types
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "editdlg.h"
#include "preferences.h"
#include "kalarmcalendar/alarmtext.h"
#include "kalarmcalendar/kaevent.h"

using namespace KAlarmCal;

class QAbstractButton;
class QGroupBox;
class QComboBox;
class EmailIdCombo;
class CheckBox;
class ComboBox;
class FontColourButton;
class ButtonGroup;
class RadioButton;
class Reminder;
class SoundPicker;
class SpecialActionsButton;
class CommandEdit;
class LineEdit;
class TextEdit;
class SoundWidget;
class MessageWindow;
class PickLogFileRadio;

class EditDisplayAlarmDlg : public EditAlarmDlg
{
    Q_OBJECT
public:
    explicit EditDisplayAlarmDlg(bool Template, QWidget* parent = nullptr, GetResourceType = RES_PROMPT);
    EditDisplayAlarmDlg(bool Template, const KAEvent&, bool newAlarm, QWidget* parent = nullptr,
                 GetResourceType = RES_PROMPT, bool readOnly = false);

    // Methods to initialise values in the New Alarm dialogue.
    // N.B. setTime() must be called first to set the date-only characteristic,
    //      followed by setRecurrence().
    void            setAction(KAEvent::SubAction, const AlarmText& = AlarmText()) override;
    void            setBgColour(const QColor&);
    void            setFgColour(const QColor&);
    void            setNotify(bool);
    void            setConfirmAck(bool);
    void            setAutoClose(bool);
    void            setAudio(Preferences::SoundType, const QString& file = QString(), float volume = -1, int repeatPause = -1);
    void            setReminder(int minutes, bool onceOnly);

    Reminder*       createReminder(QWidget* parent) override;
    static CheckBox* createConfirmAckCheckbox(QWidget* parent);

    static QString  i18n_lbl_DisplayMethod(); // text of 'Display method' label
    static QString  i18n_combo_Window();      // text of 'Window' selection
    static QString  i18n_combo_Notify();      // text of 'Notification' selection
    static QString  i18n_chk_ConfirmAck();    // text of 'Confirm acknowledgement' checkbox

protected:
    QString         type_caption() const override;
    void            type_init(QWidget* parent, QVBoxLayout* frameLayout) override;
    void            type_initValues(const KAEvent&) override;
    void            type_showOptions(bool more) override;
    void            setReadOnly(bool readOnly) override;
    void            saveState(const KAEvent*) override;
    bool            type_stateChanged() const override;
    void            type_setEvent(KAEvent&, const KADateTime&, const QString& name, const QString& text, int lateCancel, bool trial) override;
    KAEvent::Flags  getAlarmFlags() const override;
    bool            type_validate(bool trial) override { Q_UNUSED(trial); return true; }
    CheckBox*       type_createConfirmAckCheckbox(QWidget* parent) override  { mConfirmAck = createConfirmAckCheckbox(parent); return mConfirmAck; }
    bool            checkText(QString& result, bool showErrorMessage = true) const override;

private Q_SLOTS:
    void            slotAlarmTypeChanged(int index);
    void            slotDisplayMethodChanged(int index);
    void            slotPickFile();
    void            slotCmdScriptToggled(bool);
    void            setColours(const QColor& fg, const QColor& bg);

private:

    // Display alarm options
    ComboBox*           mTypeCombo;
    QWidget*            mDisplayMethodBox;
    ComboBox*           mDisplayMethodCombo;
    QWidget*            mFileBox;
    QWidget*            mFilePadding;
    SoundPicker*        mSoundPicker;
    CheckBox*           mConfirmAck;
    FontColourButton*   mFontColourButton;
    SpecialActionsButton* mSpecialActionsButton {nullptr};
    KAEvent::EmailId    mEmailId;            // if email text, message's Akonadi item ID, else -1
    bool                mReminderDeferral {false};
    bool                mReminderArchived {false};
    // Text message alarm widgets
    TextEdit*           mTextMessageEdit;    // text message edit box
    // Text file alarm widgets
    LineEdit*           mFileMessageEdit;    // text file URL edit box
    QPushButton*        mFileBrowseButton;   // text file browse button
    QString             mFileDefaultDir;     // default directory for browse button
    // Command output alarm widgets
    CommandEdit*        mCmdEdit;

    // Initial state of all controls
    int                 mSavedType;             // mTypeCombo index
    Preferences::SoundType mSavedSoundType;     // mSoundPicker sound type
    bool                mSavedSound;            // mSoundPicker sound status
    int                 mSavedRepeatPause;      // mSoundPicker repeat pause
    QUrl                mSavedSoundFile;        // mSoundPicker sound file
    float               mSavedSoundVolume;      // mSoundPicker volume
    float               mSavedSoundFadeVolume;  // mSoundPicker fade volume
    int                 mSavedSoundFadeSeconds; // mSoundPicker fade time
    bool                mSavedCmdScript;        // mCmdEdit->isScript() status
    int                 mSavedDisplayMethod;    // mDisplayMethodCombo index
    bool                mSavedConfirmAck;       // mConfirmAck status
    QFont               mSavedFont;             // mFontColourButton font
    QColor              mSavedBgColour;         // mBgColourChoose selection
    QColor              mSavedFgColour;         // mFontColourButton foreground colour
    QString             mSavedPreAction;        // mSpecialActionsButton pre-alarm action
    QString             mSavedPostAction;       // mSpecialActionsButton post-alarm action
    int                 mSavedReminder;         // mReminder value
    bool                mSavedAutoClose;        // mLateCancel->isAutoClose() value
    bool                mSavedOnceOnly;         // mReminder once-only status
    KAEvent::ExtraActionOptions mSavedPreActionOptions; // mSpecialActionsButton pre-alarm action options
};


class EditCommandAlarmDlg : public EditAlarmDlg
{
    Q_OBJECT
public:
    explicit EditCommandAlarmDlg(bool Template, QWidget* parent = nullptr, GetResourceType = RES_PROMPT);
    EditCommandAlarmDlg(bool Template, const KAEvent&, bool newAlarm, QWidget* parent = nullptr,
                        GetResourceType = RES_PROMPT, bool readOnly = false);

    // Methods to initialise values in the New Alarm dialogue.
    // N.B. setTime() must be called first to set the date-only characteristic,
    //      followed by setRecurrence().
    void            setAction(KAEvent::SubAction, const AlarmText& = AlarmText()) override;

    static QString  i18n_chk_EnterScript();        // text of 'Enter a script' checkbox
    static QString  i18n_radio_ExecInTermWindow(); // text of 'Execute in terminal window' radio button
    static QString  i18n_chk_ExecInTermWindow();   // text of 'Execute in terminal window' checkbox

protected:
    QString         type_caption() const override;
    void            type_init(QWidget* parent, QVBoxLayout* frameLayout) override;
    void            type_initValues(const KAEvent&) override;
    void            type_showOptions(bool more) override;
    void            setReadOnly(bool readOnly) override;
    void            saveState(const KAEvent*) override;
    bool            type_stateChanged() const override;
    void            type_setEvent(KAEvent&, const KADateTime&, const QString& name, const QString& text, int lateCancel, bool trial) override;
    KAEvent::Flags  getAlarmFlags() const override;
    bool            type_validate(bool trial) override;
    void            type_executedTry(const QString& text, void* obj) override;
    bool            checkText(QString& result, bool showErrorMessage = true) const override;

private Q_SLOTS:
    void            slotCmdScriptToggled(bool);

private:
    // Command alarm options
    CommandEdit*      mCmdEdit;
    CheckBox*         mCmdDontShowError;
    QGroupBox*        mCmdOutputBox;
    ButtonGroup*      mCmdOutputGroup;     // what to do with command output
    RadioButton*      mCmdExecInTerm;
    PickLogFileRadio* mCmdLogToFile;
    RadioButton*      mCmdDiscardOutput;
    LineEdit*         mCmdLogFileEdit;     // log file URL edit box
    QWidget*          mCmdPadding;

    // Initial state of all controls
    bool              mSavedCmdScript;        // mCmdEdit->isScript() status
    bool              mSavedCmdDontShowError; // mCmdDontShowError value
    QAbstractButton*  mSavedCmdOutputRadio;   // selected button in mCmdOutputGroup
    QString           mSavedCmdLogFile;       // mCmdLogFileEdit value
};


class EditEmailAlarmDlg : public EditAlarmDlg
{
    Q_OBJECT
public:
    explicit EditEmailAlarmDlg(bool Template, QWidget* parent = nullptr, GetResourceType = RES_PROMPT);
    EditEmailAlarmDlg(bool Template, const KAEvent&, bool newAlarm, QWidget* parent = nullptr,
                      GetResourceType = RES_PROMPT, bool readOnly = false);

    // Methods to initialise values in the New Alarm dialogue.
    // N.B. setTime() must be called first to set the date-only characteristic,
    //      followed by setRecurrence().
    void            setAction(KAEvent::SubAction, const AlarmText& = AlarmText()) override;
    void            setEmailFields(uint fromID, const KCalendarCore::Person::List&, const QString& subject,
                                   const QStringList& attachments);
    void            setBcc(bool);

    static QString  i18n_chk_CopyEmailToSelf();    // text of 'Copy email to self' checkbox

protected:
    QString         type_caption() const override;
    void            type_init(QWidget* parent, QVBoxLayout* frameLayout) override;
    void            type_initValues(const KAEvent&) override;
    void            type_showOptions(bool) override  {}
    void            setReadOnly(bool readOnly) override;
    void            saveState(const KAEvent*) override;
    bool            type_stateChanged() const override;
    void            type_setEvent(KAEvent&, const KADateTime&, const QString& name, const QString& text, int lateCancel, bool trial) override;
    KAEvent::Flags  getAlarmFlags() const override;
    bool            type_validate(bool trial) override;
    void            type_aboutToTry() override;
    bool            checkText(QString& result, bool showErrorMessage = true) const override;

private Q_SLOTS:
    void            slotTrySuccess();
    void            openAddressBook();
    void            slotAddAttachment();
    void            slotRemoveAttachment();

private:
    void            attachmentEnable();

    // Email alarm options
    EmailIdCombo*   mEmailFromList;
    LineEdit*       mEmailToEdit;
    QPushButton*    mEmailAddressButton; // email open address book button
    LineEdit*       mEmailSubjectEdit;
    TextEdit*       mEmailMessageEdit;   // email body edit box
    QComboBox*      mEmailAttachList;
    QPushButton*    mEmailAddAttachButton;
    QPushButton*    mEmailRemoveButton {nullptr};
    CheckBox*       mEmailBcc;
    QString         mAttachDefaultDir;

    KCalendarCore::Person::List mEmailAddresses;  // list of addresses to send email to

    QStringList     mEmailAttachments;   // list of email attachment file names

    // Initial state of all controls
    QString         mSavedEmailFrom;        // mEmailFromList current value
    QString         mSavedEmailTo;          // mEmailToEdit value
    QString         mSavedEmailSubject;     // mEmailSubjectEdit value
    QStringList     mSavedEmailAttach;      // mEmailAttachList values
    bool            mSavedEmailBcc;         // mEmailBcc status
};


class EditAudioAlarmDlg : public EditAlarmDlg
{
    Q_OBJECT
public:
    explicit EditAudioAlarmDlg(bool Template, QWidget* parent = nullptr, GetResourceType = RES_PROMPT);
    EditAudioAlarmDlg(bool Template, const KAEvent&, bool newAlarm, QWidget* parent = nullptr,
                 GetResourceType = RES_PROMPT, bool readOnly = false);
    ~EditAudioAlarmDlg() override;

    // Methods to initialise values in the New Alarm dialogue.
    // N.B. setTime() must be called first to set the date-only characteristic,
    //      followed by setRecurrence().
    void            setAction(KAEvent::SubAction, const AlarmText& = AlarmText()) override;
    void            setAudio(const QString& file, float volume = -1);

protected:
    QString         type_caption() const override;
    void            type_init(QWidget* parent, QVBoxLayout* frameLayout) override;
    void            type_initValues(const KAEvent&) override;
    void            type_showOptions(bool) override  {}
    void            setReadOnly(bool readOnly) override;
    void            saveState(const KAEvent*) override;
    bool            type_stateChanged() const override;
    void            type_setEvent(KAEvent&, const KADateTime&, const QString& name, const QString& text, int lateCancel, bool trial) override;
    KAEvent::Flags  getAlarmFlags() const override;
    bool            type_validate(bool trial) override { Q_UNUSED(trial); return true; }
    void            type_executedTry(const QString& text, void* obj) override;
    bool            checkText(QString& result, bool showErrorMessage = true) const override;

protected Q_SLOTS:
    void            slotTry() override;

private Q_SLOTS:
    void            audioWinDestroyed()  { slotAudioPlaying(false); }
    void            slotAudioPlaying(bool playing);

private:
    MessageWindow*  mMessageWindow {nullptr}; // MessageWindow controlling test audio playback

    // Audio alarm options
    SoundWidget*    mSoundConfig;
    QWidget*        mPadding;          // allow top-adjustment of controls

    // Initial state of all controls
    QString         mSavedFile;        // sound file
    float           mSavedVolume;      // volume
    float           mSavedFadeVolume;  // fade volume
    int             mSavedFadeSeconds; // fade time
    int             mSavedRepeatPause; // sound file repeat pause
};

// vim: et sw=4:
