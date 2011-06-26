/*
    Copyright (c) 2008 Bertjan Broeksema <b.broeksema@kdemail.org>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#ifndef AKONADI_SINGLEFILERESOURCECONFIGDIALOG_H
#define AKONADI_SINGLEFILERESOURCECONFIGDIALOG_H

#include "singlefileresourceconfigdialogbase.h"

#include <KDE/KConfigDialogManager>

namespace Akonadi {

/**
 * Configuration dialog for single file resources.
 */
template <typename Settings>
class SingleFileResourceConfigDialog : public SingleFileResourceConfigDialogBase
{
  Settings *mSettings;

  public:
    explicit SingleFileResourceConfigDialog( WId windowId, Settings *settings )
        : SingleFileResourceConfigDialogBase( windowId )
        , mSettings( settings )
    {
      ui.kcfg_Path->setUrl( KUrl( mSettings->path() ) );
      mManager = new KConfigDialogManager( this, mSettings );
      mManager->updateWidgets();
    }

  protected:
    void save()
    {
      mManager->updateSettings();
      mSettings->setPath( ui.kcfg_Path->url().url() );
      mSettings->writeConfig();
    }
};

}

#endif
