/*
 *  colourcombo.h  -  colour selection combo box
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

#ifndef COLOURCOMBO_H
#define COLOURCOMBO_H

#include <kcolorcombo.h>


class ColourCombo : public KColorCombo
{
	public:
		ColourCombo(QWidget* parent = 0L, const char* name = 0L, const QColor& = 0xFFFFFF);
		void    setColour(const QColor&);
	public slots:
		virtual void setEnabled(bool);
	protected:
		/**
		 * @reimplemented
		 */
		virtual void resizeEvent(QResizeEvent*);
	private:
		void         deleteColours();
		void         addDisabledColour();

		QColor       selection;
		bool         disabled;
};

#endif // COLOURCOMBO_H
