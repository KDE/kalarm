/*
 *  combobox.h  -  combo box with read-only option
 *  Program:  kalarm
 *  (C) 2002, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <qcombobox.h>


/**
 *  The ComboBox class is a QComboBox with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short QComboBox with read-only option.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ComboBox : public QComboBox
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ComboBox(QWidget* parent = 0, const char* name = 0);
		/** Constructor.
		 *  @param rw True for a read-write combo box, false for a read-only combo box.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ComboBox(bool rw, QWidget* parent = 0, const char* name = 0);
		/** Returns true if the widget is read only. */
		bool  isReadOnly() const          { return mReadOnly; }
		/** Sets whether the combo box is read-only for the user. If read-only,
		 *  its state cannot be changed by the user.
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
		bool    mReadOnly;      // value cannot be changed
};

#endif // COMBOBOX_H
