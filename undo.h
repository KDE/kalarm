/*
 *  undo.h  -  edit undo facility
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

#ifndef UNDO_H
#define UNDO_H

#ifdef UNDO_FEATURE
#include <qvaluelist.h>
#include <qstringlist.h>

class KAEvent;
class UndoItem;


class Undo : public QObject
{
		Q_OBJECT
	public:
		enum Type { NONE, UNDO, REDO };

		static Undo*       instance();
		static void        saveAdd(const QString& eventID);
		static void        saveEdit(const KAEvent& oldEvent, const QString& newEventID);
		static void        saveDelete(const KAEvent&);
		static void        saveDeletes(const QValueList<KAEvent>&);
		static QString     undo()             { return undo(mUndoList.begin(), UNDO); }
		static QString     undo(int id)       { return undo(findItem(id, UNDO), UNDO); }
		static QString     redo()             { return undo(mRedoList.begin(), REDO); }
		static QString     redo(int id)       { return undo(findItem(id, REDO), REDO); }
		static void        clear();
		static bool        haveUndo()         { return !mUndoList.isEmpty(); }
		static bool        haveRedo()         { return !mRedoList.isEmpty(); }
		static QString     description(Type);
		static QString     description(Type, int id);
		static QString     tooltip(Type, int id);
		static QValueList<int> ids(Type);
		static void        emitChanged();

	signals:
		void               changed(const QString& undo, const QString& redo);

	protected:
		// Methods for use by UndoItem class
		static void        add(UndoItem*, bool undo);
		static void        remove(UndoItem*, bool undo);
		static void        replace(UndoItem* old, UndoItem* New);

	private:
		typedef QValueList<UndoItem*>           List;
		typedef QValueList<UndoItem*>::Iterator Iterator;

		Undo(QObject* parent)  : QObject(parent) { }
		static void        removeRedos(const QString& eventID);
		static QString     undo(Iterator, Type);
		static UndoItem*   getItem(int id, Type);
		static Iterator    findItem(int id, Type);
		void               emitChanged(const QString& undo, const QString& redo)
		                                   { emit changed(undo, redo); }

		static Undo*       mInstance;     // the one and only Undo instance
		static List        mUndoList;     // edit history for undo, latest undo first
		static List        mRedoList;     // edit history for redo, latest redo first

	friend class UndoItem;
	friend class UndoAdd;
	friend class UndoEdit;
	friend class UndoDelete;
	friend class UndoDeletes;
};

#endif
#endif // UNDO_H
