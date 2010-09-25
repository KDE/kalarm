/*
 *  templatelistview.h  -  widget showing list of alarm templates
 *  Program:  kalarm
 *  Copyright © 2007,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef TEMPLATELISTVIEW_H
#define TEMPLATELISTVIEW_H

#include "kalarm.h"

#include "eventlistview.h"


class TemplateListView : public EventListView
{
		Q_OBJECT
	public:
		explicit TemplateListView(QWidget* parent = 0);
		virtual void setModel(QAbstractItemModel*);
};

class TemplateListDelegate : public EventListDelegate
{
		Q_OBJECT
	public:
		explicit TemplateListDelegate(TemplateListView* parent = 0)
		           : EventListDelegate(parent) {}
		virtual void edit(KAEvent*, EventListView*);
};

#endif // TEMPLATELISTVIEW_H

