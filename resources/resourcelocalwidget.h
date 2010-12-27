/*
 *  resourcelocalwidget.h  -  configuration widget for local file calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef RESOURCELOCALWIDGET_H
#define RESOURCELOCALWIDGET_H

/* @file resourcelocalwidget.h - configuration widget for local file calendar resource */

#include "resourcewidget.h"
#include "kalarm_resources_export.h"

class KUrlRequester;

/**
  Configuration widget for local file alarm calendar resource.
  @see KAResourceLocal
*/
class KALARM_RESOURCES_EXPORT ResourceLocalConfigWidget : public ResourceConfigWidget
{
        Q_OBJECT
    public:
        explicit ResourceLocalConfigWidget(QWidget* parent = 0);

    public slots:
        virtual void loadSettings(KRES::Resource*);
        virtual void saveSettings(KRES::Resource*);

    private:
        KUrlRequester* mURL;
};

#endif

// vim: et sw=4:
