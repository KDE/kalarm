/*
 *  spinbox2.cpp  -  spin box with extra pair of spin buttons (for Qt 3)
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <qglobal.h>
#if QT_VERSION >= 300

#include <stdlib.h>

#include <qstyle.h>
#include <qobjectlist.h>
#include <qapplication.h>
#include <qpixmap.h>
#include <qwmatrix.h>

#include "spinbox2.moc"
#include "spinbox2private.moc"


/*  List of styles which need to display the extra pair of spin buttons as a
 *  left-to-right mirror image. This is only necessary when, for example, the
 *  corners of widgets are rounded. For most styles, it is better not to mirror
 *  the spin widgets so as to keep the normal lighting/shading on either side.
 */
static const char* mirrorStyles[] = {
	"PlastikStyle",
	0     // list terminator
};
static bool mirrorStyle(const QStyle&);


int SpinBox2::mReverseLayout = -1;

SpinBox2::SpinBox2(QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReverseWithLayout(true)
{
	mUpdown2Frame = new QFrame(this);
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(mUpdown2Frame, "updown2");
//	mSpinbox = new MainSpinBox(0, 1, 1, this, mSpinboxFrame);
	mSpinbox = new MainSpinBox(this, mSpinboxFrame);
	init();
}

SpinBox2::SpinBox2(int minValue, int maxValue, int step, int step2, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReverseWithLayout(true)
{
	mUpdown2Frame = new QFrame(this);
	mSpinboxFrame = new QFrame(this);
	mUpdown2 = new ExtraSpinBox(minValue, maxValue, step2, mUpdown2Frame, "updown2");
	mSpinbox = new MainSpinBox(minValue, maxValue, step, this, mSpinboxFrame);
	setSteps(step, step2);
	init();
}

void SpinBox2::init()
{
	if (mReverseLayout < 0)
		mReverseLayout = QApplication::reverseLayout() ? 1 : 0;
	mMinValue      = mSpinbox->minValue();
	mMaxValue      = mSpinbox->maxValue();
	mLineStep      = mSpinbox->lineStep();
	mLineShiftStep = mSpinbox->lineShiftStep();
	mPageStep      = mUpdown2->lineStep();
	mPageShiftStep = mUpdown2->lineShiftStep();
	mSpinbox->setSelectOnStep(false);    // default
	mUpdown2->setSelectOnStep(false);    // always false
	setFocusProxy(mSpinbox);
	mUpdown2->setFocusPolicy(QWidget::NoFocus);
	mSpinMirror = new SpinMirror(mUpdown2, this);
	if (!mirrorStyle(style()))
		mSpinMirror->hide();    // hide mirrored spin buttons when they are inappropriate
	connect(mSpinbox, SIGNAL(valueChanged(int)), SLOT(valueChange()));
	connect(mSpinbox, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
	connect(mSpinbox, SIGNAL(valueChanged(const QString&)), SIGNAL(valueChanged(const QString&)));
	connect(mUpdown2, SIGNAL(stepped(int)), SLOT(stepPage(int)));
	connect(mUpdown2, SIGNAL(styleUpdated()), SLOT(updateMirror()));
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
		setSteps(mLineStep, mPageStep);
		setShiftSteps(mLineShiftStep, mPageShiftStep);
	}
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

void SpinBox2::setLineStep(int step)
{
	mLineStep = step;
	if (reverseButtons())
		mUpdown2->setLineStep(step);   // reverse layout, but still set the right buttons
	else
		mSpinbox->setLineStep(step);
}

void SpinBox2::setSteps(int line, int page)
{
	mLineStep = line;
	mPageStep = page;
	if (reverseButtons())
	{
		mUpdown2->setLineStep(line);   // reverse layout, but still set the right buttons
		mSpinbox->setLineStep(page);
	}
	else
	{
		mSpinbox->setLineStep(line);
		mUpdown2->setLineStep(page);
	}
}

void SpinBox2::setShiftSteps(int line, int page)
{
	mLineShiftStep = line;
	mPageShiftStep = page;
	if (reverseButtons())
	{
		mUpdown2->setLineShiftStep(line);   // reverse layout, but still set the right buttons
		mSpinbox->setLineShiftStep(page);
	}
	else
	{
		mSpinbox->setLineShiftStep(line);
		mUpdown2->setLineShiftStep(page);
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

void SpinBox2::setMinValue(int val)
{
	mMinValue = val;
	mSpinbox->setMinValue(val);
	mUpdown2->setMinValue(val);
}

void SpinBox2::setMaxValue(int val)
{
	mMaxValue = val;
	mSpinbox->setMaxValue(val);
	mUpdown2->setMaxValue(val);
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
	arrange();
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
	if (mirrorStyle(style()))
		mSpinMirror->show();    // show rounded corners with Plastik etc.
	else
		mSpinMirror->hide();    // keep normal shading with other styles
	arrange();
}

/******************************************************************************
* Called when the extra pair of spin buttons has repainted after a style change. 
* Updates the mirror image of the spin buttons.
*/
void SpinBox2::updateMirror()
{
	mSpinMirror->setNormalButtons(QPixmap::grabWidget(mUpdown2Frame, 0, 0));
}

/******************************************************************************
* Set the positions and sizes of all the child widgets. 
*/
void SpinBox2::arrange()
{
	getMetrics();
	QRect arrowRect = QStyle::visualRect(QRect(0, 0, wUpdown2, height()), this);
	mUpdown2Frame->setGeometry(arrowRect);
	mUpdown2->setGeometry(-xUpdown2, 0, mUpdown2->width(), height());
	mSpinboxFrame->setGeometry(QStyle::visualRect(QRect(wUpdown2 + wGap, 0, width() - wUpdown2 - wGap, height()), this));
	mSpinbox->setGeometry(-xSpinbox, 0, mSpinboxFrame->width() + xSpinbox, height());
	mSpinMirror->resize(wUpdown2, mUpdown2->height());
	mSpinMirror->setGeometry(arrowRect);
//mSpinMirror->setGeometry(QStyle::visualRect(QRect(0, 11, wUpdown2, height()), this));
	mSpinMirror->setNormalButtons(QPixmap::grabWidget(mUpdown2Frame, 0, 0));
}

/******************************************************************************
* Calculate the width and position of the extra pair of spin buttons.
* Style-specific adjustments are made for a better appearance. 
*/
void SpinBox2::getMetrics() const
{
	QRect rect = mUpdown2->style().querySubControlMetrics(QStyle::CC_SpinWidget, mUpdown2, QStyle::SC_SpinWidgetButtonField);
	if (style().inherits("PlastikStyle"))
		rect.setLeft(rect.left() - 1);    // Plastik excludes left border from spin widget rectangle
	xUpdown2 = mReverseLayout ? 0 : rect.left();
	wUpdown2 = mUpdown2->width() - rect.left();
	xSpinbox = mSpinbox->style().querySubControlMetrics(QStyle::CC_SpinWidget, mSpinbox, QStyle::SC_SpinWidgetEditField).left();
	wGap = 0;

	// Make style-specific adjustments for a better appearance
	if (style().inherits("QMotifPlusStyle"))
	{
		xSpinbox = 0;      // show the edit control left border
		wGap = 2;          // leave a space to the right of the left-hand pair of spin buttons
	}
}

/******************************************************************************
* Called when the extra pair of spin buttons is clicked to step the value.
* Normally this is a page step, but with a right-to-left language where the
* button functions are reversed, this is a line step.
*/
void SpinBox2::stepPage(int step)
{
	if (abs(step) == mUpdown2->lineStep())
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
				oldValue -= oldValue % mUpdown2->lineStep();
			else
				oldValue += (-oldValue) % mUpdown2->lineStep();
		}
		int adjust = mSpinbox->shiftStepAdjustment(oldValue, step);
		if (adjust == -step
		&&  (step > 0  &&  oldValue + step >= mSpinbox->maxValue()
		  || step < 0  &&  oldValue + step <= mSpinbox->minValue()))
			adjust = 0;    // allow stepping to the minimum or maximum value
		mSpinbox->addValue(adjust + step);
	}
	bool focus = mSpinbox->selectOnStep() && mUpdown2->hasFocus();
	if (focus)
		mSpinbox->selectAll();

	// Make the covering arrows image show the pressed arrow
	mSpinMirror->redraw(QPixmap::grabWidget(mUpdown2Frame, 0, 0));
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
			oldValue -= oldValue % lineStep();
		else
			oldValue += (-oldValue) % lineStep();
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
	if (mNewStylePending)
	{
		mNewStylePending = false;
		emit styleUpdated();
	}
}


/*=============================================================================
= Class SpinMirror
=============================================================================*/

SpinMirror::SpinMirror(SpinBox* spinbox, QWidget* parent, const char* name)
	: QCanvasView(new QCanvas, parent, name),
	  mSpinbox(spinbox),
	  mReadOnly(false)
{
	setVScrollBarMode(QScrollView::AlwaysOff);
	setHScrollBarMode(QScrollView::AlwaysOff);
	setFrameStyle(QFrame::NoFrame);

	// Find the spin widget which is part of the spin box, in order to
	// pass on its shift-button presses.
	QObjectList* spinwidgets = spinbox->queryList("QSpinWidget", 0, false, true);
	mSpinWidget = (SpinBox*)spinwidgets->getFirst();
	delete spinwidgets;
}

void SpinMirror::setNormalButtons(const QPixmap& px)
{
	mNormalButtons = px;
	redraw(mNormalButtons);
}

void SpinMirror::redraw(const QPixmap& px)
{
	QCanvas* c = canvas();
	c->setBackgroundPixmap(px);
	c->setAllChanged();
	c->update();
}

void SpinMirror::resize(int w, int h)
{
	canvas()->resize(w, h);
	QCanvasView::resize(w, h);
	resizeContents(w, h);
	setWorldMatrix(QWMatrix(-1, 0, 0, 1, w - 1, 0));  // mirror left to right
}

/******************************************************************************
* Pass on all mouse events to the spinbox which we're covering up.
*/
void SpinMirror::contentsMouseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
	{
		QPoint pt = contentsToViewport(e->pos());
		pt.setX(pt.x() + mSpinbox->upRect().left());
		QApplication::postEvent(mSpinWidget, new QMouseEvent(e->type(), pt, e->button(), e->state()));

		// If the mouse button has been released, display unpressed spin buttons
		if (e->type() == QEvent::MouseButtonRelease)
			redraw(mNormalButtons);
	}
}


/*=============================================================================
= Local functions
=============================================================================*/

/******************************************************************************
* Determine whether the extra pair of spin buttons needs to be mirrored
* left-to-right in the specified style.
*/
static bool mirrorStyle(const QStyle& style)
{
	for (const char** s = mirrorStyles;  *s;  ++s)
		if (style.inherits(*s))
			return true;
	return false;
}

#endif // QT_VERSION >= 300
