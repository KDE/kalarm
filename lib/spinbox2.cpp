/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons (for Qt 3)
 *  Program:  kalarm
 *  Copyright © 2001-2008 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include <stdlib.h>

#include <QMouseEvent>
#include <QStyleOptionSpinBox>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QFrame>
#include <QBrush>
#include <QStyle>
#include <QObject>
#include <QApplication>
#include <QPixmap>
#include <QMatrix>

#include <kdebug.h>

#include "spinbox2.moc"
#include "spinbox2private.moc"


static inline QPixmap grabWidget(QWidget* w, QRect r = QRect())
{
	QPixmap p(r.isEmpty() ? w->size() : r.size());
	w->render(&p, QPoint(0,0), r, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
	return p;
}

int SpinBox2::mRightToLeft = -1;

SpinBox2::SpinBox2(QWidget* parent)
	: QFrame(parent),
	  mReverseWithLayout(true)
{
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(this);
//	mSpinbox = new MainSpinBox(0, 1, this, mSpinboxFrame);
	mSpinbox = new MainSpinBox(this, mSpinboxFrame);
	init();
}

SpinBox2::SpinBox2(int minValue, int maxValue, int pageStep, QWidget* parent)
	: QFrame(parent),
	  mReverseWithLayout(true)
{
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(minValue, maxValue, this);
	mSpinbox = new MainSpinBox(minValue, maxValue, this, mSpinboxFrame);
	setSteps(1, pageStep);
	init();
}

void SpinBox2::init()
{
	if (mRightToLeft < 0)
		mRightToLeft = QApplication::isRightToLeft() ? 1 : 0;
	mMinValue        = mSpinbox->minimum();
	mMaxValue        = mSpinbox->maximum();
	mSingleStep      = mSpinbox->singleStep();
	mSingleShiftStep = mSpinbox->singleShiftStep();
	mPageStep        = mUpdown2->singleStep();
	mPageShiftStep   = mUpdown2->singleShiftStep();
	mSpinbox->setSelectOnStep(false);    // default
	mUpdown2->setSelectOnStep(false);    // always false
	setFocusProxy(mSpinbox);
	mUpdown2->setFocusPolicy(Qt::NoFocus);
	mSpinMirror = new SpinMirror(mUpdown2, mSpinbox, this);
	mSpinbox->installEventFilter(this);
	mUpdown2->installEventFilter(this);
	connect(mSpinbox, SIGNAL(valueChanged(int)), SLOT(valueChange()));
	connect(mSpinbox, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
	connect(mSpinbox, SIGNAL(valueChanged(const QString&)), SIGNAL(valueChanged(const QString&)));
	connect(mUpdown2, SIGNAL(stepped(int)), SLOT(stepPage(int)));
	connect(mUpdown2, SIGNAL(painted()), SLOT(paintTimer()));
}

void SpinBox2::setReadOnly(bool ro)
{
	if (static_cast<int>(ro) != static_cast<int>(mSpinbox->isReadOnly()))
	{
		mSpinbox->setReadOnly(ro);
		mUpdown2->setReadOnly(ro);
		mSpinMirror->setReadOnly(ro);
	}
}

void SpinBox2::setReverseWithLayout(bool reverse)
{
	if (reverse != mReverseWithLayout)
	{
		mReverseWithLayout = reverse;
		setSteps(mSingleStep, mPageStep);
		setShiftSteps(mSingleShiftStep, mPageShiftStep);
	}
}

void SpinBox2::setEnabled(bool enabled)
{
	QFrame::setEnabled(enabled);
	mSpinbox->setEnabled(enabled);
	mUpdown2->setEnabled(enabled);
	updateMirror();
}

void SpinBox2::setWrapping(bool on)
{
	mSpinbox->setWrapping(on);
	mUpdown2->setWrapping(on);
}

QRect SpinBox2::up2Rect() const
{
	return mUpdown2->upRect();
}

QRect SpinBox2::down2Rect() const
{
	return mUpdown2->downRect();
}

void SpinBox2::setSingleStep(int step)
{
	mSingleStep = step;
	if (reverseButtons())
		mUpdown2->setSingleStep(step);   // reverse layout, but still set the right buttons
	else
		mSpinbox->setSingleStep(step);
}

void SpinBox2::setSteps(int single, int page)
{
	mSingleStep = single;
	mPageStep   = page;
	if (reverseButtons())
	{
		mUpdown2->setSingleStep(single);   // reverse layout, but still set the right buttons
		mSpinbox->setSingleStep(page);
	}
	else
	{
		mSpinbox->setSingleStep(single);
		mUpdown2->setSingleStep(page);
	}
}

void SpinBox2::setShiftSteps(int single, int page)
{
	mSingleShiftStep = single;
	mPageShiftStep   = page;
	if (reverseButtons())
	{
		mUpdown2->setSingleShiftStep(single);   // reverse layout, but still set the right buttons
		mSpinbox->setSingleShiftStep(page);
	}
	else
	{
		mSpinbox->setSingleShiftStep(single);
		mUpdown2->setSingleShiftStep(page);
	}
}

void SpinBox2::setButtonSymbols(QSpinBox::ButtonSymbols newSymbols)
{
	if (mSpinbox->buttonSymbols() == newSymbols)
		return;
	mSpinbox->setButtonSymbols(newSymbols);
	mUpdown2->setButtonSymbols(newSymbols);
}

int SpinBox2::bound(int val) const
{
	return (val < mMinValue) ? mMinValue : (val > mMaxValue) ? mMaxValue : val;
}

void SpinBox2::setMinimum(int val)
{
	mMinValue = val;
	mSpinbox->setMinimum(val);
	mUpdown2->setMinimum(val);
}

void SpinBox2::setMaximum(int val)
{
	mMaxValue = val;
	mSpinbox->setMaximum(val);
	mUpdown2->setMaximum(val);
}

void SpinBox2::valueChange()
{
	int val = mSpinbox->value();
	bool blocked = mUpdown2->signalsBlocked();
	mUpdown2->blockSignals(true);
	mUpdown2->setValue(val);
	mUpdown2->blockSignals(blocked);
}

/******************************************************************************
* Called when the widget is about to be displayed.
* (At construction time, the spin button widths cannot be determined correctly,
*  so we need to wait until now to definitively rearrange the widget.)
*/
void SpinBox2::showEvent(QShowEvent*)
{
	setUpdown2Size();   // set the new size of the second pair of spin buttons
	arrange();
	mSpinMirror->setFrame();
}

QSize SpinBox2::sizeHint() const
{
	getMetrics();
	QSize size = mSpinbox->sizeHint();
	size.setWidth(size.width() - wSpinboxHide + wUpdown2);
	return size;
}

QSize SpinBox2::minimumSizeHint() const
{
	getMetrics();
	QSize size = mSpinbox->minimumSizeHint();
	size.setWidth(size.width() - wSpinboxHide + wUpdown2);
	return size;
}

void SpinBox2::styleChange(QStyle&)
{
	setUpdown2Size();   // set the new size of the second pair of spin buttons
	arrange();
	mSpinMirror->setFrame();
}

void SpinBox2::paintEvent(QPaintEvent* e)
{
	QFrame::paintEvent(e);
	QTimer::singleShot(0, this, SLOT(updateMirrorFrame()));
}

void SpinBox2::paintTimer()
{
	QTimer::singleShot(0, this, SLOT(updateMirrorButtons()));
}

void SpinBox2::updateMirrorButtons()
{
	mSpinMirror->setButtons();
}

void SpinBox2::updateMirrorFrame()
{
	mSpinMirror->setFrame();
}

void SpinBox2::spinboxResized(QResizeEvent* e)
{
	int h = e->size().height();
	if (h != mUpdown2->height())
	{
		mUpdown2->setFixedSize(mUpdown2->width(), e->size().height());
		setUpdown2Size();
	}
}

/******************************************************************************
* Set the size of the second spin button widget.
* It is necessary to fix the size to avoid infinite recursion in arrange().
*/
void SpinBox2::setUpdown2Size()
{
	mSpinMirror->setButtons();
}

/******************************************************************************
* Called when the extra pair of spin buttons has repainted after a style change. 
* Updates the mirror image of the spin buttons.
*/
void SpinBox2::updateMirror()
{
	mSpinMirror->setButtons();
	mSpinMirror->setFrame();
}

bool SpinBox2::eventFilter(QObject* obj, QEvent* e)
{
	bool updateButtons = false;
	if (obj == mSpinbox)
	{
//kDebug()<<"activated="<<
//if (e->type() != QEvent::Paint) kDebug()<<e->type();
		switch (e->type())
		{
			case QEvent::Enter:
			case QEvent::Leave:
				QApplication::postEvent(mUpdown2, new QEvent(e->type()));
				updateButtons = true;
				break;
			case QEvent::HoverEnter:
			{
				QHoverEvent* he = (QHoverEvent*)e;
				QApplication::postEvent(mUpdown2, new QHoverEvent(e->type(), QPoint(1, he->pos().y()), he->oldPos()));
				updateButtons = true;
				break;
			}
			case QEvent::HoverLeave:
			{
				QHoverEvent* he = (QHoverEvent*)e;
				QApplication::postEvent(mUpdown2, new QHoverEvent(e->type(), he->pos(), QPoint(1, he->oldPos().y())));
				updateButtons = true;
				break;
			}
			case QEvent::FocusIn:
			case QEvent::FocusOut:
			{
				QFocusEvent* fe = (QFocusEvent*)e;
				QApplication::postEvent(mUpdown2, new QFocusEvent(e->type(), fe->reason()));
				updateButtons = true;
				break;
			}
			default:
				break;
		}
	}
	else if (obj == mUpdown2)
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
			default:
				break;
		}
	}
	if (updateButtons)
		QTimer::singleShot(0, this, SLOT(updateMirrorButtons()));
	return false;
}

/******************************************************************************
* Set the positions and sizes of all the child widgets. 
*/
void SpinBox2::arrange()
{
	getMetrics();
	mUpdown2->move(-mUpdown2->width(), 0);   // keep completely hidden
	QRect arrowRect = style()->visualRect((mRightToLeft ? Qt::RightToLeft : Qt::LeftToRight), rect(), QRect(0, 0, wUpdown2, height()));
//	QRect r = style()->visualRect((mRightToLeft ? Qt::RightToLeft : Qt::LeftToRight), rect(), QRect(wUpdown2, 0, width() - wUpdown2, height()));
	QRect r(wUpdown2, 0, width() - wUpdown2, height());
	if (mRightToLeft)
		r.moveLeft(0);
	mSpinboxFrame->setGeometry(r);
	mSpinbox->setGeometry(mRightToLeft ? 0 : -wSpinboxHide, 0, mSpinboxFrame->width() + wSpinboxHide, height());
	kDebug() << "arrowRect="<<arrowRect<<", mUpdown2="<<mUpdown2->geometry()<<", mSpinboxFrame="<<mSpinboxFrame->geometry()<<", mSpinbox="<<mSpinbox->geometry()<<", width="<<width();

	mSpinMirror->resize(wUpdown2, mUpdown2->height());
	mSpinMirror->setGeometry(arrowRect);
	mSpinMirror->setButtons();
}

/******************************************************************************
* Calculate the width and position of the extra pair of spin buttons.
* Style-specific adjustments are made for a better appearance. 
*/
void SpinBox2::getMetrics() const
{
	QStyleOptionSpinBox option;
	mUpdown2->initStyleOption(option);
	QStyle* udStyle = mUpdown2->style();
	QRect butRect = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
	              | udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
	if (style()->inherits("PlastikStyle"))
		butRect.setLeft(butRect.left() - 1);    // Plastik excludes left border from spin widget rectangle
	QRect r = mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField);
	wSpinboxHide = mRightToLeft ? mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame).right() - r.right() : r.left();
	int x = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).right() + 1;
	r = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame);
	wUpdown2 = r.width() - x;
	if (mRightToLeft)
	{
		wUpdown2 = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).left() - r.left();
		mSpinMirror->setButtonPos(QPoint(wUpdown2 - (butRect.left() - r.left() + butRect.width()), butRect.top()));
	}
	else
		mSpinMirror->setButtonPos(QPoint(r.right() - butRect.right(), butRect.top()));
	kDebug() << ", x="<<x<<", wUpdown2="<<wUpdown2<<", wSpinboxHide="<<wSpinboxHide<<", frame right="<<r.right() - butRect.right();
}

/******************************************************************************
* Called when the extra pair of spin buttons is clicked to step the value.
* Normally this is a page step, but with a right-to-left language where the
* button functions are reversed, this is a line step.
*/
void SpinBox2::stepPage(int step)
{
	if (abs(step) == mUpdown2->singleStep())
		mSpinbox->setValue(mUpdown2->value());
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
				oldValue -= oldValue % mUpdown2->singleStep();
			else
				oldValue += (-oldValue) % mUpdown2->singleStep();
		}
		int adjust = mSpinbox->shiftStepAdjustment(oldValue, step);
		if (adjust == -step
		&&  (step > 0  &&  oldValue + step >= mSpinbox->maximum()
		  || step < 0  &&  oldValue + step <= mSpinbox->minimum()))
			adjust = 0;    // allow stepping to the minimum or maximum value
		mSpinbox->addValue(adjust + step);
	}
	mSpinbox->setFocus();
	if (mSpinbox->selectOnStep())
		mSpinbox->selectAll();

	// Make the covering arrows image show the pressed arrow
	mSpinMirror->setButtons();
}


/*=============================================================================
= Class SpinBox2::MainSpinBox
=============================================================================*/

/******************************************************************************
* Return the initial adjustment to the value for a shift step up or down, for
* the main (visible) spin box.
* Normally this is a line step, but with a right-to-left language where the
* button functions are reversed, this is a page step.
*/
int SpinBox2::MainSpinBox::shiftStepAdjustment(int oldValue, int shiftStep)
{
	if (owner->reverseButtons())
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
//kDebug()<<"paintEvent(): "<<mInhibitPaintSignal;
	SpinBox::paintEvent(e);
	if (!mInhibitPaintSignal)
		emit painted();
	else
		--mInhibitPaintSignal;
}


/*=============================================================================
= Class SpinMirror
=============================================================================*/

SpinMirror::SpinMirror(ExtraSpinBox* spinbox, SpinBox* mainspin, QWidget* parent)
	: QGraphicsView(new QGraphicsScene, parent),
	  mSpinbox(spinbox),
	  mMainSpinbox(mainspin),
	  mReadOnly(false)
{
	setAttribute(Qt::WA_Hover);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setFrameStyle(QFrame::NoFrame);
	setMouseTracking(mSpinbox->hasMouseTracking());
	mButtons = scene()->addPixmap(QPixmap());
	mButtons->setZValue(1);
	mButtons->setAcceptedMouseButtons(Qt::LeftButton);
}

void SpinMirror::setFrame()
{
	// Paint the left hand frame of the main spinbox.
	// Use the part to the left of the edit field, plus a slice at
	// the left of the edit field stretched for the rest of the width.
	// This avoids possibly grabbing text and displaying it in the
	// spin button area.
	QGraphicsScene* c = scene();
	QStyleOptionSpinBox option;
	option.initFrom(mMainSpinbox);
	QRect r = mMainSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField);
	bool rtl = QApplication::isRightToLeft();
	int x = rtl ? r.right() - 2 : r.left() + 2;
	QPixmap p(grabWidget(mMainSpinbox, QRect(x, 0, 1, height())).scaled(size()));
	QRect endr = rect();
	if (rtl)
	{
		int mr = mMainSpinbox->width() - 1;
		endr.setWidth(mr - r.right() + 2);
		endr.moveRight(mr);
	}
	else
		endr.setWidth(r.left() + 2);
	x = rtl ? width() - endr.width() : 0;
	mMainSpinbox->render(&p, QPoint(x, 0), endr, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
	c->setBackgroundBrush(p);
}

void SpinMirror::setButtons()
{
	mSpinbox->inhibitPaintSignal(2);
	QStyleOptionSpinBox option;
	mSpinbox->initStyleOption(option);
	QStyle* st = mSpinbox->style();
	QRect r = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
	        | st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
	if (st->inherits("OxygenStyle"))
	{
		// They also don't use all their height, so shorten them to 
		// allow frame highlighting to work properly.
		r.setTop(r.top() + 1);
		r.setHeight(r.height() - 2);
	}
	mSpinbox->inhibitPaintSignal(1);
	mButtons->setPixmap(grabWidget(mSpinbox, r));
	mSpinbox->inhibitPaintSignal(0);
}

void SpinMirror::setButtonPos(const QPoint& pos)
{
	int x = pos.x();
	int y = pos.y();
	if (style()->inherits("OxygenStyle"))
	{
		// Oxygen spin buttons don't use all their height. Prevent
		// the top overlapping the frame highlighting. Their height
		// is shortened in setButton() above.
		++y;
	}
	mButtons->setPos(x, y);
}

void SpinMirror::resizeEvent(QResizeEvent* e)
{
	QSize sz = e->size();
	scene()->setSceneRect(0, 0, sz.width(), sz.height());
}

/******************************************************************************
* Pass on to the extra spinbox all mouse events which occur over the spin
* button area.
*/
void SpinMirror::mouseEvent(QMouseEvent* e)
{
	if (mReadOnly)
		return;
	QPoint pt = e->pos();
	QGraphicsItem* item = scene()->itemAt(pt);
	if (item == mButtons)
	{
		QRect r = mSpinbox->upRect();
		QPointF ptf = item->mapFromScene(pt.x(), pt.y());
		pt = QPoint(ptf.x(), ptf.y());
		pt.setX(ptf.x() + r.left());
		pt.setY(ptf.y() + r.top());
	}
	else
		pt = QPoint(0, 0);  // allow auto-repeat to stop
	QApplication::postEvent(mSpinbox, new QMouseEvent(e->type(), pt, e->button(), e->buttons(), e->modifiers()));
}

/******************************************************************************
* Pass on to the main spinbox events which are needed to activate mouseover and
* other graphic effects when the mouse cursor enters and leaves the widget.
*/
bool SpinMirror::event(QEvent* e)
{
//kDebug()<<e->type();
	switch (e->type())
	{
		case QEvent::Leave:
			if (mMainSpinbox->rect().contains(mMainSpinbox->mapFromGlobal(QCursor::pos())))
				break;
			// fall through to QEvent::Enter
		case QEvent::Enter:
			QApplication::postEvent(mMainSpinbox, new QEvent(e->type()));
			break;
		case QEvent::HoverLeave:
			if (mMainSpinbox->rect().contains(mMainSpinbox->mapFromGlobal(QCursor::pos())))
				break;
			// fall through to QEvent::HoverEnter
		case QEvent::HoverEnter:
		{
			QHoverEvent* he = (QHoverEvent*)e;
			QApplication::postEvent(mMainSpinbox, new QHoverEvent(e->type(), he->pos(), he->oldPos()));
			break;
		}
		case QEvent::FocusIn:
			mMainSpinbox->setFocus();
			break;
		default:
			break;
	}
	return QGraphicsView::event(e);
}
