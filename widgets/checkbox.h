/*
 *  checkbox.h  -  check box with read-only option
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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

#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <qcheckbox.h>


class CheckBox : public QCheckBox
{
		Q_OBJECT
	public:
		CheckBox(QWidget* parent, const char* name = 0);
		CheckBox(const QString& text, QWidget* parent, const char* name = 0);
		void     setReadOnly(bool);
		QWidget* focusWidget() const         { return mFocusWidget; }
		void     setFocusWidget(QWidget*, bool enable = true);
	protected:
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void mouseMoveEvent(QMouseEvent*);
		virtual void keyPressEvent(QKeyEvent*);
		virtual void keyReleaseEvent(QKeyEvent*);
	protected slots:
		void         slotClicked();
	private:
		QWidget::FocusPolicy mFocusPolicy;       // default focus policy for the QCheckBox
		QWidget*             mFocusWidget;       // widget to receive focus when button is clicked on
		bool                 mFocusWidgetEnable; // enable focus widget before setting focus
		bool                 mReadOnly;          // value cannot be changed
};

#endif // CHECKBOX_H
