/*
 *  pushbutton.h  -  push button with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2003,2005,2006,2009 by David Jarvie <djarvie@kde.org>
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

#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include <QPushButton>
class QMouseEvent;
class QKeyEvent;
class KGuiItem;
class KIcon;


/**
 *  @short A QPushButton with read-only option.
 *
 *  The PushButton class is a QPushButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class PushButton : public QPushButton
{
        Q_OBJECT
        Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit PushButton(QWidget* parent);
        /** Constructor for a push button which displays a text.
         *  @param gui    The text, icon etc. to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const KGuiItem& gui, QWidget* parent);
        /** Constructor for a push button which displays a text.
         *  @param text The text to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const QString& text, QWidget* parent);
        /** Constructor for a push button which displays an icon and a text.
         *  @param icon The icon to show on the button.
         *  @param text The text to show on the button.
         *  @param parent The parent object of this widget.
         */
        PushButton(const KIcon& icon, const QString& text, QWidget* parent);
        /** Sets whether the push button is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to enable its action.
         *  @param noHighlight True to prevent the button being highlighted on mouse-over.
         */
        virtual void  setReadOnly(bool readOnly, bool noHighlight = false);
        /** Returns true if the widget is read only. */
        virtual bool  isReadOnly() const  { return mReadOnly; }
    protected:
        virtual void mousePressEvent(QMouseEvent*);
        virtual void mouseReleaseEvent(QMouseEvent*);
        virtual void mouseMoveEvent(QMouseEvent*);
        virtual void keyPressEvent(QKeyEvent*);
        virtual void keyReleaseEvent(QKeyEvent*);
        virtual bool event(QEvent*);
    private:
        Qt::FocusPolicy mFocusPolicy;   // default focus policy for the PushButton
        bool            mReadOnly;      // value cannot be changed
        bool            mNoHighlight;   // don't highlight on mouse hover, if read-only
};

#endif // PUSHBUTTON_H

// vim: et sw=4:
