/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons (for QT3)
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include <qglobal.h>
#if QT_VERSION >= 300

#include <qstyle.h>

#include "spinbox2.moc"


SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	updown2Frame = new QFrame(this);
	spinboxFrame = new QFrame(this);
	updown2 = new SB2_SpinWidget(updown2Frame, "updown2");
	spinbox = new SB2_SpinBox(this, spinboxFrame);
	initSpinBox2();
}


SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	updown2Frame = new QFrame(this);
	spinboxFrame = new QFrame(this);
	updown2 = new SB2_SpinWidget(minValue, maxValue, step2, updown2Frame, "updown2");
	spinbox = new SB2_SpinBox(minValue, maxValue, step, this, spinboxFrame);
	spinbox->setSteps(step, step2);
	initSpinBox2();
}

void SpinBox2::initSpinBox2()
{
	selectOnStep = false;
	setFocusProxy(spinbox);
	connect(spinbox, SIGNAL(valueChanged(int)), SLOT(valueChange()));
	connect(spinbox, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
	connect(spinbox, SIGNAL(valueChanged(const QString&)), SIGNAL(valueChanged(const QString&)));
	connect(updown2, SIGNAL(stepped(int)), SLOT(stepped2(int)));
}

void SpinBox2::setButtonSymbols(QSpinBox::ButtonSymbols newSymbols)
{
	if (spinbox->buttonSymbols() == newSymbols)
		return;
	spinbox->setButtonSymbols(newSymbols);
	updown2->setButtonSymbols(newSymbols);
}

void SpinBox2::addVal(int change)
{
	int newval = spinbox->value() + change;
	if (newval > spinbox->maxValue())
	{
		if (spinbox->wrapping())
		{
			int range = spinbox->maxValue() - spinbox->minValue() + 1;
			newval = spinbox->minValue() + (newval - spinbox->maxValue() - 1) % range;
		}
		else
			newval = spinbox->maxValue();
	}
	else if (newval < spinbox->minValue())
	{
		if (spinbox->wrapping())
		{
			int range = spinbox->maxValue() - spinbox->minValue() + 1;
			newval = spinbox->maxValue() - (spinbox->minValue() - 1 - newval) % range;
		}
		else
			newval = spinbox->minValue();
	}
	spinbox->setValue(newval);
}

void SpinBox2::valueChange()
{
	bool blocked = updown2->signalsBlocked();
	updown2->blockSignals(true);
	updown2->setValue(spinbox->value());
	updown2->blockSignals(blocked);
}

void SpinBox2::stepped2(int direction)
{
	bool focus = selectOnStep && updown2->hasFocus();
	if (focus)
		spinbox->setFocus();    // make displayed text be selected, as for stepping with the spinbox buttons
	int step = spinbox->pageStep();
	addVal(direction >= 0 ? step : -step);
	if (focus)
		updown2->setFocus();
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


void SpinBox2::SB2_SpinBox::valueChange()
{
	bool focus = !spinBox2->selectOnStep && hasFocus();
	if (focus)
		clearFocus();     // prevent selection of the spin box text
	QSpinBox::valueChange();
	if (focus)
		setFocus();
}

void SB2_SpinWidget::valueChange()
{
	bool focus = hasFocus();
	if (focus)
		clearFocus();     // prevent selection of the invisible spin box text
	QSpinBox::valueChange();
	if (focus)
		setFocus();
}

#endif // QT_VERSION >= 300
