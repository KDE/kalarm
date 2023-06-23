/*
 *  resourcecreator.cpp  -  base class to interactively create a resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resourcecreator.h"

#include <QTimer>

using namespace KAlarmCal;


ResourceCreator::ResourceCreator(CalEvent::Type defaultType, QWidget* parent)
    : QObject()
    , mParent(parent)
    , mDefaultType(defaultType)
{
}

/******************************************************************************
* Create a new resource. The user will be prompted to enter its configuration.
*/
void ResourceCreator::createResource()
{
    QTimer::singleShot(0, this, &ResourceCreator::doCreateResource);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

#include "moc_resourcecreator.cpp"

// vim: et sw=4:
