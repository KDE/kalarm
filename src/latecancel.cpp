/*
 *  latecancel.cpp  -  widget to specify cancellation if late
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2026 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "latecancel.h"

#include "timeselector.h"
#include "lib/checkbox.h"

#include <KCalendarCore/Duration>
using namespace KCalendarCore;

#include <KLocalizedString>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString LateCancelSelector::i18n_chk_CancelIfLate()    { return i18nc("@option:check", "Cancel if late"); }
QString LateCancelSelector::i18n_chk_AutoCloseWin()    { return i18nc("@option:check", "Auto-close window after this time"); }
QString LateCancelSelector::i18n_chk_AutoCloseWinLC()  { return i18nc("@option:check", "Auto-close window after late-cancellation time"); }


LateCancelSelector::LateCancelSelector(bool allowHourMinute, QWidget* parent)
    : QFrame(parent)
{
    QString whatsThis = xi18nc("@info:whatsthis",
                              "<para>If checked, the alarm will be canceled if it cannot be triggered within the "
                             "specified period after its scheduled time. Possible reasons for not triggering "
                             "include your being logged off, X not running, or <application>KAlarm</application> not running.</para>"
                             "<para>If unchecked, the alarm will be triggered at the first opportunity after "
                             "its scheduled time, regardless of how late it is.</para>");

    auto topLayout = new QGridLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);

    mStack = new QStackedWidget(this);
    topLayout->addWidget(mStack, 0, 0, Qt::AlignLeft);
    mCheckboxFrame = new QFrame();
    mCheckboxFrame->setFrameStyle(QFrame::NoFrame);
    mStack->addWidget(mCheckboxFrame);
    auto hlayout = new QHBoxLayout(mCheckboxFrame);
    hlayout->setContentsMargins(0, 0, 0, 0);
    mCheckbox = new CheckBox(i18n_chk_CancelIfLate(), mCheckboxFrame);
    connect(mCheckbox, &CheckBox::toggled, this, &LateCancelSelector::slotToggled);
    connect(mCheckbox, &CheckBox::toggled, this, &LateCancelSelector::changed);
    mCheckbox->setWhatsThis(whatsThis);
    hlayout->addWidget(mCheckbox, 0, Qt::AlignLeft);

    mTimeSelectorFrame = new QFrame;
    mTimeSelectorFrame->setFrameStyle(QFrame::NoFrame);
    mStack->addWidget(mTimeSelectorFrame);
    hlayout = new QHBoxLayout(mTimeSelectorFrame);
    hlayout->setContentsMargins(0, 0, 0, 0);
    mTimeSelector = new TimeSelector(i18nc("@option:check Cancel if late by 10 minutes", "Cancel if late by"),
                                     whatsThis, i18nc("@info:whatsthis", "Enter how late will cause the alarm to be canceled"),
                                     allowHourMinute, mTimeSelectorFrame);
    connect(mTimeSelector, &TimeSelector::toggled, this, &LateCancelSelector::slotToggled);
    connect(mTimeSelector, &TimeSelector::valueChanged, this, &LateCancelSelector::changed);
    hlayout->addWidget(mTimeSelector, 0, Qt::AlignLeft);

    auto frame = new QFrame;
    frame->setFrameStyle(QFrame::NoFrame);
    topLayout->addWidget(frame, 1, 0, Qt::AlignLeft);
    hlayout = new QHBoxLayout(frame);
    const int indent = CheckBox::textIndent(mCheckbox);
    if (layoutDirection() == Qt::LeftToRight)
        hlayout->setContentsMargins(indent, 0, 0, 0);
    else
        hlayout->setContentsMargins(0, 0, indent, 0);
    mAutoClose = new CheckBox(i18n_chk_AutoCloseWin(), frame);
    connect(mAutoClose, &CheckBox::toggled, this, &LateCancelSelector::changed);
    mAutoClose->setWhatsThis(i18nc("@info:whatsthis", "Automatically close the alarm window after the expiry of the late-cancellation period"));
    hlayout->addWidget(mAutoClose);
    hlayout->addStretch();

    // Layout to contain the extra widget set by addWidget().
    mExtraLayout = new QVBoxLayout;
    mExtraLayout->setContentsMargins(0, 0, 0, 0);
    mExtraLayout->setStretch(0, 1);
    topLayout->addLayout(mExtraLayout, 0, 1, 2, 1, Qt::AlignRight);

    mAutoClose->hide();
    mAutoClose->setEnabled(false);
}

/******************************************************************************
* Set the read-only status.
*/
void LateCancelSelector::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        mCheckbox->setReadOnly(mReadOnly);
        mTimeSelector->setReadOnly(mReadOnly);
        mAutoClose->setReadOnly(mReadOnly);
    }
}

/******************************************************************************
* Add a widget to the layout, right adjusted, beside Auto-close checkbox.
*/
void LateCancelSelector::addWidget(QWidget* widget)
{
    mExtraWidget = widget;
    mExtraLayout->addWidget(widget);
    alignExtraWidget();
}

int LateCancelSelector::minutes() const
{
    return mTimeSelector->period().asSeconds() / 60;
}

void LateCancelSelector::setMinutes(int minutes, bool dateOnly, TimePeriod::Units defaultUnits)
{
    slotToggled(minutes);
    Duration period;
    if (minutes % (24*60))
        period = Duration(minutes * 60, Duration::Seconds);
    else
        period = Duration(minutes / (24*60), Duration::Days);
    mTimeSelector->setPeriod(period, dateOnly, defaultUnits);
}

void LateCancelSelector::setDateOnly(bool dateOnly)
{
    if (dateOnly != mDateOnly)
    {
        mDateOnly = dateOnly;
        if (mTimeSelector->isChecked())      // don't change when it's not visible
            mTimeSelector->setDateOnly(dateOnly);
    }
}

void LateCancelSelector::allowAutoClose(bool allow)
{
    mAutoCloseAllowed = allow;
    updateAutoClose();
}

bool LateCancelSelector::isAutoClose() const
{
    return mAutoCloseShown  &&  mAutoClose->isEnabled()  &&  mAutoClose->isChecked();
}

void LateCancelSelector::setAutoClose(bool autoClose)
{
    mAutoClose->setChecked(autoClose);
}

/******************************************************************************
* Called when either of the checkboxes is toggled.
*/
void LateCancelSelector::slotToggled(bool on)
{
    mCheckbox->setChecked(on);
    mTimeSelector->setChecked(on);
    if (on)
    {
        mTimeSelector->setDateOnly(mDateOnly);
        mStack->setCurrentWidget(mTimeSelectorFrame);
    }
    else
        mStack->setCurrentWidget(mCheckboxFrame);
    updateAutoClose();
}

void LateCancelSelector::updateAutoClose()
{
    const bool show = mTimeSelector->isChecked() && mAutoCloseAllowed;
    if (show != mAutoCloseShown)
    {
        mAutoClose->setEnabled(show);
        if (show)
            mAutoClose->show();
        else
            mAutoClose->hide();
        mAutoCloseShown = show;
        updateGeometry();
        alignExtraWidget();
    }
}

void LateCancelSelector::alignExtraWidget()
{
    if (mExtraWidget)
        mExtraLayout->setAlignment(mExtraWidget, (mAutoCloseShown ? Qt::AlignBottom : Qt::AlignCenter));
}

#include "moc_latecancel.cpp"

// vim: et sw=4:
