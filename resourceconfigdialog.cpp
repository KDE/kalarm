/*
 *  resourceconfigdialog.cpp  -  KAlarm resource configuration dialog
 *  Program:  kalarm
 *  Copyright © 2006-2011 by David Jarvie <djarvie@kde.org>
 *  Based on configdialog.cpp in kdelibs/kresources,
 *  Copyright (c) 2002 Tobias Koenig <tokoe@kde.org>
 *  Copyright (c) 2002 Jan-Pascal van Best <janpascal@vanbest.org>
 *  Copyright (c) 2003 Cornelius Schumacher <schumacher@kde.org>
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

#include "alarmresource.h"
#include "resourceconfigdialog.moc"
#include "messagebox.h"

#include <kresources/factory.h>

#include <klocale.h>
#include <klineedit.h>

#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGridLayout>

using namespace KRES;

ResourceConfigDialog::ResourceConfigDialog(QWidget* parent, AlarmResource* resource)
    : KDialog(parent), mResource(resource)
{
    Factory* factory = Factory::self(QLatin1String("alarms"));

    QFrame* main = new QFrame(this);
    setMainWidget(main);
    setCaption(i18nc("@title:window", "Calendar Configuration"));
    setButtons(Ok | Cancel);
    setDefaultButton(Ok);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(main);
    mainLayout->setSpacing(spacingHint());

    QGroupBox* generalGroupBox = new QGroupBox(main);
    QGridLayout* gbLayout = new QGridLayout;
    gbLayout->setSpacing(spacingHint());
    generalGroupBox->setLayout(gbLayout);

    generalGroupBox->setTitle(i18nc("@title:group", "General Settings"));

    gbLayout->addWidget(new QLabel(i18nc("@label:textbox Calendar name", "Name:"), generalGroupBox), 0, 0);

    mName = new KLineEdit();
    gbLayout->addWidget(mName, 0, 1);

    mReadOnly = new QCheckBox(i18nc("@option:check", "Read-only"), generalGroupBox);
    gbLayout->addWidget(mReadOnly, 1, 0, 1, 2);

    mName->setText(mResource->resourceName());
    mReadOnly->setChecked(mResource->readOnly());

    mainLayout->addWidget(generalGroupBox);

    QGroupBox* resourceGroupBox = new QGroupBox(main);
    QGridLayout* resourceLayout = new QGridLayout;
    resourceLayout->setSpacing(spacingHint());
    resourceGroupBox->setLayout(resourceLayout);

    resourceGroupBox->setTitle(i18nc("@title:group", "<resource>%1</resource> Calendar Settings",
                                     factory->typeName(resource->type())));
    mainLayout->addWidget(resourceGroupBox);

    mainLayout->addStretch();

    mConfigWidget = factory->configWidget(resource->type(), resourceGroupBox);
    if (mConfigWidget)
    {
        resourceLayout->addWidget(mConfigWidget);
        mConfigWidget->setInEditMode(false);
        mConfigWidget->loadSettings(mResource);
        mConfigWidget->show();
        connect(mConfigWidget, SIGNAL(setReadOnly(bool)), SLOT(setReadOnly(bool)));
    }

    connect(mName, SIGNAL(textChanged(QString)), SLOT(slotNameChanged(QString)));

    slotNameChanged(mName->text());
    setMinimumSize(sizeHint());
}

void ResourceConfigDialog::setInEditMode(bool value)
{
    if (mConfigWidget)
        mConfigWidget->setInEditMode(value);
}

void ResourceConfigDialog::slotNameChanged(const QString& text)
{
    enableButtonOk(!text.isEmpty());
}

void ResourceConfigDialog::setReadOnly(bool value)
{
    mReadOnly->setChecked(value);
}

void ResourceConfigDialog::accept()
{
    if (mName->text().isEmpty())
    {
        KAMessageBox::sorry(this, i18nc("@info", "Please enter a calendar name."));
        return;
    }

    mResource->startReconfig();
    mResource->setResourceName(mName->text());
    mResource->setReadOnly(mReadOnly->isChecked());

    if (mConfigWidget)
    {
        // First save generic information
        // Also save setting of specific resource type
        mConfigWidget->saveSettings(mResource);
    }
    mResource->applyReconfig();

    KDialog::accept();
}

// vim: et sw=4:
