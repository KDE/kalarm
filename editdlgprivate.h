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
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
		enum Type { Basic, Url, EmailAddresses };
		explicit LineEdit(Type, QWidget* parent = 0, const char* name = 0);
		explicit LineEdit(QWidget* parent = 0, const char* name = 0);
		void setNoSelect()   { mNoSelect = true; }
	protected:
		virtual void focusInEvent(QFocusEvent*);
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
	private:
		void  init(Type);

		bool  mNoSelect;
		bool  mEmailAddresses;
};

class TextEdit : public QTextEdit
{
		Q_OBJECT
	public:
		TextEdit(QWidget* parent, const char* name = 0);
};

#endif // EDITDLGPRIVATE_H
