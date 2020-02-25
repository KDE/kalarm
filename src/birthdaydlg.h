/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2002-2005,2007-2011 David Jarvie <djarvie@kde.org>
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

#include <KAlarmCal/KAEvent>

#include <QDialog>
#include <QVector>
#include <QLineEdit>

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
class QDialogButtonBox;

using namespace KAlarmCal;

class BirthdayDlg : public QDialog
{
        Q_OBJECT
    public:
        explicit BirthdayDlg(QWidget* parent = nullptr);
        QVector<KAEvent> events() const;

    protected Q_SLOTS:
        virtual void   slotOk();

    private Q_SLOTS:
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
        SpecialActionsButton* mSpecialActionsButton {nullptr};
        RepetitionButton*     mSubRepetition;
        QDialogButtonBox*     mButtonBox;
        QString               mPrefixText;   // last entered value of prefix text
        QString               mSuffixText;   // last entered value of suffix text
        KAEvent::Flags        mFlags;        // event flag bits
};


class BLineEdit : public QLineEdit
{
        Q_OBJECT
    public:
        explicit BLineEdit(QWidget* parent = nullptr)                       : QLineEdit(parent) {}
        explicit BLineEdit(const QString& text, QWidget* parent = nullptr)  : QLineEdit(text, parent) {}

    Q_SIGNALS:
        void         focusLost();

    protected:
        void         focusOutEvent(QFocusEvent*) override { Q_EMIT focusLost(); }
};

#endif // BIRTHDAYDLG_H

// vim: et sw=4:
