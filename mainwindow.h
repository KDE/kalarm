/*
 *  mainwindow.h  -  description
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <map>

#include <qvariant.h>
class QListViewItem;

#include <kapp.h>
#include <kmainwindow.h>
#include <kaccel.h>
#include <kaction.h>
#include <klistview.h>

#include "msgevent.h"
using namespace KCal;

class AlarmListView;


class KAlarmMainWindow : public KMainWindow
{
		Q_OBJECT

	public:
		KAlarmMainWindow();
		~KAlarmMainWindow();

		void  addMessage(const KAlarmEvent&);
		void  modifyMessage(const KAlarmEvent& event)    { modifyMessage(event.id(), event); }
		void  modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent);
		void  deleteMessage(const KAlarmEvent&);

	protected:
		virtual void resizeEvent(QResizeEvent*);
		virtual void showEvent(QShowEvent*);

	private:
		void         initActions();

		AlarmListView*  listView;
		KAction*        actionNew;
		KAction*        actionModify;
		KAction*        actionDelete;
		KAction*        actionResetDaemon;
		KAction*        actionQuit;
		bool            hidden;      // this is the main window which is never displayed

	protected slots:
		virtual void slotDelete();
		virtual void slotNew();
		virtual void slotModify();
		virtual void slotResetDaemon();
		virtual void slotPreferences();
		virtual void slotQuit();
		virtual void slotSelection();
		virtual void slotListRightClick(QListViewItem*, const QPoint&, int);
};


class AlarmListViewItem;
struct AlarmItemData
{
		KAlarmEvent event;
		QString     messageText;     // message as displayed
		QString     dateTimeText;    // date/time as displayed
		QString     repeatCountText; // repeat count as displayed
		QString     repeatCountOrder;  // repeat count item ordering text
		int         messageWidth;    // width required to display 'messageText'
};


class AlarmListView : public KListView
{
	public:
		enum { TIME_COLUMN, REPEAT_COLUMN, COLOUR_COLUMN, MESSAGE_COLUMN };

		AlarmListView(QWidget* parent = 0L, const char* name = 0L);
		virtual void         clear();
		void                 refresh();
		AlarmListViewItem*   addEntry(const KAlarmEvent&, bool setSize = false);
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const KAlarmEvent& newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const KAlarmEvent    getEntry(AlarmListViewItem* item) const	{ return getData(item)->event; }
		AlarmListViewItem*   getEntry(const QString& eventID);
		const AlarmItemData* getData(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const		      { return drawMessageInColour_; }
		void                 setDrawMessageInColour(bool inColour)	{ drawMessageInColour_ = inColour; }
		AlarmListViewItem*   selectedItem() const	{ return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const	{ return (AlarmListViewItem*)KListView::currentItem(); }
	private:
		std::map<AlarmListViewItem*, AlarmItemData> entries;
		int                  lastColumnHeaderWidth_;
		bool                 drawMessageInColour_;
};


class AlarmListViewItem : public QListViewItem
{
	public:
		AlarmListViewItem(QListView* parent, const QString&, const QString&);
		virtual void         paintCell(QPainter*, const QColorGroup&, int column, int width, int align);
		AlarmListView*       alarmListView() const		{ return (AlarmListView*)listView(); }
};

#endif // MAINWINDOW_H

