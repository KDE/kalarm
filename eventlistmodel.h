/*
 *  eventlistmodel.h  -  model class for lists of alarms or templates
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef EVENTLISTMODEL_H
#define EVENTLISTMODEL_H

#include "kalarm.h"

#include <QAbstractTableModel>
#include <QList>
#include <QSize>

class QPixmap;
namespace KCal { class Event; }
class DateTime;
class KAEvent;


class EventListModel : public QAbstractTableModel
{
		Q_OBJECT
	public:
		enum {   // data columns
			TimeColumn, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, TextColumn,
			TemplateNameColumn,
			ColumnCount
		};
		enum {   // additional roles
			StatusRole = Qt::UserRole,  // return ACTIVE/ARCHIVED
			SortRole                    // return the value to use for sorting
		};

		static EventListModel* instance();
		virtual int           rowCount(const QModelIndex& parent = QModelIndex()) const;
		virtual int           columnCount(const QModelIndex& parent = QModelIndex()) const;
		virtual QModelIndex   index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
		virtual QVariant      data(const QModelIndex&, int role = Qt::DisplayRole) const;
		virtual bool          setData(const QModelIndex&, const QVariant& value, int role = Qt::EditRole);
		virtual QVariant      headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
		virtual Qt::ItemFlags flags(const QModelIndex&) const;
		int                   iconWidth() const       { return mIconSize.width(); }
		QModelIndex           eventIndex(const KCal::Event*) const;
		QModelIndex           eventIndex(const QString& eventId) const;
		void                  addEvent(KCal::Event*);
		void                  updateEvent(KCal::Event* event)      { updateEvent(mEvents.indexOf(event)); }
		void                  updateEvent(const QString& eventId)  { updateEvent(findEvent(eventId)); }
		void                  updateEvent(KCal::Event* oldEvent, KCal::Event* newEvent);
		void                  removeEvent(const KCal::Event* event) { removeEvent(mEvents.indexOf(const_cast<KCal::Event*>(event))); }
		void                  removeEvent(const QString& eventId)  { removeEvent(findEvent(eventId)); }
		static KCal::Event*   event(const QModelIndex&);

	public slots:
		void                  reload();

	private slots:
		void     slotUpdateTimeTo();

	private:
		EventListModel(QObject* parent = 0);
		void     updateEvent(int row);
		void     removeEvent(int row);
		int      findEvent(const QString& eventId) const;
		QString  alarmText(const KAEvent&, bool* truncated = 0) const;
		QString  alarmTimeText(const DateTime&) const;
		QString  timeToAlarmText(const DateTime&) const;
		QString  repeatText(const KAEvent&) const;
		QString  repeatOrder(const KAEvent&) const;
		QPixmap* eventIcon(const KAEvent&) const;
		QString  whatsThisText(int column) const;

		static EventListModel* mInstance;   // the one and only instance of this class
		static QPixmap* mTextIcon;
		static QPixmap* mFileIcon;
		static QPixmap* mCommandIcon;
		static QPixmap* mEmailIcon;
		static QSize    mIconSize;      // maximum size of any icon
		static int      mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included

		QList<KCal::Event*> mEvents;
};

#endif // EVENTLISTMODEL_H

