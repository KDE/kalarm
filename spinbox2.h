/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons (for QT3)
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SPINBOX2_H
#define SPINBOX2_H

#include <qglobal.h>
#if QT_VERSION >= 300

#include "qspinbox.h"

/*
 * This class has a second pair of spin buttons at its left hand side.
 * These buttons use the inherited QRangeControl page step functions.
 */
class SpinBox2 : public QWidget
{
		Q_OBJECT
	public:
		SpinBox2(QWidget* parent = 0L, const char* name = 0L);
		SpinBox2(int minValue, int maxValue, int step = 1, int step2 = 1,
		         QWidget* parent = 0L, const char* name = 0L);

		QString             text() const                { return spinbox->text(); }
		virtual QString     prefix() const              { return spinbox->prefix(); }
		virtual QString     suffix() const              { return spinbox->suffix(); }
		virtual QString     cleanText() const           { return spinbox->cleanText(); }

		virtual void        setSpecialValueText(const QString &text)  { spinbox->setSpecialValueText(text); }
		QString             specialValueText() const    { return spinbox->specialValueText(); }

		virtual void        setWrapping(bool on)        { spinbox->setWrapping(on); }
		bool                wrapping() const            { return spinbox->wrapping(); }

		virtual void        setButtonSymbols(QSpinBox::ButtonSymbols);
		QSpinBox::ButtonSymbols buttonSymbols() const   { return spinbox->buttonSymbols(); }

		virtual void        setValidator(const QValidator* v)  { spinbox->setValidator(v); }
		const QValidator*   validator() const           { return spinbox->validator(); }

		QSize               sizeHint() const;
		QSize               minimumSizeHint() const;

		int                 minValue() const            { return spinbox->minValue(); }
		int                 maxValue() const            { return spinbox->maxValue(); }
		void                setMinValue(int val)        { spinbox->setMinValue(val); }
		void                setMaxValue(int val)        { spinbox->setMaxValue(val); }
		int                 lineStep() const            { return spinbox->lineStep(); }
		void                setLineStep(int step)       { spinbox->setLineStep(step); }
		int                 value() const               { return spinbox->value(); }

		QRect               upRect() const              { return spinbox->upRect(); }
		QRect               downRect() const            { return spinbox->downRect(); }
		QRect               upRect2() const             { return updown2->upRect(); }
		QRect               downRect2() const           { return updown2->downRect(); }

		// QRangeControl methods
		void                addPage()                   { spinbox->addPage(); }
		void                subtractPage()              { spinbox->subtractPage(); }
		void                addLine()                   { spinbox->addLine(); }
		void                subtractLine()              { spinbox->subtractLine(); }

		void                setRange(int minValue, int maxValue)   { spinbox->setRange(minValue, maxValue); }

		int                 pageStep() const            { return spinbox->pageStep(); }
		void                setSteps(int line, int page){ spinbox->setSteps(line, page); }

		int                 bound(int b) const          { return spinbox->bound(b); }


	public slots:
		virtual void        setValue(int val)           { spinbox->setValue(val); }
		virtual void        setPrefix(const QString& text)  { spinbox->setPrefix(text); }
		virtual void        setSuffix(const QString& text)  { spinbox->setSuffix(text); }
		virtual void        stepUp()                    { addVal(spinbox->lineStep()); }
		virtual void        stepDown()                  { addVal(-spinbox->lineStep()); }
		virtual void        pageUp()                    { addVal(spinbox->pageStep()); }
		virtual void        pageDown()                  { addVal(-spinbox->pageStep()); }
		virtual void        selectAll()                 { spinbox->selectAll(); }

	signals:
		void                valueChanged(int value);
		void                valueChanged(const QString& valueText);

	protected:
		virtual QString     mapValueToText(int v)       { return spinbox->mapValueToText(v); }
		virtual int         mapTextToValue(bool* ok)    { return spinbox->mapTextToValue(ok); }
		void                resizeEvent(QResizeEvent*);
		virtual void        updateDisplay();
		virtual void        enabledChange(bool);
		virtual void        styleChange(QStyle&);

	protected slots:
		virtual void        valueChange();

	private:
		void                initSpinBox2();
		void                addVal(int change);

		class SpinBox2_ : public QSpinBox
		{
			public:
				SpinBox2_(SpinBox2* parent, const char* name = 0L)   : QSpinBox(parent, name), spinBox2(parent) { }
				SpinBox2_(int minValue, int maxValue, int step, SpinBox2* parent, const char* name = 0L)
				                  : QSpinBox(minValue, maxValue, step, parent, name), spinBox2(parent) { }
//			protected:
				virtual QString mapValueToText(int v)     { return spinBox2->mapValueToText(v); }
				virtual int     mapTextToValue(bool* ok)  { return spinBox2->mapTextToValue(ok); }
				QString         currentValueText()        { return QSpinBox::currentValueText(); }
			private:
				SpinBox2*    spinBox2;
//			friend class SpinBox2;
		};

		SpinBox2_*     spinbox;
		QSpinWidget*   updown2;

	friend class SpinBox2_;
};

#endif // QT_VERSION >= 300
#endif // SPINBOX2_H
