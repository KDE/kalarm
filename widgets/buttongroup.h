/*
 *  buttongroup.h  -  QButtonGroup with an extra signal and KDE 2 compatibility
 *  Program:  kalarm
 *  (C) 2002, 2004 by David Jarvie <software@astrojar.org.uk>
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
#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <qbuttongroup.h>

/*=============================================================================
= Class ButtonGroup
= Emits the signal buttonSet(int) whenever any of its buttons changes state,
= for whatever reason.
=============================================================================*/

class ButtonGroup : public QButtonGroup
{
		Q_OBJECT
	public:
		ButtonGroup(QWidget* parent, const char* name = 0);
		ButtonGroup(const QString& title, QWidget* parent, const char* name = 0);
		ButtonGroup(int strips, Qt::Orientation, QWidget* parent, const char* name = 0);
		ButtonGroup(int strips, Qt::Orientation, const QString& title, QWidget* parent, const char* name = 0);
		void         insert(QButton*, int id = -1);
		virtual void setButton(int id)  { QButtonGroup::setButton(id);  emit buttonSet(id); }
#if QT_VERSION < 300
		void         setInsideMargin(int) { }
	protected:
		virtual void childEvent(QChildEvent*);
	private:
		int          defaultAlignment;
#endif
	private slots:
		void         slotButtonToggled(bool);
	signals:
		void         buttonSet(int id);
};

#endif // BUTTONGROUP_H
