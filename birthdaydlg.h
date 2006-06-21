/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  Copyright (c) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <QLineEdit>
#include <QList>
#include <k3listview.h>
#include <kdialog.h>

#include "alarmevent.h"

class QFocusEvent;
class QCheckBox;
class K3ListView;
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
class BListView;


class BirthdayDlg : public KDialog
{
		Q_OBJECT
	public:
		BirthdayDlg(QWidget* parent = 0);
		QList<KAEvent>    events() const;

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
		BListView*               mAddresseeList;
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
		BLineEdit(QWidget* parent = 0)                       : QLineEdit(parent) { }
		BLineEdit(const QString& text, QWidget* parent = 0)  : QLineEdit(text, parent) { }
	signals:
		void         focusLost();
	protected:
		virtual void focusOutEvent(QFocusEvent*)  { emit focusLost(); }
};

class BListView : public K3ListView
{
		Q_OBJECT
	public:
		BListView(QWidget* parent = 0);
	public slots:
		virtual void slotSelectAll()   { selectAll(true); }
		virtual void slotDeselect()    { selectAll(false); }
};

#endif // BIRTHDAYDLG_H
