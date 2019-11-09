/*
 *  resourceselectdialog.cpp  -  Resource selection dialog
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

#include "resourceselectdialog.h"

#include "resourcemodel.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>

namespace
{
static const char DialogName[] = "ResourceSelectDialog";
}


ResourceSelectDialog::ResourceSelectDialog(ResourceListModel* model, QWidget* parent)
    : QDialog(parent)
    , mModel(model)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLineEdit* filterEdit = new QLineEdit(this);
    filterEdit->setClearButtonEnabled(true);
    filterEdit->setPlaceholderText(i18nc("@info A prompt for user to enter what to search for", "Search"));
    layout->addWidget(filterEdit);

    mResourceList = new QListView(this);
    mResourceList->setModel(model);
    layout->addWidget(mResourceList);

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(mButtonBox);
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(filterEdit, &QLineEdit::textChanged, this, &ResourceSelectDialog::slotFilterText);
    connect(mResourceList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ResourceSelectDialog::slotSelectionChanged);
    connect(mResourceList, &QAbstractItemView::doubleClicked, this, &ResourceSelectDialog::slotDoubleClicked);

    QSize s;
    if (KAlarm::readConfigWindowSize(DialogName, s))
        resize(s);
}

ResourceSelectDialog::~ResourceSelectDialog()
{
    KAlarm::writeConfigWindowSize(DialogName, size());
}

void ResourceSelectDialog::setDefaultResource(const Resource& resource)
{
    const QModelIndex index = mModel->resourceIndex(resource);
    mResourceList->selectionModel()->select(index, QItemSelectionModel::SelectCurrent);
}

Resource ResourceSelectDialog::selectedResource() const
{
    const QModelIndexList indexes = mResourceList->selectionModel()->selectedRows();
    if (indexes.isEmpty())
        return Resource();
    return mModel->resource(indexes[0].row());
}

void ResourceSelectDialog::slotFilterText(const QString& text)
{
    mModel->setFilterText(text);
}

void ResourceSelectDialog::slotDoubleClicked()
{
    if (!mResourceList->selectionModel()->selectedIndexes().isEmpty())
        accept();
}

void ResourceSelectDialog::slotSelectionChanged()
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(!mResourceList->selectionModel()->selectedIndexes().isEmpty());
}

// vim: et sw=4:
