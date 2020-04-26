/*
 *  singlefileresourceconfigdialog.h - configuration dialog for single file resources.
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

#ifndef SINGLEFILERESOURCECONFIGDIALOG_H
#define SINGLEFILERESOURCECONFIGDIALOG_H

#include <KAlarmCal/KACalendar>

#include <QDialog>
#include <QUrl>

class KJob;
namespace KIO { class StatJob; }
class Ui_SingleFileResourceConfigWidget;

class SingleFileResourceConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SingleFileResourceConfigDialog(bool create, QWidget* parent);
    ~SingleFileResourceConfigDialog();

    /** Return the file URL. */
    QUrl url() const;

    /** Set the file URL. */
    void setUrl(const QUrl& url, bool readOnly = false);

    /** Return the resource's display name. */
    QString displayName() const;

    /** Set the resource's display name. */
    void setDisplayName(const QString& name);

    /** Return whether the resource is read-only. */
    bool readOnly() const;

    /** Set the read-only status of the resource. */
    void setReadOnly(bool readonly);

    /** Return the resource's alarm type. */
    KAlarmCal::CalEvent::Type alarmType() const;

    /** Set the resource's alarm type. */
    void setAlarmType(KAlarmCal::CalEvent::Type);

    /** Set a function to validate the entered URL.
     *  The function should return an error text to display to the user, or
     *  empty string if no error.
     */
    void setUrlValidation(QString (*func)(const QUrl&));

protected:
    void showEvent(QShowEvent*) override;

private Q_SLOTS:
    void validate();
    void slotStatJobResult(KJob*);

private:
    void initiateUrlStatusCheck(const QUrl&);
    void enableOkButton();
    void disableOkButton(const QString& statusMessage, bool errorColour = false);

    Ui_SingleFileResourceConfigWidget* mUi {nullptr};
    QString     (*mUrlValidationFunc)(const QUrl&) {nullptr};
    KIO::StatJob* mStatJob {nullptr};
    const bool    mCreating;              // whether creating or editing the resource
    bool          mCheckingDir {false};
};

#endif // SINGLEFILERESOURCECONFIGDIALOG_H

// vim: et sw=4:
