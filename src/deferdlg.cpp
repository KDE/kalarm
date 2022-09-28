/*
 *  deferdlg.cpp  -  dialog to defer an alarm
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "deferdlg.h"

#include "alarmtimewidget.h"
#include "resources/resources.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/kaevent.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QStyle>
#include <QDialogButtonBox>
#include <QPushButton>


/******************************************************************************
* Constructor.
* If 'cancelButton' is true, the Cancel Deferral button will be shown to allow
* any existing deferral to be cancelled.
*/
DeferAlarmDlg::DeferAlarmDlg(const DateTime& initialDT, bool anyTimeOption, bool cancelButton, QWidget* parent)
    : QDialog(parent)
{
    setWindowModality(Qt::WindowModal);
    setWindowTitle(i18nc("@title:window", "Defer Alarm"));

    auto layout = new QVBoxLayout(this);

    mTimeWidget = new AlarmTimeWidget((anyTimeOption ? AlarmTimeWidget::DEFER_ANY_TIME : AlarmTimeWidget::DEFER_TIME), this);
    mTimeWidget->setDateTime(initialDT);
    mTimeWidget->setMinDateTimeIsCurrent();
    connect(mTimeWidget, &AlarmTimeWidget::pastMax, this, &DeferAlarmDlg::slotPastLimit);
    layout->addWidget(mTimeWidget);
    layout->addSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));

    mButtonBox = new QDialogButtonBox(this);
    layout->addWidget(mButtonBox);
    QPushButton* okButton = mButtonBox->addButton(QDialogButtonBox::Ok);
    okButton->setWhatsThis(i18nc("@info:whatsthis", "Defer the alarm until the specified time."));
    mButtonBox->addButton(QDialogButtonBox::Cancel);
    connect(mButtonBox, &QDialogButtonBox::accepted,
            this, &DeferAlarmDlg::slotOk);
    connect(mButtonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    if (cancelButton)
    {
        QPushButton* deferButton = mButtonBox->addButton(i18nc("@action:button", "Cancel Deferral"), QDialogButtonBox::ActionRole);
        deferButton->setWhatsThis(i18nc("@info:whatsthis", "Cancel the deferred alarm. This does not affect future recurrences."));
        connect(mButtonBox, &QDialogButtonBox::clicked, this,
                [this, deferButton](const QAbstractButton* btn)
                {
                    if (btn == deferButton)
                        slotCancelDeferral();
                });
    }
}

/******************************************************************************
* Called when the OK button is clicked.
*/
void DeferAlarmDlg::slotOk()
{
    mAlarmDateTime = mTimeWidget->getDateTime(&mDeferMinutes);
    if (!mAlarmDateTime.isValid())
        return;
    KAEvent::DeferLimitType limitType = KAEvent::LIMIT_NONE;
    DateTime endTime;
    if (!mLimitEventId.isEmpty())
    {
        // Get the event being deferred
        Resource resource = Resources::resource(mLimitEventId.resourceId());
        const KAEvent event = resource.event(mLimitEventId.eventId());
        if (event.isValid())
            endTime = event.deferralLimit(&limitType);
    }
    else
    {
        endTime = mLimitDateTime;
        limitType = mLimitDateTime.isValid() ? KAEvent::LIMIT_MAIN : KAEvent::LIMIT_NONE;
    }
    if (endTime.isValid()  &&  mAlarmDateTime > endTime)
    {
        QString text;
        switch (limitType)
        {
            case KAEvent::LIMIT_REPETITION:
                text = i18nc("@info", "Cannot defer past the alarm's next sub-repetition (currently %1)",
                             endTime.formatLocale());
                break;
            case KAEvent::LIMIT_RECURRENCE:
                text = i18nc("@info", "Cannot defer past the alarm's next recurrence (currently %1)",
                             endTime.formatLocale());
                break;
            case KAEvent::LIMIT_REMINDER:
                text = i18nc("@info", "Cannot defer past the alarm's next reminder (currently %1)",
                            endTime.formatLocale());
                break;
            case KAEvent::LIMIT_MAIN:
                text = i18nc("@info", "Cannot defer reminder past the main alarm time (%1)",
                            endTime.formatLocale());
                break;
            case KAEvent::LIMIT_NONE:
                break;   // can't happen with a valid endTime
        }
        KAMessageBox::error(this, text);
    }
    else
        accept();
}

/******************************************************************************
* Select the 'Time from now' radio button and preset its value.
*/
void DeferAlarmDlg::setDeferMinutes(int minutes)
{
    mTimeWidget->selectTimeFromNow(minutes);
}

/******************************************************************************
* Called the maximum date/time for the date/time edit widget has been passed.
*/
void DeferAlarmDlg::slotPastLimit()
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

/******************************************************************************
* Set the time limit for deferral based on the next occurrence of the alarm
* with the specified ID.
*/
void DeferAlarmDlg::setLimit(const DateTime& limit)
{
    mLimitEventId.clear();
    mLimitDateTime = limit;
    mTimeWidget->setMaxDateTime(mLimitDateTime);
}

/******************************************************************************
* Set the time limit for deferral based on the next occurrence of the alarm
* with the specified ID.
*/
DateTime DeferAlarmDlg::setLimit(const KAEvent& event)
{
    Q_ASSERT(event.resourceId() >= 0);
    mLimitEventId = EventId(event);
    Resource resource = Resources::resource(event.resourceId());
    const KAEvent evnt = resource.event(event.id());
    mLimitDateTime = evnt.isValid() ? evnt.deferralLimit() : DateTime();
    mTimeWidget->setMaxDateTime(mLimitDateTime);
    return mLimitDateTime;
}

/******************************************************************************
* Called when the Cancel Deferral button is clicked.
*/
void DeferAlarmDlg::slotCancelDeferral()
{
    mAlarmDateTime = DateTime();
    accept();
}

// vim: et sw=4:
