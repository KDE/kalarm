/*
 *  dirresourceimportdialog.h - configuration dialog to import directory resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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

// vim: et sw=4:
