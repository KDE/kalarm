/*
    Copyright (c) 2008 Bertjan Broeksema <b.broeksema@kdemail.org>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010,2011 David Jarvie <djarvie@kde.org>

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

#include <KTabWidget>
#include <KConfigDialogManager>
#include <KFileItem>
#include <KIO/Job>
#include <KWindowSystem>

#include <QTimer>

using namespace Akonadi;

SingleFileResourceConfigDialogBase::SingleFileResourceConfigDialogBase( WId windowId ) :
    KDialog(),
    mStatJob( 0 ),
    mAppendedWidget( 0 ),
    mDirUrlChecked( false ),
    mMonitorEnabled( true ),
    mLocalFileOnly( false )
{
  ui.setupUi( mainWidget() );
  ui.kcfg_Path->setMode( KFile::File );
#ifndef KDEPIM_MOBILE_UI
  ui.statusLabel->setText( QString() );
#endif

  setButtons( Ok | Cancel );
  
  if ( windowId )
    KWindowSystem::setMainWindow( this, windowId );

  ui.ktabwidget->setTabBarHidden( true );

  connect( this, SIGNAL(okClicked()), SLOT(save()) );

  connect( ui.kcfg_Path, SIGNAL(textChanged(QString)), SLOT(validate()) );
  connect( ui.kcfg_ReadOnly, SIGNAL(toggled(bool)), SLOT(validate()) );
  connect( ui.kcfg_MonitorFile, SIGNAL(toggled(bool)), SLOT(validate()) );
  ui.kcfg_Path->setFocus();
  QTimer::singleShot( 0, this, SLOT(validate()) );
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
  ui.kcfg_Path->setFilter( filter );
}

void SingleFileResourceConfigDialogBase::setMonitorEnabled(bool enable)
{
  mMonitorEnabled = enable;
#ifdef KDEPIM_MOBILE_UI
  ui.kcfg_MonitorFile->setVisible( mMonitorEnabled );
#else
  ui.groupBox_MonitorFile->setVisible( mMonitorEnabled );
#endif
}

void SingleFileResourceConfigDialogBase::setUrl(const KUrl &url )
{
  ui.kcfg_Path->setUrl( url );
}

KUrl SingleFileResourceConfigDialogBase::url() const
{
  return ui.kcfg_Path->url();
}

void SingleFileResourceConfigDialogBase::setLocalFileOnly( bool local )
{
  mLocalFileOnly = local;
  ui.kcfg_Path->setMode( mLocalFileOnly ? KFile::File | KFile::LocalOnly : KFile::File );
}

void SingleFileResourceConfigDialogBase::appendWidget( SingleFileValidatingWidget* widget )
{
  widget->setParent( static_cast<QWidget*>( ui.tab ) );
  ui.tabLayout->addWidget( widget );
  connect( widget, SIGNAL(changed()), SLOT(validate()) );
  mAppendedWidget = widget;
}

void SingleFileResourceConfigDialogBase::validate()
{
  if ( mAppendedWidget && !mAppendedWidget->validate() ) {
    enableButton( Ok, false );
    return;
  }

  const KUrl currentUrl = ui.kcfg_Path->url();
  if ( currentUrl.isEmpty() ) {
    enableButton( Ok, false );
    return;
  }

  if ( currentUrl.isLocalFile() ) {
    if ( mMonitorEnabled ) {
      ui.kcfg_MonitorFile->setEnabled( true );
    }
#ifndef KDEPIM_MOBILE_UI
    ui.statusLabel->setText( QString() );
#endif

    const QFileInfo file( currentUrl.toLocalFile() );
    if ( file.exists() && file.isFile() && !file.isWritable() ) {
      ui.kcfg_ReadOnly->setEnabled( false );
      ui.kcfg_ReadOnly->setChecked( true );
    } else {
      ui.kcfg_ReadOnly->setEnabled( true );
    }
    enableButton( Ok, true );
  } else {
    if ( mLocalFileOnly ) {
      enableButton( Ok, false );
      return;
    }
    if ( mMonitorEnabled ) {
      ui.kcfg_MonitorFile->setEnabled( false );
    }
#ifndef KDEPIM_MOBILE_UI
    ui.statusLabel->setText( i18nc( "@info:status", "Checking file information..." ) );
#endif

    if ( mStatJob )
      mStatJob->kill();

    mStatJob = KIO::stat( currentUrl, KIO::DefaultFlags | KIO::HideProgressInfo );
    mStatJob->setDetails( 2 ); // All details.
    mStatJob->setSide( KIO::StatJob::SourceSide );

    connect( mStatJob, SIGNAL( result( KJob * ) ),
             SLOT( slotStatJobResult( KJob * ) ) );

    // Allow the OK button to be disabled until the MetaJob is finished.
    enableButton( Ok, false );
  }
}

void SingleFileResourceConfigDialogBase::slotStatJobResult( KJob* job )
{
  if ( job->error() == KIO::ERR_DOES_NOT_EXIST && !mDirUrlChecked ) {
    // The file did not exists, so let's see if the directory the file should
    // reside supports writing.
    const KUrl dirUrl = ui.kcfg_Path->url().upUrl();

    mStatJob = KIO::stat( dirUrl, KIO::DefaultFlags | KIO::HideProgressInfo );
    mStatJob->setDetails( 2 ); // All details.
    mStatJob->setSide( KIO::StatJob::SourceSide );

    connect( mStatJob, SIGNAL( result( KJob * ) ),
             SLOT( slotStatJobResult( KJob * ) ) );

    // Make sure we don't check the whole path upwards.
    mDirUrlChecked = true;
    return;
  } else if ( job->error() ) {
    // It doesn't seem possible to read nor write from the location so leave the
    // ok button disabled
#ifndef KDEPIM_MOBILE_UI
    ui.statusLabel->setText( QString() );
#endif
    enableButton( Ok, false );
    mDirUrlChecked = false;
    mStatJob = 0;
    return;
  }

  KIO::StatJob* statJob = qobject_cast<KIO::StatJob *>( job );
  const KFileItem item( statJob->statResult(), KUrl() );

  if ( item.isWritable() ) {
    ui.kcfg_ReadOnly->setEnabled( true );
  } else {
    ui.kcfg_ReadOnly->setEnabled( false );
    ui.kcfg_ReadOnly->setChecked( true );
  }

#ifndef KDEPIM_MOBILE_UI
  ui.statusLabel->setText( QString() );
#endif
  enableButton( Ok, true );

  mDirUrlChecked = false;
  mStatJob = 0;
}

SingleFileValidatingWidget::SingleFileValidatingWidget( QWidget* parent )
  : QWidget( parent )
{
}
