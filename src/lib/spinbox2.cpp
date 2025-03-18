/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spinbox2.h"

#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPixmap>
#include <QStyleOptionSpinBox>
#include <QTimer>

#include <stdlib.h>

namespace
{

/* List of styles which look correct using spin buttons mirrored left-to-right.
 * This is needed for some styles which use rounded corners.
 *
 * Note: All styles which work when mirrored should be included in this list,
 *       since this is the most efficient and most accurate way to render the
 *       second pair of spin buttons.
 */
const char* mirrorStyles[] = {
    "QPlastiqueStyle", "QCleanlooksStyle",
    "Oxygen::Style",
    "QFusionStyle",
    nullptr    // list terminator
};
bool isMirrorStyle(const QStyle*);
QRect spinBoxEditFieldRect(const SpinBox*);
QRect spinBoxButtonsRect(const SpinBox*, bool includeBorders);

inline QPixmap grabWidget(QWidget* w, QRect r = QRect())
{
    QPixmap p(r.isEmpty() ? w->size() : r.size());
    w->render(&p, QPoint(0,0), r, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
    return p;
}

}

/*===========================================================================*/

SpinBox2::SpinBox2(QWidget* parent)
    : QWidget(parent)
    , mSpinbox2(new SpinBox2p(this))
{
    init();
    // Can't call SpinBox2p methods until this widget is contructed
    mSpinbox2->init();
}

SpinBox2::SpinBox2(int minValue, int maxValue, int pageStep, QWidget* parent)
    : QWidget(parent)
    , mSpinbox2(new SpinBox2p(this, minValue, maxValue))
{
    init();
    // Can't call SpinBox2p methods until this widget is contructed
    mSpinbox2->setSteps(1, pageStep);
    mSpinbox2->init();
}

void SpinBox2::init()
{
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(mSpinbox2);
    connect(mSpinbox2, &SpinBox2p::valueChanged, this, &SpinBox2::valueChanged);
    setFocusProxy(mSpinbox2);
#if 0   // for testing
QPalette pal = palette();
pal.setColor(QPalette::Window, Qt::blue);
setPalette(pal);
setAutoFillBackground(true);
#endif
}

void SpinBox2::resizeEvent(QResizeEvent* event)
{
    if (!event->oldSize().isEmpty()  &&  size() != event->oldSize())
        mSpinbox2->rearrange();
    QWidget::resizeEvent(event);
}

/*===========================================================================*/

int SpinBox2p::mRightToLeft = -1;

SpinBox2p::SpinBox2p(SpinBox2* spinbox2, QWidget* parent)
    : QFrame(parent)
{
    mSpinbox2 = new ExtraSpinBox(this);
    mSpinbox  = new MainSpinBox(spinbox2, this);
}

SpinBox2p::SpinBox2p(SpinBox2* spinbox2, int minValue, int maxValue, QWidget* parent)
    : QFrame(parent)
{
    mSpinbox2 = new ExtraSpinBox(minValue, maxValue, this);
    mSpinbox  = new MainSpinBox(spinbox2, minValue, maxValue, this);
}

void SpinBox2p::init()
{
    if (mRightToLeft < 0)
        mRightToLeft = QApplication::isRightToLeft() ? 1 : 0;
    mMinValue        = mSpinbox->minimum();
    mMaxValue        = mSpinbox->maximum();
    mSingleStep      = mSpinbox->singleStep();
    mSingleShiftStep = mSpinbox->singleShiftStep();
    mPageStep        = mSpinbox2->singleStep();
    mPageShiftStep   = mSpinbox2->singleShiftStep();
    mSpinbox->setSelectOnStep(false);    // default
    mSpinbox2->setSelectOnStep(false);    // always false
    setFocusProxy(mSpinbox);
    mSpinbox2->setFocusPolicy(Qt::NoFocus);
    mSpinMirror = new SpinMirror(mSpinbox2, mSpinbox, this);
    mSpinMirror->setFocusPolicy(Qt::NoFocus);
    mSpinbox->installEventFilter(this);
    mSpinbox2->installEventFilter(this);
    connect(mSpinbox, &QSpinBox::valueChanged, this, &SpinBox2p::valueChange);
    connect(mSpinbox, &QSpinBox::valueChanged, this, &SpinBox2p::valueChanged);
    connect(mSpinbox2, &SpinBox::stepped, this, &SpinBox2p::stepPage);
    connect(mSpinbox2, &ExtraSpinBox::painted, this, &SpinBox2p::paintTimer);

    mShowUpdown2 = false;   // ensure that setShowUpdown2(true) actually does something
    setShowUpdown2(true);
}

void SpinBox2p::setReadOnly(bool ro)
{
    if (static_cast<int>(ro) != static_cast<int>(mSpinbox->isReadOnly()))
    {
        mSpinbox->setReadOnly(ro);
        mSpinbox2->setReadOnly(ro);
        mSpinMirror->setReadOnly(ro);
    }
}

void SpinBox2p::setReverseWithLayout(bool reverse)
{
    if (reverse != mReverseWithLayout)
    {
        mReverseWithLayout = reverse;
        setSteps();
        setShiftSteps();
    }
}

void SpinBox2p::setEnabled(bool enabled)
{
    QFrame::setEnabled(enabled);
    mSpinbox->setEnabled(enabled);
    mSpinbox2->setEnabled(enabled);
    updateMirror();
}

void SpinBox2p::setWrapping(bool on)
{
    mSpinbox->setWrapping(on);
    mSpinbox2->setWrapping(on);
}

QRect SpinBox2p::up2Rect() const
{
    return mShowUpdown2 ? mSpinbox2->upRect() : QRect();
}

QRect SpinBox2p::down2Rect() const
{
    return mShowUpdown2 ? mSpinbox2->downRect() : QRect();
}

void SpinBox2p::setSingleStep(int step)
{
    mSingleStep = step;
    if (reverseButtons())
        mSpinbox2->setSingleStep(step);   // reverse layout, but still set the right buttons
    else
        mSpinbox->setSingleStep(step);
}

void SpinBox2p::setSteps(int single, int page)
{
    mSingleStep = single;
    mPageStep   = page;
    setSteps();
}

void SpinBox2p::setSteps() const
{
    if (reverseButtons()  &&  mShowUpdown2)
    {
        mSpinbox2->setSingleStep(mSingleStep);   // reverse layout, but still set the right buttons
        mSpinbox->setSingleStep(mPageStep);
    }
    else
    {
        mSpinbox->setSingleStep(mSingleStep);
        mSpinbox2->setSingleStep(mPageStep);
    }
}

void SpinBox2p::setShiftSteps(int single, int page, int control, bool modControl)
{
    mSingleShiftStep   = single;
    mPageShiftStep     = page;
    mSingleControlStep = control;
    mModControlStep    = modControl;
    setShiftSteps();
}

void SpinBox2p::setShiftSteps() const
{
    if (reverseButtons()  &&  mShowUpdown2)
    {
        mSpinbox2->setSingleShiftStep(mSingleShiftStep);   // reverse layout, but still set the right buttons
        mSpinbox->setSingleShiftStep(mPageShiftStep);
    }
    else
    {
        mSpinbox->setSingleShiftStep(mSingleShiftStep);
        mSpinbox2->setSingleShiftStep(mPageShiftStep);
    }
    if (mShowUpdown2)
        mSpinbox->setSingleControlStep(0);
    else
        mSpinbox->setSingleControlStep(mSingleControlStep, mModControlStep);
}

void SpinBox2p::setButtonSymbols(QSpinBox::ButtonSymbols newSymbols)
{
    if (mSpinbox->buttonSymbols() == newSymbols)
        return;
    mSpinbox->setButtonSymbols(newSymbols);
    mSpinbox2->setButtonSymbols(newSymbols);
}

int SpinBox2p::bound(int val) const
{
    return (val < mMinValue) ? mMinValue : (val > mMaxValue) ? mMaxValue : val;
}

void SpinBox2p::setMinimum(int val)
{
    mMinValue = val;
    mSpinbox->setMinimum(val);
    mSpinbox2->setMinimum(val);
}

void SpinBox2p::setMaximum(int val)
{
    mMaxValue = val;
    mSpinbox->setMaximum(val);
    mSpinbox2->setMaximum(val);
}

void SpinBox2p::valueChange()
{
    const int val = mSpinbox->value();
    const bool blocked = mSpinbox2->signalsBlocked();
    mSpinbox2->blockSignals(true);
    mSpinbox2->setValue(val);
    mSpinbox2->blockSignals(blocked);
}

/******************************************************************************
* Called when the widget is about to be displayed.
* (At construction time, the spin button widths cannot be determined correctly,
*  so we need to wait until now to definitively rearrange the widget.)
*/
void SpinBox2p::showEvent(QShowEvent*)
{
    rearrange();
}

QSize SpinBox2p::sizeHint() const
{
    getMetrics();
    QSize size = mSpinbox->sizeHint();
    if (mShowUpdown2)
        size.setWidth(size.width() + wUpdown2);
    return size;
}

QSize SpinBox2p::minimumSizeHint() const
{
    getMetrics();
    QSize size = mSpinbox->minimumSizeHint();
    if (mShowUpdown2)
        size.setWidth(size.width() + wUpdown2);
    return size;
}

void SpinBox2p::paintEvent(QPaintEvent* e)
{
    QFrame::paintEvent(e);
    if (mShowUpdown2)
        QTimer::singleShot(0, this, &SpinBox2p::updateMirrorFrame);
}

void SpinBox2p::paintTimer()
{
    if (mShowUpdown2)
        QTimer::singleShot(0, this, &SpinBox2p::updateMirrorButtons);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void SpinBox2p::updateMirrorButtons()
{
    if (mShowUpdown2)
        mSpinMirror->setButtonsImage();
}

void SpinBox2p::updateMirrorFrame()
{
    if (mShowUpdown2)
        mSpinMirror->setFrameImage();
}

void SpinBox2p::spinboxResized(QResizeEvent* e)
{
    if (mShowUpdown2)
    {
        const int h = e->size().height();
        if (h != mSpinbox2->height())
        {
            mSpinbox2->setFixedSize(mSpinbox2->width(), e->size().height());
            setUpdown2Size();
        }
    }
}

/******************************************************************************
* Set the size of the second spin button widget.
* It is necessary to fix the size to avoid infinite recursion in arrange().
*/
void SpinBox2p::setUpdown2Size()
{
    if (mShowUpdown2)
        mSpinMirror->setButtonsImage();
}

/******************************************************************************
* Called when the extra pair of spin buttons has repainted after a style change.
* Updates the mirror image of the spin buttons.
*/
void SpinBox2p::updateMirror()
{
    mSpinMirror->setButtonsImage();
    mSpinMirror->setFrameImage();
}

bool SpinBox2p::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == mSpinbox  &&  e->type() == QEvent::StyleChange)
    {
        rearrange();
        return false;
    }
    if (!mShowUpdown2)
        return false;
    bool updateButtons = false;
    if (obj == mSpinbox)
    {
//if (e->type() != QEvent::Paint) qCDebug(KALARM_LOG)<<e->type();
        switch (e->type())
        {
            case QEvent::Enter:
            case QEvent::Leave:
                QApplication::postEvent(mSpinbox2, new QEvent(e->type()));
                updateButtons = true;
                break;
            case QEvent::HoverEnter:
            {
                auto* he = (QHoverEvent*)e;
                QApplication::postEvent(mSpinbox2, new QHoverEvent(e->type(), QPointF(1.0, he->position().y()), he->globalPosition(), he->oldPosF()));
                updateButtons = true;
                break;
            }
            case QEvent::HoverLeave:
            {
                auto* he = (QHoverEvent*)e;
                QApplication::postEvent(mSpinbox2, new QHoverEvent(e->type(), he->position(), he->globalPosition(), QPointF(1.0, he->oldPosF().y())));
                updateButtons = true;
                break;
            }
            case QEvent::FocusIn:
            case QEvent::FocusOut:
            {
                auto* fe = (QFocusEvent*)e;
                QApplication::postEvent(mSpinbox2, new QFocusEvent(e->type(), fe->reason()));
                updateButtons = true;
                break;
            }
            default:
                break;
        }
    }
    else if (obj == mSpinbox2)
    {
        switch (e->type())
        {
            case QEvent::Enter:
            case QEvent::Leave:
            case QEvent::HoverEnter:
            case QEvent::HoverLeave:
            case QEvent::EnabledChange:
                updateButtons = true;
                break;
            case QEvent::MouseButtonRelease:
            {
                auto* me = static_cast<QMouseEvent*>(e);
                if (me->button() == Qt::LeftButton)
                    paintTimer();   // cause the mirror widget buttons to be updated
                break;
            }
            default:
                break;
        }
    }
    if (updateButtons)
        QTimer::singleShot(0, this, &SpinBox2p::updateMirrorButtons);
    return false;
}

/******************************************************************************
* Set up the widget's geometry. Called when the widget is about to be
* displayed, or when the style changes.
*/
void SpinBox2p::rearrange()
{
    setUpdown2Size();   // set the new size of the second pair of spin buttons
    arrange();
    if (mShowUpdown2)
    {
        mSpinMirror->setFrameImage();
        mSpinMirror->rearrange();
    }
}

/******************************************************************************
* Set the positions and sizes of all the child widgets.
*/
void SpinBox2p::arrange()
{
    QSize sz = mSpinbox->minimumSizeHint();
    mSpinbox->setMinimumSize(sz);
    sz.setWidth(sz.width() + wUpdown2);
    setMinimumSize(sz);
    getMetrics();
#if 0   // for testing
QPalette pal = palette();
pal.setColor(QPalette::Window, Qt::green);
setPalette(pal);
setAutoFillBackground(true);
pal.setColor(QPalette::Window, Qt::yellow);
mSpinbox->setPalette(pal);
mSpinbox->setAutoFillBackground(true);
pal.setColor(QPalette::Window, Qt::magenta);
mSpinbox2->setPalette(pal);
mSpinbox2->setAutoFillBackground(true);
pal.setColor(QPalette::Window, Qt::red);
mSpinMirror->setPalette(pal);
mSpinMirror->setAutoFillBackground(true);
#endif
    if (mShowUpdown2)
    {
        const int mirrorWidth   = wUpdown2 + wBorderWidth;
        const int offsetX       = wFrameWidth - wBorderWidth;
        const int spinboxOffset = wUpdown2 - offsetX;
        mSpinbox2->move(-mSpinbox2->width(), 0);   // keep completely hidden
        const QRect mirrorRect = style()->visualRect((mRightToLeft ? Qt::RightToLeft : Qt::LeftToRight), rect(), QRect(0, 0, mirrorWidth, height()));
        mSpinbox->setGeometry(mRightToLeft ? 0 : spinboxOffset, 0, width() - spinboxOffset, height());
        QRect rf(0, 0, mSpinbox->width() + spinboxOffset, height());
        setGeometry(rf);
//        qCDebug(KALARM_LOG) << "SpinBox2p::getMetrics: mirrorRect="<<mirrorRect<<", mSpinbox2="<<mSpinbox2->geometry()<<", mSpinbox="<<mSpinbox->geometry()<<", hint="<<mSpinbox->sizeHint()<<", width="<<width()<<", setGeom="<<rf;

        mSpinMirror->resize(mirrorWidth, mSpinbox2->height());
        mSpinMirror->setGeometry(mirrorRect);
        mSpinMirror->setButtonPos(mButtonPos);
        mSpinMirror->setButtonsImage();
    }
}

/******************************************************************************
* Calculate the width and position of the extra pair of spin buttons.
* Style-specific adjustments are made for a better appearance.
*/
void SpinBox2p::getMetrics() const
{
    QStyleOptionSpinBox option;
    mSpinbox->initStyleOption(&option);
    const QRect editRect = spinBoxEditFieldRect(mSpinbox);
    {
        // Check whether both mSpinbox spin buttons are on the same side of the control,
        // and if not, show only the normal spinbox without extra spin buttons.
        const QRect upRect_   = mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp);
        const QRect downRect_ = mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
        bool showUpdown2 = ((upRect_.left() > editRect.left())  &&  (downRect_.left() > editRect.left()))
                       ||  ((upRect_.right() < editRect.right())  &&  (downRect_.right() < editRect.right()));
        setShowUpdown2(showUpdown2);
        if (!mShowUpdown2)
            return;
    }

    const QRect buttons2Rect = spinBoxButtonsRect(mSpinbox2, true);
    const QRect buttons2DrawRect = spinBoxButtonsRect(mSpinbox2, false);
    QStyle* udStyle = mSpinbox2->style();
    mSpinbox2->initStyleOption(&option);
    const QRect frame2Rect = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame);
    wFrameWidth = udStyle->pixelMetric(QStyle::PM_SpinBoxFrameWidth, &option);
    wBorderWidth = mRightToLeft ? buttons2Rect.left() : mSpinbox2->width() - buttons2Rect.right() - 1;
    wUpdown2 = buttons2Rect.width();
    const int butx = mRightToLeft           ? buttons2DrawRect.left()
                   : isMirrorStyle(udStyle) ? buttons2DrawRect.left() - buttons2Rect.left()
                   :                          frame2Rect.right() - buttons2DrawRect.right();
    mButtonPos = QPoint(butx, buttons2Rect.top());
//    qCDebug(KALARM_LOG) << "SpinBox2p::getMetrics: mSpinbox2:"<<mSpinbox2->geometry()<<", buttons:"<<buttons2Rect<<", buttons draw:"<<buttons2DrawRect<<", edit:"<<spinBoxEditFieldRect(mSpinbox2)<<", frame:"<<frame2Rect<<", frame width:"<<wFrameWidth<<", border:"<<wBorderWidth<<", wUpdown2="<<wUpdown2<<", wFrameWidth="<<wFrameWidth<<", frame right="<<mSpinbox2->width() - buttons2Rect.right() - 1<<", button Pos:"<<mButtonPos;
}

/******************************************************************************
* Called when the extra pair of spin buttons is clicked to step the value.
* Normally this is a page step, but with a right-to-left language where the
* button functions are reversed, this is a line step.
*/
void SpinBox2p::stepPage(int step, bool modified)
{
    if (abs(step) == mSpinbox2->singleStep()  ||  modified)
        mSpinbox->setValue(mSpinbox2->value());
    else
    {
        // It's a shift step
        int oldValue = mSpinbox->value();
        if (!reverseButtons())
        {
            // The button pairs have the normal function.
            // Page shift stepping - step up or down to a multiple of the
            // shift page increment, leaving unchanged the part of the value
            // which is the remainder from the page increment.
            if (oldValue >= 0)
                oldValue -= oldValue % mSpinbox2->singleStep();
            else
                oldValue += (-oldValue) % mSpinbox2->singleStep();
        }
        int adjust = mSpinbox->shiftStepAdjustment(oldValue, step);
        if (adjust == -step
        &&  ((step > 0  &&  oldValue + step >= mSpinbox->maximum())
          || (step < 0  &&  oldValue + step <= mSpinbox->minimum())))
            adjust = 0;    // allow stepping to the minimum or maximum value
        mSpinbox->addValue(adjust + step);
    }
    mSpinbox->setFocus();
    if (mSpinbox->selectOnStep())
        mSpinbox->selectAll();

    // Make the covering arrows image show the pressed arrow
    mSpinMirror->setButtonsImage();
}

/******************************************************************************
* Set whether the second pair of spin buttons should be shown.
*/
void SpinBox2p::setShowUpdown2(bool show) const
{
    if (show != mShowUpdown2)
    {
        mShowUpdown2 = show;
        mSpinbox2->setVisible(mShowUpdown2);
        mSpinMirror->setVisible(mShowUpdown2);
        setSteps();
        setShiftSteps();
    }
}


/*=============================================================================
= Class SpinBox2p::MainSpinBox
=============================================================================*/

QString SpinBox2p::MainSpinBox::textFromValue(int v) const
{
    return owner->textFromValue(v);
}

int SpinBox2p::MainSpinBox::valueFromText(const QString& t) const
{
    return owner->valueFromText(t);
}

QValidator::State SpinBox2p::MainSpinBox::validate(QString& text, int& pos) const
{
    return owner->validate(text, pos);
}

/******************************************************************************
* Return the initial adjustment to the value for a shift step up or down, for
* the main (visible) spin box.
* Normally this is a line step, but with a right-to-left language where the
* button functions are reversed, this is a page step.
*/
int SpinBox2p::MainSpinBox::shiftStepAdjustment(int oldValue, int shiftStep)
{
    if (owner_p->mShowUpdown2  &&  owner_p->reverseButtons())
    {
        // The button pairs have the opposite function from normal.
        // Page shift stepping - step up or down to a multiple of the
        // shift page increment, leaving unchanged the part of the value
        // which is the remainder from the page increment.
        if (oldValue >= 0)
            oldValue -= oldValue % singleStep();
        else
            oldValue += (-oldValue) % singleStep();
    }
    return SpinBox::shiftStepAdjustment(oldValue, shiftStep);
}


/*=============================================================================
= Class ExtraSpinBox
=============================================================================*/

/******************************************************************************
* Repaint the widget.
*/
void ExtraSpinBox::paintEvent(QPaintEvent* e)
{
    SpinBox::paintEvent(e);
    if (!mInhibitPaintSignal)
        Q_EMIT painted();
    else
        --mInhibitPaintSignal;
}


/*=============================================================================
= Class SpinMirror
=============================================================================*/

SpinMirror::SpinMirror(ExtraSpinBox* spinbox, SpinBox* mainspin, QWidget* parent)
    : QGraphicsView(parent)
    , mSpinbox(spinbox)
    , mMainSpinbox(mainspin)
    , mReadOnly(false)
    , mMirrored(false)
{
    setScene(new QGraphicsScene(this));
    setAttribute(Qt::WA_Hover);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);
    setMouseTracking(mSpinbox->hasMouseTracking());
    mButtons = scene()->addPixmap(QPixmap());
    mButtons->setZValue(1);
    mButtons->setAcceptedMouseButtons(Qt::LeftButton);
    mMirrored = isMirrorStyle(style());
    setMirroredState();
}

void SpinMirror::setMirroredState(bool clear)
{
    // Some styles only look right when the buttons are mirrored
    if (mMirrored)
        setTransform(QTransform(-1, 0, 0, 1, width() , 0));  // mirror left to right
    else if (clear)
        setTransform(QTransform());
}

/******************************************************************************
* Copy the left hand frame of the main spinbox to use as the background for
* this widget. The image of the spin buttons to be painted on top is set up by
* setButtonsImage().
* Copy the frame to the left of the edit field, plus a single pixel slice to
* the left of the spin buttons. Then stretch the slice to the full width - this
* sets the correct background spin button color.
*/
void SpinMirror::setFrameImage()
{
    QGraphicsScene* c = scene();
    const bool rtl = QApplication::isRightToLeft();
    QPixmap p;
    if (mMirrored)
    {
        const int x = rtl ? 0 : mMainSpinbox->width() - width();
        p = grabWidget(mMainSpinbox, QRect(x, 0, width(), height()));
    }
    else
    {
        // Grab a single pixel wide vertical slice through the main spinbox, from
        // just to the left of the spin buttons.
        QRect rb = spinBoxButtonsRect(mMainSpinbox, false);
        const int x = rtl ? rb.right() + 1 : rb.left() - 1;
        p = grabWidget(mMainSpinbox, QRect(x, 0, 1, height()));
        // Horizontally fill the mirror widget with the vertical slice, to set the
        // correct background color.
        p = p.scaled(size());

        // Grab the left hand border of the main spinbox, and draw it into the mirror widget.
        // Also grab the right hand border of the edit field, and draw it.
        QRect endr{0, 0, 0, height()};
        QRect editr;   // area within mMainSpinbox to get border between spin buttons and edit field
        int editx;
        const int editOffsetX = 2;   // offset into edit field
        const QRect buttonsRect = spinBoxButtonsRect(mMainSpinbox, true);
        if (rtl)
        {
            const int mr = mMainSpinbox->width() - 1;
            const QRect re = spinBoxEditFieldRect(mMainSpinbox);
            endr.setWidth(mr - re.right() + editOffsetX);
            endr.moveRight(mr);
            editr = QRect(buttonsRect.right(), 0, 1, height());
            editx = 0;
        }
        else
        {
        QStyle* mainStyle = mMainSpinbox->style();
        QStyleOptionSpinBox option;
        mMainSpinbox->initStyleOption(&option);
        const int frameWidth = mainStyle->pixelMetric(QStyle::PM_SpinBoxFrameWidth, &option);  // offset to edit field
            endr.setWidth(frameWidth + editOffsetX);
            editr = QRect(buttonsRect.left(), 0, 1, height());
            editx = width() - 1;
        }
        const int endx = rtl ? width() - endr.width() : 0;
//        qCDebug(KALARM_LOG) << "SpinMirror::setFrameImage: editx:"<<editx<<", editr:"<<editr<<"... endx:"<<endx<<", endr:"<<endr;
        mMainSpinbox->render(&p, QPoint(endx, 0), endr, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
        mMainSpinbox->render(&p, QPoint(editx, 0), editr, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
    }
    c->setBackgroundBrush(p);
}

/******************************************************************************
* Copy the image of the spin buttons from the extra spin box, ready to be
* painted into this widget, on top of the background set up by setFrameImage().
*/
void SpinMirror::setButtonsImage()
{
    mSpinbox->inhibitPaintSignal(2);
    QRect r = spinBoxButtonsRect(mSpinbox, false);
    mSpinbox->inhibitPaintSignal(1);
    mButtons->setPixmap(grabWidget(mSpinbox, r));
    mSpinbox->inhibitPaintSignal(0);
}

/******************************************************************************
* Set the position where the spin buttons will be painted in this widget.
*/
void SpinMirror::setButtonPos(const QPoint& pos)
{
    mButtons->setPos(pos);
}

void SpinMirror::resizeEvent(QResizeEvent* e)
{
    const QSize sz = e->size();
    scene()->setSceneRect(0, 0, sz.width(), sz.height());
    setMirroredState();
}

void SpinMirror::rearrange()
{
    mMirrored = isMirrorStyle(style());
    setMirroredState(true);
}

/******************************************************************************
* Pass on to the extra spinbox all mouse events which occur over the spin
* button area.
*/
void SpinMirror::mouseEvent(QMouseEvent* e)
{
    if (mReadOnly)
        return;
    QPointF pt = e->pos();
    const QGraphicsItem* item = scene()->itemAt(pt, QTransform());
    if (item == mButtons)
        pt = spinboxPoint(pt);
    else
        pt = QPointF(0, 0);  // allow auto-repeat to stop
    QApplication::postEvent(mSpinbox, new QMouseEvent(e->type(), pt, e->globalPosition(), e->button(), e->buttons(), e->modifiers()));
}

/******************************************************************************
* Pass on to the extra spinbox all wheel events which occur over the spin
* button area.
*/
void SpinMirror::wheelEvent(QWheelEvent* e)
{
    if (mReadOnly)
        return;
    QPointF pt = e->position();
    const QGraphicsItem* item = scene()->itemAt(pt, QTransform());
    if (item == mButtons)
    {
        pt = spinboxPoint(pt);
        QApplication::postEvent(mSpinbox, new QWheelEvent(pt, e->globalPosition(), e->pixelDelta(), e->angleDelta(), e->buttons(), e->modifiers(), e->phase(), e->inverted(), e->source()));
    }
}

/******************************************************************************
* Translate SpinMirror coordinates to those of the mirrored spinbox.
*/
QPointF SpinMirror::spinboxPoint(const QPointF& param) const
{
    const QRect r = mSpinbox->upRect();
    const QPointF ptf = mButtons->mapFromScene(param.x(), param.y());
    QPointF pt(ptf.x(), ptf.y());
    pt.setX(ptf.x() + r.left());
    pt.setY(ptf.y() + r.top());
    return pt;
}

/******************************************************************************
* Pass on to the main spinbox events which are needed to activate mouseover and
* other graphic effects when the mouse cursor enters and leaves the widget.
*/
bool SpinMirror::event(QEvent* e)
{
//qCDebug(KALARM_LOG)<<e->type();
    QHoverEvent* he = nullptr;
    switch (e->type())
    {
        case QEvent::Leave:
            if (mMainSpinbox->rect().contains(mMainSpinbox->mapFromGlobal(QCursor::pos())))
                break;
            [[fallthrough]];   // fall through to QEvent::Enter
        case QEvent::Enter:
            QApplication::postEvent(mMainSpinbox, new QEvent(e->type()));
            break;
        case QEvent::HoverLeave:
            he = (QHoverEvent*)e;
            if (mMainSpinbox->rect().contains(mMainSpinbox->mapFromGlobal(QCursor::pos())))
                break;
            [[fallthrough]];   // fall through to QEvent::HoverEnter
        case QEvent::HoverEnter:
            he = (QHoverEvent*)e;
            QApplication::postEvent(mMainSpinbox, new QHoverEvent(e->type(), he->position(), he->globalPosition(), he->oldPosF()));
            break;
        case QEvent::HoverMove:
            he = (QHoverEvent*)e;
            break;
        case QEvent::FocusIn:
            mMainSpinbox->setFocus();
            break;
        default:
            break;
    }

    if (he)
    {
        QApplication::postEvent(mSpinbox, new QHoverEvent(e->type(), spinboxPoint(he->position()), he->globalPosition(), spinboxPoint(he->oldPosF())));
        setButtonsImage();
    }

    return QGraphicsView::event(e);
}

namespace
{

/******************************************************************************
* Determine whether the extra pair of spin buttons needs to be mirrored
* left-to-right in the specified style.
*/
bool isMirrorStyle(const QStyle* style)
{
    for (const char** s = mirrorStyles;  *s;  ++s)
        if (style->inherits(*s))
            return true;
    return false;
}

QRect spinBoxEditFieldRect(const SpinBox* w)
{
    QStyleOptionSpinBox option;
    w->initStyleOption(&option);
    QRect r = w->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField);
    return r;
}

QRect spinBoxButtonsRect(const SpinBox* w, bool includeBorders)
{
    QStyleOptionSpinBox option;
    w->initStyleOption(&option);
    QRect r = w->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
            | w->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
    if (w->style()->inherits("PlastikStyle"))
        r.setLeft(r.left() - 1);    // Plastik excludes left border from spin widget rectangle
    if (!includeBorders)
    {
        const int frameWidth = std::max(w->style()->pixelMetric(QStyle::PM_SpinBoxFrameWidth, &option) - 2, 0);
        r.setLeft(r.left() + frameWidth);
        r.setWidth(r.width() - frameWidth);
    }
    return r;
}

}

#include "moc_spinbox2_p.cpp"
#include "moc_spinbox2.cpp"

// vim: et sw=4:
