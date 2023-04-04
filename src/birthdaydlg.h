/*
 *  birthdaydlg.h  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "kalarmcalendar/kaevent.h"

#include <KLineEdit>

#include <QDialog>
#include <QList>

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
class QSortFilterProxyModel;
class QDialogButtonBox;

using namespace KAlarmCal;

class BirthdayDlg : public QDialog
{
    Q_OBJECT
public:
    explicit BirthdayDlg(QWidget* parent = nullptr);
    QList<KAEvent> events() const;

  protected Q_SLOTS:
    virtual void   slotOk();

private Q_SLOTS:
    void           slotSelectionChanged();
    void           slotTextLostFocus();
    void           resizeViewColumns();
    void           setColours(const QColor& fg, const QColor& bg);

private:
    void           setSortModelSelectionList();

    QSortFilterProxyModel* mBirthdaySortModel;   // actually a BirthdaySortModel, inherited from QSortFilterProxyModel
    QTreeView*             mListView;
    KLineEdit*             mName {nullptr};
    BLineEdit*             mPrefix;
    BLineEdit*             mSuffix;
    Reminder*              mReminder;
    SoundPicker*           mSoundPicker;
    FontColourButton*      mFontColourButton;
    CheckBox*              mConfirmAck;
    LateCancelSelector*    mLateCancel;
    SpecialActionsButton*  mSpecialActionsButton {nullptr};
    RepetitionButton*      mSubRepetition;
    QDialogButtonBox*      mButtonBox;
    QString                mPrefixText;   // last entered value of prefix text
    QString                mSuffixText;   // last entered value of suffix text
    KAEvent::Flags         mFlags;        // event flag bits
    int                    mBirthdayModel_NameColumn {-1};
    int                    mBirthdayModel_DateColumn {-1};
    int                    mBirthdayModel_DateRole {-1};
};


/** Line edit with a focusLost() signal. */
class BLineEdit : public KLineEdit
{
    Q_OBJECT
public:
    explicit BLineEdit(QWidget* parent = nullptr)                       : KLineEdit(parent) {}
    explicit BLineEdit(const QString& text, QWidget* parent = nullptr)  : KLineEdit(text, parent) {}

Q_SIGNALS:
    void focusLost();

protected:
    void focusOutEvent(QFocusEvent*) override { Q_EMIT focusLost(); }
};

// vim: et sw=4:
