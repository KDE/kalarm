/*
 *  editdlgprivate.h  -  private classes for editdlg.cpp
 *  Program:  kalarm
 *  Copyright © 2003-2005,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef EDITDLGPRIVATE_H
#define EDITDLGPRIVATE_H

#include <ktextedit.h>


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

class TextEdit : public KTextEdit
{
		Q_OBJECT
	public:
		TextEdit(QWidget* parent, const char* name = 0);
	protected:
		virtual void dragEnterEvent(QDragEnterEvent*);
};

#endif // EDITDLGPRIVATE_H
