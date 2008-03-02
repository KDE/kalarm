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
#include <QGraphicsProxyWidget>
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


/*  List of styles which need to display the extra pair of spin buttons as a
 *  left-to-right mirror image. This is only necessary when, for example, the
 *  corners of widgets are rounded. For most styles, it is better not to mirror
 *  the spin widgets so as to keep the normal lighting/shading on either side.
 */
static const char* mirrorStyles[] = {
	"QCleanlooksStyle", "OxygenStyle", "PlastikStyle", "QPlastiqueStyle",
	0     // list terminator
};
static bool mirrorStyle(const QStyle*);


int SpinBox2::mRightToLeft = -1;

SpinBox2::SpinBox2(QWidget* parent)
	: QFrame(parent),
	  mReverseWithLayout(true)
{
	mUpdown2Frame = new QFrame(this);
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(mUpdown2Frame);
//	mSpinbox = new MainSpinBox(0, 1, this, mSpinboxFrame);
	mSpinbox = new MainSpinBox(this, mSpinboxFrame);
	init();
}

SpinBox2::SpinBox2(int minValue, int maxValue, int pageStep, QWidget* parent)
	: QFrame(parent),
	  mReverseWithLayout(true)
{
	mUpdown2Frame = new QFrame(this);
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(minValue, maxValue, mUpdown2Frame);
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
	mSpinMirror = new SpinMirror(mUpdown2, this);
	mUseMirror = mirrorStyle(style());
	if (!mUseMirror)
		mSpinMirror->hide();    // hide mirrored spin buttons when they are inappropriate
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
	mSpinMirror->setFrame(mSpinbox);
}

QSize SpinBox2::sizeHint() const
{
	getMetrics();
	QSize size = mSpinbox->sizeHint();
	size.setWidth(size.width() - xSpinbox + wUpdown2 + wGap);
	return size;
}

QSize SpinBox2::minimumSizeHint() const
{
	getMetrics();
	QSize size = mSpinbox->minimumSizeHint();
	size.setWidth(size.width() - xSpinbox + wUpdown2 + wGap);
	return size;
}

void SpinBox2::styleChange(QStyle&)
{
	mUseMirror = mirrorStyle(style());
	if (mUseMirror)
	{
		mSpinMirror->show();    // show rounded corners with Plastik etc.
		mSpinMirror->setFrame(mSpinbox);
	}
	else
		mSpinMirror->hide();    // keep normal shading with other styles
	setUpdown2Size();   // set the new size of the second pair of spin buttons
	arrange();
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
	mSpinMirror->setButtons(mUpdown2Frame);
}

void SpinBox2::updateMirrorFrame()
{
	mSpinMirror->setFrame(mSpinbox);
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
	QStyleOptionSpinBox option;
	mUpdown2->initStyleOption(option);
	QStyle* st = mUpdown2->style();
	int x = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).right() + 1;
	QSize s = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame).size();
	mUpdown2Frame->setFixedSize(s.width() - x, s.height());
	QRect r = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
	        | st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
kDebug()<<r<<", pixmap="<<QPixmap::grabWidget(mUpdown2Frame, r).size();
//	mSpinMirror->setButtons(QPixmap::grabWidget(mUpdown2));
	mSpinMirror->setButtons(mUpdown2Frame);
}

/******************************************************************************
* Called when the extra pair of spin buttons has repainted after a style change. 
* Updates the mirror image of the spin buttons.
*/
void SpinBox2::updateMirror()
{
	mSpinMirror->setButtons(mUpdown2Frame);
	mSpinMirror->setFrame(mSpinbox);
}

/******************************************************************************
* Set the positions and sizes of all the child widgets. 
*/
void SpinBox2::arrange()
{
	getMetrics();
	QRect arrowRect = style()->visualRect((mRightToLeft ? Qt::RightToLeft : Qt::LeftToRight), rect(), QRect(0, 0, wUpdown2, height()));
	QRect r(-xUpdown2, 0, mUpdown2->width(), height());
	if (mRightToLeft)
		arrowRect.setLeft(arrowRect.left() - wPadding);
	else
	{
		r.setLeft(r.left() + wPadding);
		arrowRect.setWidth(arrowRect.width() - wPadding);
	}
	mUpdown2Frame->move(arrowRect.topLeft());
	mUpdown2->move(r.topLeft());
	r = style()->visualRect((mRightToLeft ? Qt::RightToLeft : Qt::LeftToRight), rect(), QRect(wUpdown2 + wGap, 0, width() - wUpdown2 - wGap, height()));
	mSpinboxFrame->setGeometry(r);
	mSpinbox->setGeometry(-xSpinbox, 0, mSpinboxFrame->width() + xSpinbox, height());
	kDebug() << "arrowRect="<<arrowRect<<", mUpdown2Frame="<<mUpdown2Frame->geometry()<<", mUpdown2="<<mUpdown2->geometry()<<", mSpinboxFrame="<<mSpinboxFrame->geometry()<<", mSpinbox="<<mSpinbox->geometry()<<", width="<<width();
	if (mUseMirror)
	{
		mSpinMirror->resize(wUpdown2, mUpdown2->height());
		mSpinMirror->setGeometry(arrowRect);
//mSpinMirror->setGeometry(QStyle::visualRect(QRect(0, 11, wUpdown2, height()), this));
		mSpinMirror->setButtons(mUpdown2Frame);
	}
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
	if (mUseMirror)
	{
		// It's a style which needs a mirror image of the spin buttons
		// for the left-hand pair of buttons.
		xSpinbox = mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).left();
		xUpdown2 = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).right() + 1;
		QRect r = udStyle->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame);
		wUpdown2 = r.width() - xUpdown2;
		wPadding = wGap = 0;
		mSpinMirror->setButtonPos(QPoint(r.right() - butRect.right(), butRect.top()));
		kDebug() << ", xUpdown2="<<xUpdown2<<", wUpdown2="<<wUpdown2<<", xSpinbox="<<xSpinbox<<", wPadding"<<wPadding;
	}
	else
	{
		mSpinbox->initStyleOption(option);
		if (mRightToLeft)
		{
			wPadding = butRect.left();
			xUpdown2 = 0;
			wUpdown2 = butRect.right();
			xSpinbox = 0;
		}
		else
		{
			xUpdown2 = butRect.left();
			wUpdown2 = mUpdown2->width() - butRect.left();
			wPadding = mSpinbox->width() - butRect.right();
			xSpinbox = mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).left();
		}
		wGap = 0;
		kDebug() << "butRect="<<butRect<<", xUpdown2="<<xUpdown2<<", wUpdown2="<<wUpdown2<<", xSpinbox="<<xSpinbox<<", wPadding"<<wPadding;
		kDebug()<<"up="<<mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)<<", down="<<mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown)<<", edit="<<mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField)<<", frame="<<mSpinbox->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxFrame);

		// Make style-specific adjustments for a better appearance
		if (style()->inherits("QMotifPlusStyle"))
		{
			xSpinbox = 0;      // show the edit control left border
			wGap = 2;          // leave a space to the right of the left-hand pair of spin buttons
		}
	}
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
	mSpinMirror->setButtons(mUpdown2Frame);
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
* If it's the first time since a style change, tell the parent SpinBox2 to
* update the SpinMirror with the new unpressed button image. We make the
* presumably reasonable assumption that when a style change occurs, the spin
* buttons are unpressed.
*/
void ExtraSpinBox::paintEvent(QPaintEvent* e)
{
	SpinBox::paintEvent(e);
	if (!mInhibitPaintSignal)
		emit painted();
	else
		--mInhibitPaintSignal;
}


/*=============================================================================
= Class SpinMirror
=============================================================================*/

SpinMirror::SpinMirror(ExtraSpinBox* spinbox, QWidget* parent)
	: QGraphicsView(new QGraphicsScene, parent),
	  mSpinbox(spinbox),
	  mReadOnly(false)
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setFrameStyle(QFrame::NoFrame);
	mBackground = scene()->addRect(QRectF());
	mButtons = scene()->addPixmap(QPixmap());
	mButtons->setZValue(1);
	mButtons->setAcceptedMouseButtons(Qt::LeftButton);
}

void SpinMirror::setFrame(QSpinBox* w)
{
kDebug();
	QGraphicsScene* c = scene();
	c->setBackgroundBrush(QPixmap::grabWidget(w, rect()));
	QStyleOptionSpinBox option;
	option.initFrom(w);
	QRect r = w->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField);
	QColor colour(QPixmap::grabWidget(w).toImage().pixel(r.left()+2, r.bottom()-2));
	mBackground->setRect(r.left(), r.top() + 2, width() - r.left() - 1, r.height() - 4);
	mBackground->setBrush(QBrush(colour));
	mBackground->setPen(QPen(colour));
	c->update(c->sceneRect());
}

void SpinMirror::setButtons(QWidget* w)
{
	mSpinbox->inhibitPaintSignal(2);
	QStyleOptionSpinBox option;
	mSpinbox->initStyleOption(option);
	QStyle* st = mSpinbox->style();
	int x = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxEditField).right() + 1;
	QRect r = st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp)
	        | st->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
r.setWidth(r.width() + 1);
r.setHeight(r.height() - 1);
kDebug()<<"Updown="<<r<<", x="<<x;
	QPixmap p(r.size());
	r.translate(-x - 1, -2);
	r.setWidth(r.width() + 1);
	r.setHeight(r.height() + 2);
	QPoint pixoffset(-r.left(), -r.top() + 1);
kDebug()<<r<<", pixmap="<<QPixmap::grabWidget(w, r).size()<<", pixsize="<<p.size();
	mSpinbox->inhibitPaintSignal(2);
	w->render(&p, pixoffset, r, QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
	mButtons->setPixmap(p);
//	mButtons->setPixmap(QPixmap::grabWidget(w, r));
}

void SpinMirror::setButtonPos(const QPoint& pos)
{
	mButtons->setPos(pos.x(), pos.y());
}

void SpinMirror::resizeEvent(QResizeEvent* e)
{
	QSize sz = e->size();
	scene()->setSceneRect(0, 0, sz.width(), sz.height());
}

/******************************************************************************
* Pass on all mouse events to the spinbox which we're covering up.
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
		QApplication::postEvent(mSpinbox, new QMouseEvent(e->type(), pt, e->button(), e->buttons(), e->modifiers()));
	}
}


/*=============================================================================
= Local functions
=============================================================================*/

/******************************************************************************
* Determine whether the extra pair of spin buttons needs to be mirrored
* left-to-right in the specified style.
*/
static bool mirrorStyle(const QStyle* style)
{
	for (const char** s = mirrorStyles;  *s;  ++s)
		if (style->inherits(*s))
			return true;
	return false;
}
