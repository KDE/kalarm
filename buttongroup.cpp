/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal and KDE 2 compatibility
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
#include "kalarm.h"

#include <qlayout.h>
#include <kdialog.h>

#include "buttongroup.moc"


ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, QWidget* parent, const char* name)
#if QT_VERSION >= 300
	: QButtonGroup(strips, orient, parent, name)
{ }
#else
	: QButtonGroup(parent, name)
{
	init(orient);
}
#endif

ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, const QString& title, QWidget* parent, const char* name)
#if QT_VERSION >= 300
	: QButtonGroup(strips, orient, title, parent, name)
{ }
#else
	: QButtonGroup(title, parent, name)
{
	init(orient);
}

void ButtonGroup::init(Qt::Orientation orient)
{
	if (orient == Qt::Horizontal)
	{
		new QVBoxLayout(this, 0, KDialog::spacingHint());
		defaultAlignment = Qt::AlignLeft;
	}
	else
	{
		new QHBoxLayout(this, 0, KDialog::spacingHint());
		defaultAlignment = 0;
	}
}

void ButtonGroup::addWidget(QWidget* w, int stretch, int alignment)
{
	((QBoxLayout*)layout())->addWidget(w, stretch, (alignment ? alignment : defaultAlignment));
}
#endif
