/*
 *  slider.h  -  slider control with read-only option
 *  Program:  kalarm
 *  Copyright © 2004-2006 by David Jarvie <djarvie@kde.org>
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

#ifndef SLIDER_H
#define SLIDER_H

#include <QSlider>
class QMouseEvent;
class QKeyEvent;


/**
 *  @short A QSlider with read-only option.
 *
 *  The Slider class is a QSlider with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class Slider : public QSlider
{
        Q_OBJECT
        Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit Slider(QWidget* parent = Q_NULLPTR);
        /** Constructor.
         *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
         *  @param parent The parent object of this widget.
         */
        explicit Slider(Qt::Orientation orient, QWidget* parent = Q_NULLPTR);
        /** Constructor.
         *  @param minValue The minimum value which the slider can have.
         *  @param maxValue The maximum value which the slider can have.
         *  @param pageStep The page step increment.
         *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
         *  @param parent The parent object of this widget.
         */
        Slider(int minValue, int maxValue, int pageStep, Qt::Orientation orient,
               QWidget* parent = Q_NULLPTR);
        /** Returns true if the slider is read only. */
        bool         isReadOnly() const  { return mReadOnly; }
        /** Sets whether the slider is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void setReadOnly(bool readOnly);

    protected:
        void         mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void         mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void         mouseMoveEvent(QMouseEvent*) Q_DECL_OVERRIDE;
        void         keyPressEvent(QKeyEvent*) Q_DECL_OVERRIDE;
        void         keyReleaseEvent(QKeyEvent*) Q_DECL_OVERRIDE;

    private:
        bool    mReadOnly;      // value cannot be changed by the user
};

#endif // SLIDER_H

// vim: et sw=4:
