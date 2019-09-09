/*
 *  calendardatamodel.h  -  base for models containing calendars and events
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#ifndef CALENDARDATAMODEL_H
#define CALENDARDATAMODEL_H

#include "kalarm.h"

#include <kalarmcal/kacalendar.h>

#include <QSize>

class QModelIndex;
class QPixmap;

namespace KAlarmCal { class KAEvent; }

using namespace KAlarmCal;


/*=============================================================================
= Class: CalendarDataModel
= Base class for models containing all calendars and events.
=============================================================================*/
class CalendarDataModel
{
    public:
        enum {   // data columns
            // Item columns
            TimeColumn = 0, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, TextColumn,
            TemplateNameColumn,
            ColumnCount
        };
        enum {   // additional data roles
            // Calendar roles
            EnabledTypesRole = UserRole, // alarm types which are enabled for the collection
            BaseColourRole,            // background colour ignoring collection colour
            AlarmTypeRole,             // OR of event types which collection contains
            IsStandardRole,            // OR of event types which collection is standard for
            KeepFormatRole,            // user has chosen not to update collection's calendar storage format
            // Event roles
            EnabledRole,               // true for enabled alarm, false for disabled
            StatusRole,                // KAEvent::ACTIVE/ARCHIVED/TEMPLATE
            AlarmActionsRole,          // KAEvent::Actions
            AlarmSubActionRole,        // KAEvent::Action
            ValueRole,                 // numeric value
            SortRole,                  // the value to use for sorting
            TimeDisplayRole,           // time column value with '~' representing omitted leading zeroes
            ColumnTitleRole,           // column titles (whether displayed or not)
            CommandErrorRole           // last command execution error for alarm (per user)
        };

        ~CalendarDataModel();

    public:
        static QSize   iconSize()       { return mIconSize; }

    protected:
        CalendarDataModel();

        static QVariant headerData(int section, Qt::Orientation, int role, bool eventHeaders, bool& handled);

        QVariant eventData(const QModelIndex& index, int role, const KAEvent& event, bool& calendarColour, bool& handled) const;

        /** Get the foreground color for a resource, based on specified mime types. */
        static QColor foregroundColor(CalEvent::Type alarmType, bool readOnly);

        /** Return the storage type (file/directory/URL etc.) for a resource. */
        QString storageTypeForLocation(const QString& location) const;

        /** Get the tooltip for a resource. The resource's enabled status is
         *  evaluated for specified alarm types. */
        static QString tooltip(bool writable, bool inactive, const QString& name, const QString& type, const QString& locn, const QString& disabled, const QString& readonly);

        /** Return the read-only status tooltip for a resource.
         * A null string is returned if the resource is fully writable. */
        static QString readOnlyTooltip(KACalendar::Compat compat, int writable);

        static QString  repeatText(const KAEvent&);
        static QString  repeatOrder(const KAEvent&);
        static QString  whatsThisText(int column);
        static QPixmap* eventIcon(const KAEvent&);

    private:
        static QPixmap* mTextIcon;
        static QPixmap* mFileIcon;
        static QPixmap* mCommandIcon;
        static QPixmap* mEmailIcon;
        static QPixmap* mAudioIcon;
        static QSize    mIconSize;      // maximum size of any icon
};

#endif // CALENDARDATAMODEL_H

// vim: et sw=4:
