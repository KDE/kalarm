/*
 *  spinbox2private.h  -  private classes for SpinBox2 (for Qt 3)
 *  Program:  kalarm
 *  (C) 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef SPINBOX2PRIVATE_H
#define SPINBOX2PRIVATE_H

#if QT_VERSION >= 300

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
		ExtraSpinBox(QWidget* parent, const char* name = 0)
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
		SpinMirror(SpinBox*, QWidget* parent = 0, const char* name = 0);
		void         setReadOnly(bool ro)        { mReadOnly = ro; }
		bool         isReadOnly() const          { return mReadOnly; }
		void         setNormalButtons(const QPixmap&);
		void         redraw(const QPixmap&);

	public slots:
		virtual void resize(int w, int h);

	protected:
		virtual void contentsMousePressEvent(QMouseEvent* e)        { contentsMouseEvent(e); }
		virtual void contentsMouseReleaseEvent(QMouseEvent* e)      { contentsMouseEvent(e); }
		virtual void contentsMouseMoveEvent(QMouseEvent* e)         { contentsMouseEvent(e); }
		virtual void contentsMouseDoubleClickEvent(QMouseEvent* e)  { contentsMouseEvent(e); }

	private:
		void         contentsMouseEvent(QMouseEvent*);

		SpinBox*     mSpinbox;        // spinbox whose spin buttons are being mirrored
		QWidget*     mSpinWidget;     // spin buttons in mSpinbox
		QPixmap      mNormalButtons;  // image of spin buttons in unpressed state
		bool         mReadOnly;       // value cannot be changed
};

#endif // QT_VERSION >= 300
#endif // SPINBOX2PRIVATE_H
