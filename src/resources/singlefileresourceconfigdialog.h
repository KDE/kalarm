/*
 *  singlefileresourceconfigdialog.h - configuration dialog for single file resources.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"

#include <QDialog>
#include <QUrl>

class KJob;
namespace KIO { class StatJob; }
class Ui_SingleFileResourceConfigWidget;

class SingleFileResourceConfigDialog : public QDialog
{
    Q_OBJECT
public:
    SingleFileResourceConfigDialog(bool create, QWidget* parent);
    ~SingleFileResourceConfigDialog() override;

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

// vim: et sw=4:
