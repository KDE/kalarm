/*
 *  editdlgprivate.h  -  private classes for editdlg.cpp
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007,2008 by David Jarvie <djarvie@kde.org>
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

#include <QFrame>
#include <ktextedit.h>
#include <ktabwidget.h>
class QDragEnterEvent;
class QShowEvent;
class CheckBox;
class LineEdit;


class PageFrame : public QFrame
{
		Q_OBJECT
	public:
		explicit PageFrame(QWidget* parent = 0) : QFrame(parent) { }
	protected:
		virtual void     showEvent(QShowEvent*)    { emit shown(); }
	signals:
		void             shown();
};

class TabWidget : public KTabWidget
{
		Q_OBJECT
	public:
		explicit TabWidget(QWidget* parent) : KTabWidget(parent) {}
		void updateTabSizes();
		virtual QSize sizeHint() const         { return minimumSizeHint(); }
		virtual QSize minimumSizeHint() const;//  { return minimumSize().isEmpty() ? KTabWidget::minimumSizeHint() : minimumSize(); }
};

class TextEdit : public KTextEdit
{
		Q_OBJECT
	public:
		explicit TextEdit(QWidget* parent);
		virtual QSize sizeHint() const         { return minimumSizeHint(); }
		virtual QSize minimumSizeHint() const  { return minimumSize(); }
	protected:
		virtual void dragEnterEvent(QDragEnterEvent*);
};

class CommandEdit : public QWidget
{
		Q_OBJECT
	public:
		explicit CommandEdit(QWidget* parent);
		bool      isScript() const;
		void      setScript(bool);
		QString   text() const;
		void      setText(const AlarmText&);
		void      setReadOnly(bool);
		virtual QSize minimumSizeHint() const;
		virtual QSize sizeHint() const   { return minimumSizeHint(); }

	signals:
		void      scriptToggled(bool);

	private slots:
		void      slotCmdScriptToggled(bool);

	private:
		CheckBox* mTypeScript;      // entering a script
		LineEdit* mCommandEdit;     // command line edit box
		TextEdit* mScriptEdit;      // script edit box
};

#endif // EDITDLGPRIVATE_H
