/*
 *  prefdlg.h  -  program preferences dialog
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
class TimeSpinBox;
class SpinBox;

class MessagePrefTab;
class EditPrefTab;
class EmailPrefTab;
class ViewPrefTab;
class MiscPrefTab;


// The Preferences dialog
class KAlarmPrefDlg : public KDialogBase
{
		Q_OBJECT
	public:
		KAlarmPrefDlg();
		~KAlarmPrefDlg();

		MessagePrefTab* mMessagePage;
		EditPrefTab*    mEditPage;
		EmailPrefTab*   mEmailPage;
		ViewPrefTab*    mViewPage;
		MiscPrefTab*    mMiscPage;

	protected slots:
		virtual void slotOk();
		virtual void slotApply();
		virtual void slotHelp();
		virtual void slotDefault();
		virtual void slotCancel();

	private:
		void            restore();
		bool            mValid;
};

// Base class for each tab in the Preferences dialog
class PrefsTabBase : public QWidget
{
		Q_OBJECT
	public:
		PrefsTabBase(QVBox*);

		void         setPreferences();
		virtual void restore() = 0;
		virtual void apply(bool syncToDisc) = 0;
		virtual void setDefaults() = 0;

	protected:
		QVBox*       mPage;
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
		void         slotClearExpired();

	private:
		void         setExpiredControls(int purgeDays);

		QRadioButton*  mRunInSystemTray;
		QRadioButton*  mRunOnDemand;
		QCheckBox*     mDisableAlarmsIfStopped;
		QCheckBox*     mQuitWarn;
		QCheckBox*     mAutostartTrayIcon1;
		QCheckBox*     mAutostartTrayIcon2;
		QCheckBox*     mConfirmAlarmDeletion;
		QButtonGroup*  mFeb29;
		QCheckBox*     mKeepExpired;
		QCheckBox*     mPurgeExpired;
		SpinBox*       mPurgeAfter;
		QLabel*        mPurgeAfterLabel;
		QPushButton*   mClearExpired;
		TimeSpinBox*   mStartOfDay;
};


// Email tab of the Preferences dialog
class EmailPrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		EmailPrefTab(QVBox*);

		QString      validateAddress();
		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotEmailUseCCToggled(bool);
		void         slotEmailBccUseCCToggled(bool);
		void         slotAddressChanged()    { mAddressChanged = true; }

	private:
		void         setEmailAddress(bool useControlCentre, const QString& address);
		void         setEmailBccAddress(bool useControlCentre, const QString& address);

		QButtonGroup*  mEmailClient;
		QLineEdit*     mEmailAddress;
		QCheckBox*     mEmailUseControlCentre;
		QLineEdit*     mEmailBccAddress;
		QCheckBox*     mEmailBccUseControlCentre;
		QCheckBox*     mEmailQueuedNotify;
		bool           mAddressChanged;
};


// Edit defaults tab of the Preferences dialog
class EditPrefTab : public PrefsTabBase
{
		Q_OBJECT
	public:
		EditPrefTab(QVBox*);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotBrowseSoundFile();

	private:
		QCheckBox*     mDefaultLateCancel;
		QCheckBox*     mDefaultConfirmAck;
		QCheckBox*     mDefaultEmailBcc;
		QCheckBox*     mDefaultBeep;
		QCheckBox*     mDefaultSound;
		QLabel*        mDefaultSoundFileLabel;
		QLineEdit*     mDefaultSoundFile;
		QPushButton*   mDefaultSoundFileBrowse;
		QCheckBox*     mDefaultSoundRepeat;
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
