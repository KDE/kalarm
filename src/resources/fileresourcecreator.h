/*
 *  fileresourcecreator.h  -  interactively create a file system resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "resourcecreator.h"
#include "kalarmcalendar/kacalendar.h"

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

// vim: et sw=4:
