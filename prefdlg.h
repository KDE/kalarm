/*
 *  prefdlg.h  -  program preferences dialog
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef PREFDLG_H
#define PREFDLG_H

#include <qsize.h>
#include <qdatetime.h>
#include <ktabctl.h>
#include <kdialogbase.h>

#include "recurrenceedit.h"

class QButtonGroup;
class QCheckBox;
class QRadioButton;
class QPushButton;
class QComboBox;
class QLineEdit;
class KColorCombo;
class FontColourChooser;
class Preferences;
class TimeSpinBox;
class SpinBox;

class MessagePrefTab;
class DefaultPrefTab;
class ViewPrefTab;
class MiscPrefTab;


// The Preferences dialog
class KAlarmPrefDlg : public KDialogBase
{
		Q_OBJECT
	public:
		KAlarmPrefDlg(Preferences*);
		~KAlarmPrefDlg();

		MessagePrefTab*    mMessagePage;
		DefaultPrefTab*    mDefaultPage;
		ViewPrefTab*       mViewPage;
		MiscPrefTab*       mMiscPage;

	protected slots:
		virtual void slotOk();
		virtual void slotApply();
		virtual void slotHelp();
		virtual void slotDefault();
		virtual void slotCancel();
};

// Base class for each tab in the Preferences dialog
class PrefsTabBase : public QWidget
{
		Q_OBJECT
	public:
		PrefsTabBase(QVBox*);

		void         setPreferences(Preferences*);
		virtual void restore() = 0;
		virtual void apply(bool syncToDisc) = 0;
		virtual void setDefaults() = 0;

	protected:
		QVBox*       mPage;
		Preferences* mPreferences;
};


// Miscellaneous tab of the Preferences dialog
class MiscPrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		MiscPrefTab(QVBox*);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotRunModeToggled(bool);
		void         slotDisableIfStoppedToggled(bool);
		void         slotExpiredToggled(bool);
		void         slotEmailUseCCToggled(bool);
		void         slotClearExpired();

	private:
		void         setExpiredControls(int purgeDays);
		void         setEmailAddress(bool useControlCentre, const QString& address);

		QRadioButton*  mRunInSystemTray;
		QRadioButton*  mRunOnDemand;
		QCheckBox*     mDisableAlarmsIfStopped;
		QCheckBox*     mQuitWarn;
		QCheckBox*     mAutostartTrayIcon1;
		QCheckBox*     mAutostartTrayIcon2;
		QCheckBox*     mConfirmAlarmDeletion;
		QCheckBox*     mKeepExpired;
		QCheckBox*     mPurgeExpired;
		SpinBox*       mPurgeAfter;
		QLabel*        mPurgeAfterLabel;
		QPushButton*   mClearExpired;
		TimeSpinBox*   mStartOfDay;
		QButtonGroup*  mEmailClient;
		QCheckBox*     mEmailUseControlCentre;
		QCheckBox*     mEmailQueuedNotify;
		QLineEdit*     mEmailAddress;
};


// Edit defaults tab of the Preferences dialog
class DefaultPrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		DefaultPrefTab(QVBox*);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotBeepToggled(bool);
		void         slotBrowseSoundFile();

	private:
		QCheckBox*     mDefaultLateCancel;
		QCheckBox*     mDefaultConfirmAck;
		QCheckBox*     mDefaultEmailBcc;
		QCheckBox*     mDefaultBeep;
		QLabel*        mDefaultSoundFileLabel;
		QLineEdit*     mDefaultSoundFile;
		QPushButton*   mDefaultSoundFileBrowse;
		QComboBox*     mDefaultRecurPeriod;
		QComboBox*     mDefaultReminderUnits;

		static int recurIndex(RecurrenceEdit::RepeatType);
};


// View tab of the Preferences dialog
class ViewPrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		ViewPrefTab(QVBox*);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotListTimeToggled(bool);
		void         slotListTimeToToggled(bool);
		void         slotTooltipAlarmsToggled(bool);
		void         slotTooltipMaxToggled(bool);
		void         slotTooltipTimeToggled(bool);
		void         slotTooltipTimeToToggled(bool);

	private:
		void         setList(bool time, bool timeTo);
		void         setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix);

		QCheckBox*     mListShowTime;
		QCheckBox*     mListShowTimeTo;
		QCheckBox*     mTooltipShowAlarms;
		QCheckBox*     mTooltipMaxAlarms;
		SpinBox*       mTooltipMaxAlarmCount;
		QCheckBox*     mTooltipShowTime;
		QCheckBox*     mTooltipShowTimeTo;
		QLineEdit*     mTooltipTimeToPrefix;
		QLabel*        mTooltipTimeToPrefixLabel;
		QCheckBox*     mModalMessages;
		QCheckBox*     mShowExpiredAlarms;
		SpinBox*       mDaemonTrayCheckInterval;
		bool           mIgnoreToggle;    // prevent checkbox toggle processing
};


// Message appearance tab of the Preferences dialog
class MessagePrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		MessagePrefTab(QVBox*);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private:
		FontColourChooser*  mFontChooser;
		KColorCombo*        mExpiredColour;
};

#endif // PREFDLG_H
