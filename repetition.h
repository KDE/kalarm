/*
 *  repetition.h  -  pushbutton and dialogue to specify alarm repetition
 *  Program:  kalarm
 *  Copyright © 2004,2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef REPETITION_H
#define REPETITION_H

#include <qpushbutton.h>
#include <kdialogbase.h>

class ButtonGroup;
class RadioButton;
class SpinBox;
class TimeSelector;
class TimePeriod;
class RepetitionDlg;


class RepetitionButton : public QPushButton
{
		Q_OBJECT
	public:
		RepetitionButton(const QString& caption, bool waitForInitialisation, QWidget* parent, const char* name = 0);
		void           set(int interval, int count);
		void           set(int interval, int count, bool dateOnly, int maxDuration = -1);
		void           initialise(int interval, int count, bool dateOnly, int maxDuration = -1);   // use only after needsInitialisation() signal
		void           activate()               { activate(false); }
		int            interval() const         { return mInterval; }
		int            count() const            { return mCount; }
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
		int            mInterval;     // interval between repetitions, in minutes
		int            mCount;        // total number of repetitions, including first occurrence
		int            mMaxDuration;  // maximum allowed duration in minutes, or -1 for infinite
		bool           mDateOnly;     // hours/minutes cannot be displayed
		bool           mWaitForInit;  // emit needsInitialisation() when button pressed, display when initialise() called
		bool           mReadOnly;
};


class RepetitionDlg : public KDialogBase
{
		Q_OBJECT
	public:
		RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent = 0, const char* name = 0);
		void           setReadOnly(bool);
		void           set(int interval, int count, bool dateOnly = false, int maxDuration = -1);
		int            interval() const;     // get the interval between repetitions, in minutes
		int            count() const;        // get the repeat count

	private slots:
		void           typeClicked();
		void           intervalChanged(int);
		void           countChanged(int);
		void           durationChanged(int);
		void           repetitionToggled(bool);

	private:
		TimeSelector*  mTimeSelector;
		ButtonGroup*   mButtonGroup;
		RadioButton*   mCountButton;
		SpinBox*       mCount;
		RadioButton*   mDurationButton;
		TimePeriod*    mDuration;
		int            mMaxDuration;     // maximum allowed duration in minutes, or -1 for infinite
		bool           mDateOnly;        // hours/minutes cannot be displayed
		bool           mReadOnly;        // the widget is read only
};

#endif // REPETITION_H
