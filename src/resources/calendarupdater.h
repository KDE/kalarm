/*
 *  calendarupdater.h  -  base class to update a calendar to current KAlarm format
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "resource.h"


// Updates the backend calendar format of a single alarm calendar
class CalendarUpdater : public QObject
{
    Q_OBJECT
public:
    CalendarUpdater(ResourceId, bool ignoreKeepFormat, QObject* parent, QWidget* promptParent = nullptr);
    ~CalendarUpdater() override;

    /** Check whether any instance is for the given resource ID. */
    static bool containsResource(ResourceId);

    /** Return whether another instance is already updating this collection. */
    bool isDuplicate() const   { return mDuplicate; }

    /** Return whether this instance has completed, and its deletion is pending. */
    bool isComplete() const     { return mCompleted; }

    static bool pending()   { return !mInstances.isEmpty(); }

    /** Wait until all completed instances have completed and been deleted. */
    static void waitForCompletion();

#if 0
    /** If an existing resource calendar can be converted to the current KAlarm
     *  format, prompt the user whether to convert it, and if yes, tell the resource
     *  to update the backend storage to the current format.
     *  The resource's KeepFormat property will be updated if the user chooses not to
     *  update the calendar.
     *  This method should call update() on a single shot timer to prompt the
     *  user and convert the calendar.
     *  @param  parent  Parent object. If possible, this should be a QWidget.
     */
    static void updateToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent);
#endif

public Q_SLOTS:
    /** If the calendar is not in the current KAlarm format, prompt the user
     *  whether to convert to the current format, and then perform the conversion.
     *  This method must call deleteLater() on completion.
     *  @return  false if the calendar is not in current format and the user
     *           chose not to update it; true otherwise.
     */
    virtual bool update() = 0;

protected:
    /** Mark the instance as completed, and schedule its deletion. */
    void setCompleted();

    static QString conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole);

    static QVector<CalendarUpdater*> mInstances;
    ResourceId mResourceId;
    QObject*   mParent;
    QWidget*   mPromptParent;
    const bool mIgnoreKeepFormat;
    const bool mDuplicate;          // another instance is already updating this resource
    bool       mCompleted {false};  // completed, and deleteLater() called
};

// vim: et sw=4:
