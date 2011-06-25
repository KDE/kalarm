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

#ifndef AKONADI_SINGLEFILERESOURCECONFIGWIDGET_H
#define AKONADI_SINGLEFILERESOURCECONFIGWIDGET_H

#ifdef KDEPIM_MOBILE_UI
#include "ui_singlefileresourceconfigwidget_mobile.h"
#else
#include "ui_singlefileresourceconfigwidget_desktop.h"
#endif

#include <QWidget>

class KJob;

namespace KIO {
class StatJob;
}

namespace Akonadi {

/**
 * Base class for the configuration widget for single file based resources.
 * @see SingleFileResourceConfigDialog
 */
class SingleFileResourceConfigWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit SingleFileResourceConfigWidget( QWidget *parent );

    /**
     * Set file extension filter.
     */
    void setFilter( const QString &filter );

    /**
     * Enable and show, or disable and hide, the monitor option.
     * If the option is disabled, its value will not be saved.
     * By default, the monitor option is enabled.
     */
    void setMonitorEnabled( bool enable );

    /**
     * Return the file URL.
     */
    KUrl url() const;

    /**
     * Set the file URL.
     */
    void setUrl( const KUrl& url );

    /**
     * Specify whether the file must be local.
     * The default is to allow both local and remote files.
     */
    void setLocalFileOnly( bool local );

    /**
     * Append an extra widget to the bottom of this widget.
     */
    void appendWidget( QWidget* );

  signals:
    /**
     * Signal emitted when the user input has been validated.
     * @param ok true if valid, false if invalid or waiting for job to finish.
     */
    void validated( bool ok );

  protected:
    Ui::SingleFileResourceConfigWidget ui;

  private:
    KIO::StatJob* mStatJob;
    bool mDirUrlChecked;
    bool mMonitorEnabled;
    bool mLocalFileOnly;

  private Q_SLOTS:
    void validate();
    void slotStatJobResult( KJob * );
};

}

#endif
