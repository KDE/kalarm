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

#ifndef DIRRESOURCEIMPORTDIALOG_P_H
#define DIRRESOURCEIMPORTDIALOG_P_H

#include <KAlarmCal/KACalendar>
using namespace KAlarmCal;

#include <QWidget>
#include <QUrl>

class KJob;
namespace KIO { class StatJob; }
class Ui_DirResourceImportIntroWidget;
class Ui_DirResourceImportTypeWidget;

/*=============================================================================
= Base class for page widgets
*/
class DirResourceImportWidgetBase : public QWidget
{
    Q_OBJECT
public:
    explicit DirResourceImportWidgetBase(QWidget* parent = nullptr);

    virtual void setTextSizes() = 0;
};


/*=============================================================================
= Introductory page
*/
class DirResourceImportIntroWidget : public DirResourceImportWidgetBase
{
    Q_OBJECT
public:
    DirResourceImportIntroWidget(const QString& dirResourceName, const QString& dirResourcePath, KAlarmCal::CalEvent::Types types, QWidget* parent = nullptr);
    ~DirResourceImportIntroWidget() override;

    void setTextSizes() override;

private:
    Ui_DirResourceImportIntroWidget* mUi {nullptr};
};


/*=============================================================================
= Page to import one alarm type
*/
class DirResourceImportTypeWidget : public DirResourceImportWidgetBase
{
    Q_OBJECT
public:
    explicit DirResourceImportTypeWidget(CalEvent::Type, QWidget* parent = nullptr);
    ~DirResourceImportTypeWidget() override;

    ResourceId resourceId() const;
    QUrl url() const;
    QString displayName() const;
    void setUrlValidation(QString (*func)(const QUrl&));
    void setLastPage();
    void setTextSizes() override;

public Q_SLOTS:
    void validate();

Q_SIGNALS:
    void status(bool ok);

protected:
    bool eventFilter(QObject*, QEvent*) override;

private Q_SLOTS:
    void importTypeSelected();
    void slotStatJobResult(KJob*);

private:
    void setStatus(bool ok, const QString& errorMessage = QString(), bool errorColour = true);

    Ui_DirResourceImportTypeWidget* mUi {nullptr};
    QString     (*mUrlValidationFunc)(const QUrl&) {nullptr};
    KIO::StatJob* mStatJob {nullptr};
    bool          mLastPage {false};
    bool          mCheckingDir {false};

};

#endif // DIRRESOURCEIMPORTDIALOG_P_H

// vim: et sw=4:
