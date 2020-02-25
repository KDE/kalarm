/*
 *  resourceselectdialog.h  -  Resource selection dialog
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#ifndef RESOURCESELECTDIALOG_H
#define RESOURCESELECTDIALOG_H

#include "resource.h"

#include <QDialog>

class ResourceListModel;
class QListView;
class QDialogButtonBox;

/**
 * @short Resource selection dialog.
 *
 * Provides a dialog that shows a list of resources, from which the user can
 * select one.
 *
 * A text box lets the user filter the displayed resources based on a search text.
 */
class ResourceSelectDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * Constructor.
     * @param model  The model which provides the resource list to select from.
     * @param parent The parent widget.
     */
    explicit ResourceSelectDialog(ResourceListModel* model, QWidget* parent = nullptr);

    ~ResourceSelectDialog();

    /** Set the initial resource to select. */
    void setDefaultResource(const Resource&);

    /** Return the selected resource, or invalid if nothing selected. */
    Resource selectedResource() const;

private Q_SLOTS:
    void slotFilterText(const QString& text);
    void slotSelectionChanged();
    void slotDoubleClicked();

private:
    ResourceListModel* mModel;
    QListView*         mResourceList {nullptr};
    QDialogButtonBox*  mButtonBox {nullptr};
    ResourceId         mDefaultId {-1};
    CalEvent::Types    mAlarmTypes {CalEvent::EMPTY};
    bool               mWritable {false};
};

#endif // RESOURCESELECTDIALOG_H

// vim: et sw=4:
