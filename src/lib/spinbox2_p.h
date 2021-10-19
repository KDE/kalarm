/*
 *  spinbox2_p.h  -  private classes for SpinBox2
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
    void inhibitPaintSignal(int i)  { mInhibitPaintSignal = i; }

Q_SIGNALS:
    void painted();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int  mInhibitPaintSignal;
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
    void    setReadOnly(bool ro)        { mReadOnly = ro; }
    bool    isReadOnly() const          { return mReadOnly; }
    void    setButtons();
    void    setFrame();
    void    setButtonPos(const QPoint&);
    void    rearrange();

protected:
    bool    event(QEvent*) override;
    void    resizeEvent(QResizeEvent*) override;
    void    mousePressEvent(QMouseEvent* e) override        { mouseEvent(e); }
    void    mouseReleaseEvent(QMouseEvent* e) override      { mouseEvent(e); }
    void    mouseMoveEvent(QMouseEvent* e) override         { mouseEvent(e); }
    void    mouseDoubleClickEvent(QMouseEvent* e) override  { mouseEvent(e); }
    void    wheelEvent(QWheelEvent*) override;

private:
    void    mouseEvent(QMouseEvent*);
    void    setMirroredState(bool clear = false);
    QPointF spinboxPoint(const QPointF&) const;

    ExtraSpinBox*        mSpinbox;    // spinbox whose spin buttons are being mirrored
    SpinBox*             mMainSpinbox;
    QGraphicsPixmapItem* mButtons;    // image of spin buttons
    bool                 mReadOnly;   // value cannot be changed
    bool                 mMirrored;   // mirror left-to-right
};


// vim: et sw=4:
