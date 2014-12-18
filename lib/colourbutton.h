/*
 *  colourbutton.h  -  colour selection button
 *  Program:  kalarm
 *  Copyright © 2008 by David Jarvie <djarvie@kde.org>
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

#ifndef COLOURBUTTON_H
#define COLOURBUTTON_H

#include <kcolorbutton.h>


/**
 *  @short A colour selection button with read-only option.
 *
 *  The ColourButton class is a KColorButton with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ColourButton : public KColorButton
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit ColourButton(QWidget* parent = Q_NULLPTR);
        /** Returns the selected colour. */
        QColor       colour() const              { return color(); }
        /** Sets the selected colour to @p c. */
        void         setColour(const QColor& c)  { setColor(c); }
        /** Returns true if the widget is read only. */
        bool         isReadOnly() const          { return mReadOnly; }
        /** Sets whether the button can be changed by the user.
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
        bool         mReadOnly;        // value cannot be changed
};

#endif // COLOURBUTTON_H

// vim: et sw=4:
