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

	private:
		void              updateSelectionList();

		KListView*               mAddresseeList;
		BLineEdit*               mPrefix;
		BLineEdit*               mSuffix;
		Reminder*                mReminder;
		SoundPicker*             mSoundPicker;
		FontColourButton*        mFontColourButton;
		ColourCombo*             mBgColourChoose;
		CheckBox*                mConfirmAck;
		CheckBox*                mLateCancel;
		SpecialActionsButton*    mSpecialActionsButton;
		const KABC::AddressBook* mAddressBook;
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
		void         lostFocus();
	protected:
		virtual void focusOutEvent(QFocusEvent*)  { emit lostFocus(); }
};

#endif // BIRTHDAYDLG_H
