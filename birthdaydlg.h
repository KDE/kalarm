/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef BIRTHDAYDLG_H
#define BIRTHDAYDLG_H

#include <qlineedit.h>
#include <kdialogbase.h>

#include "alarmevent.h"

class QCheckBox;
class KListView;
class CheckBox;
class ColourCombo;
class FontColourButton;
class SoundPicker;
class SpecialActionsButton;
class RepetitionButton;
class LateCancelSelector;
class Reminder;
namespace KABC { class AddressBook; }
class BLineEdit;


class BirthdayDlg : public KDialogBase
{
		Q_OBJECT
	public:
		BirthdayDlg(QWidget* parent = 0);
		QValueList<KAEvent> events() const;

	protected slots:
		virtual void      slotOk();

	private slots:
		void              slotSelectionChanged();
		void              slotTextLostFocus();
		void              slotFontColourSelected();
		void              slotBgColourSelected(const QColor&);
		void              updateSelectionList();

	private:
		void              loadAddressBook();

		static const KABC::AddressBook* mAddressBook;
		KListView*               mAddresseeList;
		BLineEdit*               mPrefix;
		BLineEdit*               mSuffix;
		Reminder*                mReminder;
		SoundPicker*             mSoundPicker;
		FontColourButton*        mFontColourButton;
		ColourCombo*             mBgColourChoose;
		CheckBox*                mConfirmAck;
		LateCancelSelector*      mLateCancel;
		SpecialActionsButton*    mSpecialActionsButton;
		RepetitionButton*        mSimpleRepetition;
		QString                  mPrefixText;   // last entered value of prefix text
		QString                  mSuffixText;   // last entered value of suffix text
		int                      mFlags;        // event flag bits
};


class BLineEdit : public QLineEdit
{
		Q_OBJECT
	public:
		BLineEdit(QWidget* parent = 0, const char* name = 0)
			     : QLineEdit(parent, name) { }
		BLineEdit(const QString& text, QWidget* parent = 0, const char* name = 0)
			     : QLineEdit(text, parent, name) { }
	signals:
		void         focusLost();
	protected:
		virtual void focusOutEvent(QFocusEvent*)  { emit focusLost(); }
};

#endif // BIRTHDAYDLG_H
