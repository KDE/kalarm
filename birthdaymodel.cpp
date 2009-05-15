/*
 *  birthdaymodel.cpp  -  model class for birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2007-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include <klocale.h>
#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kdebug.h>

#include <kcal/event.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "birthdaymodel.moc"


BirthdayModel*           BirthdayModel::mInstance = 0;
const KABC::AddressBook* BirthdayModel::mAddressBook = 0;


BirthdayModel* BirthdayModel::instance(QObject* parent)
{
	if (!mInstance)
		mInstance = new BirthdayModel(parent);
	return mInstance;
}

void BirthdayModel::close()
{
	if (mAddressBook)
	{
		KABC::StdAddressBook::close();
		mAddressBook = 0;
	}
	if (mInstance)
	{
		delete mInstance;
		mInstance = 0;
	}
}

BirthdayModel::BirthdayModel(QObject* parent)
	: QAbstractTableModel(parent)
{
}

BirthdayModel::~BirthdayModel()
{
	int count = mData.count();
	if (count)
	{
		beginRemoveRows(QModelIndex(), 0, count - 1);
		while (!mData.isEmpty())
			delete mData.takeLast();   // alarm exists, so remove from selection list
		endRemoveRows();
	}
	if (this == mInstance)
		mInstance = 0;
}

void BirthdayModel::setPrefixSuffix(const QString& prefix, const QString& suffix)
{
	if (prefix != mPrefix  ||  suffix != mSuffix)
	{
		// Text has changed - re-evaluate the selection list
		mPrefix = prefix;
		mSuffix = suffix;
		loadAddressBook();
	}
}

int BirthdayModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return mData.count();
}

int BirthdayModel::columnCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return ColumnCount;
}

QModelIndex BirthdayModel::index(int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid()  ||  row >= mData.count())
		return QModelIndex();
	return createIndex(row, column, mData[row]);
}

QVariant BirthdayModel::data(const QModelIndex& index, int role) const
{
	int column = index.column();
	Data* data = static_cast<Data*>(index.internalPointer());
	if (!data)
		return QVariant();
	if (role == Qt::DisplayRole)
	{
		switch (column)
		{
			case NameColumn:
				return data->name;
			case DateColumn:
				return data->birthday;
			default:
				break;
		}
	}
	return QVariant();
}

QVariant BirthdayModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (section)
			{
				case NameColumn:
					return i18nc("@title:column Name of person", "Name");
				case DateColumn:
					return i18nc("@title:column", "Birthday");
			}
		}
	}
	return QVariant();
}

/******************************************************************************
* Return the event referred to by an index.
*/
BirthdayModel::Data* BirthdayModel::rowData(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;
	return static_cast<Data*>(index.internalPointer());
}

/******************************************************************************
* Load the address book in preparation for displaying the birthday selection list.
*/
void BirthdayModel::loadAddressBook()
{
	if (!mAddressBook)
	{
		mAddressBook = KABC::StdAddressBook::self(true);
		if (mAddressBook)
			connect(mAddressBook, SIGNAL(addressBookChanged(AddressBook*)), SLOT(refresh()));
	}
	else
		refresh();
	if (!mAddressBook)
		emit addrBookError();
}

/******************************************************************************
* Initialise or update the birthday selection list by fetching all birthdays
* from the address book and displaying those which do not already have alarms.
*/
void BirthdayModel::refresh()
{
	// Compile a list of all pending alarm messages which look like birthdays
	QStringList messageList;
	KAEvent event;
	KAEvent::List events = AlarmCalendar::resources()->events(KCalEvent::ACTIVE);
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		if (event->action() == KAEvent::MESSAGE
		&&  event->recurType() == KARecurrence::ANNUAL_DATE
		&&  (mPrefix.isEmpty()  ||  event->message().startsWith(mPrefix)))
			messageList.append(event->message());
	}

	// Fetch all birthdays from the address book
	QList<Data*> newEntries;
	for (KABC::AddressBook::ConstIterator abit = mAddressBook->begin();  abit != mAddressBook->end();  ++abit)
	{
		const KABC::Addressee& addressee = *abit;
		if (addressee.birthday().isValid())
		{
			// Create a list entry for this birthday
			QDate birthday = addressee.birthday().date();
			QString name = addressee.nickName();
			if (name.isEmpty())
				name = addressee.realName();
			// Check if the birthday already has an alarm
			QString text = mPrefix + name + mSuffix;
			bool alarmExists = messageList.contains(text);
			// Check if the birthday is already in the selection list
			int row = -1;
			int count = mData.count();
			while (++row < count
			   &&  (name != mData[row]->name  ||  birthday != mData[row]->birthday)) ;
			if (alarmExists  &&  row < count)
			{
				beginRemoveRows(QModelIndex(), row, row);
				delete mData[row];
				mData.removeAt(row);     // alarm exists, so remove from selection list
				endRemoveRows();
			}
			else if (!alarmExists  &&  row >= count)
				newEntries += new Data(name, birthday);
		}
	}
	if (!newEntries.isEmpty())
	{
		int row = mData.count();
		beginInsertRows(QModelIndex(), row, row + newEntries.count() - 1);
		mData += newEntries;
		endInsertRows();
	}
}


BirthdaySortModel::BirthdaySortModel(QAbstractItemModel* baseModel, QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSourceModel(baseModel);
}

/******************************************************************************
* Return the event referred to by an index.
*/
BirthdayModel::Data* BirthdaySortModel::rowData(const QModelIndex& index) const
{
	return static_cast<BirthdayModel*>(sourceModel())->rowData(mapToSource(index));
}
