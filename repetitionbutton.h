/*
 *  repetitionbutton.h  -  pushbutton and dialog to specify alarm repetition
 *  Program:  kalarm
 *  Copyright © 2004-2007,2009-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef REPETITIONBUTTON_H
#define REPETITIONBUTTON_H

#include <KAlarmCal/repetition.h>

#include <kdialog.h>
#include <QPushButton>

using namespace KAlarmCal;

class QGroupBox;
class ButtonGroup;
class RadioButton;
class SpinBox;
class TimeSelector;
class TimePeriod;
class RepetitionDlg;

#ifdef USE_AKONADI
namespace KCalCore { class Duration; }
#else
namespace KCal { class Duration; }
#endif

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

    signals:
        void           needsInitialisation();   // dialog has been created and needs set() to be called
        void           changed();               // the repetition dialog has been edited

    private slots:
        void           slotPressed()            { activate(mWaitForInit); }

    private:
        void           activate(bool waitForInitialisation);
        void           displayDialog();

        RepetitionDlg* mDialog;
        Repetition     mRepetition;   // repetition interval and count
        int            mMaxDuration;  // maximum allowed duration in minutes, or -1 for infinite
        bool           mDateOnly;     // hours/minutes cannot be displayed
        bool           mWaitForInit;  // emit needsInitialisation() when button pressed, display when initialise() called
        bool           mReadOnly;
};


class RepetitionDlg : public KDialog
{
        Q_OBJECT
    public:
        RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent = 0);
        void       setReadOnly(bool);
        void       set(const Repetition&, bool dateOnly = false, int maxDuration = -1);
        Repetition repetition() const;   // get the repetition interval and count

    private slots:
        void       typeClicked();
        void       countChanged(int);
#ifdef USE_AKONADI
        void       intervalChanged(const KCalCore::Duration&);
        void       durationChanged(const KCalCore::Duration&);
#else
        void       intervalChanged(const KCal::Duration&);
        void       durationChanged(const KCal::Duration&);
#endif
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

#endif // REPETITIONBUTTON_H

// vim: et sw=4:
