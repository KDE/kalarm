/*
 *  timeedit.cpp  -  time-of-day edit widget, with AM/PM shown depending on locale
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "timeedit.h"

#include "combobox.h"
#include "timespinbox.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLocale>

namespace
{
bool use12HourClock();
}

TimeEdit::TimeEdit(QWidget* parent)
    : QWidget(parent)
{
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    bool use12hour = use12HourClock();
    mSpinBox = new TimeSpinBox(!use12hour, this);
    mSpinBox->setFixedSize(mSpinBox->sizeHint());
    connect(mSpinBox, &TimeSpinBox::valueChanged, this, &TimeEdit::slotValueChanged);
    layout->addWidget(mSpinBox);
    if (use12hour)
    {
        mAmPm = new ComboBox(this);
        setAmPmCombo(1, 1);     // add "am" and "pm" options to the combo box
        mAmPm->setFixedSize(mAmPm->sizeHint());
        connect(mAmPm, &ComboBox::highlighted, this, &TimeEdit::slotAmPmChanged);
        layout->addWidget(mAmPm);
    }
}

void TimeEdit::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        mSpinBox->setReadOnly(ro);
        if (mAmPm)
            mAmPm->setReadOnly(ro);
    }
}

int TimeEdit::value() const
{
    return mSpinBox->value();
}

bool TimeEdit::isValid() const
{
    return mSpinBox->isValid();
}

/******************************************************************************
 * Set the edit value as valid or invalid.
 * If newly invalid, the value is displayed as asterisks.
 * If newly valid, the value is set to the minimum value.
 */
void TimeEdit::setValid(bool valid)
{
    bool oldValid = mSpinBox->isValid();
    if ((valid  &&  !oldValid)
    ||  (!valid  &&  oldValid))
    {
        mSpinBox->setValid(valid);
        if (mAmPm)
            mAmPm->setCurrentIndex(0);
    }
}

/******************************************************************************
 * Set the widget's value.
 */
void TimeEdit::setValue(int minutes)
{
    if (mAmPm)
    {
        int i = (minutes >= 720) ? mPmIndex : mAmIndex;
        mAmPm->setCurrentIndex(i >= 0 ? i : 0);
    }
    mSpinBox->setValue(minutes);
}

bool TimeEdit::wrapping() const
{
    return mSpinBox->wrapping();
}

void TimeEdit::setWrapping(bool on)
{
    mSpinBox->setWrapping(on);
}

int TimeEdit::minimum() const
{
    return mSpinBox->minimum();
}

int TimeEdit::maximum() const
{
    return mSpinBox->maximum();
}

void TimeEdit::setMinimum(int minutes)
{
    if (mAmPm)
        setAmPmCombo((minutes < 720 ? 1 : 0), -1);   // insert/remove "am" in combo box
    mSpinBox->setMinimum(minutes);
}

void TimeEdit::setMaximum(int minutes)
{
    if (mAmPm)
        setAmPmCombo(-1, (minutes < 720 ? 0 : 1));   // insert/remove "pm" in combo box
    mSpinBox->setMaximum(minutes);
}

/******************************************************************************
 * Called when the spin box value has changed.
 */
void TimeEdit::slotValueChanged(int value)
{
    if (mAmPm)
    {
        bool pm = (mAmPm->currentIndex() == mPmIndex);
        if (pm  &&  value < 720)
            mAmPm->setCurrentIndex(mAmIndex);
        else if (!pm  &&  value >= 720)
            mAmPm->setCurrentIndex(mPmIndex);
    }
    Q_EMIT valueChanged(value);
}

/******************************************************************************
 * Called when a new selection has been made by the user in the AM/PM combo box.
 * Adjust the current time value by 12 hours.
 */
void TimeEdit::slotAmPmChanged(int item)
{
    if (mAmPm)
    {
        int value = mSpinBox->value();
        if (item == mPmIndex  &&  value < 720)
            mSpinBox->setValue(value + 720);
        else if (item != mPmIndex  &&  value >= 720)
            mSpinBox->setValue(value - 720);
    }
}

/******************************************************************************
 * Set up the AM/PM combo box to contain the specified items.
 */
void TimeEdit::setAmPmCombo(int am, int pm)
{
    if (am > 0  &&  mAmIndex < 0)
    {
        // Insert "am"
        mAmIndex = 0;
        mAmPm->insertItem(mAmIndex, i18nc("@item:inlistbox Morning, as in 2am", "am"));
        if (mPmIndex >= 0)
            mPmIndex = 1;
        mAmPm->setCurrentIndex(mPmIndex >= 0 ? mPmIndex : mAmIndex);
    }
    else if (am == 0  &&  mAmIndex >= 0)
    {
        // Remove "am"
        mAmPm->removeItem(mAmIndex);
        mAmIndex = -1;
        if (mPmIndex >= 0)
            mPmIndex = 0;
        mAmPm->setCurrentIndex(mPmIndex);
    }

    if (pm > 0  &&  mPmIndex < 0)
    {
        // Insert "pm"
        mPmIndex = mAmIndex + 1;
        mAmPm->insertItem(mPmIndex, i18nc("@item:inlistbox Afternoon, as in 2pm", "pm"));
        if (mAmIndex < 0)
            mAmPm->setCurrentIndex(mPmIndex);
    }
    else if (pm == 0  &&  mPmIndex >= 0)
    {
        // Remove "pm"
        mAmPm->removeItem(mPmIndex);
        mPmIndex = -1;
        mAmPm->setCurrentIndex(mAmIndex);
    }
}

namespace
{

bool use12HourClock()
{
    const QString fmt = QLocale().timeFormat();
    // 'A' or 'a' = show am/pm; 'H' displays 24-hour format regardless.
    return fmt.contains(QLatin1Char('a'), Qt::CaseInsensitive) && !fmt.contains(QLatin1Char('H'));
}
}

// vim: et sw=4:
