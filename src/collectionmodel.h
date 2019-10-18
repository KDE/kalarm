/*
 *  collectionmodel.h  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2010-2019 David Jarvie <djarvie@kde.org>
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

#ifndef COLLECTIONMODEL_H
#define COLLECTIONMODEL_H

#include "akonadimodel.h"

#include <kalarmcal/kacalendar.h>

#include <AkonadiCore/favoritecollectionsmodel.h>
#include <kcheckableproxymodel.h>
#include <kdescendantsproxymodel.h>

#include <QSortFilterProxyModel>
#include <QListView>

using namespace KAlarmCal;

class QEventLoop;
namespace Akonadi
{
    class EntityMimeTypeFilterModel;
}

/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the AkonadiModel collection tree into a flat list.
= The model may be restricted to specified content mime types.
= It can optionally be restricted to writable and/or enabled Collections.
=============================================================================*/
class CollectionListModel : public KDescendantsProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionListModel(QObject* parent = nullptr);
        void setEventTypeFilter(CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        void useCollectionColour(bool use)   { mUseCollectionColour = use; }
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        Resource    resource(int row) const;
        Resource    resource(const QModelIndex&) const;
        QModelIndex resourceIndex(const Resource&) const;
        virtual bool isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const;
        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    private:
        bool mUseCollectionColour;
};


/*=============================================================================
= Class: CollectionCheckListModel
= Proxy model providing a checkable list of all Collections. A Collection's
= checked status is equivalent to whether it is selected or not.
= An alarm type is specified, whereby Collections which are enabled for that
= alarm type are checked; Collections which do not contain that alarm type, or
= which are disabled for that alarm type, are unchecked.
=============================================================================*/
class CollectionCheckListModel : public KCheckableProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionCheckListModel(CalEvent::Type, QObject* parent = nullptr);
        ~CollectionCheckListModel();
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        Resource  resource(int row) const;
        Resource  resource(const QModelIndex&) const;
        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
        bool setData(const QModelIndex&, const QVariant& value, int role) override;

    Q_SIGNALS:
        void collectionTypeChange(CollectionCheckListModel*);

    private Q_SLOTS:
        void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void resourceStatusChanged(const Resource&, AkonadiModel::Change, const QVariant& value, bool inserted);

    private:
        void setSelectionStatus(const Resource&, const QModelIndex&);
        QByteArray debugType(const char* func) const;

        static CollectionListModel* mModel;
        static int             mInstanceCount;
        CalEvent::Type mAlarmType;     // alarm type contained in this model
        QItemSelectionModel*   mSelectionModel;
};


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list. The model contains all
= alarm types, but returns only one type at any given time. The selected alarm
= type may be changed as desired.
=============================================================================*/
class CollectionFilterCheckListModel : public QSortFilterProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionFilterCheckListModel(QObject* parent = nullptr);
        void setEventTypeFilter(CalEvent::Type);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        Resource  resource(int row) const;
        Resource  resource(const QModelIndex&) const;
        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    private Q_SLOTS:
        void collectionTypeChanged(CollectionCheckListModel*);

    private:
        CollectionCheckListModel* mActiveModel;
        CollectionCheckListModel* mArchivedModel;
        CollectionCheckListModel* mTemplateModel;
        CalEvent::Type    mAlarmType;     // alarm type contained in this model
};


/*=============================================================================
= Class: CollectionView
= View for a CollectionFilterCheckListModel.
=============================================================================*/
class CollectionView : public QListView
{
        Q_OBJECT
    public:
        explicit CollectionView(CollectionFilterCheckListModel*, QWidget* parent = nullptr);
        CollectionFilterCheckListModel* collectionModel() const  { return static_cast<CollectionFilterCheckListModel*>(model()); }
        Akonadi::Collection  collection(int row) const;
        Akonadi::Collection  collection(const QModelIndex&) const;
        Resource  resource(int row) const;
        Resource  resource(const QModelIndex&) const;

    protected:
        void setModel(QAbstractItemModel*) override;
        void mouseReleaseEvent(QMouseEvent*) override;
        bool viewportEvent(QEvent*) override;
};


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
= This model also controls which collections are standard for their type,
= ensuring that there is only one standard collection for any given type.
=============================================================================*/
class CollectionControlModel : public Akonadi::FavoriteCollectionsModel
{
        Q_OBJECT
    public:
        static CollectionControlModel* instance();

#if 0
        /** Return whether a collection is enabled (and valid). */
        static bool isEnabled(const Akonadi::Collection&, CalEvent::Type);

        /** Enable or disable a collection (if it is valid) for specified alarm types.
         *  Note that this only changes the status for the specified alarm types.
         *  @return alarm types which can be enabled
         */
        static CalEvent::Types setEnabled(const Akonadi::Collection&, CalEvent::Types, bool enabled);
#endif

        /** Return the standard collection for a specified mime type.
         *  If the mime type is 'archived' and there is no standard collection, the
         *  only writable archived collection is set to be the standard.
         *  @param type  The mime type
         *  Reply = invalid collection if there is no standard collection.
         */
        static Resource getStandard(CalEvent::Type type);

        /** Return whether a collection is the standard collection for a specified
         *  mime type. */
        static bool isStandard(const Resource&, CalEvent::Type);

        /** Return the alarm type(s) for which a collection is the standard collection.
         *  @param useDefault false to return the defined standard types, if any;
         *                    true to return the types for which it is the standard or
         *                    only collection.
         */
        static CalEvent::Types standardTypes(const Resource&, bool useDefault = false);

        /** Set or clear a collection as the standard collection for a specified
         *  mime type. This does not affect its status for other mime types.
         */
        static void setStandard(Resource&, CalEvent::Type, bool standard);

        /** Set which mime types a collection is the standard collection for.
         *  Its standard status is cleared for other mime types.
         */
        static void setStandard(Resource&, CalEvent::Types);

        /** Find the collection to be used to store an event of a given type.
         *  This will be the standard collection for the type, but if this is not valid,
         *  the user will be prompted to select a collection.
         *  @param type         The event type
         *  @param promptParent The parent widget for the prompt
         *  @param noPrompt     Don't prompt the user even if the standard collection is not valid
         *  @param cancelled    If non-null: set to true if the user cancelled the
         *                      prompt dialogue; set to false if any other error
         */
        static Resource destination(CalEvent::Type type, QWidget* promptParent = nullptr, bool noPrompt = false, bool* cancelled = nullptr);

        /** Return a list of all collections, both enabled and disabled.
         *  Collections which don't belong to a resource are omitted.
         *  @param type  Return only collections for this event type, or EMPTY for all.
         */
        static Akonadi::Collection::List allCollections(CalEvent::Type type = CalEvent::EMPTY);

        /** Return the enabled collections which contain a specified mime type.
         *  If 'writable' is true, only writable collections are included.
         */
        static Akonadi::Collection::List enabledCollections(CalEvent::Type, bool writable);

        /** Return the collection ID for a given resource ID.
         *  @return  collection ID, or -1 if the resource is not in KAlarm's list.
         */
        static Akonadi::Collection collectionForResource(const QString& resourceId);

        /** Return whether one or all enabled collections have been populated,
         *  i.e. whether their items have been fetched.
         */
        static bool isPopulated(Akonadi::Collection::Id);

        /** Wait until one or all enabled collections have been populated,
         *  i.e. whether their items have been fetched.
         *  @param   colId    collection ID, or -1 for all collections
         *  @param   timeout  timeout in seconds, or 0 for no timeout
         *  @return  true if successful.
         */
        bool waitUntilPopulated(Akonadi::Collection::Id colId = -1, int timeout = 0);

        /** Check for, and remove, Akonadi resources which duplicate use of
         *  calendar files/directories.
         */
        static void removeDuplicateResources();

        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    private Q_SLOTS:
        void reset();
        void resourceStatusChanged(Resource&, AkonadiModel::Change, const QVariant& value, bool inserted);
        void collectionPopulated();
        void collectionFetchResult(KJob*);

    private:
        explicit CollectionControlModel(QObject* parent = nullptr);
        void findEnabledCollections(const Akonadi::EntityMimeTypeFilterModel*, const QModelIndex& parent, QList<Akonadi::Collection::Id>&) const;
        CalEvent::Types setEnabledStatus(Resource&, CalEvent::Types, bool inserted);
        static CalEvent::Types checkTypesToEnable(const Resource&, const QList<Akonadi::Collection::Id>&, CalEvent::Types);

        static CollectionControlModel* mInstance;
        struct ResourceCol
        {
            QString                 resourceId;
            Akonadi::Collection::Id collectionId;
            ResourceCol() {}
            ResourceCol(const QString& r, Akonadi::Collection::Id c)
                : resourceId(r), collectionId(c) {}
        };
        static QHash<QString, ResourceCol> mAgentPaths;   // path, (resource identifier, collection ID) pairs
        QEventLoop* mPopulatedCheckLoop;
};

#endif // COLLECTIONMODEL_H

// vim: et sw=4:
