/*
 *  buttongroup.h  -  QButtonGroup with an extra signal and Qt 2 compatibility
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


/**
 *  The ButtonGroup class provides an enhanced version of the QButtonGroup class.
 *
 *  It emits an additional signal, buttonSet(int), whenever any of its buttons
 *  changes state, for whatever reason, including programmatic control. (The
 *  QButtonGroup class only emits signals when buttons are clicked on by the user.)
 *  The class also provides Qt 2 compatibility.
 *
 *  @short QButtonGroup with signal on new selection, plus Qt 2 compatibility.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class ButtonGroup : public QButtonGroup
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ButtonGroup(QWidget* parent, const char* name = 0);
		/** Constructor.
		 *  @param title The title displayed for this button group.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ButtonGroup(const QString& title, QWidget* parent, const char* name = 0);
		/** Constructor.
		 *  @param strips The number of rows or columns of buttons.
		 *  @param orient The orientation (Qt::Horizontal or Qt::Vertical) of the button group.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ButtonGroup(int strips, Qt::Orientation orient, QWidget* parent, const char* name = 0);
		/** Constructor.
		 *  @param strips The number of rows or columns of buttons.
		 *  @param orient The orientation (Qt::Horizontal or Qt::Vertical) of the button group.
		 *  @param title The title displayed for this button group.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		ButtonGroup(int strips, Qt::Orientation orient, const QString& title, QWidget* parent, const char* name = 0);
		/** Inserts a button in the group.
		 *  This overrides the insert() method of QButtonGroup, which should really be a virtual method...
		 *  @param button The button to insert.
		 *  @param id The identifier for the button.
		 *  @return The identifier of the inserted button.
		 */
		int          insert(QButton* button, int id = -1);
		/** Sets the button with the specified identifier to be on. If this is an exclusive group,
		 *  all other buttons in the group will be set off. The buttonSet() signal is emitted.
		 *  @param id The identifier of the button to set on.
		 */
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
		/** Signal emitted whenever whenever any button in the group changes state,
		 *  for whatever reason.
		 *  @param id The identifier of the button which is now selected.
		 */
		void         buttonSet(int id);
};

#endif // BUTTONGROUP_H
