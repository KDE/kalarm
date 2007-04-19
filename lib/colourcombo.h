/*
 *  colourcombo.h  -  colour selection combo box
 *  Program:  kalarm
 *  Copyright © 2001-2003,2005-2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef COLOURCOMBO_H
#define COLOURCOMBO_H

#include <kcolorcombo.h>
#include "colourlist.h"

class QMouseEvent;
class QResizeEvent;
class QKeyEvent;


/**
 *  @short A colour selection combo box with read-only option.
 *
 *  The ColourCombo class is a KColorCombo with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ColourCombo : public KColorCombo
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param defaultColour The colour which is selected by default.
		 */
		explicit ColourCombo(QWidget* parent = 0, const QColor& defaultColour = 0xFFFFFF);
		/** Returns the selected colour. */
		QColor       colour() const              { return color(); }
		/** Sets the selected colour to @p c. */
		void         setColour(const QColor& c)  { setColor(c); }
		/** Initialises the list of colours to @p list. */
		void         setColours(const ColourList& list);
		/** Returns true if the widget is read only. */
		bool         isReadOnly() const          { return mReadOnly; }
		/** Sets whether the combo box can be changed by the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		virtual void setReadOnly(bool readOnly);
	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	private slots:
		void         slotPreferencesChanged();
	private:
		ColourList   mColourList;      // the sorted colours to display
		bool         mReadOnly;        // value cannot be changed
};

#endif // COLOURCOMBO_H
