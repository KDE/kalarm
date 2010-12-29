/*
    Copyright (c) 2008 Bertjan Broeksema <b.broeksema@kdemail.org>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010 David Jarvie <djarvie@kde.org>

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

#include "singlefileresourceconfigdialogbase.h"
#include "singlefileresourceconfigwidget.h"

#include <KTabWidget>
#include <KConfigDialogManager>
#include <KWindowSystem>

using namespace Akonadi;

SingleFileResourceConfigDialogBase::SingleFileResourceConfigDialogBase( WId windowId ) :
    KDialog(),
    mConfigWidget( new SingleFileResourceConfigWidget(this) )
{
  ui.setupUi( mainWidget() );

  ui.ktabwidget->addTab( mConfigWidget, i18n( "File" ) );

  setButtons( Ok | Cancel );
  
  if ( windowId )
    KWindowSystem::setMainWindow( this, windowId );

  ui.ktabwidget->setTabBarHidden( true );

  connect( mConfigWidget, SIGNAL(validated(bool)), SLOT(validated(bool)));
  connect( this, SIGNAL(okClicked()), SLOT(save()) );
}

void SingleFileResourceConfigDialogBase::addPage( const QString &title, QWidget *page )
{
  ui.ktabwidget->setTabBarHidden( false );
  ui.ktabwidget->addTab( page, title );
  mManager->addWidget( page );
  mManager->updateWidgets();
}

void SingleFileResourceConfigDialogBase::setFilter(const QString & filter)
{
  mConfigWidget->setFilter( filter );
}

void SingleFileResourceConfigDialogBase::setMonitorEnabled(bool enable)
{
  mConfigWidget->setMonitorEnabled( enable );
}

void SingleFileResourceConfigDialogBase::setUrl(const KUrl &url )
{
  mConfigWidget->setUrl( url );
}

KUrl SingleFileResourceConfigDialogBase::url() const
{
  return mConfigWidget->url();
}

void SingleFileResourceConfigDialogBase::setLocalFileOnly( bool local )
{
  mConfigWidget->setLocalFileOnly( local );
}

void SingleFileResourceConfigDialogBase::validated( bool ok )
{
  enableButton( Ok, ok );
}
