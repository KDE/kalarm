/*
 *  checkbox.h  -  check box with focus widget and read-only options
 *  Program:  kalarm
 *  Copyright © 2002,2003,2005,2006 David Jarvie <djarvie@kde.org>
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

#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <QCheckBox>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QCheckBox with focus widget and read-only options.
 *
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
 *  @author David Jarvie <djarvie@kde.org>
 */
class CheckBox : public QCheckBox
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit CheckBox(QWidget* parent);
        /** Constructor.
         *  @param text Text to display.
         *  @param parent The parent object of this widget.
         */
        CheckBox(const QString& text, QWidget* parent);
        /** Returns true if the widget is read only. */
        bool         isReadOnly() const          { return mReadOnly; }
        /** Sets whether the check box is read-only for the user. If read-only,
         *  its state cannot be changed by the user.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void setReadOnly(bool readOnly);
        /** Returns the widget which receives focus when the user selects the check box by clicking on it. */
        QWidget*     focusWidget() const         { return mFocusWidget; }
        /** Specifies a widget to receive focus when the user selects the check box by clicking on it.
         *  @param widget Widget to receive focus.
         *  @param enable If true, @p widget will be enabled before receiving focus. If
         *                false, the enabled state of @p widget will be left unchanged when
         *                the check box is clicked.
         */
        void         setFocusWidget(QWidget* widget, bool enable = true);
    protected:
        void         mousePressEvent(QMouseEvent*) override;
        void         mouseReleaseEvent(QMouseEvent*) override;
        void         mouseMoveEvent(QMouseEvent*) override;
        void         keyPressEvent(QKeyEvent*) override;
        void         keyReleaseEvent(QKeyEvent*) override;
    protected Q_SLOTS:
        void         slotClicked();
    private:
        Qt::FocusPolicy mFocusPolicy;           // default focus policy for the QCheckBox
        QWidget*        mFocusWidget {nullptr}; // widget to receive focus when button is clicked on
        bool            mFocusWidgetEnable;     // enable focus widget before setting focus
        bool            mReadOnly {false};      // value cannot be changed
};

#endif // CHECKBOX_H

// vim: et sw=4:
