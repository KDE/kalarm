/*
 *  label.h  -  label with radiobutton buddy option
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef LABEL_H
#define LABEL_H

#include <qlabel.h>
class QRadioButton;


class Label : public QLabel
{
		Q_OBJECT
		friend class LabelFocusWidget;
	public:
		Label(QWidget* parent, const char* name = 0, WFlags = 0);
		Label(const QString& text, QWidget* parent, const char* name = 0, WFlags = 0);
		Label(QWidget* buddy, const QString& text, QWidget* parent, const char* name = 0, WFlags = 0);
		virtual void      setBuddy(QWidget*);
	protected:
		virtual void      drawContents(QPainter* p)  { QLabel::drawContents(p); }
	private slots:
		void              buddyDead();
	private:
		void              activated();
		QRadioButton*     mRadioButton;   // buddy widget if it's a radio button, else 0
		LabelFocusWidget* mFocusWidget;
};


class LabelFocusWidget : public QWidget
{
		Q_OBJECT
	public:
		LabelFocusWidget(QWidget* parent, const char* name = 0);
	protected:
		virtual void focusInEvent(QFocusEvent*);
};

#endif // LABEL_H
