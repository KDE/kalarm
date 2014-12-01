/*
 *  groupbox.h  -  checkable group box with read-only option
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#ifndef GROUPBOX_H
#define GROUPBOX_H

#include <QGroupBox>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QGroupBox with read-only option.
 *
 *  The GroupBox class is a QGroupBox with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class GroupBox : public QGroupBox
{
        Q_OBJECT
        Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit GroupBox(QWidget* parent = 0);
        /** Constructor for a group box with a title text.
         *  @param title The title text.
         *  @param parent The parent object of this widget.
         */
        explicit GroupBox(const QString& title, QWidget* parent = 0);
        /** Sets whether the group box is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to enable its action.
         */
        virtual void  setReadOnly(bool readOnly);
        /** Returns true if the widget is read only. */
        virtual bool  isReadOnly() const  { return mReadOnly; }
    protected:
        virtual void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        virtual void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        virtual void mouseMoveEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        virtual void keyPressEvent(QKeyEvent*) Q_DECL_OVERRIDE;
        virtual void keyReleaseEvent(QKeyEvent*) Q_DECL_OVERRIDE;
    private:
        bool  mReadOnly;      // value cannot be changed
};

#endif // GROUPBOX_H

// vim: et sw=4:
