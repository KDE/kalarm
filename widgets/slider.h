/*
 *  slider.h  -  slider control with read-only option
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef SLIDER_H
#define SLIDER_H

#include <qslider.h>


/**
 *  The Slider class is a QSlider with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short QSlider with read-only option.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class Slider : public QSlider
{
		Q_OBJECT
		Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		Slider(QWidget* parent = 0, const char* name = 0);
		/** Constructor.
		 *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		Slider(Orientation orient, QWidget* parent = 0, const char* name = 0);
		/** Constructor.
		 *  @param minValue The minimum value which the slider can have.
		 *  @param maxValue The maximum value which the slider can have.
		 *  @param pageStep The page step increment.
		 *  @param value The initial value for the slider.
		 *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		Slider(int minValue, int maxValue, int pageStep, int value, Orientation orient,
		       QWidget* parent = 0, const char* name = 0);
		/** Returns true if the slider is read only. */
		bool  isReadOnly() const  { return mReadOnly; }
		/** Sets whether the slider is read-only for the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void  setReadOnly(bool readOnly);
	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	private:
		bool    mReadOnly;      // value cannot be changed by the user
};

#endif // SLIDER_H
