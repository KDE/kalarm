/*
 *  buttongroup.cpp  -  QButtonGroup with an extra signal and KDE 2 compatibility
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
#include "kalarm.h"

#include <qlayout.h>
#include <qbutton.h>
#include <kdialog.h>

#include "buttongroup.moc"


ButtonGroup::ButtonGroup(QWidget* parent, const char* name)
	: QButtonGroup(parent, name)
#if QT_VERSION < 300
	, defaultAlignment(-1)
#endif
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(const QString& title, QWidget* parent, const char* name)
	: QButtonGroup(title, parent, name)
#if QT_VERSION < 300
	, defaultAlignment(-1)
#endif
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, QWidget* parent, const char* name)
	: QButtonGroup(strips, orient, parent, name)
#if QT_VERSION < 300
	, defaultAlignment(orient == Qt::Horizontal ? Qt::AlignLeft : 0)
#endif
{
	connect(this, SIGNAL(clicked(int)), SIGNAL(buttonSet(int)));
}

ButtonGroup::ButtonGroup(int strips, Qt::Orientation orient, const QString& title, QWidget* parent, const char* name)
	: QButtonGroup(strips, orient, title, parent, name)
#if QT_VERSION < 300
	, defaultAlignment(orient == Qt::Horizontal ? Qt::AlignLeft : 0)
#endif
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

#if QT_VERSION < 300
/******************************************************************************
 * In a vertical layout, this adds widgets left aligned instead of QT 2's
 * centre aligned default.
 */
void ButtonGroup::childEvent(QChildEvent *ce)
{
	if (defaultAlignment != -1  &&  ce->inserted()  &&  ce->child()->isWidgetType())
	{
		QWidget* child = (QWidget*)ce->child();
		if (!child->isTopLevel())    //ignore dialogs etc.
		{
			((QBoxLayout*)layout())->addWidget(child, 0, defaultAlignment);
			QApplication::postEvent(this, new QEvent(QEvent::LayoutHint));
		}
	}
}
#endif
