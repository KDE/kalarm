/*
 *  mainwindow.h  -  description
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <msgevent.h>
using namespace KCal;

class AlarmListView;


class KAlarmMainWindow : public KMainWindow
{
		Q_OBJECT

	public:
		KAlarmMainWindow(const char* name=0);
		~KAlarmMainWindow();

		void  addMessage(const MessageEvent*);
		void  modifyMessage(const MessageEvent* oldEvent, const MessageEvent* newEvent);
		void  deleteMessage(const MessageEvent*);

	protected:
		virtual void resizeEvent(QResizeEvent*);
		virtual void showEvent(QShowEvent*);

	private:
		void            initActions();

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
		const MessageEvent* event;
		QString             messageText;
		QString             dateTimeText;
		int                 messageWidth;
};


class AlarmListView : public KListView
{
	public:
		enum { TIME_COLUMN, COLOUR_COLUMN, MESSAGE_COLUMN };

		AlarmListView(QWidget* parent = 0, const char* name = 0);
		virtual void         clear();
		void                 refresh();
		AlarmListViewItem*   addEntry(const MessageEvent*, bool setSize = false);
		AlarmListViewItem*   updateEntry(AlarmListViewItem*, const MessageEvent* newEvent, bool setSize = false);
		void                 deleteEntry(AlarmListViewItem*, bool setSize = false);
		const MessageEvent*  getEntry(AlarmListViewItem* item) const	{ return getData(item).event; }
		AlarmListViewItem*   getEntry(const MessageEvent*);
		const AlarmItemData& getData(AlarmListViewItem*) const;
		void                 resizeLastColumn();
		int                  itemHeight();
		bool                 drawMessageInColour() const		{ return drawMessageInColour_; }
		void                 setDrawMessageInColour(bool inColour)	{ drawMessageInColour_ = inColour; }
		AlarmListViewItem*   selectedItem() const	{ return (AlarmListViewItem*)KListView::selectedItem(); }
		AlarmListViewItem*   currentItem() const	{ return (AlarmListViewItem*)KListView::currentItem(); }
	private:
		map<AlarmListViewItem*, AlarmItemData> entries;
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

