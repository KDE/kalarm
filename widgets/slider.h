/*
 *  slider.h  -  slider control with read-only option
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie  software@astrojar.org.uk
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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef SLIDER_H
#define SLIDER_H

#include <qslider.h>


class Slider : public QSlider
{
		Q_OBJECT
		Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
	public:
		Slider(QWidget* parent = 0, const char* name = 0);
		Slider(Orientation, QWidget* parent = 0, const char* name = 0);
		Slider(int minValue, int maxValue, int pageStep, int value, Orientation,
		       QWidget* parent = 0, const char* name = 0);
		bool  isReadOnly() const  { return mReadOnly; }
		void  setReadOnly(bool);
	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	private:
		bool    mReadOnly;      // value cannot be changed
};

#endif // SLIDER_H
