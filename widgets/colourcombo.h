/*
 *  colourcombo.h  -  colour selection combo box
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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

#ifndef COLOURCOMBO_H
#define COLOURCOMBO_H

#include <qcombobox.h>
#include "colourlist.h"


class ColourCombo : public QComboBox
{
		Q_OBJECT
		Q_PROPERTY(QColor color READ color WRITE setColor)
	public:
		ColourCombo(QWidget* parent = 0L, const char* name = 0L, const QColor& = 0xFFFFFF);
		QColor       color() const               { return mSelectedColour; }
		QColor       colour() const              { return mSelectedColour; }
		void         setColor(const QColor& c)   { setColour(c); }
		void         setColour(const QColor&);
		void         setColours(const ColourList&);
		bool         isCustomColour() const      { return !currentItem(); }
		void         setReadOnly(bool);
	signals:
		void         activated(const QColor&);    // a new colour box has been selected
		void         highlighted(const QColor&);  // a new item has been highlighted
	public slots:
		virtual void setEnabled(bool);
	protected:
		virtual void resizeEvent(QResizeEvent*);
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	private slots:
		void         slotActivated(int index);
		void         slotHighlighted(int index);
		void         slotPreferencesChanged();
	private:
		void         addColours();
		void         drawCustomItem(QRect&, bool insert);

		ColourList   mColourList;      // the sorted colours to display
		QColor       mSelectedColour;  // currently selected colour
		QColor       mCustomColour;    // current colour of the Custom item
		bool         mReadOnly;        // value cannot be changed
		bool         mDisabled;
};

#endif // COLOURCOMBO_H
