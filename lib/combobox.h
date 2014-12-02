/*
 *  combobox.h  -  combo box with read-only option
 *  Program:  kalarm
 *  Copyright © 2002,2005-2007 by David Jarvie <djarvie@kde.org>
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

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <kcombobox.h>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A KComboBox with read-only option.
 *
 *  The ComboBox class is a KComboBox with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ComboBox : public KComboBox
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit ComboBox(QWidget* parent = 0);
        /** Returns true if the widget is read only. */
        bool         isReadOnly() const          { return mReadOnly; }
        /** Sets whether the combo box is read-only for the user. If read-only,
         *  its state cannot be changed by the user.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void setReadOnly(bool readOnly);
    protected:
        void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void mouseMoveEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void keyPressEvent(QKeyEvent*) Q_DECL_OVERRIDE;
        void keyReleaseEvent(QKeyEvent*) Q_DECL_OVERRIDE;
    private:
        bool    mReadOnly;      // value cannot be changed
};

#endif // COMBOBOX_H

// vim: et sw=4:
