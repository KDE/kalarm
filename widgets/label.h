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


/**
 *  The Label class provides a text display, with special behaviour when a radio
 *  button is set as a buddy.
 *
 *  The Label object in effect acts as if it were part of the buddy radio button,
 *  in that when the label's accelerator key is pressed, the radio button receives
 *  focus and is switched on. When a non-radio button is specified as a buddy, the
 *  behaviour is the same as for QLabel.
 *
 *  @short QLabel with option for a buddy radio button.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class Label : public QLabel
{
		Q_OBJECT
		friend class LabelFocusWidget;
	public:
		/** Constructs an empty label.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 *  @param f    @see QWidget constructor for details.
		 */
		Label(QWidget* parent, const char* name = 0, WFlags f = 0);
		/** Constructs a label that displays @p text.
		 *  @param text   Text string to display.
		 *  @param parent The parent object of this widget.
		 *  @param name   The name of this widget.
		 *  @param f      @see QWidget constructor for details.
		 */
		Label(const QString& text, QWidget* parent, const char* name = 0, WFlags = 0);
		/** Constructs a label, with a buddy widget, that displays @p text.
		 *  @param buddy  Buddy widget which receives the keyboard focus when the
		 *                label's accelerator key is pressed. If @p buddy is a radio
		 *                button, @p buddy is in addition selected when the
		 *                accelerator key is pressed.
		 *  @param text   Text string to display.
		 *  @param parent The parent object of this widget.
		 *  @param name   The name of this widget.
		 *  @param f      @see QWidget constructor for details.
		 */
		Label(QWidget* buddy, const QString& text, QWidget* parent, const char* name = 0, WFlags = 0);
		/** Sets the label's buddy widget which receives the keyboard focus when the
		 *  label's accelerator key is pressed. If @p buddy is a radio button,
		 *  @p buddy is in addition selected when the accelerator key is pressed.
		 */
		virtual void      setBuddy(QWidget* buddy);
	protected:
		virtual void      drawContents(QPainter* p)  { QLabel::drawContents(p); }
	private slots:
		void              buddyDead();
	private:
		void              activated();
		QRadioButton*     mRadioButton;   // buddy widget if it's a radio button, else 0
		LabelFocusWidget* mFocusWidget;
};


// Private class for use by Label
class LabelFocusWidget : public QWidget
{
		Q_OBJECT
	public:
		LabelFocusWidget(QWidget* parent, const char* name = 0);
	protected:
		virtual void focusInEvent(QFocusEvent*);
};

#endif // LABEL_H
