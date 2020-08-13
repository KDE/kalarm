/*
 *  birthdaymodel.cpp  -  model class for birthdays from address book
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "birthdaymodel.h"

#include "resourcescalendar.h"

#include <KAlarmCal/KAEvent>

#include <AkonadiCore/ChangeRecorder>
#include <AkonadiCore/EntityDisplayAttribute>
#include <AkonadiCore/ItemFetchScope>
#include <AkonadiCore/Session>
#include <KContacts/Addressee>

#include <QLocale>

using namespace KAlarmCal;


BirthdayModel* BirthdayModel::mInstance = nullptr;

BirthdayModel::BirthdayModel(Akonadi::ChangeRecorder* recorder)
    : Akonadi::ContactsTreeModel(recorder)
{
    setColumns({FullName, Birthday});
}

BirthdayModel::~BirthdayModel()
{
    if (this == mInstance)
        mInstance = nullptr;
}

BirthdayModel* BirthdayModel::instance()
{
    if (!mInstance)
    {
        Akonadi::Session* session = new Akonadi::Session("KAlarm::BirthdayModelSession");

        Akonadi::ItemFetchScope scope;
        scope.fetchFullPayload(true);
        scope.fetchAttribute<Akonadi::EntityDisplayAttribute>();

        Akonadi::ChangeRecorder* recorder = new Akonadi::ChangeRecorder;
        recorder->setSession(session);
        recorder->fetchCollection(true);
        recorder->setItemFetchScope(scope);
        recorder->setCollectionMonitored(Akonadi::Collection::root());
        recorder->setMimeTypeMonitored(KContacts::Addressee::mimeType(), true);

        mInstance = new BirthdayModel(recorder);
    }

    return mInstance;
}

QVariant BirthdayModel::entityData(const Akonadi::Item& item, int column, int role) const
{
    if (columns().at(column) == Birthday  &&  role == Qt::DisplayRole)
    {
        const QDate date = Akonadi::ContactsTreeModel::entityData(item, column, DateRole).toDate();
        if (date.isValid())
            return QLocale().toString(date, QLocale::ShortFormat);
    }
    return Akonadi::ContactsTreeModel::entityData(item, column, role);
}

/*============================================================================*/

BirthdaySortModel::BirthdaySortModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void BirthdaySortModel::setPrefixSuffix(const QString& prefix, const QString& suffix)
{
    mContactsWithAlarm.clear();
    mPrefix = prefix;
    mSuffix = suffix;

    const QVector<KAEvent> events = ResourcesCalendar::events(CalEvent::ACTIVE);
    for (const KAEvent& event : events)
    {
        if (event.actionSubType() == KAEvent::MESSAGE
        &&  event.recurType() == KARecurrence::ANNUAL_DATE
        &&  (prefix.isEmpty()  ||  event.message().startsWith(prefix)))
            mContactsWithAlarm.append(event.message());
    }

    invalidateFilter();
}

bool BirthdaySortModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex nameIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QModelIndex birthdayIndex = sourceModel()->index(sourceRow, 1, sourceParent);

    // If the birthday is invalid, the second column is empty
    if (birthdayIndex.data(Qt::DisplayRole).toString().isEmpty())
        return false;

    const QString text = mPrefix + nameIndex.data(Qt::DisplayRole).toString() + mSuffix;
    if (mContactsWithAlarm.contains(text))
        return false;

    return true;
}

// vim: et sw=4:
