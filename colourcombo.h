/*
 *  colourcombo.h  -  colour selection combo box
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef COLOURCOMBO_H
#define COLOURCOMBO_H

#include <kcolorcombo.h>


class ColourCombo : public KColorCombo
{
	public:
		ColourCombo(QWidget* parent = 0L, const char* name = 0L, const QColor& = 0xFFFFFF);
		void    setColour(const QColor&);
	protected:
		/**
		 * @reimplemented
		 */
		virtual void resizeEvent(QResizeEvent*);
	private:
		void    deleteColours();
		QColor  selection;
};

#endif // COLOURCOMBO_H
