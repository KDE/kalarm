/*
 *  spinbox2_p.h  -  private classes for SpinBox2
 *  Program:  kalarm
 *  Copyright © 2005,2006,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef SPINBOX2_P_H
#define SPINBOX2_P_H

#include "spinbox.h"

#include <QGraphicsView>
class QMouseEvent;
class QPaintEvent;


/*=============================================================================
= Class ExtraSpinBox
* Extra pair of spin buttons for SpinBox2.
* The widget is actually a whole spin box, but only the buttons are displayed.
=============================================================================*/

class ExtraSpinBox : public SpinBox
{
        Q_OBJECT
    public:
        explicit ExtraSpinBox(QWidget* parent)
                     : SpinBox(parent), mInhibitPaintSignal(0) { }
        ExtraSpinBox(int minValue, int maxValue, QWidget* parent)
                     : SpinBox(minValue, maxValue, parent), mInhibitPaintSignal(0) { }
        void         inhibitPaintSignal(int i)  { mInhibitPaintSignal = i; }

    Q_SIGNALS:
        void         painted();

    protected:
        void         paintEvent(QPaintEvent*) override;

    private:
        int          mInhibitPaintSignal;
};


/*=============================================================================
= Class SpinMirror
* Displays the left-to-right mirror image of a pair of spin buttons, for use
* as the extra spin buttons in a SpinBox2. All mouse clicks are passed on to
* the real extra pair of spin buttons for processing.
* Mirroring in this way allows styles with rounded corners to display correctly.
=============================================================================*/

class SpinMirror : public QGraphicsView
{
        Q_OBJECT
    public:
        explicit SpinMirror(ExtraSpinBox*, SpinBox*, QWidget* parent = nullptr);
        void         setReadOnly(bool ro)        { mReadOnly = ro; }
        bool         isReadOnly() const          { return mReadOnly; }
        void         setButtons();
        void         setFrame();
        void         setButtonPos(const QPoint&);

    protected:
        bool         event(QEvent*) override;
        void         resizeEvent(QResizeEvent*) override;
        virtual void styleChange(QStyle&);
        void         mousePressEvent(QMouseEvent* e) override        { mouseEvent(e); }
        void         mouseReleaseEvent(QMouseEvent* e) override      { mouseEvent(e); }
        void         mouseMoveEvent(QMouseEvent* e) override         { mouseEvent(e); }
        void         mouseDoubleClickEvent(QMouseEvent* e) override  { mouseEvent(e); }
        void         wheelEvent(QWheelEvent*) override;

    private:
        void         mouseEvent(QMouseEvent*);
        void         setMirroredState(bool clear = false);
        QPoint       spinboxPoint(const QPoint&) const;

        ExtraSpinBox*        mSpinbox;    // spinbox whose spin buttons are being mirrored
        SpinBox*             mMainSpinbox;
        QGraphicsPixmapItem* mButtons;    // image of spin buttons
        bool                 mReadOnly;   // value cannot be changed
        bool                 mMirrored;   // mirror left-to-right
};

#endif // SPINBOX2_P_H

// vim: et sw=4:
