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

#include "spinbox2.moc"


SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	updown2Frame = new QFrame(this);
	spinboxFrame = new QFrame(this);
	updown2 = new SpinBox(updown2Frame, "updown2");
	spinbox = new SB2_SpinBox(0, 1, 1, this, spinboxFrame);
	init();
}


SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mMinValue(minValue),
	  mMaxValue(maxValue)
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

void SpinBox2::stepPage(int step)
{
	if (abs(step) == updown2->lineStep())
		spinbox->setValue(updown2->value());
	else
	{
		int val           = spinbox->value();
		int pageStep      = updown2->lineStep();
		int pageShiftStep = updown2->lineShiftStep();
		int adjust = (step > 0) ? -((val - val % pageStep) % pageShiftStep)
		                        : pageShiftStep - (((val - val % pageStep) + pageShiftStep - 1) % pageShiftStep + 1);
		spinbox->addValue(adjust + step);
	}
	bool focus = spinbox->selectOnStep() && updown2->hasFocus();
	if (focus)
		spinbox->selectAll();
}

// Called when the widget is about to be displayed.
// (At construction time, the spin button widths cannot be determined correctly,
//  so we need to wait until now to definitively rearrange the widget.)
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

void SpinBox2::getMetrics() const
{
	QRect rect = updown2->style().querySubControlMetrics(QStyle::CC_SpinWidget, updown2, QStyle::SC_SpinWidgetButtonField);
	xUpdown2 = rect.left();
	wUpdown2 = updown2->width() - xUpdown2;
	xSpinbox = spinbox->style().querySubControlMetrics(QStyle::CC_SpinWidget, spinbox, QStyle::SC_SpinWidgetEditField).left();
	wGap = 0;

	// Make style-specific adjustments for a better appearance
	if (style().isA("QMotifPlusStyle"))
	{
		xSpinbox = 0;      // show the edit control left border
		wGap = 2;          // leave a space to the right of the left-hand pair of spin buttons
	}
}

#endif // QT_VERSION >= 300
