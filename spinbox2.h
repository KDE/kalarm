/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SPINBOX2_H
#define SPINBOX2_H

#include <qglobal.h>

#include <qspinbox.h>


/*
 * This class has a second pair of spin buttons at its left hand side.
 * These buttons use the inherited QRangeControl page step functions.
 */
class SpinBox2 : public QSpinBox
{
		Q_OBJECT
	public:
		SpinBox2(QWidget* parent = 0L, const char* name = 0L);
		SpinBox2(int minValue, int maxValue, int step = 1, int step2 = 1,
		              QWidget* parent = 0L, const char* name = 0L);
		void            setButtonSymbols(ButtonSymbols);
		QSize           sizeHint() const;
	public slots:
		virtual void    pageUp()               { addVal(pageStep()); }
		virtual void    pageDown()             { addVal(-pageStep()); }
	protected:
		void            resizeEvent(QResizeEvent*);
	private:
		void            initSpinBox2();
		void            addVal(int change);

#if QT_VERSION >= 300
	public:
		QSize           minimumSizeHint() const;
		QRect           upRect2() const        { return updown2->upRect(); }
		QRect           downRect2() const      { return updown2->downRect(); }
	protected:
		virtual void    updateDisplay();
		virtual void    styleChange(QStyle&);
	private:
		QSpinWidget*    updown2;
#else
	protected:
		QPushButton*    upButton2() const      { return up2; }
		QPushButton*    downButton2() const    { return down2; }
	private:
		QPushButton*    up2;
		QPushButton*    down2;
#endif
};

#endif // SPINBOX2_H
