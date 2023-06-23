/*
 *  datepicker.cpp  -  date chooser widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2021-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "datepicker.h"

#include "daymatrix.h"
#include "functions.h"
#include "preferences.h"
#include "lib/locale.h"
#include "lib/synchtimer.h"
#include "kalarmcalendar/kadatetime.h"

#include <KLocalizedString>

#include <QLabel>
#include <QToolButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLocale>
#include <QApplication>

class DPToolButton : public QToolButton
{
    Q_OBJECT
public:
    DPToolButton(QWidget* parent) : QToolButton(parent) {}
    QSize sizeHint() const override
    {
        QSize s = QToolButton::sizeHint();
        return QSize(s.width() * 4 / 5, s.height());
    }
    QSize minimumSizeHint() const override
    {
        QSize s = QToolButton::minimumSizeHint();
        return QSize(s.width() * 4 / 5, s.height());
    }
};


DatePicker::DatePicker(QWidget* parent)
    : QWidget(parent)
{
    const QString whatsThis = i18nc("@info:whatsthis", "Select dates to show in the alarm list. Only alarms due on these dates will be shown.");

    QVBoxLayout* topLayout = new QVBoxLayout(this);
    const int spacing = topLayout->spacing();
    topLayout->setSpacing(0);

    QLabel* label = new QLabel(i18nc("@title:group", "Alarm Date Selector"), this);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setWhatsThis(whatsThis);
    topLayout->addWidget(label, 0, Qt::AlignHCenter);
    topLayout->addSpacing(spacing);

    // Set up the month/year navigation buttons at the top.
    QHBoxLayout* hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addLayout(hlayout);

    DPToolButton* leftYear   = createArrowButton(QStringLiteral("arrow-left-double"));
    DPToolButton* leftMonth  = createArrowButton(QStringLiteral("arrow-left"));
    DPToolButton* rightMonth = createArrowButton(QStringLiteral("arrow-right"));
    DPToolButton* rightYear  = createArrowButton(QStringLiteral("arrow-right-double"));
    DPToolButton* today      = createArrowButton(QStringLiteral("show-today"));
    mPrevYear  = leftYear;
    mPrevMonth = leftMonth;
    mNextYear  = rightYear;
    mNextMonth = rightMonth;
    mToday     = today;
    if (QApplication::isRightToLeft())
    {
        mPrevYear  = rightYear;
        mPrevMonth = rightMonth;
        mNextYear  = leftYear;
        mNextMonth = leftMonth;
    }
    mPrevYear->setToolTip(i18nc("@info:tooltip", "Show the previous year"));
    mPrevMonth->setToolTip(i18nc("@info:tooltip", "Show the previous month"));
    mNextYear->setToolTip(i18nc("@info:tooltip", "Show the next year"));
    mNextMonth->setToolTip(i18nc("@info:tooltip", "Show the next month"));
    mToday->setToolTip(i18nc("@info:tooltip", "Show today"));
    connect(mPrevYear, &QToolButton::clicked, this, &DatePicker::prevYearClicked);
    connect(mPrevMonth, &QToolButton::clicked, this, &DatePicker::prevMonthClicked);
    connect(mNextYear, &QToolButton::clicked, this, &DatePicker::nextYearClicked);
    connect(mNextMonth, &QToolButton::clicked, this, &DatePicker::nextMonthClicked);
    connect(mToday, &QToolButton::clicked, this, &DatePicker::todayClicked);

    const QDate currentDate = KADateTime::currentDateTime(Preferences::timeSpec()).date();
    mMonthYear = new QLabel(this);
    mMonthYear->setAlignment(Qt::AlignCenter);
    QLocale locale;
    QDate d(currentDate.year(), 1, 1);
    int maxWidth = 0;
    for (int i = 1;  i <= 12;  ++i)
    {
        mMonthYear->setText(locale.toString(d, QStringLiteral("MMM yyyy")));
        maxWidth = std::max(maxWidth, mMonthYear->minimumSizeHint().width());
        d = d.addMonths(1);
    }
    mMonthYear->setMinimumWidth(maxWidth);

    if (QApplication::isRightToLeft())
        hlayout->addWidget(mToday);
    hlayout->addWidget(mPrevYear);
    hlayout->addWidget(mPrevMonth);
    hlayout->addStretch();
    hlayout->addWidget(mMonthYear);
    hlayout->addStretch();
    hlayout->addWidget(mNextMonth);
    hlayout->addWidget(mNextYear);
    if (!QApplication::isRightToLeft())
        hlayout->addWidget(mToday);

    // Set up the day name headings.
    // These start at the user's start day of the week.
    QWidget* widget = new QWidget(this);  // this is to control the QWhatsThis text display area
    widget->setWhatsThis(whatsThis);
    topLayout->addWidget(widget);
    QVBoxLayout* vlayout = new QVBoxLayout(widget);
    vlayout->setContentsMargins(0, 0, 0, 0);
    QGridLayout* grid = new QGridLayout;
    grid->setSpacing(0);
    grid->setContentsMargins(0, 0, 0, 0);
    vlayout->addLayout(grid);
    mDayNames = new QLabel[7];
    maxWidth = 0;
    for (int i = 0;  i < 7;  ++i)
    {
        const int day = Locale::localeDayInWeek_to_weekDay(i);
        mDayNames[i].setText(locale.dayName(day, QLocale::ShortFormat));
        mDayNames[i].setAlignment(Qt::AlignCenter);
        maxWidth = std::max(maxWidth, mDayNames[i].minimumSizeHint().width());
        grid->addWidget(&mDayNames[i], 0, i, 1, 1, Qt::AlignCenter);
    }
    for (int i = 0;  i < 7;  ++i)
        mDayNames[i].setMinimumWidth(maxWidth);

    mDayMatrix = new DayMatrix(widget);
    mDayMatrix->setWhatsThis(whatsThis);
    vlayout->addWidget(mDayMatrix);
    connect(mDayMatrix, &DayMatrix::selected, this, &DatePicker::datesSelected);
    connect(mDayMatrix, &DayMatrix::newAlarm, this, &DatePicker::slotNewAlarm);
    connect(mDayMatrix, &DayMatrix::newAlarmFromTemplate, this, &DatePicker::slotNewAlarmFromTemplate);

    // Initialise the display.
    mMonthShown.setDate(currentDate.year(), currentDate.month(), 1);
    newMonthShown();
    updateDisplay();

    MidnightTimer::connect(this, SLOT(updateToday()));
}

DatePicker::~DatePicker()
{
    delete[] mDayNames;
}

QList<QDate> DatePicker::selectedDates() const
{
    return mDayMatrix->selectedDates();
}

void DatePicker::clearSelection()
{
    mDayMatrix->clearSelection();
}

/******************************************************************************
* Called when the widget is shown. Set the row height for the day matrix.
*/
void DatePicker::showEvent(QShowEvent* e)
{
    mDayMatrix->setRowHeight(mDayNames[0].height());
    QWidget::showEvent(e);
}

/******************************************************************************
* Called when the previous year arrow button has been clicked.
*/
void DatePicker::prevYearClicked()
{
    newMonthShown();
    if (mPrevYear->isEnabled())
    {
        mMonthShown = mMonthShown.addYears(-1);
        newMonthShown();
        updateDisplay();
    }
}

/******************************************************************************
* Called when the previous month arrow button has been clicked.
*/
void DatePicker::prevMonthClicked()
{
    newMonthShown();
    if (mPrevMonth->isEnabled())
    {
        mMonthShown = mMonthShown.addMonths(-1);
        newMonthShown();
        updateDisplay();
    }
}

/******************************************************************************
* Called when the next year arrow button has been clicked.
*/
void DatePicker::nextYearClicked()
{
    mMonthShown = mMonthShown.addYears(1);
    newMonthShown();
    updateDisplay();
}

/******************************************************************************
* Called when the next month arrow button has been clicked.
*/
void DatePicker::nextMonthClicked()
{
    mMonthShown = mMonthShown.addMonths(1);
    newMonthShown();
    updateDisplay();
}

/******************************************************************************
* Called when the today button has been clicked.
*/
void DatePicker::todayClicked()
{
    const QDate currentDate = KADateTime::currentDateTime(Preferences::timeSpec()).date();
    const QDate monthToShow(currentDate.year(), currentDate.month(), 1);
    if (monthToShow != mMonthShown)
    {
        mMonthShown = monthToShow;
        newMonthShown();
        updateDisplay();
    }
}

/******************************************************************************
* Called at midnight. If the month has changed, update the view.
*/
void DatePicker::updateToday()
{
    const QDate currentDate = KADateTime::currentDateTime(Preferences::timeSpec()).date();
    const QDate monthToShow(currentDate.year(), currentDate.month(), 1);
    if (monthToShow > mMonthShown)
    {
        mMonthShown = monthToShow;
        newMonthShown();
        updateDisplay();
    }
    else
        mDayMatrix->updateToday(currentDate);
}

/******************************************************************************
* Called when a new month is shown, to enable/disable 'previous' arrow buttons.
*/
void DatePicker::newMonthShown()
{
    QLocale locale;
    mMonthYear->setText(locale.toString(mMonthShown, QStringLiteral("MMM yyyy")));

    const QDate currentDate = KADateTime::currentDateTime(Preferences::timeSpec()).date();
    mPrevMonth->setEnabled(mMonthShown > currentDate);
    mPrevYear->setEnabled(mMonthShown.addMonths(-11) > currentDate);
}

/******************************************************************************
* Called when the "New Alarm" menu item is selected to edit a new alarm.
*/
void DatePicker::slotNewAlarm(EditAlarmDlg::Type type)
{
    const QList<QDate> selectedDates = mDayMatrix->selectedDates();
    const QDate startDate = selectedDates.isEmpty() ? QDate() : selectedDates[0];
    KAlarm::editNewAlarm(type, startDate);
}

/******************************************************************************
* Called when the "New Alarm" menu item is selected to edit a new alarm from a
* template.
*/
void DatePicker::slotNewAlarmFromTemplate(const KAEvent& event)
{
    const QList<QDate> selectedDates = mDayMatrix->selectedDates();
    const QDate startDate = selectedDates.isEmpty() ? QDate() : selectedDates[0];
    KAlarm::editNewAlarm(event, startDate);
}

/******************************************************************************
* Update the days shown.
*/
void DatePicker::updateDisplay()
{
    const int firstDay = Locale::weekDay_to_localeDayInWeek(mMonthShown.dayOfWeek());
    mStartDate = mMonthShown.addDays(-firstDay);
    mDayMatrix->setStartDate(mStartDate);
    mDayMatrix->update();
    mDayMatrix->repaint();
}

/******************************************************************************
* Create an arrow button for moving backwards or forwards.
*/
DPToolButton* DatePicker::createArrowButton(const QString& iconId)
{
    DPToolButton* button = new DPToolButton(this);
    button->setIcon(QIcon::fromTheme(iconId));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    return button;
}

#include "datepicker.moc"
#include "moc_datepicker.cpp"

// vim: et sw=4:
