/*
 *  checkbox.h  -  check box with focus widget and read-only options
 *  Program:  kalarm
 *  (C) 2002, 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <qcheckbox.h>


/**
 *  The CheckBox class is a QCheckBox with the ability to transfer focus to another
 *  widget when checked, and with a read-only option.
 *
 *  Another widget may be specified as the focus widget for the check box. Whenever
 *  the user clicks on the check box so as to set its state to checked, focus is
 *  automatically transferred to the focus widget.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short QCheckBox with focus widget and read-only options.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class CheckBox : public QCheckBox
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		CheckBox(QWidget* parent, const char* name = 0);
		/** Constructor.
		 *  @param text Text to display.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		CheckBox(const QString& text, QWidget* parent, const char* name = 0);
		/** Returns true if the widget is read only. */
		bool     isReadOnly() const          { return mReadOnly; }
		/** Sets whether the check box is read-only for the user. If read-only,
		 *  its state cannot be changed by the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void     setReadOnly(bool readOnly);
		/** Returns the widget which receives focus when the user selects the check box by clicking on it. */
		QWidget* focusWidget() const         { return mFocusWidget; }
		/** Specifies a widget to receive focus when the user selects the check box by clicking on it.
		 *  @param widget Widget to receive focus.
		 *  @param enable If true, @p widget will be enabled before receiving focus. If
		 *                false, the enabled state of @p widget will be left unchanged when
		 *                the check box is clicked.
		 */
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
