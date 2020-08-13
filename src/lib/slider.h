/*
 *  slider.h  -  slider control with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
        explicit Slider(QWidget* parent = nullptr);
        /** Constructor.
         *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
         *  @param parent The parent object of this widget.
         */
        explicit Slider(Qt::Orientation orient, QWidget* parent = nullptr);
        /** Constructor.
         *  @param minValue The minimum value which the slider can have.
         *  @param maxValue The maximum value which the slider can have.
         *  @param pageStep The page step increment.
         *  @param orient The orientation of the slider, either Qt::Horizonal or Qt::Vertical.
         *  @param parent The parent object of this widget.
         */
        Slider(int minValue, int maxValue, int pageStep, Qt::Orientation orient,
               QWidget* parent = nullptr);
        /** Returns true if the slider is read only. */
        bool         isReadOnly() const  { return mReadOnly; }
        /** Sets whether the slider is read-only for the user.
         *  @param readOnly True to set the widget read-only, false to set it read-write.
         */
        virtual void setReadOnly(bool readOnly);

    protected:
        void         mousePressEvent(QMouseEvent*) override;
        void         mouseReleaseEvent(QMouseEvent*) override;
        void         mouseMoveEvent(QMouseEvent*) override;
        void         keyPressEvent(QKeyEvent*) override;
        void         keyReleaseEvent(QKeyEvent*) override;

    private:
        bool    mReadOnly {false};    // value cannot be changed by the user
};

#endif // SLIDER_H

// vim: et sw=4:
