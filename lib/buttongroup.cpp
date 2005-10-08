/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal
 *  Program:  kalarm
 *  Copyright (c) 2002, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <QAbstractButton>
#include "buttongroup.moc"


ButtonGroup::ButtonGroup(QObject* parent)
	: QButtonGroup(parent)
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

/******************************************************************************
 * Inserts a button in the group.
 * This should really be a virtual method...
 */
void ButtonGroup::addButton(QAbstractButton* button)
{
	QButtonGroup::addButton(button);
	connect(button, SIGNAL(toggled(bool)), SLOT(slotButtonToggled(bool)));
}

/******************************************************************************
 * Called when one of the member buttons is toggled.
 */
void ButtonGroup::slotButtonToggled(bool)
{
	emit buttonSet(checkedButton());
}
