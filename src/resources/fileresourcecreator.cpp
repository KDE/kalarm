/*
 *  fileresourcecreator.cpp  -  interactively create a file system resource
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#include "fileresourcecreator.h"

#include "resources/resources.h"
#include "resources/fileresourcecalendarupdater.h"
#include "resources/fileresourceconfigmanager.h"
#include "resources/singlefileresourceconfigdialog.h"
#include "lib/autoqpointer.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QInputDialog>

using namespace KAlarmCal;

namespace
{
QString validateFileUrl(const QUrl&);
}


FileResourceCreator::FileResourceCreator(CalEvent::Type defaultType, QWidget* parent)
    : ResourceCreator(defaultType, parent)
{
}

/******************************************************************************
* Create a new resource. The user will be prompted to enter its configuration.
*/
void FileResourceCreator::doCreateResource()
{
    qCDebug(KALARM_LOG) << "FileResourceCreator::doCreateResource: Type:" << mDefaultType;
    const QList<ResourceType::StorageType> types = FileResourceConfigManager::storageTypes();
    if (!types.isEmpty())
    {
        QStringList typeDescs;
        for (ResourceType::StorageType t : types)
            typeDescs += Resource::storageTypeString(t);
        ResourceType::StorageType type = types[0];
        if (types.count() > 1)
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of ResourceSelector, and on return from this function).
            AutoQPointer<QInputDialog> dlg = new QInputDialog(mParent);
            dlg->setWindowTitle(i18nc("@title:window", "Calendar Configuration"));
            dlg->setLabelText(i18nc("@info", "Select storage type of new calendar:"));
            dlg->setOption(QInputDialog::UseListViewForComboBoxItems);
            dlg->setInputMode(QInputDialog::TextInput);
            dlg->setComboBoxEditable(false);
            dlg->setComboBoxItems(typeDescs);
            if (dlg->exec() != QDialog::Accepted)
            {
                deleteLater();
                return;
            }

            const QString item = dlg->textValue();
            for (int i = 0, iMax = typeDescs.count();  i < iMax;  ++i)
            {
                if (typeDescs.at(i) == item)
                {
                    type = types[i];
                    break;
                }
            }
        }

        switch (type)
        {
            case ResourceType::File:
                if (createSingleFileResource())
                    return;
                break;
            case ResourceType::Directory:   // not currently intended to be implemented
            default:
                break;
        }
    }
    deleteLater();   // error result
}

/******************************************************************************
* Configure and create a single file resource.
*/
bool FileResourceCreator::createSingleFileResource()
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<SingleFileResourceConfigDialog> dlg = new SingleFileResourceConfigDialog(true, mParent);
    dlg->setAlarmType(mDefaultType);   // set default alarm type
    dlg->setUrlValidation(&validateFileUrl);
    if (dlg->exec() != QDialog::Accepted)
        return false;

    qCDebug(KALARM_LOG) << "FileResourceCreator::createSingleFileResource: Creating" << dlg->displayName();
    FileResourceSettings::Ptr settings(new FileResourceSettings(
              FileResourceSettings::File,
              dlg->url(), dlg->alarmType(), dlg->displayName(), QColor(), dlg->alarmType(),
              CalEvent::EMPTY, dlg->readOnly()));
    Resource resource = FileResourceConfigManager::addResource(settings);

    // Update the calendar to the current KAlarm format if necessary, and
    // if the user agrees.
    FileResourceCalendarUpdater::updateToCurrentFormat(resource, true, mParent);

    Q_EMIT resourceAdded(resource, mDefaultType);
    return true;
}

namespace
{

/******************************************************************************
* Check whether the user-entered URL duplicates any existing resource.
*/
QString validateFileUrl(const QUrl& url)
{
    // Ensure that the new resource doesn't use the same file or directory as
    // an existing file resource, to avoid duplicate processing.
    const QVector<Resource> resources = Resources::allResources<FileResource>();
    for (const Resource& res : resources)
    {
        if (res.location() == url)
        {
            const QString path = url.toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile);
            qCWarning(KALARM_LOG) << "FileResourceCreator::validateFileUrl: Duplicate path for new resource:" << path;
            return xi18nc("@info", "Error!  The file is already used by an existing resource.", path);
        }
    }
    return QString();
}

}

// vim: et sw=4:
