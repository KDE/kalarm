/*
 *  pushbutton.h  -  push button with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2003,2005,2006 by David Jarvie <software@astrojar.org.uk>
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

#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include <qpushbutton.h>


/**
 *  @short A QPushButton with read-only option.
 *
 *  The PushButton class is a QPushButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class PushButton : public QPushButton
{
		Q_OBJECT
		Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		explicit PushButton(QWidget* parent, const char* name = 0);
		/** Constructor for a push button which displays a text.
		 *  @param text The text to show on the button.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		PushButton(const QString& text, QWidget* parent, const char* name = 0);
		/** Constructor for a push button which displays an icon and a text.
		 *  @param icon The icon to show on the button.
		 *  @param text The text to show on the button.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		PushButton(const QIconSet& icon, const QString& text, QWidget* parent, const char* name = 0);
		/** Sets whether the push button is read-only for the user.
		 *  @param readOnly True to set the widget read-only, false to enable its action.
		 */
		virtual void  setReadOnly(bool readOnly);
		/** Returns true if the widget is read only. */
		virtual bool  isReadOnly() const  { return mReadOnly; }
	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	private:
		QWidget::FocusPolicy mFocusPolicy;   // default focus policy for the QPushButton
		bool                 mReadOnly;      // value cannot be changed
};

#endif // PUSHBUTTON_H
