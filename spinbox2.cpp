/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons (for QT3)
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 */

#include <qglobal.h>
#if QT_VERSION >= 300

#include <kdebug.h>
#include <qstyle.h>
#if defined(QT_ACCESSIBILITY_SUPPORT)
#include "qaccessible.h"
#endif
#include "spinbox2.h"
#include "spinbox2.moc"


SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QWidget(parent, name)
{
	spinbox = new SpinBox2_(this);
	initSpinBox2();
}


SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QWidget(parent, name)
{
	spinbox = new SpinBox2_(minValue, maxValue, step, this);
	spinbox->setSteps(step, step2);
	initSpinBox2();
}


void SpinBox2::initSpinBox2()
{
	setFocusProxy(spinbox);
	updown2 = new QSpinWidget(this, "updown2");
	connect(updown2, SIGNAL(stepUpPressed()), this, SLOT(pageUp()));
	connect(updown2, SIGNAL(stepDownPressed()), this, SLOT(pageDown()));
	connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(valueChange()));
	updateDisplay();
}

void SpinBox2::setButtonSymbols(QSpinBox::ButtonSymbols newSymbols)
{
	if (spinbox->buttonSymbols() == newSymbols)
		return;
	spinbox->setButtonSymbols(newSymbols);
	switch (newSymbols)
	{
		case QSpinWidget::UpDownArrows:
			updown2->setButtonSymbols(QSpinWidget::UpDownArrows);
			break;
		case QSpinWidget::PlusMinus:
			updown2->setButtonSymbols(QSpinWidget::PlusMinus);
			break;
	}
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
    updateDisplay();
    emit valueChanged(spinbox->value());
    emit valueChanged(spinbox->currentValueText());
#if defined(QT_ACCESSIBILITY_SUPPORT)
    QAccessible::updateAccessibility(this, 0, QAccessible::ValueChanged);
#endif
}

// Called after the widget has been resized.
void SpinBox2::resizeEvent(QResizeEvent* e)
{
	int w = spinbox->upRect().width();
	int m = spinbox->style().defaultFrameWidth() + 1;
	spinbox->setGeometry(w - m, 0, width() - w + m, height());
	updown2->setGeometry(0, 0, w, height());
}

QSize SpinBox2::sizeHint() const
{
	QSize size = spinbox->sizeHint();
	size.setWidth(size.width() + updown2->downRect().width());
	return size;
}

QSize SpinBox2::minimumSizeHint() const
{
	QSize size = spinbox->minimumSizeHint();
	size.setWidth(size.width() + updown2->downRect().width());
	return size;
}

void SpinBox2::updateDisplay()
{
	bool enabled = isEnabled();
	bool wrap    = spinbox->wrapping();
	int  value   = spinbox->value();
	updown2->setUpEnabled(enabled  &&  (wrap || value < spinbox->maxValue()));
	updown2->setDownEnabled(enabled  &&  (wrap || value > spinbox->minValue()));
	// force an update of the QSpinBox display
//	spinbox->setWrapping(!wrap);
//	spinbox->setWrapping(wrap);
}

void SpinBox2::styleChange(QStyle& old)
{
	updown2->arrange();
//	spinbox->styleChange(old);
}

void SpinBox2::enabledChange(bool)
{
	bool enabled = isEnabled();
	spinbox->setEnabled(enabled);
	updown2->setEnabled(enabled);
	updateDisplay();
}

#endif // QT_VERSION >= 300
