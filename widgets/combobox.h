/*
 *  combobox.h  -  combo box with read-only option
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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


class ComboBox : public QComboBox
{
		Q_OBJECT
	public:
		ComboBox(QWidget* parent = 0, const char* name = 0);
		ComboBox(bool rw, QWidget* parent = 0, const char* name = 0);
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

#endif // COMBOBOX_H
