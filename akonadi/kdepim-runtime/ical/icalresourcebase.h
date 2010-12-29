/*
    Copyright (c) 2006 Till Adam <adam@kde.org>
    Copyright (c) 2009 David Jarvie <djarvie@kde.org>

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

#ifndef ICALRESOURCEBASE_H
#define ICALRESOURCEBASE_H

#include "singlefileresource.h"
#include "settings.h"

#include <KCalCore/MemoryCalendar>
#include <KCalCore/FileStorage>


class ICalResourceBase : public Akonadi::SingleFileResource<SETTINGS_NAMESPACE::Settings>
{
  Q_OBJECT

  public:
    ICalResourceBase( const QString &id );
    ~ICalResourceBase();

  protected Q_SLOTS:
    bool retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts );
    void retrieveItems( const Akonadi::Collection &col );

  protected:
    enum CheckType { CheckForAdded, CheckForChanged };

    void initialise( const QStringList &mimeTypes, const QString &icon );
    bool readFromFile( const QString &fileName );
    bool writeToFile( const QString &fileName );

    /**
     * Customize the configuration dialog before it is displayed.
     */
    virtual void customizeConfigDialog( Akonadi::SingleFileResourceConfigDialog<SETTINGS_NAMESPACE::Settings>* dlg );

    virtual void aboutToQuit();

    /**
     * Retrieve an incidence from the calendar, and set it into a new item's payload.
     * Retrieval of the item should be signalled by calling @p itemRetrieved().
     * @param item the incidence ID to retrieve is provided by @c item.remoteId()
     * @return true if item retrieved, false if not.
     */
    virtual bool doRetrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts ) = 0;

    /**
     * Retrieve all incidences from the calendar, and set each into a new item's payload.
     * Retrieval of the items should be signalled by calling @p itemsRetrieved().
     */
    virtual void doRetrieveItems( const Akonadi::Collection &col ) = 0;

    /**
     * To be called at the start of derived class implementations of itemAdded()
     * or itemChanged() to verify that required conditions are true.
     * @param type the type of change to perform the checks for.
     * @return true if all checks are successful, and processing can continue;
     *         false if a check failed, in which case itemAdded() or itemChanged()
     *               should stop processing.
     */
    template <typename PayloadPtr> bool checkItemAddedChanged( const Akonadi::Item &item, CheckType type );

    virtual void itemRemoved( const Akonadi::Item &item );

    /** Return the local calendar. */
    KCalCore::MemoryCalendar::Ptr calendar() const;

    /** Return the calendar file storage. */
    KCalCore::FileStorage::Ptr fileStorage() const;

  private:
    KCalCore::MemoryCalendar::Ptr mCalendar;
    KCalCore::FileStorage::Ptr mFileStorage;
};

template <typename PayloadPtr>
bool ICalResourceBase::checkItemAddedChanged( const Akonadi::Item &item, CheckType type )
{
  if ( !mCalendar ) {
    cancelTask( i18n("Calendar not loaded.") );
    return false;
  }
  if ( !item.hasPayload<PayloadPtr>() ) {
    QString msg = (type == CheckForAdded)
                          ? i18n("Unable to retrieve added item %1.", item.id() )
                          : i18n("Unable to retrieve modified item %1.", item.id() );
    cancelTask( msg );
    return false;
  }
  return true;
}

#endif
