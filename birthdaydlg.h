/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2002-2004,2006,2008 by David Jarvie <djarvie@kde.org>
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
#ifndef BIRTHDAYDLG_H
#define BIRTHDAYDLG_H

#include <qlineedit.h>
#include <klistview.h>
#include <kdialogbase.h>

#include "alarmevent.h"

class QCheckBox;
class KListView;
class CheckBox;
class FontColourButton;
class SoundPicker;
class SpecialActionsButton;
class RepetitionButton;
class LateCancelSelector;
class Reminder;
namespace KABC { class AddressBook; }
class BLineEdit;
class BListView;


class BirthdayDlg : public KDialogBase
{
		Q_OBJECT
	public:
		BirthdayDlg(QWidget* parent = 0);
		QValueList<KAEvent> events() const;
		static void close();

	protected slots:
		virtual void      slotOk();

	private slots:
		void              slotSelectionChanged();
		void              slotTextLostFocus();
		void              updateSelectionList();

	private:
		void              loadAddressBook();

		static const KABC::AddressBook* mAddressBook;
		BListView*               mAddresseeList;
		BLineEdit*               mPrefix;
		BLineEdit*               mSuffix;
		Reminder*                mReminder;
		SoundPicker*             mSoundPicker;
		FontColourButton*        mFontColourButton;
		CheckBox*                mConfirmAck;
		LateCancelSelector*      mLateCancel;
		SpecialActionsButton*    mSpecialActionsButton;
		RepetitionButton*        mSubRepetition;
		QString                  mPrefixText;   // last entered value of prefix text
		QString                  mSuffixText;   // last entered value of suffix text
		int                      mFlags;        // event flag bits
};


class BLineEdit : public QLineEdit
{
		Q_OBJECT
	public:
		BLineEdit(QWidget* parent = 0, const char* name = 0)
			     : QLineEdit(parent, name) {}
		BLineEdit(const QString& text, QWidget* parent = 0, const char* name = 0)
			     : QLineEdit(text, parent, name) {}
	signals:
		void         focusLost();
	protected:
		virtual void focusOutEvent(QFocusEvent*)  { emit focusLost(); }
};

class BListView : public KListView
{
		Q_OBJECT
	public:
		BListView(QWidget* parent = 0, const char* name = 0);
	public slots:
		virtual void slotSelectAll()   { selectAll(true); }
		virtual void slotDeselect()    { selectAll(false); }
};

#endif // BIRTHDAYDLG_H
