/*
 *  editdlgprivate.h  -  private classes for editdlg.cpp
 *  Program:  kalarm
 *  (C) 2003 by David Jarvie <software@astrojar.org.uk>
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

#ifndef EDITDLGPRIVATE_H
#define EDITDLGPRIVATE_H

#define KDE2_QTEXTEDIT_EDIT     // for KDE2 QTextEdit compatibility
#include <qtextedit.h>
#include <klineedit.h>


class PageFrame : public QFrame
{
		Q_OBJECT
	public:
		PageFrame(QWidget* parent = 0, const char* name = 0) : QFrame(parent, name) { }
	protected:
		virtual void     showEvent(QShowEvent*)    { emit shown(); }
	signals:
		void             shown();
};

class LineEdit : public KLineEdit
{
		Q_OBJECT
	public:
		explicit LineEdit(bool url, QWidget* parent = 0, const char* name = 0);
		void setNoSelect()   { noSelect = true; }
	protected:
		virtual void focusInEvent(QFocusEvent*);
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
	private:
		bool  noSelect;
};

class TextEdit : public QTextEdit
{
		Q_OBJECT
	public:
		TextEdit(QWidget* parent, const char* name = 0);
};

#endif // EDITDLGPRIVATE_H
