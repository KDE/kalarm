/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons (for QT3)
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
		int                 lineStep() const            { return spinbox->lineStep(); }
		int                 lineShiftStep() const       { return spinbox->lineShiftStep(); }
		void                setLineStep(int step)       { spinbox->setLineStep(step); }
		int                 value() const               { return spinbox->value(); }

		QRect               upRect() const              { return spinbox->upRect(); }
		QRect               downRect() const            { return spinbox->downRect(); }
		QRect               up2Rect() const             { return updown2->upRect(); }
		QRect               down2Rect() const           { return updown2->downRect(); }

		// QRangeControl methods
		void                addPage()                   { spinbox->addPage(); }
		void                subtractPage()              { spinbox->subtractPage(); }
		void                addLine()                   { spinbox->addLine(); }
		void                subtractLine()              { spinbox->subtractLine(); }
		void                addValue(int change)        { spinbox->addValue(change); }

		void                setRange(int minValue, int maxValue)   { setMinValue(minValue);  setMaxValue(maxValue); }

		int                 pageStep() const            { return updown2->lineStep(); }
		int                 pageShiftStep() const       { return updown2->lineShiftStep(); }
		void                setSteps(int line, int page)      { spinbox->setLineStep(line);  updown2->setLineStep(page); }
		void                setShiftSteps(int line, int page) { spinbox->setLineShiftStep(line);  updown2->setLineShiftStep(page); }

		int                 bound(int val) const;

	public slots:
		virtual void        setValue(int val)           { spinbox->setValue(val); }
		virtual void        setPrefix(const QString& text)  { spinbox->setPrefix(text); }
		virtual void        setSuffix(const QString& text)  { spinbox->setSuffix(text); }
		virtual void        stepUp()                    { addValue(spinbox->lineStep()); }
		virtual void        stepDown()                  { addValue(-spinbox->lineStep()); }
		virtual void        pageUp()                    { addValue(updown2->lineStep()); }
		virtual void        pageDown()                  { addValue(-updown2->lineStep()); }
		virtual void        selectAll()                 { spinbox->selectAll(); }

	signals:
		void                valueChanged(int value);
		void                valueChanged(const QString& valueText);

	protected:
		virtual QString     mapValueToText(int v)         { return spinbox->mapValueToText(v); }
		virtual int         mapTextToValue(bool* ok)      { return spinbox->mapTextToValue(ok); }
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

		class SB2_SpinBox : public SpinBox
		{
			public:
				SB2_SpinBox(SpinBox2* sb2, QWidget* parent, const char* name = 0)   : SpinBox(parent, name), spinBox2(sb2) { }
				SB2_SpinBox(int minValue, int maxValue, int step, SpinBox2* sb2, QWidget* parent, const char* name = 0)
				                  : SpinBox(minValue, maxValue, step, parent, name), spinBox2(sb2) { }
				virtual QString mapValueToText(int v)     { return spinBox2->mapValueToText(v); }
				virtual int     mapTextToValue(bool* ok)  { return spinBox2->mapTextToValue(ok); }
			private:
				SpinBox2*    spinBox2;
		};

		enum { NO_BUTTON = -1, UP, DOWN, UP2, DOWN2 };

		static int       mReverseLayout;  // widgets are mirrored right to left
		QFrame*          updown2Frame;
		QFrame*          spinboxFrame;
		SpinBox*         updown2;         // the extra pair of spin buttons
		SB2_SpinBox*     spinbox;         // the visible spin box
		int              mMinValue;
		int              mMaxValue;

	friend class SB2_SpinBox;
};

#endif // QT_VERSION >= 300
#endif // SPINBOX2_H
