/*
 *  alarmlistview.h  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
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
		enum { TIME_COLUMN, REPEAT_COLUMN, COLOUR_COLUMN, MESSAGE_COLUMN };

		AlarmListView(QWidget* parent = 0, const char* name = 0);
		virtual void         clear();
		void                 refresh();
		void                 showExpired(bool show)      { mShowExpired = show; }
		AlarmListViewItem*   addEntry(const KAlarmEvent&, bool setSize = false);
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const KAlarmEvent& newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const KAlarmEvent&   getEvent(AlarmListViewItem*) const;
		AlarmListViewItem*   getEntry(const QString& eventID);
		bool                 expired(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const               { return drawMessageInColour_; }
		void                 setDrawMessageInColour(bool inColour)     { drawMessageInColour_ = inColour; }
		AlarmListViewItem*   selectedItem() const   { return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const    { return (AlarmListViewItem*)KListView::currentItem(); }
		AlarmListViewItem*   firstChild() const     { return (AlarmListViewItem*)KListView::firstChild(); }
		virtual void         setSelected(QListViewItem*, bool selected);
		virtual void         setSelected(AlarmListViewItem*, bool selected);
		AlarmListViewItem*   singleSelectedItem() const;
		QPtrList<AlarmListViewItem> selectedItems() const;
		int                  selectedCount() const;
	signals:
		void                 itemDeleted();
	private:
		int                  lastColumnHeaderWidth_;
		bool                 drawMessageInColour_;
		bool                 mShowExpired;              // true to show expired alarms
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(QListView* parent, const KAlarmEvent&);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const     { return (AlarmListView*)listView(); }
		const KAlarmEvent&   event() const             { return mEvent; }
		int                  messageWidth() const      { return mMessageWidth; }
		AlarmListViewItem*   nextSibling() const       { return (AlarmListViewItem*)QListViewItem::nextSibling(); }
		virtual QString      key(int column, bool ascending) const;
	private:
		static QPixmap*      textIcon;
		static QPixmap*      fileIcon;
		static QPixmap*      commandIcon;
		static QPixmap*      emailIcon;
		static int           iconWidth;

		KAlarmEvent          mEvent;           // the event for this item
		QString              mDateTimeOrder;   // controls ordering of date/time column
		QString              mRepeatOrder;     // controls ordering of repeat column
		QString              mColourOrder;     // controls ordering of colour column
		int                  mMessageWidth;    // width required to display message column
};

#endif // ALARMLISTVIEW_H

