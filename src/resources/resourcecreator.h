/*
 *  resourcecreator.h  -  base class to interactively create a resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RESOURCECREATOR_H
#define RESOURCECREATOR_H

#include <KAlarmCal/KACalendar>

class QWidget;
class Resource;

/**
 * Base class for interactively creating a resource.
 * Derived classes must call deleteLater() to delete themselves if resource
 * creation is unsuccessful, i.e. if the resourceAdded() signal is not called.
 */
class ResourceCreator : public QObject
{
    Q_OBJECT
public:
    ResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);
    void createResource();

Q_SIGNALS:
    /** Signal emitted when a resource has been created.
     *  @param type  The default alarm type specified in the constructor.
     */
    void resourceAdded(Resource&, KAlarmCal::CalEvent::Type);

protected Q_SLOTS:
    virtual void doCreateResource() = 0;

protected:
    QWidget*                  mParent;
    KAlarmCal::CalEvent::Type mDefaultType;
};

#endif // RESOURCECREATOR_H

// vim: et sw=4:
