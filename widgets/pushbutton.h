/*
 *  pushbutton.h  -  push button with read-only option
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include <qpushbutton.h>


/**
 *  The PushButton class is a QPushButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short QPushButton with read-only option.
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
		PushButton(QWidget* parent, const char* name = 0);
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
		virtual void  setReadOnly(bool);
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
