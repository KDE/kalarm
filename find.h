/*
 *  find.h  -  search facility 
 *  Program:  kalarm
 *  (C) 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef FIND_H
#define FIND_H

#include <qobject.h>
#include <qstringlist.h>

class QCheckBox;
class KFindDialog;
class KFind;
class KSeparator;
class EventListViewBase;
class EventListViewItemBase;


class Find : public QObject
{
		Q_OBJECT
	public:
		Find(EventListViewBase* parent);
		void         display();
		void         findNext(bool forward)     { findNext(forward, true); }

	signals:
		void         active(bool);

	private slots:
		void         slotFind();
		void         slotDlgDestroyed();
		void         slotKFindDestroyed()       { emit active(false); }

	private:
		void         findNext(bool forward, bool sort, bool fromCurrent = false);
		EventListViewItemBase* nextItem(EventListViewItemBase*, bool forward) const;

		EventListViewBase* mListView;        // parent list view
		KFindDialog*       mDialog;
		QCheckBox*         mExpired;
		QCheckBox*         mLive;
		KSeparator*        mActiveExpiredSep;
		QCheckBox*         mMessageType;
		QCheckBox*         mFileType;
		QCheckBox*         mCommandType;
		QCheckBox*         mEmailType;
		KFind*             mFind;
		QStringList        mHistory;         // list of history items for Find dialog
		QString            mStartID;         // ID of first alarm searched if 'from cursor' was selected
		long               mOptions;         // OR of find dialog options
		bool               mFromMiddle;      // current search started in middle, not beginning or end
		bool               mNoCurrentItem;   // there is no current item for the purposes of searching
};

#endif // FIND_H

