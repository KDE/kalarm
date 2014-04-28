/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2002-2005,2007-2011 by David Jarvie <djarvie@kde.org>
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

#include <KAlarmCal/kaevent.h>

#include <kdialog.h>
#include <klineedit.h>
#include <QVector>

class QFocusEvent;
class QTreeView;
class CheckBox;
class FontColourButton;
class SoundPicker;
class SpecialActionsButton;
class RepetitionButton;
class LateCancelSelector;
class Reminder;
class BLineEdit;
class BirthdaySortModel;

using namespace KAlarmCal;

class BirthdayDlg : public KDialog
{
        Q_OBJECT
    public:
        explicit BirthdayDlg(QWidget* parent = 0);
        QVector<KAEvent> events() const;

    protected slots:
        virtual void   slotOk();

    private slots:
        void           slotSelectionChanged();
        void           slotTextLostFocus();
        void           resizeViewColumns();
        void           setColours(const QColor& fg, const QColor& bg);

    private:
        BirthdaySortModel*    mBirthdaySortModel;
        QTreeView*            mListView;
        BLineEdit*            mPrefix;
        BLineEdit*            mSuffix;
        Reminder*             mReminder;
        SoundPicker*          mSoundPicker;
        FontColourButton*     mFontColourButton;
        CheckBox*             mConfirmAck;
        LateCancelSelector*   mLateCancel;
        SpecialActionsButton* mSpecialActionsButton;
        RepetitionButton*     mSubRepetition;
        QString               mPrefixText;   // last entered value of prefix text
        QString               mSuffixText;   // last entered value of suffix text
        KAEvent::Flags        mFlags;        // event flag bits
};


class BLineEdit : public KLineEdit
{
        Q_OBJECT
    public:
        explicit BLineEdit(QWidget* parent = 0)                       : KLineEdit(parent) { }
        explicit BLineEdit(const QString& text, QWidget* parent = 0)  : KLineEdit(text, parent) { }

    signals:
        void         focusLost();

    protected:
        virtual void focusOutEvent(QFocusEvent*)  { emit focusLost(); }
};

#endif // BIRTHDAYDLG_H

// vim: et sw=4:
