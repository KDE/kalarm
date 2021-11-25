/*
 *  resourceselectdialog.h  -  Resource selection dialog
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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

    ~ResourceSelectDialog() override;

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

// vim: et sw=4:
