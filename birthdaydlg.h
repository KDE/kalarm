/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */
#ifndef BIRTHDAYDLG_H
#define BIRTHDAYDLG_H

#include <qlineedit.h>
#include <kdialogbase.h>

#include "alarmevent.h"

class QLabel;
class QCheckBox;
class KListView;
class SpinBox;
class CheckBox;
class ComboBox;
class ColourCombo;
class FontColourButton;
class SoundPicker;
#if KDE_VERSION >= 290
namespace KABC { class AddressBook; }
using KABC::AddressBook;
#else
class AddressBook;
#endif
class BLineEdit;


class BirthdayDlg : public KDialogBase
{
		Q_OBJECT
	public:
		BirthdayDlg(QWidget* parent = 0);
		QValueList<KAlarmEvent> events() const;

	protected slots:
		virtual void      slotOk();
	private slots:
		void              slotSelectionChanged();
		void              slotTextLostFocus();
		void              slotReminderToggled(bool);
		void              slotFontColourSelected();
		void              slotBgColourSelected(const QColor&);

	private:

		void              updateSelectionList();

		KListView*        mAddresseeList;
		BLineEdit*        mPrefix;
		BLineEdit*        mSuffix;
		CheckBox*         mReminder;
		SpinBox*          mReminderCount;
		ComboBox*         mReminderUnits;
		QLabel*           mReminderLabel;
		SoundPicker*      mSoundPicker;
		FontColourButton* mFontColourButton;
		ColourCombo*      mBgColourChoose;
		CheckBox*         mConfirmAck;
		CheckBox*         mLateCancel;
		AddressBook*      mAddressBook;
		QString           mPrefixText;   // last entered value of prefix text
		QString           mSuffixText;   // last entered value of suffix text
		int               mFlags;        // event flag bits
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
		void         lostFocus();
	protected:
		virtual void focusOutEvent(QFocusEvent*)  { emit lostFocus(); }
};

#endif // BIRTHDAYDLG_H
