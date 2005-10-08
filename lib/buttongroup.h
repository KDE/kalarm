/*
 *  buttongroup.h  -  QButtonGroup with an extra signal
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
#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <QButtonGroup>
class QChildEvent;
class QAbstractButton;


/**
 *  @short A QButtonGroup with signal on new selection.
 *
 *  The ButtonGroup class provides an enhanced version of the QButtonGroup class.
 *
 *  It emits an additional signal, buttonSet(QAbstractButton*), whenever any of its
 *  buttons changes state, for whatever reason, including programmatic control. (The
 *  QButtonGroup class only emits signals when buttons are clicked on by the user.)
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ButtonGroup : public QButtonGroup
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 */
		ButtonGroup(QObject* parent);
		/** Adds a button to the group.
		 *  This overrides the add() method of QButtonGroup, which should really be a virtual method...
		 *  @param button The button to insert.
		 */
		void         add(QAbstractButton* button);

	private slots:
		void         slotButtonToggled(bool);
	signals:
		/** Signal emitted whenever whenever any button in the group changes state,
		 *  for whatever reason.
		 *  @param button The button which is now selected.
		 */
		void         buttonSet(QAbstractButton* button);
};

#endif // BUTTONGROUP_H
