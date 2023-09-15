/*
 *  spinbox.cpp  -  spin box with read-only option and shift-click step value
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spinbox.h"
#include "spinbox_p.h"

#include <QLineEdit>
#include <QObject>
#include <QApplication>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>


SpinBox::SpinBox(QWidget* parent)
    : QSpinBox(parent)
    , mMinValue(QSpinBox::minimum())
    , mMaxValue(QSpinBox::maximum())
{
    setRange(0, 99999);
    init();
}

SpinBox::SpinBox(int minValue, int maxValue, QWidget* parent)
    : QSpinBox(parent)
    , mMinValue(minValue)
    , mMaxValue(maxValue)
{
    setRange(minValue, maxValue);
    init();
}

void SpinBox::init()
{
    int step = QSpinBox::singleStep();
    mLineStep        = step;
    mLineShiftStep   = step;

    lineEdit()->installEventFilter(this);   // handle shift-up/down arrow presses

    // Detect when the text field is edited
    connect(lineEdit(), &QLineEdit::textChanged, this, &SpinBox::textEdited);
    connect(this, &SpinBox::valueChanged, this, &SpinBox::valueChange);
}

SpinBox::~SpinBox()
{
    delete mControlStyle;
}

void SpinBox::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        lineEdit()->setReadOnly(ro);
        if (ro)
            setShiftStepping(Modifier::None, mCurrentButton);
    }
}

int SpinBox::bound(int val) const
{
    return (val < mMinValue) ? mMinValue : (val > mMaxValue) ? mMaxValue : val;
}

void SpinBox::setMinimum(int val)
{
    mMinValue = val;
    QSpinBox::setMinimum(val);
    mShiftMinBound = false;
}

void SpinBox::setMaximum(int val)
{
    mMaxValue = val;
    QSpinBox::setMaximum(val);
    mShiftMaxBound = false;
}

void SpinBox::setSingleStep(int step)
{
    mLineStep = step;
    if (mMouseKey == Modifier::None)
        QSpinBox::setSingleStep(step);
}

void SpinBox::setSingleShiftStep(int step)
{
    mLineShiftStep = step;
    if (mMouseKey == Modifier::Shift)
        QSpinBox::setSingleStep(step);
}

void SpinBox::setSingleControlStep(int step, bool mod)
{
    if (mLineControlStep != step  ||  mModControlStep != mod)
    {
        mLineControlStep = step;
        mModControlStep  = step && mod;
        if (mMouseKey == Modifier::Control)
            QSpinBox::setSingleStep(mLineControlStep ? mLineControlStep : mLineStep);
        if (mLineControlStep  &&  !mControlStyle)
            mControlStyle = new SpinBoxStyle;
        setStyle(mLineControlStep ? mControlStyle : QApplication::style());
    }
}

void SpinBox::stepBy(int steps)
{
    int increment = steps * QSpinBox::singleStep();
    addValue(increment);
    Q_EMIT stepped(increment, steps != 1);
}

/******************************************************************************
* Adds a positive or negative increment to the current value, wrapping as appropriate.
* If 'current' is true, any temporary 'shift' values for the range are used instead
* of the real maximum and minimum values.
*/
void SpinBox::addValue(int change, bool current)
{
    int newval = value() + change;
    const int maxval = current ? QSpinBox::maximum() : mMaxValue;
    const int minval = current ? QSpinBox::minimum() : mMinValue;
    if (wrapping())
    {
        int range = maxval - minval + 1;
        if (newval > maxval)
            newval = minval + (newval - maxval - 1) % range;
        else if (newval < minval)
            newval = maxval - (minval - 1 - newval) % range;
    }
    else
    {
        if (newval > maxval)
            newval = maxval;
        else if (newval < minval)
            newval = minval;
    }
    setValue(newval);
}

void SpinBox::valueChange()
{
    if (!mSuppressSignals)
    {
        const int val = value();
        if (mShiftMinBound  &&  val >= mMinValue)
        {
            // Reinstate the minimum bound now that the value has returned to the normal range.
            QSpinBox::setMinimum(mMinValue);
            mShiftMinBound = false;
        }
        if (mShiftMaxBound  &&  val <= mMaxValue)
        {
            // Reinstate the maximum bound now that the value has returned to the normal range.
            QSpinBox::setMaximum(mMaxValue);
            mShiftMaxBound = false;
        }

         if (!mSelectOnStep && hasFocus())
            lineEdit()->deselect();   // prevent selection of the spin box text
    }
}

/******************************************************************************
* Called whenever the line edit text is changed.
*/
void SpinBox::textEdited()
{
    mEdited = true;
}

/******************************************************************************
* Receives events destined for the spin widget or for the edit field.
*/
bool SpinBox::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == lineEdit())
    {
        int step = 0;
        switch (e->type())
        {
            case QEvent::KeyPress:
            {
                // Up and down arrow keys step the value
                auto* ke = (QKeyEvent*)e;
                const int key = ke->key();
                if (key == Qt::Key_Up)
                    step = 1;
                else if (key == Qt::Key_Down)
                    step = -1;
                break;
            }
            case QEvent::Wheel:
            {
                auto* we = (QWheelEvent*)e;
                step = (we->angleDelta().y() > 0) ? 1 : -1;
                break;
            }
            default:
                break;
        }
        if (step)
        {
            if (mReadOnly)
                return true;    // discard up/down arrow keys
            auto* ie = (QInputEvent*)e;
            const Modifier modifier = getModifier(ie->modifiers());
            const int lineStep = (modifier == Modifier::Shift) ? mLineShiftStep : (modifier == Modifier::Control && mLineControlStep) ? mLineControlStep : mLineStep;
            if (modifier == Modifier::Shift  ||  (modifier == Modifier::Control && mModControlStep))
            {
                // Shift/control stepping, to a multiple of the step
                const int val = value();
                if (step > 0)
                    step = lineStep - val % lineStep;
                else
                    step = - ((val + lineStep - 1) % lineStep + 1);
            }
            else
                step = (step > 0) ? lineStep : -lineStep;
            addValue(step, false);
            return true;
        }
    }
    return QSpinBox::eventFilter(obj, e);
}

void SpinBox::focusOutEvent(QFocusEvent* e)
{
    if (mEdited)
    {
        interpretText();
        mEdited = false;
    }
    QSpinBox::focusOutEvent(e);
}

void SpinBox::mousePressEvent(QMouseEvent* e)
{
    if (!clickEvent(e))
        QSpinBox::mousePressEvent(e);
}

void SpinBox::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (!clickEvent(e))
        QSpinBox::mouseDoubleClickEvent(e);
}

bool SpinBox::clickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        // It's a left button press. Set normal or shift stepping as appropriate.
        if (mReadOnly)
            return true;   // discard the event
        mCurrentButton = whichButton(e->pos());
        if (mCurrentButton == NO_BUTTON)
        {
            e->accept();
            return true;
        }
        if (setShiftStepping(getModifier(e->modifiers()), mCurrentButton))
        {
            e->accept();
            return true;     // hide the event from the spin widget
        }
    }
    return false;
}

void SpinBox::wheelEvent(QWheelEvent* e)
{
    if (mReadOnly)
        return;   // discard the event
    if (setShiftStepping(getModifier(e->modifiers()), (e->angleDelta().y() > 0 ? UP : DOWN)))
    {
        e->accept();
        return;     // hide the event from the spin widget
    }
    QSpinBox::wheelEvent(e);
}

void SpinBox::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton  &&  mMouseKey != Modifier::None)
        setShiftStepping(Modifier::None, mCurrentButton);    // cancel shift stepping
    QSpinBox::mouseReleaseEvent(e);
}

void SpinBox::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton)
    {
        // The left button is down. Track which spin button it's in.
        if (mReadOnly)
            return;   // discard the event
        int newButton = whichButton(e->pos());
        if (newButton != mCurrentButton)
        {
            // The mouse has moved to a new spin button.
            // Set normal or shift stepping as appropriate.
            mCurrentButton = newButton;
            if (setShiftStepping(getModifier(e->modifiers()), mCurrentButton))
            {
                e->accept();
                return;     // hide the event from the spin widget
            }
        }
    }
    QSpinBox::mouseMoveEvent(e);
}

void SpinBox::keyPressEvent(QKeyEvent* e)
{
    if (!keyEvent(e))
        QSpinBox::keyPressEvent(e);
}

void SpinBox::keyReleaseEvent(QKeyEvent* e)
{
    if (!keyEvent(e))
        QSpinBox::keyReleaseEvent(e);
}

bool SpinBox::keyEvent(QKeyEvent* e)
{
    const int key = e->key();
    if ((QApplication::mouseButtons() & Qt::LeftButton)
    &&  (key == Qt::Key_Shift  ||  key == Qt::Key_Control  ||  key == Qt::Key_Alt))
    {
        // The left mouse button is down, and the Shift, Control or Alt key has changed
        if (mReadOnly)
            return true;   // discard the event
        const Modifier modifier = getModifier(e->modifiers());
        if (modifier != mMouseKey)
        {
            // The effective shift or control state has changed.
            // Set normal or shift stepping as appropriate.
            if (setShiftStepping(modifier, mCurrentButton))
            {
                e->accept();
                return true;     // hide the event from the spin widget
            }
        }
    }
    return false;
}

/******************************************************************************
* Set spin widget stepping to the normal or shift increment.
*/
bool SpinBox::setShiftStepping(Modifier modifier, int currentButton)
{
    if (currentButton == NO_BUTTON)
        modifier = Modifier::None;
    if (modifier == Modifier::Control  &&  !mLineControlStep)
        modifier = Modifier::None;    // Qt automatically handles Control key modifier
    if (modifier != Modifier::None  &&  modifier != mMouseKey)
    {
        /* The value is to be stepped to a multiple of the shift or control increment.
         * Adjust the value so that after the spin widget steps it, it will be correct.
         * Then, if the mouse button is held down, the spin widget will continue to
         * step by the shift amount.
         */
        const int val = value();
        const int lineStep = (modifier == Modifier::Shift) ? mLineShiftStep : mLineControlStep;
        const int step = (currentButton == UP) ? lineStep : (currentButton == DOWN) ? -lineStep : 0;
        const int adjust = (modifier == Modifier::Shift || mModControlStep) ? shiftStepAdjustment(val, step) : 0;
        mMouseKey = modifier;
        if (adjust)
        {
            /* The value is to be stepped by other than the shift increment,
             * presumably because it is being set to a multiple of the shift
             * increment. Achieve this by making the adjustment here, and then
             * allowing the normal step processing to complete the job by
             * adding/subtracting the normal shift increment.
             */
            if (!wrapping())
            {
                // Prevent the step from going past the spinbox's range, or
                // to the minimum value if that has a special text unless it is
                // already at the minimum value + 1.
                int newval = val + adjust + step;
                const int svt = specialValueText().isEmpty() ? 0 : 1;
                const int minval = mMinValue + svt;
                if (newval <= minval  ||  newval >= mMaxValue)
                {
                    // Stepping to the minimum or maximum value
                    if (svt  &&  newval <= mMinValue  &&  val == mMinValue)
                        newval = mMinValue;
                    else
                        newval = (newval <= minval) ? minval : mMaxValue;
                    QSpinBox::setValue(newval);
                    Q_EMIT stepped(step, false);
                    return true;
                }

                // If the interim value will lie outside the spinbox's range,
                // temporarily adjust the range to allow the value to be set.
                const int tempval = val + adjust;
                if (tempval < mMinValue)
                {
                    QSpinBox::setMinimum(tempval);
                    mShiftMinBound = true;
                }
                else if (tempval > mMaxValue)
                {
                    QSpinBox::setMaximum(tempval);
                    mShiftMaxBound = true;
                }
            }

            // Don't process changes since this new value will be stepped immediately
            mSuppressSignals = true;
            const bool blocked = signalsBlocked();
            blockSignals(true);
            addValue(adjust, true);
            blockSignals(blocked);
            mSuppressSignals = false;
        }
        QSpinBox::setSingleStep(lineStep);
    }
    else if (modifier == Modifier::None  &&  mMouseKey != Modifier::None)
    {
        // Reinstate to normal (non-shift) stepping
        QSpinBox::setSingleStep(mLineStep);
        QSpinBox::setMinimum(mMinValue);
        QSpinBox::setMaximum(mMaxValue);
        mShiftMinBound = mShiftMaxBound = false;
        mMouseKey = Modifier::None;
    }
    return false;
}

/******************************************************************************
* Return the initial adjustment to the value for a shift step up or down.
* The default is to step up or down to the nearest multiple of the shift
* increment, so the adjustment returned is for stepping up the decrement
* required to round down to a multiple of the shift increment <= current value,
* or for stepping down the increment required to round up to a multiple of the
* shift increment >= current value.
* This method's caller then adjusts the resultant value if necessary to cater
* for the widget's minimum/maximum value, and wrapping.
* This should really be a static method, but it needs to be virtual...
*/
int SpinBox::shiftStepAdjustment(int oldValue, int shiftStep)
{
    if (oldValue == 0  ||  shiftStep == 0)
        return 0;
    if (shiftStep > 0)
    {
        if (oldValue >= 0)
            return -(oldValue % shiftStep);
        else
            return (-oldValue - 1) % shiftStep + 1 - shiftStep;
    }
    else
    {
        shiftStep = -shiftStep;
        if (oldValue >= 0)
            return shiftStep - ((oldValue - 1) % shiftStep + 1);
        else
            return (-oldValue) % shiftStep;
    }
}

/******************************************************************************
*  Find which spin widget button a mouse event is in.
*/
int SpinBox::whichButton(const QPoint& pos)
{
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    if (style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp).contains(pos))
        return UP;
    if (style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown).contains(pos))
        return DOWN;
    return NO_BUTTON;
}

QRect SpinBox::upRect() const
{
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    return style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp);
}

QRect SpinBox::downRect() const
{
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    return style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
}

QRect SpinBox::upDownRect() const
{
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    return style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
         | style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
}

void SpinBox::paintEvent(QPaintEvent* pe)
{
    if (mUpDownOnly)
    {
        QStyleOptionSpinBox option;
        initStyleOption(&option);
        QPainter painter(this);
        style()->drawComplexControl(QStyle::CC_SpinBox, &option, &painter, this);
    }
    else
        QSpinBox::paintEvent(pe);
}

void SpinBox::initStyleOption(QStyleOptionSpinBox* so) const
{
    so->initFrom(this);
//    so->activeSubControls = ??;
    so->subControls   = mUpDownOnly ? (QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown | QStyle::SC_SpinBoxFrame)
                                   : (QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown | QStyle::SC_SpinBoxFrame | QStyle::SC_SpinBoxEditField);
    so->buttonSymbols = buttonSymbols();
    so->frame         = hasFrame();
    so->stepEnabled   = stepEnabled();
}

/******************************************************************************
* Given input modifier keys, find which modifier is active.
*/
SpinBox::Modifier SpinBox::getModifier(Qt::KeyboardModifiers modifiers)
{
    const int state = modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);
    return (state == Qt::ShiftModifier) ? Modifier::Shift
         : (state == Qt::ControlModifier) ? Modifier::Control
         : Modifier::None;
}


/*=============================================================================
* SpinBoxStyle class.
*/
SpinBoxStyle::SpinBoxStyle()
{
}

void SpinBoxStyle::polish(QWidget* widget)
{
    QApplication::style()->polish(widget);
}

void SpinBoxStyle::unpolish(QWidget* widget)
{
    QApplication::style()->unpolish(widget);
}

void SpinBoxStyle::polish(QApplication* application)
{
    QApplication::style()->polish(application);
}

void SpinBoxStyle::unpolish(QApplication* application)
{
    QApplication::style()->unpolish(application);
}

void SpinBoxStyle::polish(QPalette& palette)
{
    QApplication::style()->polish(palette);
}

QRect SpinBoxStyle::itemTextRect(const QFontMetrics& fm, const QRect& r,
                       int flags, bool enabled,
                       const QString& text) const
{
    return QApplication::style()->itemTextRect(fm, r, flags, enabled, text);
}

QRect SpinBoxStyle::itemPixmapRect(const QRect& r, int flags, const QPixmap& pixmap) const
{
    return QApplication::style()->itemPixmapRect(r, flags, pixmap);
}

void SpinBoxStyle::drawItemText(QPainter* painter, const QRect& rect,
                          int flags, const QPalette& pal, bool enabled,
                          const QString& text, QPalette::ColorRole textRole) const
{
    QApplication::style()->drawItemText(painter, rect, flags, pal, enabled, text, textRole);
}

void SpinBoxStyle::drawItemPixmap(QPainter* painter, const QRect& rect,
                            int alignment, const QPixmap& pixmap) const
{
    QApplication::style()->drawItemPixmap(painter, rect, alignment, pixmap);
}

QPalette SpinBoxStyle::standardPalette() const
{
    return QApplication::style()->standardPalette();
}

void SpinBoxStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                           const QWidget* w) const
{
    QApplication::style()->drawPrimitive(pe, opt, p, w);
}

void SpinBoxStyle::drawControl(ControlElement element, const QStyleOption* opt, QPainter* p,
                         const QWidget* w) const
{
    QApplication::style()->drawControl(element, opt, p, w);
}

QRect SpinBoxStyle::subElementRect(SubElement subElement, const QStyleOption* option,
                             const QWidget* widget) const
{
    return QApplication::style()->subElementRect(subElement, option, widget);
}

void SpinBoxStyle::drawComplexControl(ComplexControl cc, const QStyleOptionComplex* opt, QPainter* p,
                                const QWidget* widget) const
{
    QApplication::style()->drawComplexControl(cc, opt, p, widget);
}

QStyle::SubControl SpinBoxStyle::hitTestComplexControl(ComplexControl cc, const QStyleOptionComplex* opt,
                                         const QPoint& pt, const QWidget* widget) const
{
    return QApplication::style()->hitTestComplexControl(cc, opt, pt, widget);
}

QRect SpinBoxStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex* opt,
                             SubControl sc, const QWidget* widget) const
{
    return QApplication::style()->subControlRect(cc, opt, sc, widget);
}

int SpinBoxStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                        const QWidget* widget) const
{
    return QApplication::style()->pixelMetric(metric, option, widget);
}

QSize SpinBoxStyle::sizeFromContents(ContentsType ct, const QStyleOption* opt,
                               const QSize& contentsSize, const QWidget* w) const
{
    return QApplication::style()->sizeFromContents(ct, opt, contentsSize, w);
}

int SpinBoxStyle::styleHint(StyleHint stylehint, const QStyleOption* opt,
                      const QWidget* widget, QStyleHintReturn* returnData) const
{
    if (stylehint == SH_SpinBox_StepModifier)
        return Qt::NoModifier;
    return QApplication::style()->styleHint(stylehint, opt, widget, returnData);
}

QPixmap SpinBoxStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption* opt,
                               const QWidget* widget) const
{
    return QApplication::style()->standardPixmap(standardPixmap, opt, widget);
}

QIcon SpinBoxStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption* option,
                           const QWidget* widget) const
{
    return QApplication::style()->standardIcon(standardIcon, option, widget);
}

QPixmap SpinBoxStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap& pixmap,
                                    const QStyleOption* opt) const
{
    return QApplication::style()->generatedIconPixmap(iconMode, pixmap, opt);
}

int SpinBoxStyle::layoutSpacing(QSizePolicy::ControlType control1,
                          QSizePolicy::ControlType control2, Qt::Orientation orientation,
                          const QStyleOption* option, const QWidget* widget) const
{
    return QApplication::style()->layoutSpacing(control1, control2, orientation, option, widget);
}

// vim: et sw=4:
