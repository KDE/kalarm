/*
 *  spinbox.h  -  spin box with shift-click step value and read-only option
 *  Program:  kalarm
 *  (C) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
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


/**
 *  The SpinBox class provides a QSpinBox with accelerated stepping using the shift key.
 *
 *  A separate step increment may optionally be specified for use when the shift key is
 *  held down. Typically this would be larger than the normal step. Then, when the user
 *  clicks the spin buttons, he/she can increment or decrement the value faster by holding
 *  the shift key down.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short Spin box with accelerated shift key stepping and read-only option.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class SpinBox : public QSpinBox
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		SpinBox(QWidget* parent = 0, const char* name = 0);
		/** Constructor.
		 *  @param minValue The minimum value which the spin box can have.
		 *  @param maxValue The maximum value which the spin box can have.
		 *  @param step The (unshifted) step interval.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		SpinBox(int minValue, int maxValue, int step = 1, QWidget* parent = 0, const char* name = 0);
		/** Returns true if the widget is read only. */
		bool         isReadOnly() const                    { return mReadOnly; }
		/** Sets whether the spin box can be changed by the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void         setReadOnly(bool);
		/** Returns whether the spin box value text is selected when its value is stepped. */
		bool         selectOnStep() const                  { return mSelectOnStep; }
		/** Sets whether the spin box value text should be selected when its value is stepped. */
		void         setSelectOnStep(bool sel)             { mSelectOnStep = sel; }
		/** Adds a value to the current value of the spin box. */
		void         addValue(int change)                  { addValue(change, false); }
		/** Returns the minimum value of the spin box. */
		int          minValue() const                      { return mMinValue; }
		/** Returns the maximum value of the spin box. */
		int          maxValue() const                      { return mMaxValue; }
		/** Sets the minimum value of the spin box. */
		void         setMinValue(int val);
		/** Sets the maximum value of the spin box. */
		void         setMaxValue(int val);
		/** Sets the minimum and maximum values of the spin box. */
		void         setRange(int minValue, int maxValue)  { setMinValue(minValue);  setMaxValue(maxValue); }
		/** Returns the specified value clamped to the range of the spin box. */
		int          bound(int val) const;
		/** Returns the unshifted step increment, i.e. the amount by which the spin box value
		 *  changes when a spin button is clicked without the shift key being pressed.
		 */
		int          lineStep() const                      { return mLineStep; }
		/** Sets the unshifted step increment, i.e. the amount by which the spin box value
		 *  changes when a spin button is clicked without the shift key being pressed.
		 */
		void         setLineStep(int step);
		/** Returns the shifted step increment, i.e. the amount by which the spin box value
		 *  changes when a spin button is clicked while the shift key is pressed.
		 */
		int          lineShiftStep() const                 { return mLineShiftStep; }
		/** Sets the shifted step increment, i.e. the amount by which the spin box value
		 *  changes when a spin button is clicked while the shift key is pressed.
		 */
		void         setLineShiftStep(int step);
	public slots:
		/** Increments the value of the spin box by the unshifted step increment. */
		virtual void stepUp();
		/** Decrements the value of the spin box by the unshifted step increment. */
		virtual void stepDown();
	signals:
		/** Signal emitted when the spin box's value is stepped (by the shifted or unshifted increment).
		 *  @param step The requested step in the spin box's value. Note that the actual change in value
		 *              may have been less than this.
		 */
		void         stepped(int step);

	protected:
		/** A virtual method called whenever the value of the spin box has changed. */
		virtual void valueChange();
		/** Returns the initial adjustment to the value for a shift step up or down.
		 * The default is to step up or down to the nearest multiple of the shift
		 * increment, so the adjustment returned is for stepping up the decrement
		 * required to round down to a multiple of the shift increment <= current value,
		 * or for stepping down the increment required to round up to a multiple of the
		 * shift increment >= current value.
		 * This method's caller then adjusts the resultant value if necessary to cater
		 * for the widget's minimum/maximum value, and wrapping.
		 * This should really be a static method, but it needs to be virtual...
		 */
		virtual int  shiftStepAdjustment(int oldValue, int shiftStep);
		/** Receives events destined for the spin widget or for the edit field. */
		virtual bool eventFilter(QObject*, QEvent*);
		/** Updates the contents of the embedded QLineEdit to reflect the current value
		 *  using mapValueToText(). Also enables/disables the up/down push buttons accordingly.
		 */
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
