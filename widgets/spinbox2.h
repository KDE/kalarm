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


/**
 *  The SpinBox2 class provides a spin box with two pairs of spin buttons, one on either side.
 *
 *  It is designed as a base class for implementing such facilities as time spin boxes, where
 *  the hours and minutes values are separately displayed in the edit field. When the
 *  appropriate step increments are configured, the left spin arrows can then be used to
 *  change the hours value, while the right spin arrows can be used to change the minutes
 *  value.
 *
 *  Rather than using SpinBox2 directly for time entry, use in preference TimeSpinBox or
 *  TimeEdit classes which are tailored from SpinBox2 for this purpose.
 *
 *  Separate step increments may optionally be specified for use when the shift key is
 *  held down. Typically these would be larger than the normal steps. Then, when the user
 *  clicks the spin buttons, he/she can increment or decrement the value faster by holding
 *  the shift key down.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short Spin box with a pair of spin buttons on either side.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class SpinBox2 : public QFrame
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		SpinBox2(QWidget* parent = 0, const char* name = 0);
		/** Constructor.
		 *  @param minValue The minimum value which the spin box can have.
		 *  @param maxValue The maximum value which the spin box can have.
		 *  @param step The (unshifted) step interval for the right-hand spin buttons.
		 *  @param step2 The (unshifted) step interval for the left-hand spin buttons.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		SpinBox2(int minValue, int maxValue, int step = 1, int step2 = 1,
		         QWidget* parent = 0, const char* name = 0);
		/** Sets whether the spin box can be changed by the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void                setReadOnly(bool readOnly);
		/** Returns true if the widget is read only. */
		bool                isReadOnly() const          { return mSpinbox->isReadOnly(); }
		/** Sets whether the spin box value text should be selected when its value is stepped. */
		void                setSelectOnStep(bool sel)   { mSpinbox->setSelectOnStep(sel); }
		/** Sets whether the spin button pairs should be reversed for a right-to-left language.
		 *  The default is for them to be reversed.
		 */
		void                setReverseWithLayout(bool reverse);
		/** Returns whether the spin button pairs will be reversed for a right-to-left language. */
		bool                reverseButtons() const      { return mReverseLayout  &&  !mReverseWithLayout; }

		/** Returns the spin box's text, including any prefix() and suffix(). */
		QString             text() const                { return mSpinbox->text(); }
		/** Returns the prefix for the spin box's text. */
		virtual QString     prefix() const              { return mSpinbox->prefix(); }
		/** Returns the suffix for the spin box's text. */
		virtual QString     suffix() const              { return mSpinbox->suffix(); }
		/** Returns the spin box's text with no prefix(), suffix() or leading or trailing whitespace. */
		virtual QString     cleanText() const           { return mSpinbox->cleanText(); }

		/** Sets the special-value text which, if non-null, is displayed instead of a numeric
		 *  value when the current value is equal to minValue().
		 */
		virtual void        setSpecialValueText(const QString& text)  { mSpinbox->setSpecialValueText(text); }
		/** Returns the special-value text which, if non-null, is displayed instead of a numeric
		 *  value when the current value is equal to minValue().
		 */
		QString             specialValueText() const    { return mSpinbox->specialValueText(); }

		/** Sets whether it is possible to step the value from the highest value to the
		 *  lowest value and vice versa.
		 */
		virtual void        setWrapping(bool on);
		/** Returns whether it is possible to step the value from the highest value to the
		 *  lowest value and vice versa.
		 */
		bool                wrapping() const            { return mSpinbox->wrapping(); }

		/** Sets the button symbols to use (arrows or plus/minus). */
		virtual void        setButtonSymbols(QSpinBox::ButtonSymbols);
		/** Returns the button symbols currently in use (arrows or plus/minus). */
		QSpinBox::ButtonSymbols buttonSymbols() const   { return mSpinbox->buttonSymbols(); }

		/** Sets the validator to @p v. The validator controls what keyboard input is accepted
		 *  when the user is editing the value field.
		 */
		virtual void        setValidator(const QValidator* v)  { mSpinbox->setValidator(v); }
		/** Returns the current validator. The validator controls what keyboard input is accepted
		 *  when the user is editing the value field.
		 */
		const QValidator*   validator() const           { return mSpinbox->validator(); }

		virtual QSize       sizeHint() const;
		virtual QSize       minimumSizeHint() const;

		/** Returns the minimum value of the spin box. */
		int                 minValue() const            { return mMinValue; }
		/** Returns the maximum value of the spin box. */
		int                 maxValue() const            { return mMaxValue; }
		/** Sets the minimum value of the spin box. */
		void                setMinValue(int val);
		/** Sets the maximum value of the spin box. */
		void                setMaxValue(int val);
		/** Sets the minimum and maximum values of the spin box. */
		void                setRange(int minValue, int maxValue)   { setMinValue(minValue);  setMaxValue(maxValue); }
		/** Returns the current value of the spin box. */
		int                 value() const               { return mSpinbox->value(); }
		/** Returns the specified value clamped to the range of the spin box. */
		int                 bound(int val) const;

		/** Returns the geometry of the right-hand "up" button. */
		QRect               upRect() const              { return mSpinbox->upRect(); }
		/** Returns the geometry of the right-hand "down" button. */
		QRect               downRect() const            { return mSpinbox->downRect(); }
		/** Returns the geometry of the left-hand "up" button. */
		QRect               up2Rect() const;
		/** Returns the geometry of the left-hand "down" button. */
		QRect               down2Rect() const;

		/** Returns the unshifted step increment for the right-hand spin buttons,
		 *  i.e. the amount by which the spin box value changes when a right-hand
		 *  spin button is clicked without the shift key being pressed.
		 */
		int                 lineStep() const            { return mLineStep; }
		/** Returns the shifted step increment for the right-hand spin buttons,
		 *  i.e. the amount by which the spin box value changes when a right-hand
		 *  spin button is clicked while the shift key is pressed.
		 */
		int                 lineShiftStep() const       { return mLineShiftStep; }
		/** Returns the unshifted step increment for the left-hand spin buttons,
		 *  i.e. the amount by which the spin box value changes when a left-hand
		 *  spin button is clicked without the shift key being pressed.
		 */
		int                 pageStep() const            { return mPageStep; }
		/** Returns the shifted step increment for the left-hand spin buttons,
		 *  i.e. the amount by which the spin box value changes when a left-hand
		 *  spin button is clicked while the shift key is pressed.
		 */
		int                 pageShiftStep() const       { return mPageShiftStep; }
		/** Sets the unshifted step increment for the right-hand spin buttons,
		 *  i.e. the amount by which the spin box value changes when a right-hand
		 *  spin button is clicked without the shift key being pressed.
		 */
		void                setLineStep(int step);
		/** Sets the unshifted step increments for the two pairs of spin buttons,
		 *  i.e. the amount by which the spin box value changes when a spin button
		 *  is clicked without the shift key being pressed.
		 *  @param line The step increment for the right-hand spin buttons.
		 *  @param page The step increment for the left-hand spin buttons.
		 */
		void                setSteps(int line, int page);
		/** Sets the shifted step increments for the two pairs of spin buttons,
		 *  i.e. the amount by which the spin box value changes when a spin button
		 *  is clicked while the shift key is pressed.
		 *  @param line The shift step increment for the right-hand spin buttons.
		 *  @param page The shift step increment for the left-hand spin buttons.
		 */
		void                setShiftSteps(int line, int page);

		/** Increments the current value by adding the unshifted step increment for
		 *  the left-hand spin buttons.
		 */
		void                addPage()                   { addValue(mPageStep); }
		/** Decrements the current value by subtracting the unshifted step increment for
		 *  the left-hand spin buttons.
		 */
		void                subtractPage()              { addValue(-mPageStep); }
		/** Increments the current value by adding the unshifted step increment for
		 *  the right-hand spin buttons.
		 */
		void                addLine()                   { addValue(mLineStep); }
		/** Decrements the current value by subtracting the unshifted step increment for
		 *  the right-hand spin buttons.
		 */
		void                subtractLine()              { addValue(-mLineStep); }
		/** Adjusts the current value by adding @p change. */
		void                addValue(int change)        { mSpinbox->addValue(change); }

	public slots:
		/** Sets the current value to @p val. */
		virtual void        setValue(int val)           { mSpinbox->setValue(val); }
		/** Sets the prefix which is prepended to the start of the displayed text. */
		virtual void        setPrefix(const QString& text)  { mSpinbox->setPrefix(text); }
		/** Sets the suffix which is prepended to the start of the displayed text. */
		virtual void        setSuffix(const QString& text)  { mSpinbox->setSuffix(text); }
		/** Increments the current value by adding the unshifted step increment for
		 *  the right-hand spin buttons.
		 */
		virtual void        stepUp()                    { addValue(mLineStep); }
		/** Decrements the current value by subtracting the unshifted step increment for
		 *  the right-hand spin buttons.
		 */
		virtual void        stepDown()                  { addValue(-mLineStep); }
		/** Increments the current value by adding the unshifted step increment for
		 *  the left-hand spin buttons.
		 */
		virtual void        pageUp()                    { addValue(mPageStep); }
		/** Decrements the current value by subtracting the unshifted step increment for
		 *  the left-hand spin buttons.
		 */
		virtual void        pageDown()                  { addValue(-mPageStep); }
		/** Selects all the text in the spin box's editor. */
		virtual void        selectAll()                 { mSpinbox->selectAll(); }

	signals:
		/** Signal which is emitted whenever the value of the spin box changes. */
		void                valueChanged(int value);
		/** Signal which is emitted whenever the value of the spin box changes. */
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
