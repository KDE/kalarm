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

#ifndef AKONADI_SINGLEFILERESOURCECONFIGDIALOGBASE_H
#define AKONADI_SINGLEFILERESOURCECONFIGDIALOGBASE_H

#include "ui_singlefileresourceconfigdialog.h"

#include <KDE/KDialog>
#include <KDE/KUrl>

class KTabWidget;
class KConfigDialogManager;

namespace Akonadi {

class SingleFileResourceConfigWidget;

/**
 * Base class for the configuration dialog for single file based resources.
 * @see SingleFileResourceConfigDialog
 */
class SingleFileResourceConfigDialogBase : public KDialog
{
    Q_OBJECT
  public:
    explicit SingleFileResourceConfigDialogBase( WId windowId );

    /**
     * Adds @param page to the tabwidget. This can be used to add custom
     * settings for a specific single file resource.
     */
    void addPage( const QString &title, QWidget *page );

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
     * Add a widget to the SingleFileResourceConfigWidget.
     */
    void appendToConfigWidget(QWidget* widget);

  protected Q_SLOTS:
    virtual void save() = 0;

  protected:
    Ui::SingleFileResourceConfigDialog ui;
    KConfigDialogManager* mManager;

  private Q_SLOTS:
    void validated( bool ok );

  private:
    SingleFileResourceConfigWidget* mConfigWidget;
};

}

#endif
