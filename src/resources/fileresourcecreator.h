/*
 *  fileresourcecreator.h  -  interactively create a file system resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILERESOURCECREATOR_H
#define FILERESOURCECREATOR_H

#include "resourcecreator.h"

#include <KAlarmCal/KACalendar>

class FileResourceCreator : public ResourceCreator
{
    Q_OBJECT
public:
    FileResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);

protected Q_SLOTS:
    void doCreateResource() override;

private:
    bool createSingleFileResource();
};

#endif // FILERESOURCECREATOR_H

// vim: et sw=4:
