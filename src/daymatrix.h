/*
 *  daymatrix.h  -  calendar day matrix display
 *  Program:  kalarm
 *  This class is adapted from KODayMatrix in KOrganizer.
 *
 *  SPDX-FileCopyrightText: 2001 Eitzenberger Thomas <thomas.eitzenberger@siemens.at>
 *  SPDX-FileCopyrightText: 2003 Cornelius Schumacher <schumacher@kde.org>
 *  SPDX-FileCopyrightText: 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
 *  SPDX-FileCopyrightText: 2021-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later WITH Qt-Commercial-exception-1.0
*/

#pragma once

#include "editdlg.h"
#include "kalarmcalendar/kaevent.h"

#include <QFrame>
#include <QDate>
#include <QSet>

class Resource;
namespace { struct TextColours; }

/**
 *  Displays one month's dates in a grid, one line per week, highlighting days
 *  on which alarms occur. It has an option to allow one or more consecutive
 *  days to be selected by dragging the mouse. Days before today are disabled.
 */
class DayMatrix : public QFrame
{
    Q_OBJECT
public:
    /** constructor to create a day matrix widget.
     *
     *  @param parent widget that is the parent of the day matrix.
     *  Normally this should be a KDateNavigator
     */
    explicit DayMatrix(QWidget* parent = nullptr);

    /** destructor that deallocates all dynamically allocated private members.
     */
    ~DayMatrix() override;

    /** Set a new start date for the matrix. If changed, or other changes are
     *  pending, recalculates which days in the matrix alarms occur on, and
     *  which are holidays/non-work days, and repaints.
     *
     *  @param startDate  The first day to be displayed in the matrix.
     */
    void setStartDate(const QDate& startDate);

    /** Notify the matrix that the current date has changed.
     *  The month currently being displayed will not be changed.
     */
    void updateToday(const QDate& newDate);

    /** Returns all selected dates, in date order. */
    QVector<QDate> selectedDates() const;

    /** Clear all selections. */
    void clearSelection();

    void setRowHeight(int rowHeight);

Q_SIGNALS:
    /** Emitted when the user selects or deselects dates.
     *
     *  @param dates       The dates selected, in date order, or empty if none.
     *  @param workChange  The holiday region or work days have changed.
     */
    void selected(const QVector<QDate>& dates, bool workChange);

    void newAlarm(EditAlarmDlg::Type);
    void newAlarmFromTemplate(const KAlarmCal::KAEvent&);

protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private Q_SLOTS:
    void resourceUpdated(Resource&);
    void resourceRemoved(KAlarmCal::ResourceId);
    void resourceSettingsChanged(Resource&, ResourceType::Changes);
    void slotUpdateView();

private:
    QString getHolidayLabel(int offset) const;
    void setMouseSelection(int start, int end, bool emitSignal);
    void popupMenu(const QPoint&);     // pop up a context menu for creating a new alarm
    int getDayIndex(const QPoint&) const;   // get index of the day located at a point in the matrix

    // If changes are pending, recalculates which days in the matrix have
    // alarms occurring, and which are holidays/non-work days, and repaints.
    void updateView(const Resource& = Resource());
    void updateEvents(const Resource& = Resource());
    void updateEvents(const Resource&, const KADateTime& before, const KADateTime& to);
    void colourBackground(QPainter&, const QColor&, int start, int end);
    QColor textColour(const TextColours&, const QPalette&, int dayIndex, bool workDay) const;

    int     mRowHeight {1};    // height of each row
    QDate   mStartDate;        // starting date of the matrix

    QVector<QString> mDayLabels;  // array of day labels, to optimize drawing performance

    QSet<QDate> mEventDates;   // days on which alarms occur, for any resource
    QHash<ResourceId, QSet<QDate>> mResourceEventDates;  // for each resource, days on which alarms occur

    QStringList mHolidays;     // holiday names, indexed by day index

    int    mTodayIndex {-1};   // index of today, or -1 if today is not visible in the matrix
    int    mMonthStartIndex;   // index of the first day of the main month shown
    int    mMonthEndIndex;     // index of the last day of the main month shown

    int    mSelInit;           // index of day where dragged selection was initiated
    int    mSelStart;          // index of the first selected day
    int    mSelEnd;            // index of the last selected day
    QVector<QDate> mLastSelectedDates; // last dates emitted in selected() signal

    QRectF mDaySize;                  // the geometric size of each day in the matrix

    bool   mAllowMultipleSelection {false};  // selection may contain multiple days
    bool   mSelectionMustBeVisible {true};   // selection will be cancelled if not wholly visible
    bool   mPendingChanges {false};   // the display needs to be updated
};

// vim: et sw=4:
