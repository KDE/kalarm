/*
 *  alarmlistview.h  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ALARMLISTVIEW_H
#define ALARMLISTVIEW_H

#include "kalarm.h"

#include <qmap.h>
#include <klistview.h>

#include "alarmevent.h"

class AlarmListViewItem;


class AlarmListView : public KListView
{
		Q_OBJECT
	public:
		AlarmListView(QWidget* parent = 0, const char* name = 0);
		virtual void         clear();
		void                 refresh();
		void                 showExpired(bool show)      { mShowExpired = show; }
		bool                 showingTimeTo() const	 { return columnWidth(mTimeToColumn); }
		void                 selectTimeColumns(bool time, bool timeTo);
		void                 updateTimeToAlarms(bool forceDisplay = false);
		AlarmListViewItem*   addEntry(const KAlarmEvent& e, bool setSize = false)
		                             { return addEntry(e, QDateTime::currentDateTime(), setSize); }
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const KAlarmEvent& newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const KAlarmEvent&   getEvent(AlarmListViewItem*) const;
		AlarmListViewItem*   getEntry(const QString& eventID);
		bool                 expired(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const               { return mDrawMessageInColour; }
		void                 setDrawMessageInColour(bool inColour)     { mDrawMessageInColour = inColour; }
		AlarmListViewItem*   selectedItem() const   { return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const    { return (AlarmListViewItem*)KListView::currentItem(); }
		AlarmListViewItem*   firstChild() const     { return (AlarmListViewItem*)KListView::firstChild(); }
		virtual void         setSelected(QListViewItem*, bool selected);
		virtual void         setSelected(AlarmListViewItem*, bool selected);
		AlarmListViewItem*   singleSelectedItem() const;
		QPtrList<AlarmListViewItem> selectedItems() const;
		int                  selectedCount() const;
		int                  timeColumn() const     { return mTimeColumn; }
		int                  timeToColumn() const   { return mTimeToColumn; }
		int                  repeatColumn() const   { return mRepeatColumn; }
		int                  colourColumn() const   { return mColourColumn; }
		int                  messageColumn() const  { return mMessageColumn; }
	signals:
		void                 itemDeleted();
	private:
		AlarmListViewItem*   addEntry(const KAlarmEvent&, const QDateTime& now, bool setSize = false);

		int                  mTimeColumn;           // index to alarm time column
		int                  mTimeToColumn;         // index to time-to-alarm column
		int                  mRepeatColumn;         // index to repetition type column
		int                  mColourColumn;         // index to colour column
		int                  mMessageColumn;        // index to message column
		int                  mLastColumnHeaderWidth;
		int                  mTimeColumnHeaderWidth;
		int                  mTimeToColumnHeaderWidth;
		bool                 mDrawMessageInColour;
		bool                 mShowExpired;          // true to show expired alarms
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(AlarmListView* parent, const KAlarmEvent&, const QDateTime& now);
		QTime                showTimeToAlarm(bool show);
		void                 updateTimeToAlarm(const QDateTime& now, bool forceDisplay = false);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const     { return (AlarmListView*)listView(); }
		const KAlarmEvent&   event() const             { return mEvent; }
		int                  messageWidth() const      { return mMessageWidth; }
		AlarmListViewItem*   nextSibling() const       { return (AlarmListViewItem*)QListViewItem::nextSibling(); }
		virtual QString      key(int column, bool ascending) const;
		static QString       alarmText(const KAlarmEvent&);
	private:
		static QPixmap*      textIcon;
		static QPixmap*      fileIcon;
		static QPixmap*      commandIcon;
		static QPixmap*      emailIcon;
		static int           iconWidth;

		QString              alarmTimeText() const;
		QString              timeToAlarmText(const QDateTime& now) const;

		KAlarmEvent          mEvent;            // the event for this item
		QString              mDateTimeOrder;    // controls ordering of date/time column
		QString              mRepeatOrder;      // controls ordering of repeat column
		QString              mColourOrder;      // controls ordering of colour column
		int                  mMessageWidth;     // width required to display message column
		int                  mTimeToAlarmShown; // relative alarm time is displayed
};

#endif // ALARMLISTVIEW_H

