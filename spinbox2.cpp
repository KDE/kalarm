/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <qpushbutton.h>
#include <qlineedit.h>
#include "spinbox2.h"
#include "spinbox2.moc"


SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QSpinBox(parent, name)
{
	initSpinBox2();
}


SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QSpinBox(minValue, maxValue, step, parent, name)
{
	setSteps(step, step2);
	initSpinBox2();
}


void SpinBox2::initSpinBox2()
{
#if QT_VERSION >= 300
	updown2 = new QSpinWidget(this, "updown2");
	connect(updown2, SIGNAL(stepUpPressed()), this, SLOT(pageUp()));
	connect(updown2, SIGNAL(stepDownPressed()), this, SLOT(pageDown()));
#else
	QPushButton* up = upButton();
	up2 = new QPushButton( this, "up2" );
	up2->setFocusPolicy(up->focusPolicy());
	up2->setAutoDefault(up->autoDefault());
	up2->setAutoRepeat(up->autoRepeat());

	QPushButton* down = downButton();
	down2 = new QPushButton( this, "down2" );
	down2->setFocusPolicy(down->focusPolicy());
	down2->setAutoDefault(down->autoDefault());
	down2->setAutoRepeat(down->autoRepeat());

	connect(up2, SIGNAL(pressed()), this, SLOT(pageUp()));
	connect(down2, SIGNAL(pressed()), this, SLOT(pageDown()));
#endif

	updateDisplay();
}

void SpinBox2::setButtonSymbols(ButtonSymbols newSymbols)
{
	if (buttonSymbols() == newSymbols)
		return;
	QSpinBox::setButtonSymbols(newSymbols);
#if QT_VERSION >= 300
	switch (newSymbols)
	{
		case UpDownArrows:
			updown2->setButtonSymbols(QSpinWidget::UpDownArrows);
			break;
		case PlusMinus:
			updown2->setButtonSymbols(QSpinWidget::PlusMinus);
			break;
	}
#else
	up2->setPixmap(*upButton()->pixmap());
	down2->setPixmap(*downButton()->pixmap());
#endif
}

void SpinBox2::addVal(int change)
{
	int newval = value() + change;
	if (newval > maxValue())
	{
		if (wrapping())
		{
			int range = maxValue() - minValue() + 1;
			newval = minValue() + (newval - maxValue() - 1) % range;
		}
		else
			newval = maxValue();
	}
	else if (newval < minValue())
	{
		if (wrapping())
		{
			int range = maxValue() - minValue() + 1;
			newval = maxValue() - (minValue() - 1 - newval) % range;
		}
		else
			newval = minValue();
	}
	setValue(newval);
}

/*!\reimp
*/
void SpinBox2::resizeEvent(QResizeEvent* e)
{
	QSpinBox::resizeEvent(e);
#if QT_VERSION >= 300
	if (!updown2)
		return;

	QRect spinbox(QPoint(0, 0), size());
	QRect edit = editor()->geometry();
	int margin = spinbox.right() - upRect().right();
	spinbox.setLeft(spinbox.left() + (spinbox.right() - edit.right() - edit.left()));
	updown2->setGeometry(margin, 0, margin + upRect().right(), height() - 1);
	QSpinBox::setGeometry(spinbox);
#else
	if (!up2 || !down2)
		return;

	QSize bs = upButton()->size();
	if (bs != up2->size())
	{
		up2->resize(bs);
		down2->resize(bs);
		up2->setPixmap(*upButton()->pixmap());
		down2->setPixmap(*downButton()->pixmap());
	}
	int margin = width() - upButton()->x() - bs.width();
	QRect edit = editor()->geometry();
	edit.setLeft(edit.left() + bs.width());
	editor()->setGeometry(edit);
	QRect frame = frameRect();
	if (frame.right() != width())
	{
		frame.setLeft(frame.left() + bs.width());
		setFrameRect(frame);
	}
	up2->move(margin, upButton()->y());
	down2->move(margin, downButton()->y());
#endif
}

/*!\reimp
*/
QSize SpinBox2::sizeHint() const
{
	QSize size = QSpinBox::sizeHint();
#if QT_VERSION >= 300
	size.setWidth(size.width() + updown2->downRect().width());
#else
	QFontMetrics fm = fontMetrics();
	int w = fontMetrics().height();
	if ( w < 12 ) 	// ensure enough space for the button pixmaps
		w = 12;
	w -= frameWidth() * 2;
	size.setWidth(size.width() + w);
#endif
	return size;
}

#if QT_VERSION >= 300
QSize SpinBox2::minimumSizeHint() const
{
	QSize size = QSpinBox::minimumSizeHint();
	size.setWidth(size.width() + updown2->downRect().width());
	return size;
}

void SpinBox2::updateDisplay()
{
	updown2->setUpEnabled(isEnabled()  &&  (wrapping() || value() < maxValue()));
	updown2->setDownEnabled(isEnabled()  &&  (wrapping() || value() > minValue()));
	QSpinBox::updateDisplay();
}

void SpinBox2::styleChange(QStyle& old)
{
	updown2->arrange();
	QSpinBox::styleChange(old);
}
#endif
