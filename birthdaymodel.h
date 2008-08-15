/*
 *  birthdaymodel.h  -  model class for birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2007,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef BIRTHDAYMODEL_H
#define BIRTHDAYMODEL_H

#include "kalarm.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QList>
#include <QDate>

namespace KABC { class AddressBook; }


class BirthdayModel : public QAbstractTableModel
{
		Q_OBJECT
	public:
		enum {   // data columns
			NameColumn, DateColumn,
			ColumnCount
		};
		struct Data
		{
			Data(const QString& n, const QDate& b) : birthday(b), name(n) {}
			QDate   birthday;
			QString name;
		};

		static BirthdayModel* instance(QObject* parent = 0);
		static void         close();
		~BirthdayModel();
		void                setPrefixSuffix(const QString& prefix, const QString& suffix);
		virtual int         rowCount(const QModelIndex& parent = QModelIndex()) const;
		virtual int         columnCount(const QModelIndex& parent = QModelIndex()) const;
		virtual QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
		virtual QVariant    data(const QModelIndex&, int role = Qt::DisplayRole) const;
		virtual QVariant    headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
		Data*               rowData(const QModelIndex&) const;

	signals:
		void                addrBookError();    // error loading address book

	private slots:
		void                refresh();

	private:
		explicit BirthdayModel(QObject* parent = 0);
		void                loadAddressBook();

		static BirthdayModel* mInstance;
		static const KABC::AddressBook* mAddressBook;
		QList<Data*> mData;
		QString      mPrefix;
		QString      mSuffix;
};


class BirthdaySortModel : public QSortFilterProxyModel
{
	public:
		BirthdaySortModel(QAbstractItemModel* baseModel, QObject* parent);
		BirthdayModel::Data* rowData(const QModelIndex&) const;
};

#endif // BIRTHDAYMODEL_H
