/*
 *  spinbox2.h  -  spin box with extra pair of spin buttons
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "spinbox2_p.h"

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
class SpinBox2 : public QWidget
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
    virtual void     setReadOnly(bool readOnly)  { return mSpinbox2->setReadOnly(readOnly); }

    /** Returns true if the widget is read only. */
    bool             isReadOnly() const          { return mSpinbox2->isReadOnly(); }

    /** Sets whether the spin box value text should be selected when its value is stepped. */
    void             setSelectOnStep(bool sel)   { mSpinbox2->setSelectOnStep(sel); }

    /** Sets whether the spin button pairs should be reversed for a right-to-left language.
     *  The default is for them to be reversed.
     */
    void             setReverseWithLayout(bool reverse)  { mSpinbox2->setReverseWithLayout(reverse); }

    /** Returns whether the spin button pairs will be reversed for a right-to-left language. */
    bool             reverseButtons() const      { return mSpinbox2->reverseButtons(); }

    /** Returns the spin box's text, including any prefix() and suffix(). */
    QString          text() const                { return mSpinbox2->text(); }

    /** Returns the prefix for the spin box's text. */
    QString          prefix() const              { return mSpinbox2->prefix(); }

    /** Returns the suffix for the spin box's text. */
    QString          suffix() const              { return mSpinbox2->suffix(); }

    /** Sets the prefix which is prepended to the start of the displayed text. */
    void             setPrefix(const QString& text)  { mSpinbox2->setPrefix(text); }

    /** Sets the suffix which is prepended to the start of the displayed text. */
    void             setSuffix(const QString& text)  { mSpinbox2->setSuffix(text); }

    /** Returns the spin box's text with no prefix(), suffix() or leading or trailing whitespace. */
    QString          cleanText() const           { return mSpinbox2->cleanText(); }

    /** Sets the special-value text which, if non-null, is displayed instead of a numeric
     *  value when the current value is equal to minimum().
     */
    void             setSpecialValueText(const QString& text)  { mSpinbox2->setSpecialValueText(text); }

    /** Returns the special-value text which, if non-null, is displayed instead of a numeric
     *  value when the current value is equal to minimum().
     */
    QString          specialValueText() const    { return mSpinbox2->specialValueText(); }

    /** Sets whether it is possible to step the value from the highest value to the
     *  lowest value and vice versa.
     */
    void             setWrapping(bool on)        { mSpinbox2->setWrapping(on); }

    /** Returns whether it is possible to step the value from the highest value to the
     *  lowest value and vice versa.
     */
    bool             wrapping() const            { return mSpinbox2->wrapping(); }

    /** Set the text alignment of the widget */
    void             setAlignment(Qt::Alignment a) { mSpinbox2->setAlignment(a); }

    /** Sets the button symbols to use (arrows or plus/minus). */
    void             setButtonSymbols(QSpinBox::ButtonSymbols s)  { mSpinbox2->setButtonSymbols(s); }

    /** Returns the button symbols currently in use (arrows or plus/minus). */
    QSpinBox::ButtonSymbols buttonSymbols() const   { return mSpinbox2->buttonSymbols(); }

    /** Determine whether the current input is valid. */
    virtual QValidator::State validate(QString& s, int& pos) const  { return mSpinbox2->validate(s, pos); }

    QSize            sizeHint() const override         { return mSpinbox2->sizeHint(); }
    QSize            minimumSizeHint() const override  { return mSpinbox2->minimumSizeHint(); }

    /** Returns the minimum value of the spin box. */
    int              minimum() const             { return mSpinbox2->minimum(); }

    /** Returns the maximum value of the spin box. */
    int              maximum() const             { return mSpinbox2->maximum(); }

    /** Sets the minimum value of the spin box. */
    virtual void     setMinimum(int val)         { mSpinbox2->setMinimum(val); }

    /** Sets the maximum value of the spin box. */
    virtual void     setMaximum(int val)         { mSpinbox2->setMaximum(val); }

    /** Sets the minimum and maximum values of the spin box. */
    void             setRange(int minValue, int maxValue)   { mSpinbox2->setRange(minValue, maxValue); }

    /** Returns the current value of the spin box. */
    int              value() const               { return mSpinbox2->value(); }

    /** Returns the specified value clamped to the range of the spin box. */
    int              bound(int val) const        { return mSpinbox2->bound(val); }

    /** Returns the geometry of the right-hand "up" button. */
    QRect            upRect() const              { return mSpinbox2->upRect(); }

    /** Returns the geometry of the right-hand "down" button. */
    QRect            downRect() const            { return mSpinbox2->downRect(); }

    /** Returns the geometry of the left-hand "up" button.
     *  @return Button geometry, or invalid if left-hand buttons are not visible.
     */
    QRect            up2Rect() const             { return mSpinbox2->up2Rect(); }

    /** Returns the geometry of the left-hand "down" button.
     *  @return Button geometry, or invalid if left-hand buttons are not visible.
     */
    QRect            down2Rect() const           { return mSpinbox2->down2Rect(); }

    /** Returns the unshifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked without the shift key being pressed.
     */
    int              singleStep() const          { return mSpinbox2->singleStep(); }

    /** Returns the shifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked while the shift key is pressed.
     */
    int              singleShiftStep() const     { return mSpinbox2->singleShiftStep(); }

    /** Returns the unshifted step increment for the left-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a left-hand
     *  spin button is clicked without the shift key being pressed.
     */
    int              pageStep() const            { return mSpinbox2->pageStep(); }

    /** Returns the shifted step increment for the left-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a left-hand
     *  spin button is clicked while the shift key is pressed.
     */
    int              pageShiftStep() const       { return mSpinbox2->pageShiftStep(); }

    /** Sets the unshifted step increment for the right-hand spin buttons,
     *  i.e. the amount by which the spin box value changes when a right-hand
     *  spin button is clicked without the shift key being pressed.
     */
    void             setSingleStep(int step)     { mSpinbox2->setSingleStep(step); }

    /** Sets the unshifted step increments for the two pairs of spin buttons,
     *  i.e. the amount by which the spin box value changes when a spin button
     *  is clicked without the shift key being pressed.
     *  @param line The step increment for the right-hand spin buttons.
     *  @param page The step increment for the left-hand spin buttons.
     */
    void             setSteps(int line, int page)  { mSpinbox2->setSteps(line, page); }

    /** Sets the shifted step increments for the two pairs of spin buttons,
     *  i.e. the amount by which the spin box value changes when a spin button
     *  is clicked while the shift key is pressed. It also sets the increment
     *  when the left spin button is clicked when the control key is pressed.
     *  @param line     The shift step increment for the right-hand spin buttons.
     *  @param page     The shift step increment for the left-hand spin buttons.
     *  @param control  The increment for the left-hand spin buttons when Control,
     *                  is pressed, or 0 to use default Qt handling which
     *                  multiplies the single step by 10.
     *  @param modControl  Control steps should always set value to multiple of @p control.
     */
    void             setShiftSteps(int line, int page, int control, bool modControl = true)  { mSpinbox2->setShiftSteps(line, page, control, modControl); }

    /** Increments the current value by adding the unshifted step increment for
     *  the left-hand spin buttons.
     */
    void             addPage()                   { mSpinbox2->addPage(); }

    /** Decrements the current value by subtracting the unshifted step increment for
     *  the left-hand spin buttons.
     */
    void             subtractPage()              { mSpinbox2->subtractPage(); }

    /** Increments the current value by adding the unshifted step increment for
     *  the right-hand spin buttons.
     */
    void             addSingle()                 { mSpinbox2->addSingle(); }

    /** Decrements the current value by subtracting the unshifted step increment for
     *  the right-hand spin buttons.
     */
    void             subtractSingle()            { mSpinbox2->subtractSingle(); }

    /** Adjusts the current value by adding @p change. */
    void             addValue(int change)        { mSpinbox2->addValue(change); }

    /** Increments the current value by adding the unshifted increment for
     *  the right-hand spin buttons.
     */
    virtual void     stepBy(int increment)       { mSpinbox2->stepBy(increment); }

public Q_SLOTS:
    /** Sets the current value to @p val. */
    void             setValue(int val)           { mSpinbox2->setValue(val); }
    /** Increments the current value by adding the unshifted step increment for
     *  the left-hand spin buttons.
     */
    virtual void     pageUp()                    { mSpinbox2->pageUp(); }
    /** Decrements the current value by subtracting the unshifted step increment for
     *  the left-hand spin buttons.
     */
    virtual void     pageDown()                  { mSpinbox2->pageDown(); }
    /** Selects all the text in the spin box's editor. */
    void             selectAll()                 { mSpinbox2->selectAll(); }
    /** Sets whether the widget is enabled. */
    virtual void     setEnabled(bool enabled)    { mSpinbox2->setEnabled(enabled); }

Q_SIGNALS:
    /** Signal which is emitted whenever the value of the spin box changes. */
    void             valueChanged(int value);

protected:
    virtual QString  textFromValue(int v) const    { return mSpinbox2->textFromValue(v); }
    virtual int      valueFromText(const QString& t) const  { return mSpinbox2->valueFromText(t); }

    void resizeEvent(QResizeEvent*) override;

private:
    void init();

    SpinBox2p* mSpinbox2;

friend class SpinBox2p;
};

// vim: et sw=4:
