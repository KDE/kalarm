/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons (for Qt 3)
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef SPINBOX2_H
#define SPINBOX2_H

#include <qglobal.h>
#if QT_VERSION >= 300

#include "spinbox.h"


/*
 * This class has a second pair of spin buttons at its left hand side.
 * These buttons use the inherited QRangeControl page step functions.
 */
class SpinBox2 : public QFrame
{
		Q_OBJECT
	public:
		SpinBox2(QWidget* parent = 0, const char* name = 0);
		SpinBox2(int minValue, int maxValue, int step = 1, int step2 = 1,
		         QWidget* parent = 0, const char* name = 0);

		void                setReadOnly(bool);
		bool                isReadOnly() const          { return spinbox->isReadOnly(); }
		void                setSelectOnStep(bool sel)   { spinbox->setSelectOnStep(sel); }
		void                setReverseWithLayout(bool);   // reverse buttons if right to left language?
		bool                reverseButtons() const      { return mReverseLayout  &&  !mReverseWithLayout; }

		QString             text() const                { return spinbox->text(); }
		virtual QString     prefix() const              { return spinbox->prefix(); }
		virtual QString     suffix() const              { return spinbox->suffix(); }
		virtual QString     cleanText() const           { return spinbox->cleanText(); }

		virtual void        setSpecialValueText(const QString& text)  { spinbox->setSpecialValueText(text); }
		QString             specialValueText() const    { return spinbox->specialValueText(); }

		virtual void        setWrapping(bool on)        { spinbox->setWrapping(on);  updown2->setWrapping(on); }
		bool                wrapping() const            { return spinbox->wrapping(); }

		virtual void        setButtonSymbols(QSpinBox::ButtonSymbols);
		QSpinBox::ButtonSymbols buttonSymbols() const   { return spinbox->buttonSymbols(); }

		virtual void        setValidator(const QValidator* v)  { spinbox->setValidator(v); }
		const QValidator*   validator() const           { return spinbox->validator(); }

		virtual QSize       sizeHint() const;
		virtual QSize       minimumSizeHint() const;

		int                 minValue() const            { return mMinValue; }
		int                 maxValue() const            { return mMaxValue; }
		void                setMinValue(int val);
		void                setMaxValue(int val);
		void                setRange(int minValue, int maxValue)   { setMinValue(minValue);  setMaxValue(maxValue); }
		int                 value() const               { return spinbox->value(); }
		int                 bound(int val) const;

		QRect               upRect() const              { return spinbox->upRect(); }
		QRect               downRect() const            { return spinbox->downRect(); }
		QRect               up2Rect() const             { return updown2->upRect(); }
		QRect               down2Rect() const           { return updown2->downRect(); }

		int                 lineStep() const            { return mLineStep; }
		int                 lineShiftStep() const       { return mLineShiftStep; }
		int                 pageStep() const            { return mPageStep; }
		int                 pageShiftStep() const       { return mPageShiftStep; }
		void                setLineStep(int step);
		void                setSteps(int line, int page);
		void                setShiftSteps(int line, int page);

		void                addPage()                   { addValue(mPageStep); }
		void                subtractPage()              { addValue(-mPageStep); }
		void                addLine()                   { addValue(mLineStep); }
		void                subtractLine()              { addValue(-mLineStep); }
		void                addValue(int change)        { spinbox->addValue(change); }

	public slots:
		virtual void        setValue(int val)           { spinbox->setValue(val); }
		virtual void        setPrefix(const QString& text)  { spinbox->setPrefix(text); }
		virtual void        setSuffix(const QString& text)  { spinbox->setSuffix(text); }
		virtual void        stepUp()                    { addValue(mLineStep); }
		virtual void        stepDown()                  { addValue(-mLineStep); }
		virtual void        pageUp()                    { addValue(mPageStep); }
		virtual void        pageDown()                  { addValue(-mPageStep); }
		virtual void        selectAll()                 { spinbox->selectAll(); }

	signals:
		void                valueChanged(int value);
		void                valueChanged(const QString& valueText);

	protected:
		virtual QString     mapValueToText(int v)         { return spinbox->mapValToText(v); }
		virtual int         mapTextToValue(bool* ok)      { return spinbox->mapTextToVal(ok); }
		virtual void        resizeEvent(QResizeEvent*)    { arrange(); }
		virtual void        showEvent(QShowEvent*);
		virtual void        styleChange(QStyle&)          { arrange(); }
		virtual void        getMetrics() const;

		mutable int      wUpdown2;        // width of second spin widget
		mutable int      xUpdown2;        // x offset of visible area in 'updown2'
		mutable int      xSpinbox;        // x offset of visible area in 'spinbox'
		mutable int      wGap;            // gap between updown2Frame and spinboxFrame

	protected slots:
		virtual void        valueChange();
		virtual void        stepPage(int);

	private:
		void                init();
		void                arrange();
		int                 whichButton(QObject* spinWidget, const QPoint&);
		void                setShiftStepping(bool on);

		// Visible spin box class
		class SB2_SpinBox : public SpinBox
		{
			public:
				SB2_SpinBox(SpinBox2* sb2, QWidget* parent, const char* name = 0)   : SpinBox(parent, name), owner(sb2) { }
				SB2_SpinBox(int minValue, int maxValue, int step, SpinBox2* sb2, QWidget* parent, const char* name = 0)
				                  : SpinBox(minValue, maxValue, step, parent, name), owner(sb2) { }
				virtual QString mapValueToText(int v)     { return owner->mapValueToText(v); }
				virtual int     mapTextToValue(bool* ok)  { return owner->mapTextToValue(ok); }
				QString         mapValToText(int v)       { return SpinBox::mapValueToText(v); }
				int             mapTextToVal(bool* ok)    { return SpinBox::mapTextToValue(ok); }
				virtual int     shiftStepAdjustment(int oldValue, int shiftStep);
			private:
				SpinBox2* owner;   // owner SpinBox2
		};

		enum { NO_BUTTON = -1, UP, DOWN, UP2, DOWN2 };

		static int       mReverseLayout;  // widgets are mirrored right to left
		QFrame*          updown2Frame;
		QFrame*          spinboxFrame;
		SpinBox*         updown2;         // the extra pair of spin buttons
		SB2_SpinBox*     spinbox;         // the visible spin box
		int              mMinValue;
		int              mMaxValue;
		int              mLineStep;           // right button increment
		int              mLineShiftStep;      // right button increment with shift pressed
		int              mPageStep;           // left button increment
		int              mPageShiftStep;      // left button increment with shift pressed
		bool             mReverseWithLayout;  // reverse button positions if reverse layout (default = true)

	friend class SB2_SpinBox;
};

#endif // QT_VERSION >= 300
#endif // SPINBOX2_H
