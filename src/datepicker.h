/*
 *  datepicker.h  -  date chooser widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2021-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "editdlg.h"

#include <QWidget>
#include <QDate>

class DPToolButton;
class QLabel;
class DayMatrix;


/**
 *  Displays the calendar for a month, to allow the user to select days.
 *  Dates before today are disabled.
 */
class DatePicker : public QWidget
{
    Q_OBJECT
public:
    explicit DatePicker(QWidget* parent = nullptr);
    ~DatePicker() override;

    /** Return the currently selected dates, if any. */
    QList<QDate> selectedDates() const;

    /** Deselect all dates. */
    void clearSelection();

Q_SIGNALS:
     /** Emitted when the user selects or deselects dates.
      *
     *  @param dates       The dates selected, in date order, or empty if none.
     *  @param workChange  The holiday region or work days have changed.
     */
    void datesSelected(const QList<QDate>& dates, bool workChange);

protected:
    void showEvent(QShowEvent*) override;

private Q_SLOTS:
    void prevYearClicked();
    void prevMonthClicked();
    void nextYearClicked();
    void nextMonthClicked();
    void todayClicked();
    void updateToday();
    void slotNewAlarm(EditAlarmDlg::Type);
    void slotNewAlarmFromTemplate(const KAEvent&);

private:
    void newMonthShown();
    void updateDisplay();
    DPToolButton* createArrowButton(const QString& iconId);

    DPToolButton* mPrevYear;
    DPToolButton* mPrevMonth;
    DPToolButton* mNextYear;
    DPToolButton* mNextMonth;
    DPToolButton* mToday;
    QLabel*       mMonthYear;
    QLabel*       mDayNames;
    DayMatrix*    mDayMatrix;
    QDate         mMonthShown;     // 1st of month currently displayed
    QDate         mStartDate;      // earliest date currently displayed
};

// vim: et sw=4:
