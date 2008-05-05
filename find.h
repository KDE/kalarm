/*
 *  find.h  -  search facility 
 *  Program:  kalarm
 *  Copyright © 2005,2006,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef FIND_H
#define FIND_H

#include <qobject.h>
#include <qguardedptr.h>
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
		explicit Find(EventListViewBase* parent);
		~Find();
		void         display();
		void         findNext(bool forward)     { findNext(forward, true); }

	signals:
		void         active(bool);

	private slots:
		void         slotFind();
		void         slotKFindDestroyed()       { emit active(false); }

	private:
		void         findNext(bool forward, bool sort, bool checkEnd = false, bool fromCurrent = false);
		EventListViewItemBase* nextItem(EventListViewItemBase*, bool forward) const;

		EventListViewBase* mListView;        // parent list view
		QGuardedPtr<KFindDialog> mDialog;
		QCheckBox*         mExpired;
		QCheckBox*         mLive;
		KSeparator*        mActiveExpiredSep;
		QCheckBox*         mMessageType;
		QCheckBox*         mFileType;
		QCheckBox*         mCommandType;
		QCheckBox*         mEmailType;
		KFind*             mFind;
		QStringList        mHistory;         // list of history items for Find dialog
		QString            mLastPattern;     // pattern used in last search
		QString            mStartID;         // ID of first alarm searched if 'from cursor' was selected
		long               mOptions;         // OR of find dialog options
		bool               mNoCurrentItem;   // there is no current item for the purposes of searching
		bool               mFound;           // true if any matches have been found
};

#endif // FIND_H

