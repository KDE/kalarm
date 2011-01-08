/*
 *  settingsdialog.cpp  -  Akonadi KAlarm directory resource configuration dialog
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "settingsdialog.h"
#include "settings.h"

#include <KConfigDialogManager>
#include <KWindowSystem>

#include <QTimer>

//using namespace Akonadi;

namespace Akonadi_KAlarm_Dir_Resource
{

SettingsDialog::SettingsDialog( WId windowId, Settings* settings )
  : KDialog(),
    mSettings(settings)
{
  ui.setupUi( mainWidget() );
  ui.ktabwidget->setTabBarHidden(true);
  ui.kcfg_Path->setMode( KFile::LocalOnly | KFile::Directory );
  setButtons( Ok | Cancel );
  setCaption( i18nc("@title", "Configure Calendar") );

  if ( windowId )
    KWindowSystem::setMainWindow( this, windowId );

  connect( this, SIGNAL( okClicked() ), SLOT( save() ) );

  connect( ui.kcfg_Path, SIGNAL( textChanged( QString ) ), SLOT( validate() ) );
  connect( ui.kcfg_ReadOnly, SIGNAL( toggled( bool ) ), SLOT( validate() ) );

  QTimer::singleShot( 0, this, SLOT( validate() ) );

  ui.kcfg_Path->setUrl( KUrl( mSettings->path() ) );
  mManager = new KConfigDialogManager( this, mSettings );
  mManager->updateWidgets();
}

void SettingsDialog::save()
{
  mManager->updateSettings();
  mSettings->setPath( ui.kcfg_Path->url().toLocalFile() );
  mSettings->writeConfig();
}

void SettingsDialog::validate()
{
  bool enableOk = false;
  const KUrl currentUrl = ui.kcfg_Path->url();
  // The entered URL must be valid and local
  if ( !currentUrl.isEmpty() && currentUrl.isLocalFile() ) {
    const QFileInfo file( currentUrl.toLocalFile() );
    // It must either be a directory, or not yet exist
    if ( !file.exists() || file.isDir() ) {
      if ( file.exists() && !file.isWritable() ) {
        ui.kcfg_ReadOnly->setEnabled( false );
        ui.kcfg_ReadOnly->setChecked( true );
      } else {
        ui.kcfg_ReadOnly->setEnabled( true );
      }
      enableOk = true;
    }
  }
  enableButton( Ok, enableOk );
}

}
