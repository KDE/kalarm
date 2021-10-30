/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "spinbox.h"

#include <QFrame>
class QShowEvent;
class QResizeEvent;
class SpinMirror;
class ExtraSpinBox;


/**
 *  @short Spin box with a pair of spin buttons on either side.
 *
 *  The SpinBox2 class provides a spin box with two pairs of spin buttons, one on either side,
 *  provided that the current style places the two spin buttons on the same side of a QSpinBox.
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
 *  @author David Jarvie <djarvie@kde.org>
 */
class SpinBox2 : public QFrame
{
    Q_OBJECT
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit SpinBox2(QWidget* parent = nullptr);

    /** Constructor.
     *  @param minValue The minimum value which the spin box can have.
     *  @param maxValue The maximum value which the spin box can have.
     *  @param pageStep The (unshifted) step interval for the left-hand spin buttons.
     *  @param parent The parent object of this widget.
     */
    SpinBox2(int minValue, int maxValue, int pageStep = 1, QWidget* parent = nullptr);

    /** Sets whether the spin box can be changed by the user.
     *  @param readOnly True to set the widget read-only, false to set it read-write.
     */
    virtual void     setReadOnly(bool readOnly);

    /** Returns true if the widget is read only. */
    bool             isReadOnly() const          { return mSpinbox->isReadOnly(); }

    /** Sets whether the spin box value text should be selected when its value is stepped. */
    void             setSelectOnStep(bool sel)   { mSpinbox->setSelectOnStep(sel); }

    /** Sets whether the spin button pairs should be reversed for a right-to-left language.
     *  The default is for them to be reversed.
     */
    void             setReverseWithLayout(bool reverse);

    /** Returns whether the spin button pairs will be reversed for a right-to-left language. */
    bool             reverseButtons() const      { return mRightToLeft  &&  !mReverseWithLayout; }

    /** Returns the spin box's text, including any prefix() and suffix(). */
    QString          text() const                { return mSpinbox->text(); }

    /** Returns the prefix for the spin box's text. */
    QString          prefix() const              { return mSpinbox->prefix(); }

    /** Returns the suffix for the spin box's text. */
    QString          suffix() const              { return mSpinbox->suffix(); }

    /** Sets the prefix which is prepended to the start of the displayed text. */
    void             setPrefix(const QString& text)  { mSpinbox->setPrefix(text); }

    /** Sets the suffix which is prepended to the start of the displayed text. */
    void             setSuffix(const QString& text)  { mSpinbox->setSuffix(text); }

    /** Returns the spin box's text with no prefix(), suffix() or leading or trailing whitespace. */
    QString          cleanText() const           { return mSpinbox->cleanText(); }

    /** Sets the special-value text which, if non-null, is displayed instead of a numeric
     *  value when the current value is equal to minimum().
     */
    void             setSpecialValueText(const QString& text)  { mSpinbox->setSpecialValueText(text); }

    /** Returns the special-value text which, if non-null, is displayed instead of a numeric
     *  value when the current value is equal to minimum().
     */
    QString          specialValueText() const    { return mSpinbox->specialValueText(); }

    /** Sets whether it is possible to step the value from the highest value to the
     *  lowest value and vice versa.
     */
    void             setWrapping(bool on);

    /** Returns whether it is possible to step the value from the highest value to the
     *  lowest value and vice versa.
     */
    bool             wrapping() const            { return mSpinbox->wrapping(); }

    /** Set the text alignment of the widget */
    void             setAlignment(Qt::Alignment a) { mSpinbox->setAlignment(a); }

    /** Sets the button symbols to use (arrows or plus/minus). */
    void             setButtonSymbols(QSpinBox::ButtonSymbols);

    /** Returns the button symbols currently in use (arrows or plus/minus). */
    QSpinBox::ButtonSymbols buttonSymbols() const   { return mSpinbox->buttonSymbols(); }

    /** Determine whether the current input is valid. */
    virtual QValidator::State validate(QString&, int& /*pos*/) const  { return QValidator::Acceptable; }

    QSize            sizeHint() const override;
    QSize            minimumSizeHint() const override;

    /** Returns the minimum value of the spin box. */
    int              minimum() const             { return mMinValue; }

    /** Returns the maximum value of the spin box. */
    int              maximum() const             { return mMaxValue; }

    /** Sets the minimum value of the spin box. */
    virtual void     setMinimum(int val);

    /** Sets the maximum value of the spin box. */
    virtual void     setMaximum(int val);

    /** Sets the minimum and maximum values of the spin box. */
    void             setRange(int minValue, int maxValue)   { setMinimum(minValue);  setMaximum(maxValue); }

    /** Returns the current value of the spin box. */
    int              value() const               { return mSpinbox->value(); }

    /** Returns the specified value clamped to the range of the spin box. */
    int              bound(int val) const;

    /** Returns the geometry of the right-hand "up" button. */
    QRect            upRect() const              { return mSpinbox->upRect(); }

    /** Returns the geometry of the right-hand "down" button. */
    QRect            downRect() const            { return mSpinbox->downRect(); }

    /** Returns the geometry of the left-hand "up" button.
     *  @return Button geometry, or invalid if left-hand buttons are not visible.
     */
    QRect            up2Rect() const;

    /** Returns the geometry of the left-hand "down" button.
     *  @return Button geometry, or invalid if left-hand buttons are not visible.
     */
    QRect            down2Rect() const;

    /** Returns the unshifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked without the shift key being pressed.
     */
    int              singleStep() const          { return mSingleStep; }

    /** Returns the shifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked while the shift key is pressed.
     */
    int              singleShiftStep() const     { return mSingleShiftStep; }

    /** Returns the unshifted step increment for the left-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a left-hand
     *  spin button is clicked without the shift key being pressed.
     */
    int              pageStep() const            { return mPageStep; }

    /** Returns the shifted step increment for the left-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a left-hand
     *  spin button is clicked while the shift key is pressed.
     */
    int              pageShiftStep() const       { return mPageShiftStep; }

    /** Sets the unshifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked without the shift key being pressed.
     */
    void             setSingleStep(int step);

    /** Sets the unshifted step increments for the two pairs of spin buttons,
     *  i.e. the amount by which the spin box value changes when a spin button
     *  is clicked without the shift key being pressed.
     *  @param line The step increment for the right-hand spin buttons.
     *  @param page The step increment for the left-hand spin buttons.
     */
    void             setSteps(int line, int page);

    /** Sets the shifted step increments for the two pairs of spin buttons,
     *  i.e. the amount by which the spin box value changes when a spin button
     *  is clicked while the shift key is pressed.
     *  @param line     The shift step increment for the right-hand spin buttons.
     *  @param page     The shift step increment for the left-hand spin buttons.
     *  @param control  The control step increment for the left-hand spin buttons.
     *                  N.B. Qt multiplies the step increment by 10 when the Control
     *                       key is pressed, so this parameter value should be
     *  @param modControl  Control steps should always set value to multiple of @p step.
     */
    void             setShiftSteps(int line, int page, int control, bool modControl = true);

    /** Increments the current value by adding the unshifted step increment for
     *  the left-hand spin buttons.
     */
    void             addPage()                   { addValue(mPageStep); }

    /** Decrements the current value by subtracting the unshifted step increment for
     *  the left-hand spin buttons.
     */
    void             subtractPage()              { addValue(-mPageStep); }

    /** Increments the current value by adding the unshifted step increment for
     *  the right-hand spin buttons.
     */
    void             addSingle()                 { addValue(mSingleStep); }

    /** Decrements the current value by subtracting the unshifted step increment for
     *  the right-hand spin buttons.
     */
    void             subtractSingle()            { addValue(-mSingleStep); }

    /** Adjusts the current value by adding @p change. */
    void             addValue(int change)        { mSpinbox->addValue(change); }

    /** Increments the current value by adding the unshifted increment for
     *  the right-hand spin buttons.
     */
    virtual void     stepBy(int increment)       { addValue(increment); }

public Q_SLOTS:
    /** Sets the current value to @p val. */
    void             setValue(int val)           { mSpinbox->setValue(val); }
    /** Increments the current value by adding the unshifted step increment for
     *  the left-hand spin buttons.
     */
    virtual void     pageUp()                    { addValue(mPageStep); }
    /** Decrements the current value by subtracting the unshifted step increment for
     *  the left-hand spin buttons.
     */
    virtual void     pageDown()                  { addValue(-mPageStep); }
    /** Selects all the text in the spin box's editor. */
    void             selectAll()                 { mSpinbox->selectAll(); }
    /** Sets whether the widget is enabled. */
    virtual void     setEnabled(bool enabled);

Q_SIGNALS:
    /** Signal which is emitted whenever the value of the spin box changes. */
    void             valueChanged(int value);

protected:
    virtual QString  textFromValue(int v) const    { return mSpinbox->textFromVal(v); }
    virtual int      valueFromText(const QString& t) const  { return mSpinbox->valFromText(t); }
    void             paintEvent(QPaintEvent*) override;
    void             showEvent(QShowEvent*) override;
    virtual void     getMetrics() const;

    mutable int      wUpdown2 {0};    // width of second spin widget, including outer frame decoration
    mutable int      wBorderWidth;    // thickness of border between spin buttons and edge of widget
    mutable int      wFrameWidth;     // thickness of frame round widget
    mutable QPoint   mButtonPos;      // position of buttons inside mirror widget
    mutable bool     mShowUpdown2 {true};  // the extra pair of spin buttons are displayed

protected Q_SLOTS:
    virtual void     valueChange();
    virtual void     stepPage(int, bool);

private Q_SLOTS:
    void             updateMirrorButtons();
    void             updateMirrorFrame();
    void             paintTimer();

private:
    void             init();
    void             rearrange();
    void             arrange();
    void             updateMirror();
    bool             eventFilter(QObject*, QEvent*) override;
    void             spinboxResized(QResizeEvent*);
    void             setUpdown2Size();

    // Visible spin box class.
    // Declared here to allow use of mSpinBox in inline methods.
    class MainSpinBox : public SpinBox
    {
        public:
            MainSpinBox(SpinBox2* parent)
                            : SpinBox(parent), owner(parent) { }
            MainSpinBox(int minValue, int maxValue, SpinBox2* parent)
                            : SpinBox(minValue, maxValue, parent), owner(parent) { }
            QString textFromValue(int v) const override  { return owner->textFromValue(v); }
            int     valueFromText(const QString& t) const override
                                                { return owner->valueFromText(t); }
            QString textFromVal(int v) const    { return SpinBox::textFromValue(v); }
            int     valFromText(const QString& t) const
                                                { return SpinBox::valueFromText(t); }
            int     shiftStepAdjustment(int oldValue, int shiftStep) override;
            QValidator::State validate(QString& text, int& pos) const override
                                                { return owner->validate(text, pos); }
        protected:
            void    resizeEvent(QResizeEvent* e) override { owner->spinboxResized(e); SpinBox::resizeEvent(e); }
        private:
            SpinBox2* owner;   // owner SpinBox2
    };

    enum { NO_BUTTON = -1, UP, DOWN, UP2, DOWN2 };

    static int       mRightToLeft;    // widgets are mirrored right to left
    ExtraSpinBox*    mSpinbox2;        // the extra pair of spin buttons
    MainSpinBox*     mSpinbox;        // the visible spin box
    SpinMirror*      mSpinMirror;     // image of the extra pair of spin buttons
    int              mMinValue;
    int              mMaxValue;
    int              mSingleStep;         // right button increment
    int              mSingleShiftStep;    // right button increment with shift pressed
    int              mSingleControlStep;  // right button increment with control pressed
    int              mPageStep;           // left button increment
    int              mPageShiftStep;      // left button increment with shift pressed
    bool             mModControlStep {true};     // control steps set value to multiple of step
    bool             mReverseWithLayout {true};  // reverse button positions if reverse layout (default = true)

friend class MainSpinBox;
};

// vim: et sw=4:
