/*
 *  buttongroup.h  -  QButtonGroup with an extra signal and KDE 2 compatibility
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */
#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <qbuttongroup.h>


class ButtonGroup : public QButtonGroup
{
		Q_OBJECT
	public:
		ButtonGroup(QWidget* parent, const char* name = 0L);
		ButtonGroup(const QString& title, QWidget* parent, const char* name = 0L);
		ButtonGroup(int strips, Qt::Orientation, QWidget* parent, const char* name = 0L);
		ButtonGroup(int strips, Qt::Orientation, const QString& title, QWidget* parent, const char* name = 0L);
		virtual void setButton(int id)  { QButtonGroup::setButton(id);  emit buttonSet(id); }
#if QT_VERSION < 300
		void setInsideMargin(int) { }
//??//		void addWidget(QWidget*, int stretch = 0, int alignment = 0);
	protected:
		virtual void childEvent(QChildEvent*);
	private:
		void init(Qt::Orientation);
		int  defaultAlignment;
#endif
	signals:
		void buttonSet(int id);
};

#endif // BUTTONGROUP_H
