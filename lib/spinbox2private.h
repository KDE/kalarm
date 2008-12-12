/*
 *  spinbox2private.h  -  private classes for SpinBox2 (for Qt 3)
 *  Program:  kalarm
 *  Copyright © 2005,2006,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef SPINBOX2PRIVATE_H
#define SPINBOX2PRIVATE_H

#include <qcanvas.h>
#include "spinbox.h"


/*=============================================================================
= Class ExtraSpinBox
* Extra pair of spin buttons for SpinBox2.
* The widget is actually a whole spin box, but only the buttons are displayed.
=============================================================================*/

class ExtraSpinBox : public SpinBox
{
		Q_OBJECT
	public:
		explicit ExtraSpinBox(QWidget* parent, const char* name = 0)
		             : SpinBox(parent, name), mNewStylePending(false) { }
		ExtraSpinBox(int minValue, int maxValue, int step, QWidget* parent, const char* name = 0)
		             : SpinBox(minValue, maxValue, step, parent, name), mNewStylePending(false) { }
	signals:
		void         styleUpdated();
	protected:
		virtual void paintEvent(QPaintEvent*);
		virtual void styleChange(QStyle&)    { mNewStylePending = true; }
	private:
		bool         mNewStylePending;  // style has changed, but not yet repainted
};


/*=============================================================================
= Class SpinMirror
* Displays the left-to-right mirror image of a pair of spin buttons, for use
* as the extra spin buttons in a SpinBox2. All mouse clicks are passed on to
* the real extra pair of spin buttons for processing.
* Mirroring in this way allows styles with rounded corners to display correctly.
=============================================================================*/

class SpinMirror : public QCanvasView
{
		Q_OBJECT
	public:
		explicit SpinMirror(SpinBox*, QFrame* spinFrame, QWidget* parent = 0, const char* name = 0);
		void         setReadOnly(bool ro)        { mReadOnly = ro; }
		bool         isReadOnly() const          { return mReadOnly; }
		void         setNormalButtons(const QPixmap&);
		void         redraw(const QPixmap&);

	public slots:
		virtual void resize(int w, int h);
		void         redraw();

	protected:
		virtual void contentsMousePressEvent(QMouseEvent* e)        { contentsMouseEvent(e); }
		virtual void contentsMouseReleaseEvent(QMouseEvent* e)      { contentsMouseEvent(e); }
		virtual void contentsMouseMoveEvent(QMouseEvent* e)         { contentsMouseEvent(e); }
		virtual void contentsMouseDoubleClickEvent(QMouseEvent* e)  { contentsMouseEvent(e); }
		virtual void contentsWheelEvent(QWheelEvent*);
		virtual bool event(QEvent*);

	private:
		void         contentsMouseEvent(QMouseEvent*);

		SpinBox*     mSpinbox;        // spinbox whose spin buttons are being mirrored
		QFrame*      mSpinFrame;      // widget containing mSpinbox's spin buttons
		QWidget*     mSpinWidget;     // spin buttons in mSpinbox
		QPixmap      mNormalButtons;  // image of spin buttons in unpressed state
		bool         mReadOnly;       // value cannot be changed
};

#endif // SPINBOX2PRIVATE_H
