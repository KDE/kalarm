/*
 *  timeselector.cpp  -  widget to optionally set a time period
 *  Program:  kalarm
 *  Copyright © 2004-2016 David Jarvie <djarvie@kde.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "timeselector.h"

#include "checkbox.h"
#include "combobox.h"
#include "kalarm_debug.h"

#include <QHBoxLayout>

using namespace KCalendarCore;


TimeSelector::TimeSelector(const QString& selectText, const QString& selectWhatsThis,
                           const QString& valueWhatsThis, bool allowHourMinute, QWidget* parent)
    : QFrame(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    mSelect = new CheckBox(selectText, this);
    mSelect->setFixedSize(mSelect->sizeHint());
    connect(mSelect, &CheckBox::toggled, this, &TimeSelector::selectToggled);
    mSelect->setWhatsThis(selectWhatsThis);
    layout->addWidget(mSelect);

    QWidget* box = new QWidget(this);    // to group widgets for QWhatsThis text
    layout->addWidget(box);
    QHBoxLayout* boxLayout = new QHBoxLayout(box);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    mPeriod = new TimePeriod(allowHourMinute, box);
    boxLayout->addWidget(mPeriod);
    mPeriod->setFixedSize(mPeriod->sizeHint());
    mPeriod->setSelectOnStep(false);
    connect(mPeriod, &TimePeriod::valueChanged, this, &TimeSelector::periodChanged);
    mSelect->setFocusWidget(mPeriod);
    mPeriod->setEnabled(false);

    box->setWhatsThis(valueWhatsThis);
    layout->addStretch();
}

/******************************************************************************
* Create a ComboBox used to select the time period's sign.
* The caller is responsible for populating the ComboBox and handling its value.
*/
ComboBox* TimeSelector::createSignCombo()
{
    delete mSignWidget;
    QWidget* p = mPeriod->parentWidget();
    mSignWidget = new ComboBox(p);
    mSignWidget->setEnabled(mPeriod->isEnabled());
    p->layout()->addWidget(mSignWidget);
    return mSignWidget;
}

/******************************************************************************
* Set the read-only status.
*/
void TimeSelector::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        mSelect->setReadOnly(mReadOnly);
        mPeriod->setReadOnly(mReadOnly);
        if (mSignWidget)
            mSignWidget->setReadOnly(mReadOnly);
    }
}

bool TimeSelector::isChecked() const
{
    return mSelect->isChecked();
}

void TimeSelector::setChecked(bool on)
{
    if (on != mSelect->isChecked())
    {
        mSelect->setChecked(on);
        Q_EMIT valueChanged(period());
    }
}

void TimeSelector::setMaximum(int hourmin, int days)
{
    mPeriod->setMaximum(hourmin, days);
}

void TimeSelector::setDateOnly(bool dateOnly)
{
    mPeriod->setDateOnly(dateOnly);
}

/******************************************************************************
 * Get the specified number of minutes.
 * Reply = 0 if unselected.
 */
Duration TimeSelector::period() const
{
    return mSelect->isChecked() ? mPeriod->period() : Duration(0);
}

/******************************************************************************
* Initialise the controls with a specified time period.
* If minutes = 0, it will be deselected.
* The time unit combo-box is initialised to 'defaultUnits', but if 'dateOnly'
* is true, it will never be initialised to hours/minutes.
*/
void TimeSelector::setPeriod(const Duration& period, bool dateOnly, TimePeriod::Units defaultUnits)
{
    bool havePeriod = !period.isNull();
    mSelect->setChecked(havePeriod);
    mPeriod->setEnabled(havePeriod);
    if (mSignWidget)
        mSignWidget->setEnabled(havePeriod);
    mPeriod->setPeriod(period, dateOnly, defaultUnits);
}

/******************************************************************************
* Set the input focus on the count field.
*/
void TimeSelector::setFocusOnCount()
{
    mPeriod->setFocusOnCount();
}

/******************************************************************************
* Called when the TimeSelector checkbox is toggled.
*/
void TimeSelector::selectToggled(bool on)
{
    mPeriod->setEnabled(on);
    if (mSignWidget)
        mSignWidget->setEnabled(on);
    if (on)
        mPeriod->setFocus();
    Q_EMIT toggled(on);
    Q_EMIT valueChanged(period());
}

/******************************************************************************
* Called when the period value changes.
*/
void TimeSelector::periodChanged(const Duration& period)
{
    if (mSelect->isChecked())
        Q_EMIT valueChanged(period);
}

// vim: et sw=4:
