/*
 *  spinbox.h  -  spin box with read-only option and shift-click step value
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie  software@astrojar.org.uk
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
 */

#ifndef SPINBOX_H
#define SPINBOX_H

#include <qspinbox.h>


class SpinBox : public QSpinBox
{
		Q_OBJECT
	public:
		SpinBox(QWidget* parent = 0, const char* name = 0);
		SpinBox(int minValue, int maxValue, int step = 1, QWidget* parent = 0, const char* name = 0);
		bool         isReadOnly() const                    { return mReadOnly; }
		void         setReadOnly(bool);
		bool         selectOnStep() const                  { return mSelectOnStep; }
		void         setSelectOnStep(bool sel)             { mSelectOnStep = sel; }
		void         addValue(int change)                  { addValue(change, false); }
		int          minValue() const                      { return mMinValue; }
		int          maxValue() const                      { return mMaxValue; }
		void         setMinValue(int val);
		void         setMaxValue(int val);
		void         setRange(int minValue, int maxValue)  { setMinValue(minValue);  setMaxValue(maxValue); }
		int          bound(int val) const;
		int          lineStep() const                      { return mLineStep; }
		void         setLineStep(int step);
		int          lineShiftStep() const                 { return mLineShiftStep; }
		void         setLineShiftStep(int step);
	public slots:
		virtual void stepUp();
		virtual void stepDown();
	signals:
		void         stepped(int step);
	protected:
		virtual void valueChange();
		virtual int  shiftStepAdjustment(int oldValue, int shiftStep);
		virtual bool eventFilter(QObject*, QEvent*);
		virtual void updateDisplay();
	private slots:
		void         textEdited();
	private:
		void         init();
		void         addValue(int change, bool current);
		int          whichButton(const QPoint&);
		bool         setShiftStepping(bool);

		enum { NO_BUTTON, UP, DOWN };

		int          mMinValue;
		int          mMaxValue;
		int          mLineStep;         // step when spin arrows are pressed
		int          mLineShiftStep;    // step when spin arrows are pressed with shift key
		int          mCurrentButton;    // current spin widget button
		bool         mShiftMouse;       // true while left button is being held down with shift key
		bool         mShiftMinBound;    // true if a temporary minimum bound has been set during shift stepping
		bool         mShiftMaxBound;    // true if a temporary maximum bound has been set during shift stepping
		bool         mSelectOnStep;     // select the editor text whenever spin buttons are clicked (default)
		bool         mReadOnly;         // value cannot be changed
		bool         mSuppressSignals;
		bool         mEdited;           // text field has been edited
};

#endif // SPINBOX_H
