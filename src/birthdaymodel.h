/*
 *  birthdaymodel.h  -  model class for birthdays from address book
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <Akonadi/Contact/ContactsTreeModel>

#include <QSortFilterProxyModel>

namespace Akonadi
{
    class ChangeRecorder;
}

/**
 * @short Provides the global model for all contacts
 *
 * This model provides the EntityTreeModel for all contacts.
 * The model is accessible via the static instance() method.
 */
class BirthdayModel : public Akonadi::ContactsTreeModel
{
        Q_OBJECT
    public:
        enum {   // data columns
            NameColumn, DateColumn,
            ColumnCount
        };

        /**
         * Destroys the global contact model.
         */
        ~BirthdayModel() override;

        /**
         * Returns the global contact model instance.
         */
        static BirthdayModel* instance();

        QVariant entityData(const Akonadi::Item&, int column, int role = Qt::DisplayRole) const override;
        QVariant entityData(const Akonadi::Collection& collection, int column, int role = Qt::DisplayRole) const override
                                    { return Akonadi::ContactsTreeModel::entityData(collection, column, role); }

  private:
        explicit BirthdayModel(Akonadi::ChangeRecorder* recorder);

        static BirthdayModel* mInstance;
};


class BirthdaySortModel : public QSortFilterProxyModel
{
        Q_OBJECT
    public:
        explicit BirthdaySortModel(QObject* parent = nullptr);

        void setPrefixSuffix(const QString& prefix, const QString& suffix);

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    private:
        QStringList mContactsWithAlarm;
        QString     mPrefix;
        QString     mSuffix;
};


// vim: et sw=4:
