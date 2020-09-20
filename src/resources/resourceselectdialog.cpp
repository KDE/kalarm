/*
 *  resourceselectdialog.cpp  -  Resource selection dialog
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resourceselectdialog.h"

#include "resourcemodel.h"
#include "lib/config.h"

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
    if (Config::readWindowSize(DialogName, s))
        resize(s);
}

ResourceSelectDialog::~ResourceSelectDialog()
{
    Config::writeWindowSize(DialogName, size());
}

void ResourceSelectDialog::setDefaultResource(const Resource& resource)
{
    const QModelIndex index = mModel->resourceIndex(resource);
    mResourceList->selectionModel()->select(index, QItemSelectionModel::SelectCurrent);
}

Resource ResourceSelectDialog::selectedResource() const
{
    const QModelIndexList indexes = mResourceList->selectionModel()->selectedRows();    //clazy:exclude=inefficient-qlist
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
