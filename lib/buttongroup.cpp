/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal and KDE 2 compatibility
 *  Program:  kalarm
 *  Copyright (c) 2002, 2004 by David Jarvie <software@astrojar.org.uk>
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
#include "kalarm.h"

#include <qlayout.h>
#include <qbutton.h>
#include <kdialog.h>

#include "buttongroup.moc"


ButtonGroup::ButtonGroup(QWidget* parent, const char* name)
	: QButtonGroup(parent, name)
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(const QString& title, QWidget* parent, const char* name)
	: QButtonGroup(title, parent, name)
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, QWidget* parent, const char* name)
	: QButtonGroup(strips, orient, parent, name)
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, const QString& title, QWidget* parent, const char* name)
	: QButtonGroup(strips, orient, title, parent, name)
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

/******************************************************************************
 * Inserts a button in the group.
 * This should really be a virtual method...
 */
int ButtonGroup::insert(QButton* button, int id)
{
	id = QButtonGroup::insert(button, id);
	connect(button, SIGNAL(toggled(bool)), SLOT(slotButtonToggled(bool)));
	return id;
}

/******************************************************************************
 * Called when one of the member buttons is toggled.
 */
void ButtonGroup::slotButtonToggled(bool)
{
	emit buttonSet(selectedId());
}
