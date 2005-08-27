/*
 *  templatemenuaction.h  -  menu action to select a template
 *  Program:  kalarm
 *  Copyright (C) 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef TEMPLATEMENUACTION_H
#define TEMPLATEMENUACTION_H

#include <kactionclasses.h>
class KAEvent;


class TemplateMenuAction : public KActionMenu
{
		Q_OBJECT
	public:
		TemplateMenuAction(const QString& label, const QString& icon, QObject* receiver,
		                   const char* slot, KActionCollection* parent, const char* name = 0);

	signals:
		void   selected(const KAEvent&);

	private slots:
		void   slotInitMenu();
		void   slotSelected(int id);

	private:
		QStringList mOriginalTexts;   // menu item texts without added ampersands
};

#endif // TEMPLATEMENUACTION_H
