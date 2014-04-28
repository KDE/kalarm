/*
 *  collectionmodel.h  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2010-2012 by David Jarvie <djarvie@kde.org>
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

#include <KAlarmCal/kacalendar.h>

#include <AkonadiCore/favoritecollectionsmodel.h>
#include <kcheckableproxymodel.h>
#include <kdescendantsproxymodel.h>
#include <kdeversion.h>

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
        explicit CollectionListModel(QObject* parent = 0);
        void setEventTypeFilter(CalEvent::Type);
        void setFilterWritable(bool writable);
        void setFilterEnabled(bool enabled);
        void useCollectionColour(bool use)   { mUseCollectionColour = use; }
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        QModelIndex collectionIndex(const Akonadi::Collection&) const;
        virtual bool isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const;
        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;

    private:
        bool mUseCollectionColour;
};


/*=============================================================================
= Class: CollectionCheckListModel
= Proxy model providing a checkable list of all Collections. A Collection's
= checked status is equivalent to whether it is selected or not.
= An alarm type is specified, whereby Collections which are enabled for that
= alarm type are checked; Collections which do not contain that alarm type, or
= which are disabled for that alarm type, are unchedked.
=============================================================================*/
class CollectionCheckListModel : public KCheckableProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionCheckListModel(CalEvent::Type, QObject* parent = 0);
        ~CollectionCheckListModel();
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;
        virtual bool setData(const QModelIndex&, const QVariant& value, int role);

    signals:
        void collectionTypeChange(CollectionCheckListModel*);

    private slots:
        void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant& value, bool inserted);

    private:
        void setSelectionStatus(const Akonadi::Collection&, const QModelIndex&);

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
        explicit CollectionFilterCheckListModel(QObject* parent = 0);
        void setEventTypeFilter(CalEvent::Type);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private slots:
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
        explicit CollectionView(CollectionFilterCheckListModel*, QWidget* parent = 0);
        CollectionFilterCheckListModel* collectionModel() const  { return static_cast<CollectionFilterCheckListModel*>(model()); }
        Akonadi::Collection  collection(int row) const;
        Akonadi::Collection  collection(const QModelIndex&) const;

    protected:
        virtual void setModel(QAbstractItemModel*);
        virtual void mouseReleaseEvent(QMouseEvent*);
        virtual bool viewportEvent(QEvent*);
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

        /** Return whether a collection is enabled (and valid). */
        static bool isEnabled(const Akonadi::Collection&, CalEvent::Type);

        /** Enable or disable a collection (if it is valid) for specified alarm types.
         *  Note that this only changes the status for the specified alarm types.
         *  @return alarm types which can be enabled
         */
        static CalEvent::Types setEnabled(const Akonadi::Collection&, CalEvent::Types, bool enabled);

        /** Return whether a collection is both enabled and fully writable for a
         *  given alarm type, i.e. with create/delete/change rights and compatible
         *  with the current KAlarm calendar format.
         *  Optionally, the enabled status can be ignored.
         *
         *  @return 1 = fully enabled and writable,
         *          0 = enabled and writable except that backend calendar is in an
         *              old KAlarm format,
         *         -1 = read-only, disabled or incompatible format.
         */
        static int isWritableEnabled(const Akonadi::Collection&, CalEvent::Type);

        /** Return whether a collection is both enabled and fully writable for a
         *  given alarm type, i.e. with create/delete/change rights and compatible
         *  with the current KAlarm calendar format.
         *  Optionally, the enabled status can be ignored.
         *
         *  @param format  If the reply is false, and the calendar is not read-only
         *                 but its backend calendar storage format is not the
         *                 current KAlarm format, @p format is set to the calendar
         *                 format used by the backend. If the calendar is
         *                 non-writable for any other reason, @p format is set
         *                 to KACalendar::Current.
         *  @return 1 = fully enabled and writable,
         *          0 = enabled and writable except that backend calendar is in an
         *              old KAlarm format,
         *         -1 = read-only (if @p format == KACalendar::Current), or
         *              disabled or incompatible format.
         */
        static int isWritableEnabled(const Akonadi::Collection&, CalEvent::Type, KACalendar::Compat& format);

        /** Return the standard collection for a specified mime type.
         *  @param useDefault false to return the defined standard collection, if any;
         *                    true to return the standard or only collection for the type.
         *  Reply = invalid collection if there is no standard collection.
         */
        static Akonadi::Collection getStandard(CalEvent::Type, bool useDefault = false);

        /** Return whether a collection is the standard collection for a specified
         *  mime type. */
        static bool isStandard(Akonadi::Collection&, CalEvent::Type);

        /** Return the alarm type(s) for which a collection is the standard collection.
         *  @param useDefault false to return the defined standard types, if any;
         *                    true to return the types for which it is the standard or
         *                    only collection.
         */
        static CalEvent::Types standardTypes(const Akonadi::Collection&, bool useDefault = false);

        /** Set or clear a collection as the standard collection for a specified
         *  mime type. This does not affect its status for other mime types.
         */
        static void setStandard(Akonadi::Collection&, CalEvent::Type, bool standard);

        /** Set which mime types a collection is the standard collection for.
         *  Its standard status is cleared for other mime types.
         */
        static void setStandard(Akonadi::Collection&, CalEvent::Types);

        /** Set whether the user should be prompted for the destination collection
         *  to add alarms to.
         *  @param ask true = prompt for which collection to add to;
         *             false = add to standard collection.
         */
        static void setAskDestinationPolicy(bool ask)  { mAskDestination = ask; }

        /** Find the collection to be used to store an event of a given type.
         *  This will be the standard collection for the type, but if this is not valid,
         *  the user will be prompted to select a collection.
         *  @param noPrompt  don't prompt the user even if the standard collection is not valid
         *  @param cancelled If non-null: set to true if the user cancelled
         *             the prompt dialogue; set to false if any other error.
         */
        static Akonadi::Collection destination(CalEvent::Type, QWidget* promptParent = 0, bool noPrompt = false, bool* cancelled = 0);

        /** Return the enabled collections which contain a specified mime type.
         *  If 'writable' is true, only writable collections are included.
         */
        static Akonadi::Collection::List enabledCollections(CalEvent::Type, bool writable);

        /** Return the collection ID for a given resource ID.
         *  @return  collection ID, or -1 if the resource is not in KAlarm's list.
         */
        static Akonadi::Collection collectionForResource(const QString& resourceId);

#if KDE_IS_VERSION(4,9,80)
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
#endif

        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;

        /** Return a bulleted list of alarm types for inclusion in an i18n message. */
        static QString typeListForDisplay(CalEvent::Types);

    private slots:
        void statusChanged(const Akonadi::Collection&, AkonadiModel::Change, const QVariant& value, bool inserted);
        void collectionPopulated();

    private:
        explicit CollectionControlModel(QObject* parent = 0);
        void findEnabledCollections(const Akonadi::EntityMimeTypeFilterModel*, const QModelIndex& parent, Akonadi::Collection::List&) const;
        CalEvent::Types setEnabledStatus(const Akonadi::Collection&, CalEvent::Types, bool inserted);
        static CalEvent::Types checkTypesToEnable(const Akonadi::Collection&, const Akonadi::Collection::List&, CalEvent::Types);

        static CollectionControlModel* mInstance;
        static bool mAskDestination;
        QEventLoop* mPopulatedCheckLoop;
};

#endif // COLLECTIONMODEL_H

// vim: et sw=4:
