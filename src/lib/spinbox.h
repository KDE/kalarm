/*
 *  spinbox.h  -  spin box with shift-click step value and read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSpinBox>
class QEvent;
class QStyle;
class QStyleOptionSpinBox;


/**
 *  @short Spin box with accelerated shift or control key stepping, and read-only option.
 *
 *  The SpinBox class provides a QSpinBox with accelerated stepping using the shift or
 *  control key.
 *
 *  A separate step increment may optionally be specified for use when the shift or
 *  control key is held down. Typically this would be larger than the normal step. Then,
 *  when the user clicks the spin buttons, he/she can increment or decrement the value
 *  faster by holding the shift key down.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class SpinBox : public QSpinBox
{
    Q_OBJECT
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit SpinBox(QWidget* parent = nullptr);

    /** Constructor.
     *  @param minValue The minimum value which the spin box can have.
     *  @param maxValue The maximum value which the spin box can have.
     *  @param parent The parent object of this widget.
     */
    SpinBox(int minValue, int maxValue, QWidget* parent = nullptr);

    ~SpinBox() override;

    /** Returns true if the widget is read only. */
    bool         isReadOnly() const                    { return mReadOnly; }

    /** Sets whether the spin box can be changed by the user.
     *  @param readOnly True to set the widget read-only, false to set it read-write.
     */
    virtual void setReadOnly(bool readOnly);

    /** Returns whether the spin box value text is selected when its value is stepped. */
    bool         selectOnStep() const                  { return mSelectOnStep; }

    /** Sets whether the spin box value text should be selected when its value is stepped. */
    void         setSelectOnStep(bool sel)             { mSelectOnStep = sel; }

    /** Adds a value to the current value of the spin box. */
    void         addValue(int change)                  { addValue(change, false); }

    /** Returns the minimum value of the spin box. */
    int          minimum() const                       { return mMinValue; }

    /** Returns the maximum value of the spin box. */
    int          maximum() const                       { return mMaxValue; }

    /** Sets the minimum value of the spin box. */
    void         setMinimum(int val);

    /** Sets the maximum value of the spin box. */
    void         setMaximum(int val);

    /** Sets the minimum and maximum values of the spin box. */
    void         setRange(int minValue, int maxValue)  { setMinimum(minValue);  setMaximum(maxValue); }

    /** Returns the specified value clamped to the range of the spin box. */
    int          bound(int val) const;

    /** Called whenever the user triggers a step, to adjust the value of
     *  the spin box by the unshifted increment.
     */
    void         stepBy(int steps) override;

    /** Returns the unshifted step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked without the shift or control key being pressed.
     */
    int          singleStep() const                    { return mLineStep; }

    /** Sets the unshifted step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked without the shift or control key being pressed.
     */
    void         setSingleStep(int step);

    /** Returns the shifted step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked while the shift key is pressed.
     */
    int          singleShiftStep() const               { return mLineShiftStep; }

    /** Sets the shifted step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked while the shift key is pressed.
     */
    void         setSingleShiftStep(int step);

    /** Returns the control step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked while the control key is pressed.
     *  @return  control key step increment, or 0 if none has been set.
     */
    int          singleControlStep() const             { return mLineControlStep; }

    /** Returns whether control steps should always set the value to a multiple of
     *  the control step increment.
     */
    bool         modControlStep() const                { return mModControlStep; }

    /** Sets the control step increment, i.e. the amount by which the spin box value
     *  changes when a spin button is clicked while the control key is pressed.
     *  By default, Qt uses the single step increment multiplied by 10.
     *  @param step Increment when control key is pressed, or 0 to use default Qt
     *              handling which multiplies the default step by 10.
     *  @param mod  Control steps should always set value to multiple of @p step.
     */
    void         setSingleControlStep(int step, bool mod = true);

    /** Returns the rectangle containing the up arrow. */
    QRect        upRect() const;

    /** Returns the rectangle containing the down arrow. */
    QRect        downRect() const;

    /** Returns the rectangle containing the up and down arrows. */
    QRect        upDownRect() const;

    /** Sets whether the edit field is displayed. */
    void         setUpDownOnly(bool only)              { mUpDownOnly = only; }

    /** Initialise a QStyleOptionSpinBox with this instance's details. */
    void         initStyleOption(QStyleOptionSpinBox*) const override;

Q_SIGNALS:
    /** Signal emitted when the spin box's value is stepped (by the shifted or unshifted increment).
     *  @param step The requested step in the spin box's value. Note that the actual change in value
     *              may have been less than this.
     *  @param modified Qt has automatically modified the step due to the control key.
     */
    void         stepped(int step, bool control);

protected:
    /** Returns the initial adjustment to the value for a shift step up or down.
     * The default is to step up or down to the nearest multiple of the shift
     * increment, so the adjustment returned is for stepping up the decrement
     * required to round down to a multiple of the shift increment <= current value,
     * or for stepping down the increment required to round up to a multiple of the
     * shift increment >= current value.
     * This method's caller then adjusts the resultant value if necessary to cater
     * for the widget's minimum/maximum value, and wrapping.
     * This should really be a static method, but it needs to be virtual...
     */
    virtual int  shiftStepAdjustment(int oldValue, int shiftStep);

    /** Receives events destined for the spin widget or for the edit field. */
    bool         eventFilter(QObject*, QEvent*) override;

    void         paintEvent(QPaintEvent*) override;
    void         focusOutEvent(QFocusEvent*) override;
    void         mousePressEvent(QMouseEvent*) override;
    void         mouseDoubleClickEvent(QMouseEvent*) override;
    void         mouseReleaseEvent(QMouseEvent*) override;
    void         mouseMoveEvent(QMouseEvent*) override;
    void         keyPressEvent(QKeyEvent*) override;
    void         keyReleaseEvent(QKeyEvent*) override;
    void         wheelEvent(QWheelEvent*) override;

private Q_SLOTS:
    void         textEdited();
    void         valueChange();
private:
    enum Modifier { NoModifier, ShiftModifier, ControlModifier };

    void         init();
    void         addValue(int change, bool current);
    int          whichButton(const QPoint&);
    bool         setShiftStepping(Modifier, int currentButton);
    bool         clickEvent(QMouseEvent*);
    bool         keyEvent(QKeyEvent*);
    static Modifier getModifier(Qt::KeyboardModifiers);

    enum { NO_BUTTON, UP, DOWN };

    int          mMinValue;
    int          mMaxValue;
    int          mLineStep;                  // step when spin arrows are pressed
    int          mLineShiftStep;             // step when spin arrows are pressed with shift key
    int          mLineControlStep {0};       // step when spin arrows are pressed with control key
    int          mCurrentButton {NO_BUTTON}; // current spin widget button
    Modifier     mMouseKey {NoModifier};     // which modifier key applies while left button is being held down
    QStyle*      mControlStyle {nullptr};    // style to prevent Qt multiplying control step by 10
    bool         mShiftMinBound {false};     // true if a temporary minimum bound has been set during shift stepping
    bool         mShiftMaxBound {false};     // true if a temporary maximum bound has been set during shift stepping
    bool         mSelectOnStep {true};       // select the editor text whenever spin buttons are clicked (default)
    bool         mModControlStep {true};     // control steps set value to multiple of step
    bool         mUpDownOnly {false};        // true if edit field isn't displayed
    bool         mReadOnly {false};          // value cannot be changed
    bool         mSuppressSignals {false};
    bool         mEdited {false};            // text field has been edited
};

// vim: et sw=4:
