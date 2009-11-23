/*
 *  birthdaymodel.cpp  -  model class for birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2009 by Tobias Koenig <tokoe@kde.org>
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

#include "birthdaymodel.h"

#include "alarmcalendar.h"
#include "alarmevent.h"

#include <akonadi/changerecorder.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/session.h>
#include <kabc/addressee.h>
#include <kcal/event.h>

BirthdayModel* BirthdayModel::mInstance = 0;

BirthdayModel::BirthdayModel(Akonadi::Session *session, Akonadi::ChangeRecorder *recorder)
  : Akonadi::ContactsTreeModel(session, recorder)
{
  setColumns(Columns() << FullName << Birthday);
}

BirthdayModel::~BirthdayModel()
{
}

BirthdayModel* BirthdayModel::instance()
{
  if (!mInstance) {
    Akonadi::Session *session = new Akonadi::Session("KAlarm::BirthdayModelSession");

    Akonadi::ItemFetchScope scope;
    scope.fetchFullPayload(true);
    scope.fetchAttribute<Akonadi::EntityDisplayAttribute>();

    Akonadi::ChangeRecorder *recorder = new Akonadi::ChangeRecorder;
    recorder->fetchCollection(true);
    recorder->setItemFetchScope(scope);
    recorder->setCollectionMonitored(Akonadi::Collection::root());
    recorder->setMimeTypeMonitored(KABC::Addressee::mimeType(), true);

    mInstance = new BirthdayModel(session, recorder);
  }

  return mInstance;
}

BirthdaySortModel::BirthdaySortModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
}

void BirthdaySortModel::setPrefixSuffix(const QString &prefix, const QString &suffix)
{
  mContactsWithAlarm.clear();
  mPrefix = prefix;
  mSuffix = suffix;

  KAEvent event;
  const KAEvent::List events = AlarmCalendar::resources()->events(KCalEvent::ACTIVE);
  for (int i = 0, end = events.count();  i < end;  ++i)
  {
    KAEvent* event = events[i];
    if (event->action() == KAEventData::MESSAGE
    &&  event->recurType() == KARecurrence::ANNUAL_DATE
    &&  (prefix.isEmpty()  ||  event->message().startsWith(prefix)))
      mContactsWithAlarm.append(event->message());
  }

  invalidateFilter();
}

bool BirthdaySortModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
  const QModelIndex nameIndex = sourceModel()->index(sourceRow, 0, sourceParent);
  const QModelIndex birthdayIndex = sourceModel()->index(sourceRow, 1, sourceParent);

  // if the birthday is invalid, the second column is empty
  if (birthdayIndex.data(Qt::DisplayRole).toString().isEmpty())
    return false;


  const QString text = mPrefix + nameIndex.data(Qt::DisplayRole).toString() + mSuffix;
  if (mContactsWithAlarm.contains(text))
    return false;

  return true;
}
