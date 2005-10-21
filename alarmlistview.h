/*
 *  alarmlistview.h  -  widget showing list of outstanding alarms
 *  Program:  kalarm
 *  Copyright (c) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ALARMLISTVIEW_H
#define ALARMLISTVIEW_H

#include "kalarm.h"
#include "eventlistviewbase.h"

class Event;
class QMouseEvent;
class AlarmListView;


class AlarmListViewItem : public EventListViewItemBase
{
	public:
		AlarmListViewItem(AlarmListView* parent, const KAEvent&, const QDateTime& now);
		QTime               showTimeToAlarm(bool show);
		void                updateTimeToAlarm(const QDateTime& now, bool forceDisplay = false);
		virtual void        paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*      alarmListView() const         { return (AlarmListView*)listView(); }
		bool                messageTruncated() const      { return mMessageTruncated; }
		int                 messageColWidthNeeded() const { return mMessageColWidth; }
		static int          typeIconWidth(AlarmListView*);
		// Overridden base class methods
		AlarmListViewItem*  nextSibling() const           { return (AlarmListViewItem*)Q3ListViewItem::nextSibling(); }
		virtual QString     key(int column, bool ascending) const;
	protected:
		virtual QString     lastColumnText() const        { return alarmText(event()); }
	private:
		QString             alarmText(const KAEvent&) const;
		QString             alarmTimeText(const DateTime&) const;
		QString             timeToAlarmText(const QDateTime& now) const;

		static int          mTimeHourPos;       // position of hour within time string, or -1 if leading zeroes included
		static int          mDigitWidth;        // display width of a digit
		QString             mDateTimeOrder;     // controls ordering of date/time column
		QString             mRepeatOrder;       // controls ordering of repeat column
		QString             mColourOrder;       // controls ordering of colour column
		QString             mTypeOrder;         // controls ordering of alarm type column
		mutable int         mMessageColWidth;   // width needed to display complete message text
		mutable bool        mMessageTruncated;  // multi-line message text has been truncated for the display
		bool                mTimeToAlarmShown;  // relative alarm time is displayed
};


class AlarmListView : public EventListViewBase
{
		Q_OBJECT       // needed by QObject::isA() calls
	public:
		explicit AlarmListView(QWidget* parent = 0);
		~AlarmListView();
		void                   showExpired(bool show)      { mShowExpired = show; }
		bool                   showingExpired() const      { return mShowExpired; }
		bool                   showingTimeTo() const	   { return columnWidth(mTimeToColumn); }
		void                   selectTimeColumns(bool time, bool timeTo);
		void                   updateTimeToAlarms(bool forceDisplay = false);
		bool                   expired(AlarmListViewItem*) const;
		bool                   drawMessageInColour() const            { return mDrawMessageInColour; }
		void                   setDrawMessageInColour(bool inColour)  { mDrawMessageInColour = inColour; }
		int                    timeColumn() const     { return mTimeColumn; }
		int                    timeToColumn() const   { return mTimeToColumn; }
		int                    repeatColumn() const   { return mRepeatColumn; }
		int                    colourColumn() const   { return mColourColumn; }
		int                    typeColumn() const     { return mTypeColumn; }
		int                    messageColumn() const  { return mMessageColumn; }
		static bool            dragging()             { return mDragging; }
		// Overridden base class methods
		static void            addEvent(const KAEvent&, EventListViewBase*);
		static void            modifyEvent(const KAEvent& e, EventListViewBase* selectionView)
		                             { EventListViewBase::modifyEvent(e.id(), e, mInstanceList, selectionView); }
		static void            modifyEvent(const QString& oldEventID, const KAEvent& newEvent, EventListViewBase* selectionView)
		                             { EventListViewBase::modifyEvent(oldEventID, newEvent, mInstanceList, selectionView); }
		static void            deleteEvent(const QString& eventID)
		                             { EventListViewBase::deleteEvent(eventID, mInstanceList); }
		static void            undeleteEvent(const QString& oldEventID, const KAEvent& event, EventListViewBase* selectionView)
		                             { EventListViewBase::modifyEvent(oldEventID, event, mInstanceList, selectionView); }
		AlarmListViewItem*     getEntry(const QString& eventID)  { return (AlarmListViewItem*)EventListViewBase::getEntry(eventID); }
		AlarmListViewItem*     currentItem() const    { return (AlarmListViewItem*)EventListViewBase::currentItem(); }
		AlarmListViewItem*     firstChild() const     { return (AlarmListViewItem*)EventListViewBase::firstChild(); }
		AlarmListViewItem*     selectedItem() const   { return (AlarmListViewItem*)EventListViewBase::selectedItem(); }
		virtual void           setSelected(Q3ListViewItem* item, bool selected)      { EventListViewBase::setSelected(item, selected); }
		virtual void           setSelected(AlarmListViewItem* item, bool selected)  { EventListViewBase::setSelected(item, selected); }
		virtual InstanceList   instances()            { return mInstanceList; }

	protected:
		virtual void           populate();
		EventListViewItemBase* createItem(const KAEvent&);
		virtual QString        whatsThisText(int column) const;
		virtual bool           shouldShowEvent(const KAEvent& e) const  { return mShowExpired || !e.expired(); }
		AlarmListViewItem*     addEntry(const KAEvent& e, bool setSize = false)
		                               { return addEntry(e, QDateTime::currentDateTime(), setSize); }
		AlarmListViewItem*     updateEntry(AlarmListViewItem* item, const KAEvent& newEvent, bool setSize = false)
		                               { return (AlarmListViewItem*)EventListViewBase::updateEntry(item, newEvent, setSize); }
		virtual bool           event(QEvent*);
		virtual void           contentsMousePressEvent(QMouseEvent*);
		virtual void           contentsMouseMoveEvent(QMouseEvent*);
		virtual void           contentsMouseReleaseEvent(QMouseEvent*);

	private:
		AlarmListViewItem*     addEntry(const KAEvent&, const QDateTime& now, bool setSize = false, bool reselect = false);

		static InstanceList    mInstanceList;         // all current instances of this class
		static bool            mDragging;
		int                    mTimeColumn;           // index to alarm time column
		int                    mTimeToColumn;         // index to time-to-alarm column
		int                    mRepeatColumn;         // index to repetition type column
		int                    mColourColumn;         // index to colour column
		int                    mTypeColumn;           // index to alarm type column
		int                    mMessageColumn;        // index to message column
		int                    mTimeColumnHeaderWidth;
		int                    mTimeToColumnHeaderWidth;
		QPoint                 mMousePressPos;        // where the mouse left button was last pressed
		bool                   mMousePressed;         // true while the mouse left button is pressed
		bool                   mDrawMessageInColour;
		bool                   mShowExpired;          // true to show expired alarms
};

#endif // ALARMLISTVIEW_H

