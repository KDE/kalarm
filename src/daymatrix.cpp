/*
 *  daymatrix.cpp  -  calendar day matrix display
 *  Program:  kalarm
 *  This class is adapted from KODayMatrix in KOrganizer.
 *
 *  SPDX-FileCopyrightText: 2001 Eitzenberger Thomas <thomas.eitzenberger@siemens.at>
 *  Parts of the source code have been copied from kdpdatebutton.cpp
 *
 *  SPDX-FileCopyrightText: 2003 Cornelius Schumacher <schumacher@kde.org>
 *  SPDX-FileCopyrightText: 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  SPDX-FileCopyrightText: 2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later WITH Qt-Commercial-exception-1.0
*/

#include "daymatrix.h"

#include "newalarmaction.h"
#include "preferences.h"
#include "resources/resources.h"

#include <KHolidays/HolidayRegion>
#include <KLocalizedString>

#include <QMenu>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <cmath>

namespace
{
const int NUMROWS = 6;                // number of rows displayed in the matrix
const int NUMDAYS = NUMROWS * 7;      // number of days displayed in the matrix
const int NO_SELECTION = -1000000;    // invalid selection start/end value

const QColor HOLIDAY_BACKGROUND_COLOUR(255,100,100);  // add a preference for this?
const int    TODAY_MARGIN_WIDTH(2);

struct TextColours
{
    QColor disabled;
    QColor thisMonth;
    QColor otherMonth;
    QColor thisMonthHoliday {HOLIDAY_BACKGROUND_COLOUR};
    QColor otherMonthHoliday;

    explicit TextColours(const QPalette& palette);

private:
    QColor getShadedColour(const QColor& colour, bool enabled) const;
};
}

DayMatrix::DayMatrix(QWidget* parent)
    : QFrame(parent)
    , mDayLabels(NUMDAYS)
    , mSelStart(NO_SELECTION)
    , mSelEnd(NO_SELECTION)
{
    mHolidays.reserve(NUMDAYS);
    for (int i = 0; i < NUMDAYS; ++i)
        mHolidays.append(QString());

    Resources* resources = Resources::instance();
    connect(resources, &Resources::resourceAdded, this, &DayMatrix::resourceUpdated);
    connect(resources, &Resources::resourceRemoved, this, &DayMatrix::resourceRemoved);
    connect(resources, &Resources::eventsAdded, this, &DayMatrix::resourceUpdated);
    connect(resources, &Resources::eventUpdated, this, &DayMatrix::resourceUpdated);
    connect(resources, &Resources::eventsRemoved, this, &DayMatrix::resourceUpdated);
    Preferences::connect(&Preferences::holidaysChanged, this, &DayMatrix::slotUpdateView);
    Preferences::connect(&Preferences::workTimeChanged, this, &DayMatrix::slotUpdateView);
}

DayMatrix::~DayMatrix()
{
}

/******************************************************************************
* Return all selected dates from mSelStart to mSelEnd, in date order.
*/
QVector<QDate> DayMatrix::selectedDates() const
{
    QVector<QDate> selDays;
    if (mSelStart != NO_SELECTION)
    {
        selDays.reserve(mSelEnd - mSelStart + 1);
        for (int i = mSelStart;  i <= mSelEnd;  ++i)
            selDays.append(mStartDate.addDays(i));
    }
    return selDays;
}

/******************************************************************************
* Clear the current selection of dates.
*/
void DayMatrix::clearSelection()
{
    setMouseSelection(NO_SELECTION, NO_SELECTION, true);
}

/******************************************************************************
* Evaluate the index for today, and update the display if it has changed.
*/
void DayMatrix::updateToday(const QDate& newDate)
{
    const int index = mStartDate.daysTo(newDate);
    if (index != mTodayIndex)
    {
        mTodayIndex = index;
        updateEvents();

        if (mSelStart != NO_SELECTION  &&  mSelStart < mTodayIndex)
        {
            if (mSelEnd < mTodayIndex)
                setMouseSelection(NO_SELECTION, NO_SELECTION, true);
            else
                setMouseSelection(mTodayIndex, mSelEnd, true);
        }
        else
            update();
    }
}

/******************************************************************************
* Set a new start date for the matrix. If changed, or other changes are
* pending, recalculates which days in the matrix alarms occur on, and which are
* holidays/non-work days, and repaints.
*/
void DayMatrix::setStartDate(const QDate& startDate)
{
    if (!startDate.isValid())
        return;

    if (startDate != mStartDate)
    {
        if (mSelStart != NO_SELECTION)
       	{
            // Adjust selection indexes to be relative to the new start date.
            const int diff = startDate.daysTo(mStartDate);
            mSelStart += diff;
            mSelEnd   += diff;
            if (mSelectionMustBeVisible)
            {
                // Ensure that the whole selection is still visible: if not, cancel the selection.
                if (mSelStart < 0  ||  mSelEnd >= NUMDAYS)
                    setMouseSelection(NO_SELECTION, NO_SELECTION, true);
            }
        }

        mStartDate = startDate;

        QLocale locale;
        mMonthStartIndex = -1;
        mMonthEndIndex = NUMDAYS-1;
        for (int i = 0; i < NUMDAYS; ++i)
        {
            const int day = mStartDate.addDays(i).day();
            mDayLabels[i] = locale.toString(day);

            if (day == 1)    // start of a month
            {
                if (mMonthStartIndex < 0)
                    mMonthStartIndex = i;
                else
                    mMonthEndIndex = i - 1;
            }
        }

        mTodayIndex = mStartDate.daysTo(KADateTime::currentDateTime(Preferences::timeSpec()).date());
        updateView();
    }
    else if (mPendingChanges)
        updateView();
}

/******************************************************************************
* If changes are pending, recalculate which days in the matrix have alarms
* occurring, and which are holidays/non-work days. Repaint the matrix.
*/
void DayMatrix::updateView()
{
    if (!mStartDate.isValid())
        return;

    // TODO_Recurrence: If we just change the selection, but not the data,
    // there's no need to update the whole list of alarms... This is just a
    // waste of computational power
    updateEvents();

    // Find which holidays occur for the dates in the matrix.
    const KHolidays::HolidayRegion &region = Preferences::holidays();
    const KHolidays::Holiday::List list = region.rawHolidaysWithAstroSeasons(
        mStartDate, mStartDate.addDays(NUMDAYS - 1));

    QHash<QDate, QStringList> holidaysByDate;
    for (const KHolidays::Holiday& holiday : list)
        if (!holiday.name().isEmpty())
            holidaysByDate[holiday.observedStartDate()].append(holiday.name());
    for (int i = 0; i < NUMDAYS; ++i)
    {
        const QStringList holidays = holidaysByDate[mStartDate.addDays(i)];
        if (!holidays.isEmpty())
            mHolidays[i] = holidays.join(i18nc("delimiter for joining holiday names", ","));
        else
            mHolidays[i].clear();
    }

    update();
}

/******************************************************************************
* Find which days currently displayed have alarms scheduled.
*/
void DayMatrix::updateEvents()
{
    const KADateTime::Spec timeSpec = Preferences::timeSpec();
    const QDate startDate = (mTodayIndex <= 0) ? mStartDate : mStartDate.addDays(mTodayIndex);
    const KADateTime before = KADateTime(startDate, QTime(0,0,0), timeSpec).addSecs(-60);
    const KADateTime to(mStartDate.addDays(NUMDAYS-1), QTime(23,59,0), timeSpec);

    mEventDates.clear();
    const QVector<Resource> resources = Resources::enabledResources(CalEvent::ACTIVE);
    for (const Resource& resource : resources)
    {
        const QList<KAEvent> events = resource.events();
        const CalEvent::Types types = resource.enabledTypes() & CalEvent::ACTIVE;
        for (const KAEvent& event : events)
        {
            if (event.enabled()  &&  (event.category() & types))
            {
                // The event has an enabled alarm type.
                // Find all its recurrences/repetitions within the time period.
                DateTime nextDt;
                for (KADateTime from = before;  ;  )
                {
                    event.nextOccurrence(from, nextDt, KAEvent::RETURN_REPETITION);
                    if (!nextDt.isValid())
                        break;
                    from = nextDt.effectiveKDateTime().toTimeSpec(timeSpec);
                    if (from > to)
                        break;
                    if (!event.excludedByWorkTimeOrHoliday(from))
                    {
                        mEventDates += from.date();
                        if (mEventDates.count() >= NUMDAYS)
                            break;   // all days have alarms due
                    }

                    // If the alarm recurs more than once per day, don't waste
                    // time checking any more occurrences for the same day.
                    from.setTime(QTime(23,59,0));
                }
                if (mEventDates.count() >= NUMDAYS)
                    break;   // all days have alarms due
            }
        }
        if (mEventDates.count() >= NUMDAYS)
            break;   // all days have alarms due
    }

    mPendingChanges = false;
}

/******************************************************************************
* Return the holiday description (if any) for a date.
*/
QString DayMatrix::getHolidayLabel(int offset) const
{
    if (offset < 0 || offset > NUMDAYS - 1)
        return {};
    return mHolidays[offset];
}

/******************************************************************************
* Determine the day index at a geometric position.
* Return = NO_SELECTION if outside the widget, or if the date is earlier than today.
*/
int DayMatrix::getDayIndex(const QPoint& pt) const
{
    const int x = pt.x();
    const int y = pt.y();
    if (x < 0  ||  y < 0  ||  x > width()  ||  y > height())
        return NO_SELECTION;
    const int xd = static_cast<int>(x / mDaySize.width());
    const int i = 7 * int(y / mDaySize.height())
                + (QApplication::isRightToLeft() ? 6 - xd : xd);
    if (i < mTodayIndex  ||  i > NUMDAYS-1)
        return NO_SELECTION;
    return i;
}

void DayMatrix::setRowHeight(int rowHeight)
{
    mRowHeight = rowHeight;
    setMinimumSize(minimumWidth(), mRowHeight * NUMROWS + TODAY_MARGIN_WIDTH*2);
}

/******************************************************************************
* Called when the events in a resource have been updated.
* Re-evaluate all events in the resource.
*/
void DayMatrix::resourceUpdated(Resource&)
{
    mPendingChanges = true;
    updateView();    //TODO: only update this resource's events
}

/******************************************************************************
* Called when a resource has been removed.
* Remove all its events from the view.
*/
void DayMatrix::resourceRemoved(ResourceId)
{
    mPendingChanges = true;
    updateView();    //TODO: only remove this resource's events
}

/******************************************************************************
* Called when the holiday or work time settings have changed.
* Re-evaluate all events in the view.
*/
void DayMatrix::slotUpdateView()
{
    mPendingChanges = true;
    updateView();
}

// ----------------------------------------------------------------------------
//  M O U S E   E V E N T   H A N D L I N G
// ----------------------------------------------------------------------------

bool DayMatrix::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        // Tooltip event: show the holiday name.
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        const int i = getDayIndex(helpEvent->pos());
        const QString tipText = getHolidayLabel(i);
        if (!tipText.isEmpty())
            QToolTip::showText(helpEvent->globalPos(), tipText);
        else
            QToolTip::hideText();
    }
    return QWidget::event(event);
}

void DayMatrix::mousePressEvent(QMouseEvent* e)
{
    int i = getDayIndex(e->pos());
    if (i < 0)
    {
        mSelInit = NO_SELECTION;   // invalid: it's not in the matrix or it's before today
        setMouseSelection(NO_SELECTION, NO_SELECTION, true);
        return;
    }
    if (e->button() == Qt::RightButton)
    {
        if (i < mSelStart  ||  i > mSelEnd)
            setMouseSelection(i, i, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        popupMenu(e->globalPosition().toPoint());
#else
        popupMenu(e->globalPos());
#endif
    }
    else if (e->button() == Qt::LeftButton)
    {
        if (i >= mSelStart  &&  i <= mSelEnd)
        {
            mSelInit = NO_SELECTION;   // already selected: cancel the current selection
            setMouseSelection(NO_SELECTION, NO_SELECTION, true);
            return;
        }
        mSelInit = i;
        setMouseSelection(i, i, false);   // don't emit signal until mouse move has completed
    }
}

void DayMatrix::popupMenu(const QPoint& pos)
{
    NewAlarmAction newAction(false, QString(), nullptr);
    QMenu* popup = newAction.menu();
    connect(&newAction, &NewAlarmAction::selected, this, &DayMatrix::newAlarm);
    connect(&newAction, &NewAlarmAction::selectedTemplate, this, &DayMatrix::newAlarmFromTemplate);
    popup->exec(pos);
}

void DayMatrix::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;

    if (mSelInit < 0)
        return;
    int i = getDayIndex(e->pos());
    if (i < 0)
    {
        // Emit signal after move (without changing the selection).
        setMouseSelection(mSelStart, mSelEnd, true);
        return;
    }

    setMouseSelection(mSelInit, i, true);
}

void DayMatrix::mouseMoveEvent(QMouseEvent* e)
{
    if (mSelInit < 0)
        return;
    int i = getDayIndex(e->pos());
    setMouseSelection(mSelInit, i, false);   // don't emit signal until mouse move has completed
}

/******************************************************************************
* Set the current day selection, and update the display.
* Note that the selection may extend past the end of the current matrix.
*/
void DayMatrix::setMouseSelection(int start, int end, bool emitSignal)
{
    if (!mAllowMultipleSelection)
        start = end;
    if (end < start)
        std::swap(start, end);
    if (start != mSelStart  ||  end != mSelEnd)
    {
        mSelStart = start;
        mSelEnd   = end;
        if (mSelStart < 0  ||  mSelEnd < 0)
            mSelStart = mSelEnd = NO_SELECTION;
        update();
    }

    if (emitSignal)
    {
        const QVector<QDate> dates = selectedDates();
        if (dates != mLastSelectedDates)
        {
            mLastSelectedDates = dates;
            Q_EMIT selected(dates);
        }
    }
}

/******************************************************************************
* Called to paint the widget.
*/
void DayMatrix::paintEvent(QPaintEvent*)
{
    QPainter p;
    const QRect rect = frameRect();
    const double dayHeight = mDaySize.height();
    const double dayWidth  = mDaySize.width();
    const bool isRTL = QApplication::isRightToLeft();

    const QPalette pal = palette();

    p.begin(this);

    // Draw the background
    p.fillRect(0, 0, rect.width(), rect.height(), QBrush(pal.color(QPalette::Base)));

    // Draw the frame
    p.setPen(pal.color(QPalette::Mid));
    p.drawRect(0, 0, rect.width() - 1, rect.height() - 1);
    p.translate(1, 1);    // don't paint over borders

    // Draw the background colour for all days not in the selected month.
    const QColor GREY_COLOUR(pal.color(QPalette::AlternateBase));
    if (mMonthStartIndex >= 0)
        colourBackground(p, GREY_COLOUR, 0, mMonthStartIndex - 1);
    colourBackground(p, GREY_COLOUR, mMonthEndIndex + 1, NUMDAYS - 1);

    // Draw the background colour for all selected days.
    if (mSelStart != NO_SELECTION)
    {
        const QColor SELECTION_COLOUR(pal.color(QPalette::Highlight));
        colourBackground(p, SELECTION_COLOUR, mSelStart, mSelEnd);
    }

    // Find holidays which are non-work days.
    QSet<QDate> nonWorkHolidays;
    {
      const KHolidays::HolidayRegion &region = Preferences::holidays();
      const KHolidays::Holiday::List list = region.rawHolidaysWithAstroSeasons(
          mStartDate, mStartDate.addDays(NUMDAYS - 1));
      for (const KHolidays::Holiday &holiday : list)
        if (holiday.dayType() == KHolidays::Holiday::NonWorkday)
          nonWorkHolidays += holiday.observedStartDate();
    }
    const QBitArray workDays = Preferences::workDays();

    // Draw the day label for each day in the matrix.
    TextColours textColours(pal);
    const QFont savedFont = font();
    QColor lastColour;
    for (int i = 0; i < NUMDAYS; ++i)
    {
        const int row    = i / 7;
        const int column = isRTL ? 6 - (i - row * 7) : i - row * 7;

        const bool nonWorkDay = (i >= mTodayIndex) && (!workDays[mStartDate.addDays(i).dayOfWeek()-1] || nonWorkHolidays.contains(mStartDate.addDays(i)));

        const QColor colour = textColour(textColours, pal, i, !nonWorkDay);
        if (colour != lastColour)
        {
            lastColour = colour;
            p.setPen(colour);
        }

        if (mTodayIndex == i)
       	{
            // Draw a rectangle round today.
            const QPen savedPen = p.pen();
            QPen todayPen = savedPen;
            todayPen.setWidth(TODAY_MARGIN_WIDTH);
            p.setPen(todayPen);
            p.drawRect(QRectF(column * dayWidth, row * dayHeight, dayWidth, dayHeight));
            p.setPen(savedPen);
        }

        // If any events occur on the day, draw it in bold
        const bool hasEvent = mEventDates.contains(mStartDate.addDays(i));
        if (hasEvent)
       	{
            QFont evFont = savedFont;
            evFont.setWeight(QFont::Black);
            evFont.setPointSize(evFont.pointSize() + 1);
            evFont.setStretch(110);
            p.setFont(evFont);
        }

        p.drawText(QRectF(column * dayWidth, row * dayHeight, dayWidth, dayHeight),
                   Qt::AlignHCenter | Qt::AlignVCenter, mDayLabels.at(i));

        if (hasEvent)
            p.setFont(savedFont);   // restore normal font
    }
    p.end();
}

/******************************************************************************
* Paint a background colour for a range of days.
*/
void DayMatrix::colourBackground(QPainter& p, const QColor& colour, int start, int end)
{
    if (end < 0)
        return;
    if (start < 0)
        start = 0;
    const int row = start / 7;
    if (row >= NUMROWS)
        return;
    const int column = start - row * 7;

    const double dayHeight = mDaySize.height();
    const double dayWidth  = mDaySize.width();
    const bool isRTL = QApplication::isRightToLeft();

    if (row == end / 7)
    {
        // Single row to highlight.
        p.fillRect(QRectF((isRTL ? (7 - (end - start + 1) - column) : column) * dayWidth,
                          row * dayHeight,
                          (end - start + 1) * dayWidth - 2,
                          dayHeight),
                   colour);
    }
    else
    {
        // Draw first row, to the right of the start day.
        p.fillRect(QRectF((isRTL ? 0 : column * dayWidth), row * dayHeight,
                          (7 - column) * dayWidth - 2, dayHeight),
                   colour);
        // Draw full block till last line
        int selectionHeight = end / 7 - row;
        if (selectionHeight + row >= NUMROWS)
            selectionHeight = NUMROWS - row;
        if (selectionHeight > 1)
            p.fillRect(QRectF(0, (row + 1) * dayHeight,
                              7 * dayWidth - 2, (selectionHeight - 1) * dayHeight),
                       colour);
        // Draw last row, to the left of the end day.
        if (end / 7 < NUMROWS)
        {
            const int selectionWidth = end - 7 * (end / 7) + 1;
            p.fillRect(QRectF((isRTL ? (7 - selectionWidth) * dayWidth : 0),
                              (row + selectionHeight) * dayHeight,
                              selectionWidth * dayWidth - 2, dayHeight),
                       colour);
        }
    }
}

/******************************************************************************
* Called when the widget is resized. Set the size of each date in the matrix.
*/
void DayMatrix::resizeEvent(QResizeEvent*)
{
    const QRect sz = frameRect();
    const int padding = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2;
    mDaySize.setHeight((sz.height() - padding) * 7.0 / NUMDAYS);
    mDaySize.setWidth(sz.width() / 7.0);
}

/******************************************************************************
* Evaluate the text color to show a given date.
*/
QColor DayMatrix::textColour(const TextColours& textColours, const QPalette& palette, int dayIndex, bool workDay) const
{
    if (dayIndex >= mSelStart  &&  dayIndex <= mSelEnd)
    {
        if (dayIndex == mTodayIndex)
            return QColor(QStringLiteral("lightgrey"));
        if (workDay)
            return palette.color(QPalette::HighlightedText);
    }
    if (dayIndex < mTodayIndex)
        return textColours.disabled;
    if (dayIndex >= mMonthStartIndex  &&  dayIndex <= mMonthEndIndex)
        return workDay ? textColours.thisMonth : textColours.thisMonthHoliday;
    else
        return workDay ? textColours.otherMonth : textColours.otherMonthHoliday;
}

/*===========================================================================*/

TextColours::TextColours(const QPalette& palette)
{
    thisMonth         = palette.color(QPalette::Text);
    disabled          = getShadedColour(thisMonth, false);
    otherMonth        = getShadedColour(thisMonth, true);
    thisMonthHoliday  = thisMonth;
    thisMonthHoliday.setRed((thisMonthHoliday.red() + 255) / 2);
    otherMonthHoliday = getShadedColour(thisMonthHoliday, true);
}

QColor TextColours::getShadedColour(const QColor& colour, bool enabled) const
{
    QColor shaded;
    int h = 0;
    int s = 0;
    int v = 0;
    colour.getHsv(&h, &s, &v);
    s = s / (enabled ? 2 : 4);
    v = enabled ? (4*v + 5*255) / 9 : (v + 5*255) / 6;
    shaded.setHsv(h, s, v);
    return shaded;
}

// vim: et sw=4:
