/*
 *  kalarmresourcecommon.cpp  -  common functions for KAlarm Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2009-2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "kalarmresourcecommon.h"
#include "collectionattribute.h"
#include "eventattribute.h"

#include <akonadi/attributefactory.h>
#include <akonadi/collectionmodifyjob.h>

#include <kcalcore/filestorage.h>
#include <kcalcore/memorycalendar.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

using namespace Akonadi;
using namespace KCalCore;
using KAlarm::CollectionAttribute;
using KAlarm::EventAttribute;


class Private : public QObject
{
        Q_OBJECT
    public:
        Private(QObject* parent) : QObject(parent) {}
        static Private* mInstance;

    private slots:
        void modifyCollectionJobDone(KJob*);
};
Private* Private::mInstance = 0;


namespace KAlarmResourceCommon
{

/******************************************************************************
* Perform common initialisation for KAlarm resources.
*/
void initialise(QObject* parent)
{
    // Create an object which can receive signals.
    if (!Private::mInstance)
        Private::mInstance = new Private(parent);

    // Set a default start-of-day time for date-only alarms.
    KAEvent::setStartOfDay(QTime(0,0,0));

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();

    KGlobal::locale()->insertCatalog("akonadi_kalarm_resource_common");
}

/******************************************************************************
* Fetch the list of mime types which KAlarm resources can potentially handle.
*/
QStringList mimeTypes(const QString& id)
{
    QStringList mimes;
    if (id.contains("_active"))
        mimes << KAlarm::MIME_ACTIVE;
    else if (id.contains("_archived"))
        mimes << KAlarm::MIME_ARCHIVED;
    else if (id.contains("_template"))
        mimes << KAlarm::MIME_TEMPLATE;
    else
        mimes << KAlarm::MIME_BASE
                  << KAlarm::MIME_ACTIVE << KAlarm::MIME_ARCHIVED << KAlarm::MIME_TEMPLATE;
    return mimes;
}

#if 0
/******************************************************************************
* Customize the configuration dialog before it is displayed.
*/
void customizeConfigDialog(SingleFileResourceConfigDialog<Settings>* dlg)
{
    ICalResourceBase::customizeConfigDialog(dlg);
    dlg->setMonitorEnabled(false);
    QString title;
    if (identifier().contains("_active"))
        title = i18nc("@title:window", "Select Active Alarm Calendar");
    else if (identifier().contains("_archived"))
        title = i18nc("@title:window", "Select Archived Alarm Calendar");
    else if (identifier().contains("_template"))
        title = i18nc("@title:window", "Select Alarm Template Calendar");
    else
        return;
    dlg->setCaption(title);
}
#endif

/******************************************************************************
* Find the compatibility of an existing calendar file.
*/
KAlarm::Calendar::Compat getCompatibility(const FileStorage::Ptr& fileStorage)
{
    QString versionString;
    int version = KAlarm::Calendar::checkCompatibility(fileStorage, versionString);
    return (version < 0) ? KAlarm::Calendar::Incompatible  // calendar is not in KAlarm format, or is in a future format
         : (version > 0) ? KAlarm::Calendar::Convertible   // calendar is in an out of date format
         :                 KAlarm::Calendar::Current;      // calendar is in the current format
}

/******************************************************************************
* Set an event into a new item's payload and return the new item.
* The caller should signal its retrieval by calling itemRetrieved(newitem).
* NOTE: the caller must set the event's compatibility beforehand.
*/
Item retrieveItem(const Akonadi::Item& item, KAEvent& event)
{
    QString mime = KAlarm::CalEvent::mimeType(event.category());
    event.setItemId(item.id());
    if (item.hasAttribute<EventAttribute>())
        event.setCommandError(item.attribute<EventAttribute>()->commandError());

    Item newItem = item;
    newItem.setMimeType(mime);
    newItem.setPayload<KAEvent>(event);
    return newItem;
}

/******************************************************************************
* Called when an item has been changed to validate it.
* Note that this only checks the calendar's compatibility status, not the
* event's individual compatibility (if applicable).
* Reply = the KAEvent for the item
*       = invalid if error, in which case errorMsg contains the error message
*         (which will be empty if the KAEvent is simply invalid).
*/
KAEvent checkItemChanged(const Akonadi::Item& item, KAlarm::Calendar::Compat calendarCompatibility, QString& errorMsg)
{
    if (calendarCompatibility != KAlarm::Calendar::Current)
    {
        kWarning() << "Calendar not in current format";
        errorMsg = errorMessage(NotCurrentFormat);
        return KAEvent();
    }

    KAEvent event;
    if (item.hasPayload<KAEvent>())
        event = item.payload<KAEvent>();
    if (event.isValid())
    {
        if (item.remoteId() != event.id())
        {
            kWarning() << "Item ID" << item.remoteId() << "differs from payload ID" << event.id();
            errorMsg = i18nc("@info", "Item ID %1 differs from payload ID %2.", item.remoteId(), event.id());
            return KAEvent();
        }
    }

    errorMsg.clear();
    return event;
}

/******************************************************************************
* Set a collection's compatibility attribute.
*/
void setCollectionCompatibility(const Collection& collection, KAlarm::Calendar::Compat compatibility)
{
    Collection col = collection;
    CollectionAttribute* attr = col.attribute<CollectionAttribute>(Collection::AddIfMissing);
    attr->setCompatibility(compatibility);
    Q_ASSERT(Private::mInstance);
    CollectionModifyJob* job = new CollectionModifyJob(col, Private::mInstance->parent());
    Private::mInstance->connect(job, SIGNAL(result(KJob*)), SLOT(modifyCollectionJobDone(KJob*)));
}

/******************************************************************************
* Return an error message common to more than one resource.
*/
QString errorMessage(ErrorCode code, const QString& param)
{
    switch (code)
    {
        case UidNotFound:
            return i18nc("@info", "Event with uid '%1' not found.", param);
        case NotCurrentFormat:
            return i18nc("@info", "Calendar is not in current KAlarm format.");
        case EventNoAlarms:
            return i18nc("@info", "Event with uid '%1' contains no usable alarms.", param);
        case EventReadOnly:
            return i18nc("@info", "Event with uid '%1' is read only", param);
    }
    return QString();
}

} // namespace KAlarmResourceCommon

/******************************************************************************
* Called when a collection modification job has completed, to report any error.
*/
void Private::modifyCollectionJobDone(KJob* j)
{
    if (j->error())
    {
        Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
        kError() << "Error: collection id" << collection.id() << ":" << j->errorString();
    }
}

#include "kalarmresourcecommon.moc"

// vim: et sw=4:
