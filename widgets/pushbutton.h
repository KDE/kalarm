/*
 *  pushbutton.h  -  push button with read-only option
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include <qpushbutton.h>


class PushButton : public QPushButton
{
		Q_OBJECT
	public:
		PushButton(QWidget* parent, const char* name = 0);
		PushButton(const QString& text, QWidget* parent, const char* name = 0);
		PushButton(const QIconSet& icon, const QString& text, QWidget* parent, const char* name = 0);
		void  setReadOnly(bool);
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
