/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons (for Qt 3)
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

#ifndef SPINBOX2_H
#define SPINBOX2_H

#include <qglobal.h>
#if QT_VERSION >= 300
class SpinMirror;
class ExtraSpinBox;

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
		bool                isReadOnly() const          { return mSpinbox->isReadOnly(); }
		void                setSelectOnStep(bool sel)   { mSpinbox->setSelectOnStep(sel); }
		void                setReverseWithLayout(bool);   // reverse buttons if right to left language?
		bool                reverseButtons() const      { return mReverseLayout  &&  !mReverseWithLayout; }

		QString             text() const                { return mSpinbox->text(); }
		virtual QString     prefix() const              { return mSpinbox->prefix(); }
		virtual QString     suffix() const              { return mSpinbox->suffix(); }
		virtual QString     cleanText() const           { return mSpinbox->cleanText(); }

		virtual void        setSpecialValueText(const QString& text)  { mSpinbox->setSpecialValueText(text); }
		QString             specialValueText() const    { return mSpinbox->specialValueText(); }

		virtual void        setWrapping(bool on);
		bool                wrapping() const            { return mSpinbox->wrapping(); }

		virtual void        setButtonSymbols(QSpinBox::ButtonSymbols);
		QSpinBox::ButtonSymbols buttonSymbols() const   { return mSpinbox->buttonSymbols(); }

		virtual void        setValidator(const QValidator* v)  { mSpinbox->setValidator(v); }
		const QValidator*   validator() const           { return mSpinbox->validator(); }

		virtual QSize       sizeHint() const;
		virtual QSize       minimumSizeHint() const;

		int                 minValue() const            { return mMinValue; }
		int                 maxValue() const            { return mMaxValue; }
		void                setMinValue(int val);
		void                setMaxValue(int val);
		void                setRange(int minValue, int maxValue)   { setMinValue(minValue);  setMaxValue(maxValue); }
		int                 value() const               { return mSpinbox->value(); }
		int                 bound(int val) const;

		QRect               upRect() const              { return mSpinbox->upRect(); }
		QRect               downRect() const            { return mSpinbox->downRect(); }
		QRect               up2Rect() const;
		QRect               down2Rect() const;

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
		void                addValue(int change)        { mSpinbox->addValue(change); }

	public slots:
		virtual void        setValue(int val)           { mSpinbox->setValue(val); }
		virtual void        setPrefix(const QString& text)  { mSpinbox->setPrefix(text); }
		virtual void        setSuffix(const QString& text)  { mSpinbox->setSuffix(text); }
		virtual void        stepUp()                    { addValue(mLineStep); }
		virtual void        stepDown()                  { addValue(-mLineStep); }
		virtual void        pageUp()                    { addValue(mPageStep); }
		virtual void        pageDown()                  { addValue(-mPageStep); }
		virtual void        selectAll()                 { mSpinbox->selectAll(); }

	signals:
		void                valueChanged(int value);
		void                valueChanged(const QString& valueText);

	protected:
		virtual QString     mapValueToText(int v)         { return mSpinbox->mapValToText(v); }
		virtual int         mapTextToValue(bool* ok)      { return mSpinbox->mapTextToVal(ok); }
		virtual void        resizeEvent(QResizeEvent*)    { arrange(); }
		virtual void        showEvent(QShowEvent*);
		virtual void        styleChange(QStyle&);
		virtual void        getMetrics() const;

		mutable int      wUpdown2;        // width of second spin widget
		mutable int      xUpdown2;        // x offset of visible area in 'mUpdown2'
		mutable int      xSpinbox;        // x offset of visible area in 'mSpinbox'
		mutable int      wGap;            // gap between mUpdown2Frame and mSpinboxFrame

	protected slots:
		virtual void        valueChange();
		virtual void        stepPage(int);

	private slots:
		void                updateMirror();

	private:
		void                init();
		void                arrange();
		int                 whichButton(QObject* spinWidget, const QPoint&);
		void                setShiftStepping(bool on);

		// Visible spin box class.
		// Declared here to allow use of mSpinBox in inline methods.
		class MainSpinBox : public SpinBox
		{
			public:
				MainSpinBox(SpinBox2* sb2, QWidget* parent, const char* name = 0)
				                : SpinBox(parent, name), owner(sb2) { }
				MainSpinBox(int minValue, int maxValue, int step, SpinBox2* sb2, QWidget* parent, const char* name = 0)
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
		QFrame*          mUpdown2Frame;   // contains visible part of the extra pair of spin buttons
		QFrame*          mSpinboxFrame;   // contains the main spin box
		ExtraSpinBox*    mUpdown2;        // the extra pair of spin buttons
		MainSpinBox*     mSpinbox;        // the visible spin box
		SpinMirror*      mSpinMirror;     // image of the extra pair of spin buttons
		int              mMinValue;
		int              mMaxValue;
		int              mLineStep;           // right button increment
		int              mLineShiftStep;      // right button increment with shift pressed
		int              mPageStep;           // left button increment
		int              mPageShiftStep;      // left button increment with shift pressed
		bool             mReverseWithLayout;  // reverse button positions if reverse layout (default = true)

	friend class MainSpinBox;
};

#endif // QT_VERSION >= 300
#endif // SPINBOX2_H
