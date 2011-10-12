/*
 *  kalarmdirresource.h  -  Akonadi directory resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef KALARMDIRRESOURCE_H
#define KALARMDIRRESOURCE_H

#include "kaevent.h"
#include <akonadi/resourcebase.h>
#include <QHash>

namespace Akonadi_KAlarm_Dir_Resource { class Settings; }
using KAlarm::KAEvent;
using KAlarm::KACalendar;

class KAlarmDirResource : public Akonadi::ResourceBase, public Akonadi::AgentBase::Observer
{
        Q_OBJECT
    public:
        KAlarmDirResource(const QString& id);
        ~KAlarmDirResource();

    public Q_SLOTS:
        virtual void configure(WId windowId);
        virtual void aboutToQuit();

    protected Q_SLOTS:
        void retrieveCollections();
        void retrieveItems(const Akonadi::Collection&);
        bool retrieveItem(const Akonadi::Item&, const QSet<QByteArray>& parts);

    protected:
        virtual void collectionChanged(const Akonadi::Collection&);
        virtual void itemAdded(const Akonadi::Item&, const Akonadi::Collection&);
        virtual void itemChanged(const Akonadi::Item&, const QSet<QByteArray>& parts);
        virtual void itemRemoved(const Akonadi::Item&);

    private Q_SLOTS:
        void    settingsChanged();
        void    fileCreated(const QString& path);
        void    fileChanged(const QString& path);
        void    fileDeleted(const QString& path);
        void    loadFiles()         { loadFiles(true); }
        void    collectionsReceived(const Akonadi::Collection::List&);
        void    collectionFetchResult(KJob*);
        void    jobDone(KJob*);

    private:
        void   changeAlarmTypes(KAlarm::CalEvent::Types removed);
        bool    loadFiles(bool sync);
        KAEvent loadFile(const QString& path, const QString& file);
        KAEvent loadNextFile(const QString& eventId, const QString& file);
        QString directoryName() const;
        QString filePath(const QString& file) const;
        QString fileName(const QString& path) const;
        void    initializeDirectory() const;
        void    setNameRights(Akonadi::Collection&);
        bool    cancelIfReadOnly();
        bool    writeToFile(const KAEvent&);
        void    setCompatibility(bool writeAttr = true);
        void    removeEvent(const QString& eventId, bool deleteFile);
        void    addEventFile(const KAEvent&, const QString& file);
        QString removeEventFile(const QString& eventId, const QString& file, KAEvent* = 0);
        bool    createItemAndIndex(const QString& path, const QString& file);
        bool    createItem(const KAEvent&);
        bool    modifyItem(const KAEvent&);
        void    deleteItem(const KAEvent&);

        struct EventFile    // data to be indexed by event ID
        {
            EventFile() {}
            EventFile(const KAEvent& e, const QStringList& f) : event(e), files(f) {}
            KAEvent     event;
            QStringList files;   // files containing this event ID, in-use one first
        };
        QHash<QString, EventFile> mEvents;         // cached alarms and file names, indexed by ID
        QHash<QString, QString>   mFileEventIds;   // alarm IDs, indexed by file name
        Akonadi_KAlarm_Dir_Resource::Settings* mSettings;
        Akonadi::Collection::Id   mCollectionId;   // ID of this resource's collection
        KACalendar::Compat        mCompatibility;
        int                       mVersion;        // calendar format version
        QStringList               mChangedFiles;   // files being written to
        bool                      mCollectionFetched;  // mCollectionId has been initialised
};

#endif

// vim: et sw=4:
