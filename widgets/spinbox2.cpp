/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons (for Qt 3)
 *  Program:  kalarm
 *  (C) 2001, 2002, 2004 by David Jarvie <software@astrojar.org.uk>
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

#include <qglobal.h>
#if QT_VERSION >= 300

#include <stdlib.h>

#include <qstyle.h>
#include <qobjectlist.h>
#include <qapplication.h>

#include "spinbox2.moc"

int SpinBox2::mReverseLayout = -1;

SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReverseWithLayout(true)
{
	updown2Frame = new QFrame(this);
	spinboxFrame = new QFrame(this);
	updown2 = new SpinBox(updown2Frame, "updown2");
//	spinbox = new SB2_SpinBox(0, 1, 1, this, spinboxFrame);
	spinbox = new SB2_SpinBox(this, spinboxFrame);
	init();
}

SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReverseWithLayout(true)
{
	updown2Frame = new QFrame(this);
	spinboxFrame = new QFrame(this);
	updown2 = new SpinBox(minValue, maxValue, step2, updown2Frame, "updown2");
	spinbox = new SB2_SpinBox(minValue, maxValue, step, this, spinboxFrame);
	setSteps(step, step2);
	init();
}

void SpinBox2::init()
{
	if (mReverseLayout < 0)
		mReverseLayout = QApplication::reverseLayout() ? 1 : 0;
	mMinValue      = spinbox->minValue();
	mMaxValue      = spinbox->maxValue();
	mLineStep      = spinbox->lineStep();
	mLineShiftStep = spinbox->lineShiftStep();
	mPageStep      = updown2->lineStep();
	mPageShiftStep = updown2->lineShiftStep();
	spinbox->setSelectOnStep(false);    // default
	updown2->setSelectOnStep(false);    // always false
	setFocusProxy(spinbox);
	updown2->setFocusPolicy(QWidget::NoFocus);
	connect(spinbox, SIGNAL(valueChanged(int)), SLOT(valueChange()));
	connect(spinbox, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
	connect(spinbox, SIGNAL(valueChanged(const QString&)), SIGNAL(valueChanged(const QString&)));
	connect(updown2, SIGNAL(stepped(int)), SLOT(stepPage(int)));
}

void SpinBox2::setReadOnly(bool ro)
{
	if (static_cast<int>(ro) != static_cast<int>(spinbox->isReadOnly()))
	{
		spinbox->setReadOnly(ro);
		updown2->setReadOnly(ro);
	}
}

void SpinBox2::setReverseWithLayout(bool reverse)
{
	if (reverse != mReverseWithLayout)
	{
		mReverseWithLayout = reverse;
		setSteps(mLineStep, mPageStep);
		setShiftSteps(mLineShiftStep, mPageShiftStep);
	}
}

void SpinBox2::setLineStep(int step)
{
	mLineStep = step;
	if (reverseButtons())
		updown2->setLineStep(step);   // reverse layout, but still set the right buttons
	else
		spinbox->setLineStep(step);
}

void SpinBox2::setSteps(int line, int page)
{
	mLineStep = line;
	mPageStep = page;
	if (reverseButtons())
	{
		updown2->setLineStep(line);   // reverse layout, but still set the right buttons
		spinbox->setLineStep(page);
	}
	else
	{
		spinbox->setLineStep(line);
		updown2->setLineStep(page);
	}
}

void SpinBox2::setShiftSteps(int line, int page)
{
	mLineShiftStep = line;
	mPageShiftStep = page;
	if (reverseButtons())
	{
		updown2->setLineShiftStep(line);   // reverse layout, but still set the right buttons
		spinbox->setLineShiftStep(page);
	}
	else
	{
		spinbox->setLineShiftStep(line);
		updown2->setLineShiftStep(page);
	}
}

void SpinBox2::setButtonSymbols(QSpinBox::ButtonSymbols newSymbols)
{
	if (spinbox->buttonSymbols() == newSymbols)
		return;
	spinbox->setButtonSymbols(newSymbols);
	updown2->setButtonSymbols(newSymbols);
}

int SpinBox2::bound(int val) const
{
	return (val < mMinValue) ? mMinValue : (val > mMaxValue) ? mMaxValue : val;
}

void SpinBox2::setMinValue(int val)
{
	mMinValue = val;
	spinbox->setMinValue(val);
	updown2->setMinValue(val);
}

void SpinBox2::setMaxValue(int val)
{
	mMaxValue = val;
	spinbox->setMaxValue(val);
	updown2->setMaxValue(val);
}

void SpinBox2::valueChange()
{
	int val = spinbox->value();
	bool blocked = updown2->signalsBlocked();
	updown2->blockSignals(true);
	updown2->setValue(val);
	updown2->blockSignals(blocked);
}

/******************************************************************************
* Called when the widget is about to be displayed.
* (At construction time, the spin button widths cannot be determined correctly,
*  so we need to wait until now to definitively rearrange the widget.)
*/
void SpinBox2::showEvent(QShowEvent*)
{
	arrange();
}

QSize SpinBox2::sizeHint() const
{
	getMetrics();
	QSize size = spinbox->sizeHint();
	size.setWidth(size.width() - xSpinbox + wUpdown2 + wGap);
	return size;
}

QSize SpinBox2::minimumSizeHint() const
{
	getMetrics();
	QSize size = spinbox->minimumSizeHint();
	size.setWidth(size.width() - xSpinbox + wUpdown2 + wGap);
	return size;
}

void SpinBox2::arrange()
{
	getMetrics();
	updown2Frame->setGeometry(QStyle::visualRect(QRect(0, 0, wUpdown2, height()), this));
	updown2->setGeometry(-xUpdown2, 0, updown2->width(), height());
	spinboxFrame->setGeometry(QStyle::visualRect(QRect(wUpdown2 + wGap, 0, width() - wUpdown2 - wGap, height()), this));
	spinbox->setGeometry(-xSpinbox, 0, spinboxFrame->width() + xSpinbox, height());
}

/******************************************************************************
* Calculate the width and position of the extra pair of spin buttons.
* Style-specific adjustments are made for a better appearance. 
*/
void SpinBox2::getMetrics() const
{
	QRect rect = updown2->style().querySubControlMetrics(QStyle::CC_SpinWidget, updown2, QStyle::SC_SpinWidgetButtonField);
	if (style().inherits("PlastikStyle"))
		rect.setLeft(rect.left() - 1);    // Plastik excludes left border from spin widget rectangle
	xUpdown2 = mReverseLayout ? 0 : rect.left();
	wUpdown2 = updown2->width() - rect.left();
	xSpinbox = spinbox->style().querySubControlMetrics(QStyle::CC_SpinWidget, spinbox, QStyle::SC_SpinWidgetEditField).left();
	wGap = 0;

	// Make style-specific adjustments for a better appearance
	if (style().inherits("QMotifPlusStyle"))
	{
		xSpinbox = 0;      // show the edit control left border
		wGap = 2;          // leave a space to the right of the left-hand pair of spin buttons
	}
}

/******************************************************************************
* Called when the extra pair of spin buttons is clicked to step the value.
* Normally this is a page step, but with a right-to-left language where the
* button functions are reversed, this is a line step.
*/
void SpinBox2::stepPage(int step)
{
	if (abs(step) == updown2->lineStep())
		spinbox->setValue(updown2->value());
	else
	{
		// It's a shift step
		int oldValue = spinbox->value();
		if (!reverseButtons())
		{
			// The button pairs have the normal function.
			// Page shift stepping - step up or down to a multiple of the
			// shift page increment, leaving unchanged the part of the value
			// which is the remainder from the page increment.
			if (oldValue >= 0)
				oldValue -= oldValue % updown2->lineStep();
			else
				oldValue += (-oldValue) % updown2->lineStep();
		}
		int adjust = spinbox->shiftStepAdjustment(oldValue, step);
		if (adjust == -step
		&&  (step > 0  &&  oldValue + step >= spinbox->maxValue()
		  || step < 0  &&  oldValue + step <= spinbox->minValue()))
			adjust = 0;    // allow stepping to the minimum or maximum value
		spinbox->addValue(adjust + step);
	}
	bool focus = spinbox->selectOnStep() && updown2->hasFocus();
	if (focus)
		spinbox->selectAll();
}

/******************************************************************************
* Return the initial adjustment to the value for a shift step up or down, for
* the main (visible) spin box.
* Normally this is a line step, but with a right-to-left language where the
* button functions are reversed, this is a page step.
*/
int SpinBox2::SB2_SpinBox::shiftStepAdjustment(int oldValue, int shiftStep)
{
	if (owner->reverseButtons())
	{
		// The button pairs have the opposite function from normal.
		// Page shift stepping - step up or down to a multiple of the
		// shift page increment, leaving unchanged the part of the value
		// which is the remainder from the page increment.
		if (oldValue >= 0)
			oldValue -= oldValue % lineStep();
		else
			oldValue += (-oldValue) % lineStep();
	}
	return SpinBox::shiftStepAdjustment(oldValue, shiftStep);
}

#endif // QT_VERSION >= 300
