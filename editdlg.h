/*
 *  editdlg.h  -  dialog to create or modify an alarm or alarm template
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

#ifndef EDITDLG_H
#define EDITDLG_H

#include <kdialog.h>

#include "alarmevent.h"
#include "alarmtext.h"
#include "datetime.h"

class QLabel;
class QShowEvent;
class QResizeEvent;
class QAbstractButton;
class QGroupBox;
class QFrame;
class QVBoxLayout;
class KLineEdit;
class KTabWidget;
class AlarmResource;
class ButtonGroup;
class TimeEdit;
class RadioButton;
class CheckBox;
class LateCancelSelector;
class AlarmTimeWidget;
class RecurrenceEdit;
class Reminder;
class ShellProcess;
class StackedScrollGroup;
class TimeSpinBox;


class EditAlarmDlg : public KDialog
{
		Q_OBJECT
	public:
		enum Type { NO_TYPE, DISPLAY, COMMAND, EMAIL };
		enum GetResourceType {
			RES_PROMPT,        // prompt for resource
			RES_USE_EVENT_ID,  // use resource containing event, or prompt if not found
			RES_IGNORE         // don't get resource
		};

		static EditAlarmDlg* create(bool Template, Type, bool newAlarm, QWidget* parent = 0,
		                            GetResourceType = RES_PROMPT);
		static EditAlarmDlg* create(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
		                            GetResourceType = RES_PROMPT, bool readOnly = false);
		virtual ~EditAlarmDlg();
		bool            getEvent(KAEvent&, AlarmResource*&);

		// Methods to initialise values in the New Alarm dialogue.
		// N.B. setTime() must be called first to set the date-only characteristic,
		//      followed by setRecurrence() if applicable.
		void            setTime(const DateTime&);    // must be called first to set date-only value
		void            setRecurrence(const KARecurrence&, int subRepeatInterval, int subRepeatCount);
		void            setRepeatAtLogin();
		virtual void    setAction(KAEventData::Action, const AlarmText& = AlarmText()) = 0;
		void            setLateCancel(int minutes);
		void            setShowInKOrganizer(bool);

		virtual QSize   sizeHint() const    { return minimumSizeHint(); }

		static QString  i18n_chk_ShowInKOrganizer();   // text of 'Show in KOrganizer' checkbox

	protected:
		EditAlarmDlg(bool Template, KAEventData::Action, QWidget* parent = 0,
		             GetResourceType = RES_PROMPT);
		EditAlarmDlg(bool Template, const KAEvent*, QWidget* parent = 0,
		             GetResourceType = RES_PROMPT, bool readOnly = false);
		void            init(const KAEvent* event, bool newAlarm);
		virtual void    resizeEvent(QResizeEvent*);
		virtual void    showEvent(QShowEvent*);
		virtual QString type_caption(bool newAlarm) const = 0;
		virtual void    type_init(QWidget* parent, QVBoxLayout* frameLayout) = 0;
		virtual void    type_initValues(const KAEvent*) = 0;
		virtual void    type_showOptions(bool more) = 0;
		virtual void    setReadOnly(bool readOnly) = 0;
		virtual void    saveState(const KAEvent*) = 0;
		virtual bool    type_stateChanged() const = 0;
		virtual void    type_setEvent(KAEvent&, const KDateTime&, const QString& text, int lateCancel, bool trial) = 0;
		virtual int     getAlarmFlags() const;
		virtual bool    type_validate(bool trial) = 0;
		virtual void    type_trySuccessMessage(ShellProcess*, const QString& text) = 0;
		virtual Reminder* createReminder(QWidget* parent)  { Q_UNUSED(parent); return 0; }
		virtual CheckBox* type_createConfirmAckCheckbox(QWidget* parent)  { Q_UNUSED(parent); return 0; }
		virtual bool    checkText(QString& result, bool showErrorMessage = true) const = 0;

		void            showMainPage();
		bool            isTemplate() const         { return mTemplate; }
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
	private slots:
		void            slotRecurTypeChange(int repeatType);
		void            slotRecurFrequencyChange();
		void            slotEditDeferral();
		void            slotShowMainPage();
		void            slotShowRecurrenceEdit();
		void            slotAnyTimeToggled(bool anyTime);
		void            slotTemplateTimeType(QAbstractButton*);
		void            slotSetSubRepetition();
		void            slotTrySuccess();
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
		KAEventData::Action mAlarmType;           // actual alarm type
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
		QPushButton*        mMoreButton;         // shows 'mMoreOptions'
		QPushButton*        mLessButton;         // hides 'mMoreOptions'

		RecurrenceEdit*     mRecurrenceEdit;

		QString             mAlarmMessage;       // message text/file name/command/email message
		DateTime            mAlarmDateTime;
		DateTime            mDeferDateTime;
		QString             mResourceEventId;    // if non-empty, save alarm in resource containing this event ID
		AlarmResource*      mResource;           // resource to save event into, or null
		int                 mDeferGroupHeight;   // height added by deferred time widget
		int                 mDesktop;            // desktop to display the dialog in
		bool                mTemplate;           // editing an alarm template
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
		int                 mSavedRecurrenceType;   // RecurrenceEdit::RepeatType value
		int                 mSavedLateCancel;       // mLateCancel value
		bool                mSavedShowInKorganizer; // mShowInKorganizer status
};

#endif // EDITDLG_H
