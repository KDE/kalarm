/*
 *  editdlg.h  -  dialog to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  Copyright © 2001-2013 by David Jarvie <djarvie@kde.org>
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

#ifndef EDITDLG_H
#define EDITDLG_H

#include <kalarmcal/alarmtext.h>
#include <kalarmcal/datetime.h>
#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <akonadi/collection.h>
#endif

#include <kdialog.h>

class QLabel;
class QShowEvent;
class QResizeEvent;
class QAbstractButton;
class QGroupBox;
class QFrame;
class QVBoxLayout;
class KLineEdit;
class KTabWidget;
#ifndef USE_AKONADI
class AlarmResource;
#endif
class ButtonGroup;
class TimeEdit;
class RadioButton;
class CheckBox;
class LateCancelSelector;
class AlarmTimeWidget;
class RecurrenceEdit;
class Reminder;
class StackedScrollGroup;
class TimeSpinBox;

using namespace KAlarmCal;


class EditAlarmDlg : public KDialog
{
        Q_OBJECT
    public:
        enum Type { NO_TYPE, DISPLAY, COMMAND, EMAIL, AUDIO };
        enum GetResourceType {
            RES_PROMPT,        // prompt for resource
            RES_USE_EVENT_ID,  // use resource containing event, or prompt if not found
            RES_IGNORE         // don't get resource
        };

        static EditAlarmDlg* create(bool Template, Type, QWidget* parent = 0,
                                    GetResourceType = RES_PROMPT);
        static EditAlarmDlg* create(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
                                    GetResourceType = RES_PROMPT, bool readOnly = false);
        virtual ~EditAlarmDlg();
#ifdef USE_AKONADI
        bool            getEvent(KAEvent&, Akonadi::Collection&);
#else
        bool            getEvent(KAEvent&, AlarmResource*&);
#endif

        // Methods to initialise values in the New Alarm dialogue.
        // N.B. setTime() must be called first to set the date-only characteristic,
        //      followed by setRecurrence() if applicable.
        void            setTime(const DateTime&);    // must be called first to set date-only value
        void            setRecurrence(const KARecurrence&, int subRepeatInterval, int subRepeatCount);
        void            setRepeatAtLogin();
        virtual void    setAction(KAEvent::SubAction, const AlarmText& = AlarmText()) = 0;
        void            setLateCancel(int minutes);
        void            setShowInKOrganizer(bool);

        virtual QSize   sizeHint() const    { return minimumSizeHint(); }

        static QString  i18n_chk_ShowInKOrganizer();   // text of 'Show in KOrganizer' checkbox

    protected:
        EditAlarmDlg(bool Template, KAEvent::SubAction, QWidget* parent = 0,
                     GetResourceType = RES_PROMPT);
        EditAlarmDlg(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
                     GetResourceType = RES_PROMPT, bool readOnly = false);
        void            init(const KAEvent* event);
        virtual void    resizeEvent(QResizeEvent*);
        virtual void    showEvent(QShowEvent*);
        virtual void    closeEvent(QCloseEvent*);
        virtual QString type_caption() const = 0;
        virtual void    type_init(QWidget* parent, QVBoxLayout* frameLayout) = 0;
        virtual void    type_initValues(const KAEvent*) = 0;
        virtual void    type_showOptions(bool more) = 0;
        virtual void    setReadOnly(bool readOnly) = 0;
        virtual void    saveState(const KAEvent*) = 0;
        virtual bool    type_stateChanged() const = 0;
        virtual void    type_setEvent(KAEvent&, const KDateTime&, const QString& text, int lateCancel, bool trial) = 0;
        virtual KAEvent::Flags getAlarmFlags() const;
        virtual bool    type_validate(bool trial) = 0;
        virtual void    type_aboutToTry() {}
        virtual void    type_executedTry(const QString& text, void* obj) { Q_UNUSED(text); Q_UNUSED(obj); }
        virtual Reminder* createReminder(QWidget* parent)  { Q_UNUSED(parent); return 0; }
        virtual CheckBox* type_createConfirmAckCheckbox(QWidget* parent)  { Q_UNUSED(parent); return 0; }
        virtual bool    checkText(QString& result, bool showErrorMessage = true) const = 0;

        void            showMainPage();
        bool            isTemplate() const         { return mTemplate; }
        bool            isNewAlarm() const         { return mNewAlarm; }
        bool            dateOnly() const;
        bool            isTimedRecurrence() const;
        bool            showingMore() const        { return mShowingMore; }
        Reminder*       reminder() const           { return mReminder; }
        LateCancelSelector* lateCancel() const     { return mLateCancel; }

    protected slots:
        virtual void    slotTry();
        virtual void    slotHelp();      // Load Template
        virtual void    slotDefault();   // More/Less Options
        virtual void    slotButtonClicked(int button);
        void            contentsChanged();

    private slots:
        void            slotRecurTypeChange(int repeatType);
        void            slotRecurFrequencyChange();
        void            slotEditDeferral();
        void            slotShowMainPage();
        void            slotShowRecurrenceEdit();
        void            slotAnyTimeToggled(bool anyTime);
        void            slotTemplateTimeType(QAbstractButton*);
        void            slotSetSubRepetition();
        void            slotResize();

    private:
        void            init(const KAEvent* event, GetResourceType getResource);
        void            initValues(const KAEvent*);
        void            setEvent(KAEvent&, const QString& text, bool trial);
        bool            validate();
        void            setRecurTabTitle(const KAEvent* = 0);
        virtual bool    stateChanged() const;
        void            showOptions(bool more);

    protected:
        KAEvent::SubAction  mAlarmType;           // actual alarm type
    private:
        KTabWidget*         mTabs;                // the tabs in the dialog
        StackedScrollGroup* mTabScrollGroup;
        int                 mMainPageIndex;
        int                 mRecurPageIndex;
        bool                mMainPageShown;            // true once the main tab has been displayed
        bool                mRecurPageShown;           // true once the recurrence tab has been displayed
        bool                mRecurSetDefaultEndDate;   // adjust default end date/time when recurrence tab is displayed

        // Templates
        KLineEdit*          mTemplateName;
        ButtonGroup*        mTemplateTimeGroup;
        RadioButton*        mTemplateDefaultTime; // no alarm time is specified
        RadioButton*        mTemplateUseTimeAfter;// alarm time is specified as an offset from current
        RadioButton*        mTemplateAnyTime;     // alarms have date only, no time
        RadioButton*        mTemplateUseTime;     // an alarm time is specified
        TimeSpinBox*        mTemplateTimeAfter;   // the specified offset from the current time
        TimeEdit*           mTemplateTime;        // the alarm time which is specified 
        QGroupBox*          mDeferGroup;
        QLabel*             mDeferTimeLabel;
        QPushButton*        mDeferChangeButton;

        AlarmTimeWidget*    mTimeWidget;
        LateCancelSelector* mLateCancel;
        Reminder*           mReminder;           // null except for display alarms
        CheckBox*           mShowInKorganizer;

        QFrame*             mMoreOptions;        // contains options hidden by default

        RecurrenceEdit*     mRecurrenceEdit;

        QString             mAlarmMessage;       // message text/file name/command/email message
        DateTime            mAlarmDateTime;
        DateTime            mDeferDateTime;
#ifdef USE_AKONADI
        Akonadi::Item::Id   mCollectionItemId;   // if >=0, save alarm in collection containing this item ID
        Akonadi::Collection mCollection;         // collection to save event into, or null
#else
        QString             mResourceEventId;    // if non-empty, save alarm in resource containing this event ID
        AlarmResource*      mResource;           // resource to save event into, or null
#endif
        int                 mDeferGroupHeight;   // height added by deferred time widget
        int                 mDesktop;            // desktop to display the dialog in
        QString             mEventId;            // UID of event being edited, or blank for new event
        bool                mTemplate;           // editing an alarm template
        bool                mNewAlarm;           // editing a new alarm
        bool                mExpiredRecurrence;  // initially a recurrence which has expired
        mutable bool        mChanged;            // controls other than deferral have changed since dialog was displayed
        mutable bool        mOnlyDeferred;       // the only change made in the dialog was to the existing deferral
        bool                mDesiredReadOnly;    // the specified read-only status of the dialog
        bool                mReadOnly;           // the actual read-only status of the dialog
        bool                mShowingMore;        // the More Options button has been clicked

        // Initial state of all controls
        KAEvent*            mSavedEvent;
        QString             mSavedTemplateName;     // mTemplateName value
        QAbstractButton*    mSavedTemplateTimeType; // selected button in mTemplateTimeGroup
        QTime               mSavedTemplateTime;     // mTemplateTime value
        int                 mSavedTemplateAfterTime;// mTemplateAfterTime value
        QString             mSavedTextFileCommandMessage;  // mTextMessageEdit/mFileMessageEdit/mCmdCommandEdit/mEmailMessageEdit value
        KDateTime           mSavedDateTime;         // mTimeWidget value
        KDateTime           mSavedDeferTime;        // mDeferDateTime value
        int                 mSavedRecurrenceType;   // RecurrenceEdit::RepeatType value
        int                 mSavedLateCancel;       // mLateCancel value
        bool                mSavedShowInKorganizer; // mShowInKorganizer status
};

#endif // EDITDLG_H

// vim: et sw=4:
