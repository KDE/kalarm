/*
 *  repetitionbutton.h  -  pushbutton and dialog to specify alarm repetition
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KAlarmCal/Repetition>

#include <QDialog>
#include <QPushButton>

using namespace KAlarmCal;

class QGroupBox;
class ButtonGroup;
class RadioButton;
class SpinBox;
class TimeSelector;
class TimePeriod;
class RepetitionDlg;

namespace KCalendarCore { class Duration; }

class RepetitionButton : public QPushButton
{
    Q_OBJECT
public:
    RepetitionButton(const QString& caption, bool waitForInitialisation, QWidget* parent);
    void           set(const Repetition&);
    void           set(const Repetition&, bool dateOnly, int maxDuration = -1);
    void           initialise(const Repetition&, bool dateOnly, int maxDuration = -1);   // use only after needsInitialisation() signal
    void           activate()               { activate(false); }
    Repetition     repetition() const       { return mRepetition; }
    virtual void   setReadOnly(bool ro)     { mReadOnly = ro; }
    virtual bool   isReadOnly() const       { return mReadOnly; }

Q_SIGNALS:
    void           needsInitialisation();   // dialog has been created and needs set() to be called
    void           changed();               // the repetition dialog has been edited

private Q_SLOTS:
    void           slotPressed()            { activate(mWaitForInit); }

private:
    void           activate(bool waitForInitialisation);
    void           displayDialog();

    RepetitionDlg* mDialog {nullptr};
    Repetition     mRepetition;       // repetition interval and count
    int            mMaxDuration {-1}; // maximum allowed duration in minutes, or -1 for infinite
    bool           mDateOnly {false}; // hours/minutes cannot be displayed
    bool           mWaitForInit;      // Q_EMIT needsInitialisation() when button pressed, display when initialise() called
    bool           mReadOnly {false};
};


class RepetitionDlg : public QDialog
{
    Q_OBJECT
public:
    RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent = nullptr);
    void       setReadOnly(bool);
    void       set(const Repetition&, bool dateOnly = false, int maxDuration = -1);
    Repetition repetition() const;   // get the repetition interval and count

private Q_SLOTS:
    void       typeClicked();
    void       countChanged(int);
    void       intervalChanged(const KCalendarCore::Duration&);
    void       durationChanged(const KCalendarCore::Duration&);
    void       repetitionToggled(bool);

private:
    TimeSelector*  mTimeSelector;
    QGroupBox*     mButtonBox;
    ButtonGroup*   mButtonGroup;
    RadioButton*   mCountButton;
    SpinBox*       mCount;
    RadioButton*   mDurationButton;
    TimePeriod*    mDuration;
    int            mMaxDuration;     // maximum allowed duration in minutes, or -1 for infinite
    bool           mDateOnly;        // hours/minutes cannot be displayed
    bool           mReadOnly;        // the widget is read only
};

// vim: et sw=4:
