/*
 *  birthdaymodel.h  -  model class for birthdays from address book
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

#include <libkdepim/contactstreemodel.h>

#include <QtGui/QSortFilterProxyModel>

#ifndef BIRTHDAYMODEL_H
#define BIRTHDAYMODEL_H

namespace Akonadi
{
  class ChangeRecorder;
  class Session;
}

/**
 * @short Provides the global model for all contacts
 *
 * This model provides the EntityTreeModel for all contacts.
 * The model is accessable via the static instance() method.
 */
class BirthdayModel : public Akonadi::ContactsTreeModel
{
  public:
    /**
     * Destroys the global contact model.
     */
    ~BirthdayModel();

    /**
     * Returns the global contact model instance.
     */
    static BirthdayModel* instance();

  private:
    BirthdayModel(Akonadi::Session *session, Akonadi::ChangeRecorder *recorder);

    static BirthdayModel *mInstance;
};

class BirthdaySortModel : public QSortFilterProxyModel
{
	public:
		BirthdaySortModel(QObject* parent = 0);

    void setPrefixSuffix(const QString &prefix, const QString &suffix);

  protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

  private:
    QStringList mContactsWithAlarm;
    QString mPrefix;
    QString mSuffix;
};

#endif
