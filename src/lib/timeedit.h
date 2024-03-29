/*
 *  timeedit.h  -  time-of-day edit widget, with AM/PM shown depending on locale
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>
#include <QTime>
class ComboBox;
class TimeSpinBox;


/**
 *  @short Widget to enter a time of day.
 *
 *  The TimeEdit class provides a widget to enter a time of day in hours and minutes,
 *  using a 12- or 24-hour clock according to the user's locale settings.
 *
 *  It displays a TimeSpinBox widget to enter hours and minutes. If a 12-hour clock is
 *  being used, it also displays a combo box to select am or pm.
 *
 *  TimeSpinBox displays a spin box with two pairs of spin buttons, one for hours and
 *  one for minutes. It provides accelerated stepping using the spin buttons, when the
 *  shift key is held down (inherited from SpinBox2). The default shift steps are 5
 *  minutes and 6 hours.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class TimeEdit : public QWidget
{
    Q_OBJECT
public:
    /** Constructor.
     *  @param parent The parent object of this widget.
     */
    explicit TimeEdit(QWidget* parent = nullptr);

    /** Returns true if the widget is read only. */
    bool isReadOnly() const           { return mReadOnly; }

    /** Sets whether the widget is read-only for the user. If read-only,
     *  the time cannot be edited and the spin buttons and am/pm combo box
     *  are inactive.
     *  @param readOnly True to set the widget read-only, false to set it read-write.
     */
    virtual void  setReadOnly(bool readOnly);

    /** Returns true if the widget contains a valid value. */
    bool isValid() const;

    /** Sets whether the edit value is valid.
     *  If newly invalid, the value is displayed as asterisks.
     *  If newly valid, the value is set to the minimum value.
     *  @param valid True to set the value valid, false to set it invalid.
     */
    void setValid(bool valid);

    /** Returns the entered time as a value in minutes. */
    int value() const;

    /** Returns the entered time as a QTime value. */
    QTime time() const                 { int m = value();  return {m/60, m%60}; }

    /** Returns true if it is possible to step the value from the highest value to the lowest value and vice versa. */
    bool wrapping() const;

    /** Sets whether it is possible to step the value from the highest value to the lowest value and vice versa.
     *  @param on True to enable wrapping, else false.
     */
    void setWrapping(bool on);

    /** Returns the minimum value of the widget in minutes. */
    int minimum() const;

    /** Returns the maximum value of the widget in minutes. */
    int maximum() const;

    /** Returns the maximum value of the widget as a QTime value. */
    QTime maxTime() const              { int mv = maximum();  return {mv/60, mv%60}; }

    /** Sets the minimum value of the widget. */
    void setMinimum(int minutes);

    /** Sets the maximum value of the widget. */
    void setMaximum(int minutes);

    /** Sets the maximum value of the widget. */
    void setMaximum(const QTime& time)  { setMaximum(time.hour()*60 + time.minute()); }

public Q_SLOTS:
    /** Sets the value of the widget. */
    virtual void  setValue(int minutes);

    /** Sets the value of the widget. */
    void setValue(const QTime& t)     { setValue(t.hour()*60 + t.minute()); }

Q_SIGNALS:
    /** This signal is emitted every time the value of the widget changes
     *  (for whatever reason).
     *  @param minutes The new value.
     */
    void valueChanged(int minutes);

private Q_SLOTS:
    void slotValueChanged(int);
    void slotAmPmChanged(int item);

private:
    void setAmPmCombo(int am, int pm);

    TimeSpinBox*  mSpinBox;           // always holds the 24-hour time
    ComboBox*     mAmPm {nullptr};
    int           mAmIndex {-1};      // mAmPm index to "am", or -1 if none
    int           mPmIndex {-1};      // mAmPm index to "pm", or -1 if none
    bool          mReadOnly {false};  // the widget is read only
};

// vim: et sw=4:
