/*
 *  dirresourceimportdialog.h - configuration dialog to import directory resources
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#ifndef DIRRESOURCEIMPORTDIALOG_H
#define DIRRESOURCEIMPORTDIALOG_H

#include "resource.h"

#include <KAlarmCal/KACalendar>

#include <KAssistantDialog>

#include <QUrl>

namespace KIO { class StatJob; }
class DirResourceImportIntroWidget;
class DirResourceImportTypeWidget;
struct TypeData;

class DirResourceImportDialog : public KAssistantDialog
{
    Q_OBJECT
public:
    DirResourceImportDialog(const QString& dirResourceName, const QString& dirResourcePath, KAlarmCal::CalEvent::Types types, QWidget* parent);
    ~DirResourceImportDialog();

    /** Return the existing resource to import into, for a specified alarm type.
     *  @return resource ID, or -1 if not importing into an existing resource.
     */
    ResourceId resourceId(KAlarmCal::CalEvent::Type) const;

    /** Return the new resource file URL, for a specified alarm type. */
    QUrl url(KAlarmCal::CalEvent::Type) const;

    /** Return the new resource's display name, for a specified alarm type. */
    QString displayName(KAlarmCal::CalEvent::Type) const;

    /** Set a function to validate the entered URL.
     *  The function should return an error text to display to the user, or
     *  empty string if no error.
     */
    void setUrlValidation(QString (*func)(const QUrl&));

private Q_SLOTS:
    void pageChanged(KPageWidgetItem* current, KPageWidgetItem* before);
    void typeStatusChanged(bool ok);

private:
    const DirResourceImportTypeWidget* typePage(KAlarmCal::CalEvent::Type) const;

    const KAlarmCal::CalEvent::Types mAlarmTypes;  // alarm types contained in directory resource
    int                              mAlarmTypeCount;  // number of alarm types in mAlarmTypes
    DirResourceImportIntroWidget* mPageIntro {nullptr};
    DirResourceImportTypeWidget*  mPageActive {nullptr};
    DirResourceImportTypeWidget*  mPageArchived {nullptr};
    DirResourceImportTypeWidget*  mPageTemplate {nullptr};
    DirResourceImportTypeWidget*  mLastPage {nullptr};

    friend struct TypeData;
};

#endif // DIRRESOURCEIMPORTDIALOG_H

// vim: et sw=4:
