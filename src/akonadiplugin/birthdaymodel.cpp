/*
 *  birthdaymodel.cpp  -  model class for birthdays from address book
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>
 *  SPDX-FileCopyrightText: 2007-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "birthdaymodel.h"

#include <Akonadi/ChangeRecorder>
#include <Akonadi/EntityDisplayAttribute>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/Session>
#include <KContacts/Addressee>

#include <QLocale>


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

        auto recorder = new Akonadi::ChangeRecorder;
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

/******************************************************************************
* Set a new prefix and suffix for the alarm message, and set the selection list
* based on them.
*/
void BirthdaySortModel::setPrefixSuffix(const QString& prefix, const QString& suffix, const QStringList& alarmMessageList)
{
    mPrefix = prefix;
    mSuffix = suffix;
    mContactsWithAlarm = alarmMessageList;

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

#include "moc_birthdaymodel.cpp"

// vim: et sw=4:
