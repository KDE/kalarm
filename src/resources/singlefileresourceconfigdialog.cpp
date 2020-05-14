/*
 *  singlefileresourceconfigdialog.cpp - configuration dialog for single file resources.
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

#include "singlefileresourceconfigdialog.h"
#include "ui_singlefileresourceconfigdialog.h"

#include <KColorScheme>
#include <KLocalizedString>
#include <KIO/Job>

#include <QUrl>
#include <QTimer>

using namespace KAlarmCal;

namespace
{
void setTextEditHeight(KTextEdit*, QGroupBox*);
}


SingleFileResourceConfigDialog::SingleFileResourceConfigDialog(bool create, QWidget* parent)
    : QDialog(parent)
    , mCreating(create)
{
    mUi = new Ui_SingleFileResourceConfigWidget;
    mUi->setupUi(this);
    setWindowTitle(i18nc("@title:window", "Configure Calendar"));
    if (mCreating)
    {
        mUi->pathText->setVisible(false);
        mUi->alarmTypeLabel->setVisible(false);

        mUi->pathRequester->setMode(KFile::File);
        mUi->pathRequester->setFilter(QStringLiteral("*.ics|%1").arg(i18nc("@item:inlistbox File type selection filter", "Calendar Files")));
        mUi->pathRequester->setFocus();
        mUi->statusLabel->setText(QString());
        connect(mUi->pathRequester, &KUrlRequester::textChanged, this, &SingleFileResourceConfigDialog::validate);
        connect(mUi->alarmTypeGroup, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), this, &SingleFileResourceConfigDialog::validate);
    }
    else
    {
        mUi->pathRequester->setVisible(false);
        mUi->statusLabel->setVisible(false);
        mUi->pathDescription->setVisible(false);
        mUi->activeRadio->setVisible(false);
        mUi->archivedRadio->setVisible(false);
        mUi->templateRadio->setVisible(false);
        mUi->alarmTypeDescription->setVisible(false);

        mUi->displayNameText->setFocus();
    }

    connect(mUi->displayNameText, &QLineEdit::textChanged, this, &SingleFileResourceConfigDialog::validate);
    connect(mUi->buttonBox, &QDialogButtonBox::rejected, this, &SingleFileResourceConfigDialog::close);
    connect(mUi->buttonBox, &QDialogButtonBox::accepted, this, &SingleFileResourceConfigDialog::accept);
    QTimer::singleShot(0, this, &SingleFileResourceConfigDialog::validate);
}

SingleFileResourceConfigDialog::~SingleFileResourceConfigDialog()
{
    delete mUi;
}

void SingleFileResourceConfigDialog::setUrl(const QUrl& url, bool readOnly)
{
    if (mCreating)
    {
        mUi->pathRequester->setUrl(url);
        if (readOnly)
        {
            mUi->pathRequester->lineEdit()->setEnabled(false);
            mUi->pathRequester->button()->setVisible(false);
            mUi->statusLabel->setVisible(false);
            mUi->activeRadio->setEnabled(false);
            mUi->archivedRadio->setEnabled(false);
            mUi->templateRadio->setEnabled(false);
            enableOkButton();
        }
    }
    else
    {
        mUi->pathText->setText(url.toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile));
    }
}

QUrl SingleFileResourceConfigDialog::url() const
{
    return mCreating ? mUi->pathRequester->url() : QUrl();
}

QString SingleFileResourceConfigDialog::displayName() const
{
    return mUi->displayNameText->text();
}

void SingleFileResourceConfigDialog::setDisplayName(const QString& name)
{
    mUi->displayNameText->setText(name);
}

bool SingleFileResourceConfigDialog::readOnly() const
{
    return mUi->readOnlyCheckbox->isChecked();
}

void SingleFileResourceConfigDialog::setReadOnly(bool readonly)
{
    mUi->readOnlyCheckbox->setChecked(readonly);
}

CalEvent::Type SingleFileResourceConfigDialog::alarmType() const
{
    if (mCreating)
    {
        if (mUi->activeRadio->isChecked())
            return CalEvent::ACTIVE;
        if (mUi->archivedRadio->isChecked())
            return CalEvent::ARCHIVED;
        if (mUi->templateRadio->isChecked())
            return CalEvent::TEMPLATE;
    }
    return CalEvent::EMPTY;
}

void SingleFileResourceConfigDialog::setAlarmType(CalEvent::Type type)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
            if (mCreating)
                mUi->activeRadio->setChecked(true);
            else
                mUi->activeAlarmsText->setVisible(true);
            break;
        case CalEvent::ARCHIVED:
            if (mCreating)
                mUi->archivedRadio->setChecked(true);
            else
                mUi->archivedAlarmsText->setVisible(true);
            break;
        case CalEvent::TEMPLATE:
            if (mCreating)
                mUi->templateRadio->setChecked(true);
            else
                mUi->templateAlarmsText->setVisible(true);
            break;
        default:
            break;
    }
}

void SingleFileResourceConfigDialog::setUrlValidation(QString (*func)(const QUrl&))
{
    if (mCreating)
        mUrlValidationFunc = func;
}

/******************************************************************************
* Validate the current user input. If invalid, disable the OK button.
*/
void SingleFileResourceConfigDialog::validate()
{
    // Validate URL first, in order to display any error message.
    const QUrl currentUrl = mUi->pathRequester->url();
    if (mUi->pathRequester->text().trimmed().isEmpty() || currentUrl.isEmpty())
    {
        disableOkButton(QString());
        return;
    }
    if (mUrlValidationFunc)
    {
        const QString error = (*mUrlValidationFunc)(currentUrl);
        if (!error.isEmpty())
        {
            disableOkButton(error, true);
            return;
        }
    }

    if (mUi->displayNameText->text().trimmed().isEmpty()
    ||  (mCreating  &&  !mUi->alarmTypeGroup->checkedButton()))
    {
        disableOkButton(QString());
        return;
    }
    if (!mCreating)
    {
        enableOkButton();
        return;
    }

    if (currentUrl.isLocalFile())
        enableOkButton();
    else
    {
        // It's a remote file.
        // Check whether the file can be read or written.
        if (mStatJob)
            mStatJob->kill();
        mCheckingDir = false;
        initiateUrlStatusCheck(currentUrl);

        // Disable the OK button until the file's status is determined.
        disableOkButton(i18nc("@info:status", "Checking file information..."));
    }
}

/******************************************************************************
* Called when the status of the remote URL has been determined.
* Checks whether the URL is accessible.
*/
void SingleFileResourceConfigDialog::slotStatJobResult(KJob* job)
{
    if (job->error())
    {
        if (job->error() == KIO::ERR_DOES_NOT_EXIST  &&  !mCheckingDir)
        {
            // The file doesn't exist, so check if the file's directory is writable.
            mCheckingDir = true;
            initiateUrlStatusCheck(KIO::upUrl(mUi->pathRequester->url()));
            return;
        }

        // Can't read or write the URL.
        disableOkButton(QString());
    }
    else
        enableOkButton();
    mCheckingDir = false;
    mStatJob = nullptr;
}

/******************************************************************************
* Creates a job to check the status of a remote URL.
*/
void SingleFileResourceConfigDialog::initiateUrlStatusCheck(const QUrl& url)
{
    mStatJob = KIO::statDetails(url,
                                KIO::StatJob::SourceSide,
                                KIO::StatDetail::StatDefaultDetails,
                                KIO::HideProgressInfo);
    connect(mStatJob, &KIO::StatJob::result, this, &SingleFileResourceConfigDialog::slotStatJobResult);
}

/******************************************************************************
* Enable the OK button, and clear the URL status message.
*/
void SingleFileResourceConfigDialog::enableOkButton()
{
    mUi->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    mUi->statusLabel->setText(QString());
}

/******************************************************************************
* Disable the OK button, and set the URL status message.
*/
void SingleFileResourceConfigDialog::disableOkButton(const QString& statusMessage, bool errorColour)
{
    mUi->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    QPalette pal = mUi->pathLabel->palette();
    if (errorColour)
        pal.setColor(QPalette::WindowText, KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color());
    mUi->statusLabel->setPalette(pal);
    mUi->statusLabel->setText(statusMessage);
}

/******************************************************************************
* When the dialog is displayed, set appropriate heights for KTextEdit elements,
* and then remove empty space between widgets.
* By default, KTextEdit has a minimum height of 4 text lines, and calling
* setMinimumHeight() doesn't affect this.
*/
void SingleFileResourceConfigDialog::showEvent(QShowEvent* se)
{
    setTextEditHeight(mUi->nameDescription, mUi->nameGroupBox);
    setTextEditHeight(mUi->readOnlyDescription, mUi->readOnlyGroupBox);
    if (mCreating)
    {
        setTextEditHeight(mUi->pathDescription, mUi->pathGroupBox);
        setTextEditHeight(mUi->alarmTypeDescription, mUi->alarmTypeGroupBox);
    }
    else
        mUi->pathDescription->setFixedHeight(1);
    setFixedHeight(sizeHint().height());
    QDialog::showEvent(se);
}

namespace
{

void setTextEditHeight(KTextEdit* textEdit, QGroupBox* groupBox)
{
    const QSize size = textEdit->document()->size().toSize();
    const int margin = textEdit->height() - textEdit->viewport()->height();
    textEdit->setFixedHeight(size.height() + margin);
    groupBox->setFixedHeight(groupBox->sizeHint().height());
}

}

// vim: et sw=4:
