/*
 *  spinbox.cpp  -  spin box with read-only option and shift-click step value
 *  Program:  kalarm
 *  (C) 2002, 2004 by David Jarvie <software@astrojar.org.uk>
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

#include <kdeversion.h>
#include <qlineedit.h>
#include <qobjectlist.h>
#include "spinbox.moc"


SpinBox::SpinBox(QWidget* parent, const char* name)
	: QSpinBox(0, 99999, 1, parent, name),
	  mMinValue(QSpinBox::minValue()),
	  mMaxValue(QSpinBox::maxValue())
{
	init();
}

SpinBox::SpinBox(int minValue, int maxValue, int step, QWidget* parent, const char* name)
	: QSpinBox(minValue, maxValue, step, parent, name),
	  mMinValue(minValue),
	  mMaxValue(maxValue)
{
	init();
}

void SpinBox::init()
{
	int step = QSpinBox::lineStep();
	mLineStep        = step;
	mLineShiftStep   = step;
	mCurrentButton   = NO_BUTTON;
	mShiftMouse      = false;
	mShiftMinBound   = false;
	mShiftMaxBound   = false;
	mSelectOnStep    = true;
	mReadOnly        = false;
	mSuppressSignals = false;
	mEdited          = false;

	// Find the spin widgets which are part of the spin boxes, in order to
	// handle their shift-button presses.
	QObjectList* spinwidgets = queryList("QSpinWidget", 0, false, true);
	QSpinWidget* spin = (QSpinWidget*)spinwidgets->getFirst();
	if (spin)
		spin->installEventFilter(this);   // handle shift-button presses
	delete spinwidgets;
	editor()->installEventFilter(this);   // handle shift-up/down arrow presses

#if KDE_IS_VERSION(3,1,90)
	// Detect when the text field is edited
	connect(editor(), SIGNAL(textChanged(const QString&)), SLOT(textEdited()));
#endif
}

void SpinBox::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		editor()->setReadOnly(ro);
		if (ro)
			setShiftStepping(false);
	}
}

int SpinBox::bound(int val) const
{
	return (val < mMinValue) ? mMinValue : (val > mMaxValue) ? mMaxValue : val;
}

void SpinBox::setMinValue(int val)
{
	mMinValue = val;
	QSpinBox::setMinValue(val);
	mShiftMinBound = false;
}

void SpinBox::setMaxValue(int val)
{
	mMaxValue = val;
	QSpinBox::setMaxValue(val);
	mShiftMaxBound = false;
}

void SpinBox::setLineStep(int step)
{
	mLineStep = step;
	if (!mShiftMouse)
		QSpinBox::setLineStep(step);
}

void SpinBox::setLineShiftStep(int step)
{
	mLineShiftStep = step;
	if (mShiftMouse)
		QSpinBox::setLineStep(step);
}

void SpinBox::stepUp()
{
	int step = QSpinBox::lineStep();
	addValue(step);
	emit stepped(step);
}

void SpinBox::stepDown()
{
	int step = -QSpinBox::lineStep();
	addValue(step);
	emit stepped(step);
}

/******************************************************************************
* Adds a positive or negative increment to the current value, wrapping as appropriate.
* If 'current' is true, any temporary 'shift' values for the range are used instead
* of the real maximum and minimum values.
*/
void SpinBox::addValue(int change, bool current)
{
	int newval = value() + change;
	int maxval = current ? QSpinBox::maxValue() : mMaxValue;
	int minval = current ? QSpinBox::minValue() : mMinValue;
	if (wrapping())
	{
		int range = maxval - minval + 1;
		if (newval > maxval)
			newval = minval + (newval - maxval - 1) % range;
		else if (newval < minval)
			newval = maxval - (minval - 1 - newval) % range;
	}
	else
	{
		if (newval > maxval)
			newval = maxval;
		else if (newval < minval)
			newval = minval;
	}
	setValue(newval);
}

void SpinBox::valueChange()
{
	if (!mSuppressSignals)
	{
		int val = value();
		if (mShiftMinBound  &&  val >= mMinValue)
		{
			// Reinstate the minimum bound now that the value has returned to the normal range.
			QSpinBox::setMinValue(mMinValue);
			mShiftMinBound = false;
		}
		if (mShiftMaxBound  &&  val <= mMaxValue)
		{
			// Reinstate the maximum bound now that the value has returned to the normal range.
			QSpinBox::setMaxValue(mMaxValue);
			mShiftMaxBound = false;
		}

		bool focus = !mSelectOnStep && hasFocus();
		if (focus)
			clearFocus();     // prevent selection of the spin box text
		QSpinBox::valueChange();
		if (focus)
			setFocus();
	}
}

/******************************************************************************
* Called whenever the line edit text is changed.
*/
void SpinBox::textEdited()
{
	mEdited = true;
}

void SpinBox::updateDisplay()
{
	mEdited = false;
	QSpinBox::updateDisplay();
}

/******************************************************************************
* Receives events destined for the spin widget or for the edit field.
*/
bool SpinBox::eventFilter(QObject* obj, QEvent* e)
{
	if (obj == editor())
	{
		if (e->type() == QEvent::KeyPress)
		{
			// Up and down arrow keys step the value
			QKeyEvent* ke = (QKeyEvent*)e;
			int key = ke->key();
			if (key == Qt::Key_Up  ||  key == Qt::Key_Down)
			{
				if (mReadOnly)
					return true;    // discard up/down arrow keys
				int step;
				if ((ke->state() & (Qt::ShiftButton | Qt::AltButton)) == Qt::ShiftButton)
				{
					// Shift stepping
					int val = value();
					if (key == Qt::Key_Up)
						step = mLineShiftStep - val % mLineShiftStep;
					else
						step = - ((val + mLineShiftStep - 1) % mLineShiftStep + 1);
				}
				else
					step = (key == Qt::Key_Up) ? mLineStep : -mLineStep;
				addValue(step, false);
				return true;
			}
		}
#if KDE_IS_VERSION(3,1,90)
		else if (e->type() == QEvent::Leave)
		{
			if (mEdited)
				interpretText();
		}
#endif
	}
	else
	{
		int etype = e->type();    // avoid switch compile warnings
		switch (etype)
		{
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonDblClick:
			{
				QMouseEvent* me = (QMouseEvent*)e;
				if (me->button() == Qt::LeftButton)
				{
					// It's a left button press. Set normal or shift stepping as appropriate.
					if (mReadOnly)
						return true;   // discard the event
					mCurrentButton = whichButton(me->pos());
					if (mCurrentButton == NO_BUTTON)
						return true;
					bool shift = (me->state() & (Qt::ShiftButton | Qt::AltButton)) == Qt::ShiftButton;
					if (setShiftStepping(shift))
						return true;     // hide the event from the spin widget
					return false;    // forward event to the destination widget
				}
				break;
			}
			case QEvent::MouseButtonRelease:
			{
				QMouseEvent* me = (QMouseEvent*)e;
				if (me->button() == Qt::LeftButton  &&  mShiftMouse)
				{
					setShiftStepping(false);    // cancel shift stepping
					return false;    // forward event to the destination widget
				}
				break;
			}
			case QEvent::MouseMove:
			{
				QMouseEvent* me = (QMouseEvent*)e;
				if (me->state() & Qt::LeftButton)
				{
					// The left button is down. Track which spin button it's in.
					if (mReadOnly)
						return true;   // discard the event
					int newButton = whichButton(me->pos());
					if (newButton != mCurrentButton)
					{
						// The mouse has moved to a new spin button.
						// Set normal or shift stepping as appropriate.
						mCurrentButton = newButton;
						bool shift = (me->state() & (Qt::ShiftButton | Qt::AltButton)) == Qt::ShiftButton;
						if (setShiftStepping(shift))
							return true;     // hide the event from the spin widget
					}
					return false;    // forward event to the destination widget
				}
				break;
			}
			case QEvent::KeyPress:
			case QEvent::KeyRelease:
			case QEvent::AccelOverride:      // this is needed to receive Shift presses!
			{
				QKeyEvent* ke = (QKeyEvent*)e;
				int key   = ke->key();
				int state = ke->state();
				if ((state & Qt::LeftButton)
				&&  (key == Qt::Key_Shift  ||  key == Qt::Key_Alt))
				{
					// The left mouse button is down, and the Shift or Alt key has changed
					if (mReadOnly)
						return true;   // discard the event
					state ^= (key == Qt::Key_Shift) ? Qt::ShiftButton : Qt::AltButton;    // new state
					bool shift = (state & (Qt::ShiftButton | Qt::AltButton)) == Qt::ShiftButton;
					if (!shift && mShiftMouse  ||  shift && !mShiftMouse)
					{
						// The effective shift state has changed.
						// Set normal or shift stepping as appropriate.
						if (setShiftStepping(shift))
							return true;     // hide the event from the spin widget
					}
				}
				break;
			}
		}
	}
	return QSpinBox::eventFilter(obj, e);
}

/******************************************************************************
* Set spin widget stepping to the normal or shift increment.
*/
bool SpinBox::setShiftStepping(bool shift)
{
	if (mCurrentButton == NO_BUTTON)
		shift = false;
	if (shift  &&  !mShiftMouse)
	{
		/* The value is to be stepped to a multiple of the shift increment.
		 * Adjust the value so that after the spin widget steps it, it will be correct.
		 * Then, if the mouse button is held down, the spin widget will continue to
		 * step by the shift amount.
		 */
		int val = value();
		int step = (mCurrentButton == UP) ? mLineShiftStep : (mCurrentButton == DOWN) ? -mLineShiftStep : 0;
		int adjust = shiftStepAdjustment(val, step);
		mShiftMouse = true;
		if (adjust)
		{
			/* The value is to be stepped by other than the shift increment,
			 * presumably because it is being set to a multiple of the shift
			 * increment. Achieve this by making the adjustment here, and then
			 * allowing the normal step processing to complete the job by
			 * adding/subtracting the normal shift increment.
			 */
			if (!wrapping())
			{
				// Prevent the step from going past the spinbox's range, or
				// to the minimum value if that has a special text unless it is
				// already at the minimum value + 1.
				int newval = val + adjust + step;
				int svt = specialValueText().isEmpty() ? 0 : 1;
				int minval = mMinValue + svt;
				if (newval <= minval  ||  newval >= mMaxValue)
				{
					// Stepping to the minimum or maximum value
					if (svt  &&  newval <= mMinValue  &&  val == mMinValue)
						newval = mMinValue;
					else
						newval = (newval <= minval) ? minval : mMaxValue;
					QSpinBox::setValue(newval);
					emit stepped(step);
					return true;
				}

				// If the interim value will lie outside the spinbox's range,
				// temporarily adjust the range to allow the value to be set.
				int tempval = val + adjust;
				if (tempval < mMinValue)
				{
					QSpinBox::setMinValue(tempval);
					mShiftMinBound = true;
				}
				else if (tempval > mMaxValue)
				{
					QSpinBox::setMaxValue(tempval);
					mShiftMaxBound = true;
				}
			}

			// Don't process changes since this new value will be stepped immediately
			mSuppressSignals = true;
			bool blocked = signalsBlocked();
			blockSignals(true);
			addValue(adjust, true);
			blockSignals(blocked);
			mSuppressSignals = false;
		}
		QSpinBox::setLineStep(mLineShiftStep);
	}
	else if (!shift  &&  mShiftMouse)
	{
		// Reinstate to normal (non-shift) stepping
		QSpinBox::setLineStep(mLineStep);
		QSpinBox::setMinValue(mMinValue);
		QSpinBox::setMaxValue(mMaxValue);
		mShiftMinBound = mShiftMaxBound = false;
		mShiftMouse = false;
	}
	return false;
}

/******************************************************************************
* Return the initial adjustment to the value for a shift step up or down.
* The default is to step up or down to the nearest multiple of the shift
* increment, so the adjustment returned is for stepping up the decrement
* required to round down to a multiple of the shift increment <= current value,
* or for stepping down the increment required to round up to a multiple of the
* shift increment >= current value.
* This method's caller then adjusts the resultant value if necessary to cater
* for the widget's minimum/maximum value, and wrapping.
* This should really be a static method, but it needs to be virtual...
*/
int SpinBox::shiftStepAdjustment(int oldValue, int shiftStep)
{
	if (oldValue == 0)
		return 0;
	if (shiftStep > 0)
	{
		if (oldValue >= 0)
			return -(oldValue % shiftStep);
		else
			return (-oldValue - 1) % shiftStep + 1 - shiftStep;
	}
	else
	{
		shiftStep = -shiftStep;
		if (oldValue >= 0)
			return shiftStep - ((oldValue - 1) % shiftStep + 1);
		else
			return (-oldValue) % shiftStep;
	}
}

/******************************************************************************
*  Find which spin widget button a mouse event is in.
*/
int SpinBox::whichButton(const QPoint& pos)
{
	if (upRect().contains(pos))
		return UP;
	if (downRect().contains(pos))
		return DOWN;
	return NO_BUTTON;
}
