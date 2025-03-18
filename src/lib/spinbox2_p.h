/*
 *  spinbox2_p.h  -  private classes for SpinBox2
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "spinbox.h"

#include <QGraphicsView>
class QMouseEvent;
class QPaintEvent;
class QShowEvent;
class QResizeEvent;
class SpinMirror;
class ExtraSpinBox;
class SpinBox2;


/*=============================================================================
= Class SpinBox2p
= Private implementation of spin box with a pair of spin buttons on either side.
= This does not contain a layout, so must be added to SpinBox2's layout to make
= positioning of SpinBox2 work with Qt 5.15.3 or later.
=============================================================================*/
class SpinBox2p : public QFrame
{
    Q_OBJECT
public:
    explicit SpinBox2p(SpinBox2* spinbox2, QWidget* parent = nullptr);
    SpinBox2p(SpinBox2* spinbox2, int minValue, int maxValue, QWidget* parent = nullptr);
    void             setReadOnly(bool readOnly);
    bool             isReadOnly() const          { return mSpinbox->isReadOnly(); }
    void             setSelectOnStep(bool sel)   { mSpinbox->setSelectOnStep(sel); }
    void             setReverseWithLayout(bool reverse);
    bool             reverseButtons() const      { return mRightToLeft  &&  !mReverseWithLayout; }
    QString          text() const                { return mSpinbox->text(); }
    QString          prefix() const              { return mSpinbox->prefix(); }
    QString          suffix() const              { return mSpinbox->suffix(); }
    void             setPrefix(const QString& text)  { mSpinbox->setPrefix(text); }
    void             setSuffix(const QString& text)  { mSpinbox->setSuffix(text); }
    QString          cleanText() const           { return mSpinbox->cleanText(); }
    void             setSpecialValueText(const QString& text)  { mSpinbox->setSpecialValueText(text); }
    QString          specialValueText() const    { return mSpinbox->specialValueText(); }
    void             setWrapping(bool on);
    bool             wrapping() const            { return mSpinbox->wrapping(); }
    void             setAlignment(Qt::Alignment a) { mSpinbox->setAlignment(a); }
    void             setButtonSymbols(QSpinBox::ButtonSymbols);
    QSpinBox::ButtonSymbols buttonSymbols() const   { return mSpinbox->buttonSymbols(); }
    QValidator::State validate(QString&, int& /*pos*/) const  { return QValidator::Acceptable; }
    QSize            sizeHint() const override;
    QSize            minimumSizeHint() const override;
    int              minimum() const             { return mMinValue; }
    int              maximum() const             { return mMaxValue; }
    void             setMinimum(int val);
    void             setMaximum(int val);
    void             setRange(int minValue, int maxValue)   { setMinimum(minValue);  setMaximum(maxValue); }
    int              value() const               { return mSpinbox->value(); }
    int              bound(int val) const;
    QRect            upRect() const              { return mSpinbox->upRect(); }
    QRect            downRect() const            { return mSpinbox->downRect(); }
    QRect            up2Rect() const;
    QRect            down2Rect() const;
    int              singleStep() const          { return mSingleStep; }
    int              singleShiftStep() const     { return mSingleShiftStep; }
    int              pageStep() const            { return mPageStep; }
    int              pageShiftStep() const       { return mPageShiftStep; }
    void             setSingleStep(int step);
    void             setSteps(int line, int page);
    void             setShiftSteps(int line, int page, int control, bool modControl = true);
    void             addPage()                   { addValue(mPageStep); }
    void             subtractPage()              { addValue(-mPageStep); }
    void             addSingle()                 { addValue(mSingleStep); }
    void             subtractSingle()            { addValue(-mSingleStep); }
    void             addValue(int change)        { mSpinbox->addValue(change); }
    void             stepBy(int increment)       { addValue(increment); }
    void             rearrange();

public Q_SLOTS:
    void             setValue(int val)           { mSpinbox->setValue(val); }
    void             pageUp()                    { addValue(mPageStep); }
    void             pageDown()                  { addValue(-mPageStep); }
    void             selectAll()                 { mSpinbox->selectAll(); }
    void             setEnabled(bool enabled);

    QString          textFromValue(int v) const    { return mSpinbox->textFromVal(v); }
    int              valueFromText(const QString& t) const  { return mSpinbox->valFromText(t); }
    void             init();
    void             setSteps() const;

Q_SIGNALS:
    void             valueChanged(int value);

protected:
    void             paintEvent(QPaintEvent*) override;
    void             showEvent(QShowEvent*) override;
    void             getMetrics() const;

    mutable int      wUpdown2 {0};    // width of second spin widget, including outer frame decoration
    mutable int      wBorderWidth;    // thickness of border between spin buttons and edge of widget
    mutable int      wFrameWidth;     // thickness of frame round widget
    mutable QPoint   mButtonPos;      // position of buttons inside mirror widget
    mutable bool     mShowUpdown2 {true};  // the extra pair of spin buttons are displayed

private Q_SLOTS:
    void             valueChange();
    void             stepPage(int, bool);
    void             updateMirrorButtons();
    void             updateMirrorFrame();
    void             paintTimer();

private:
    void             setShiftSteps() const;
    void             arrange();
    void             updateMirror();
    bool             eventFilter(QObject*, QEvent*) override;
    void             spinboxResized(QResizeEvent*);
    void             setUpdown2Size();
    void             setShowUpdown2(bool show) const;

    // Visible spin box class.
    // Declared here to allow use of mSpinBox in inline methods.
    class MainSpinBox : public SpinBox
    {
        public:
            explicit MainSpinBox(SpinBox2* spinbox2, SpinBox2p* parent)
                            : SpinBox(parent), owner(spinbox2), owner_p(parent) { }
            MainSpinBox(SpinBox2* spinbox2, int minValue, int maxValue, SpinBox2p* parent)
                            : SpinBox(minValue, maxValue, parent), owner(spinbox2), owner_p(parent) { }
            QString textFromValue(int v) const override;
            int     valueFromText(const QString& t) const override;
            QString textFromVal(int v) const    { return SpinBox::textFromValue(v); }
            int     valFromText(const QString& t) const
                                                { return SpinBox::valueFromText(t); }
            int     shiftStepAdjustment(int oldValue, int shiftStep) override;
            QValidator::State validate(QString& text, int& pos) const override;
        protected:
            void    resizeEvent(QResizeEvent* e) override  { owner_p->spinboxResized(e); SpinBox::resizeEvent(e); }
        private:
            SpinBox2* owner;      // owner SpinBox2
            SpinBox2p* owner_p;   // owner SpinBox2p
    };

    enum { NO_BUTTON = -1, UP, DOWN, UP2, DOWN2 };

    static int       mRightToLeft;    // widgets are mirrored right to left
    ExtraSpinBox*    mSpinbox2;       // hidden spin box providing an extra pair of spin buttons
    MainSpinBox*     mSpinbox;        // the visible spin box
    SpinMirror*      mSpinMirror;     // image of the extra pair of spin buttons in mSpinbox2
    int              mMinValue;
    int              mMaxValue;
    int              mSingleStep;         // right button increment
    int              mSingleShiftStep;    // right button increment with shift pressed
    int              mSingleControlStep;  // right button increment with control pressed
    int              mPageStep;           // left button increment
    int              mPageShiftStep;      // left button increment with shift pressed
    bool             mModControlStep {true};     // control steps set value to multiple of step
    bool             mReverseWithLayout {true};  // reverse button positions if reverse layout (default = true)

friend class MainSpinBox;
};


/*=============================================================================
= Class ExtraSpinBox
* Extra pair of spin buttons for SpinBox2p.
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
* as the extra spin buttons in a SpinBox2p. All mouse clicks are passed on to
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
    void    setFrameImage();
    void    setButtonsImage();
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
