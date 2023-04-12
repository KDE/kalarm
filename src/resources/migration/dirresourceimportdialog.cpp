/*
 *  dirresourceimportdialog.cpp - configuration dialog to import directory resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dirresourceimportdialog.h"
#include "dirresourceimportdialog_p.h"
#include "ui_dirresourceimportdialog_intro.h"
#include "ui_dirresourceimportdialog_type.h"

#include "resources/resources.h"
#include "resources/fileresource.h"

#include <KFileItem>
#include <KColorScheme>
#include <KLocalizedString>
#include <KIO/StatJob>

#include <QTimer>

using namespace KAlarmCal;


/******************************************************************************
* Constructor.
*/
DirResourceImportDialog::DirResourceImportDialog(const QString& dirResourceName, const QString& dirResourcePath, KAlarmCal::CalEvent::Types types, QWidget* parent)
    : KAssistantDialog(parent)
    , mAlarmTypes(types)
{
    setWindowTitle(i18nc("@title:window", "Import Directory Resource"));
    // Remove Help button
    buttonBox()->removeButton(button(QDialogButtonBox::Help));

    mPageIntro = new DirResourceImportIntroWidget(dirResourceName, dirResourcePath, types, this);
    addPage(mPageIntro, i18nc("@title:tab", "Import Calendar Directory Resource"));

    mAlarmTypeCount = 0;
    if (mAlarmTypes & CalEvent::ACTIVE)
    {
        ++mAlarmTypeCount;
        mPageActive = new DirResourceImportTypeWidget(CalEvent::ACTIVE, this);
        addPage(mPageActive, i18nc("@title:tab", "Import Active Alarms"));
        connect(mPageActive, &DirResourceImportTypeWidget::status, this, &DirResourceImportDialog::typeStatusChanged);
        mLastPage = mPageActive;
    }

    if (mAlarmTypes & CalEvent::ARCHIVED)
    {
        ++mAlarmTypeCount;
        mPageArchived = new DirResourceImportTypeWidget(CalEvent::ARCHIVED, this);
        addPage(mPageArchived, i18nc("@title:tab", "Import Archived Alarms"));
        connect(mPageArchived, &DirResourceImportTypeWidget::status, this, &DirResourceImportDialog::typeStatusChanged);
        mLastPage = mPageArchived;
    }

    if (mAlarmTypes & CalEvent::TEMPLATE)
    {
        ++mAlarmTypeCount;
        mPageTemplate = new DirResourceImportTypeWidget(CalEvent::TEMPLATE, this);
        addPage(mPageTemplate, i18nc("@title:tab", "Import Alarm Templates"));
        connect(mPageTemplate, &DirResourceImportTypeWidget::status, this, &DirResourceImportDialog::typeStatusChanged);
        mLastPage = mPageTemplate;
    }

    connect(this, &KPageDialog::currentPageChanged, this, &DirResourceImportDialog::pageChanged);
}

DirResourceImportDialog::~DirResourceImportDialog()
{
}

/******************************************************************************
* Return the existing resource to import into, for a specified alarm type.
*/
ResourceId DirResourceImportDialog::resourceId(KAlarmCal::CalEvent::Type type) const
{
    const DirResourceImportTypeWidget* page = typePage(type);
    return page ? page->resourceId() : -1;
}

/******************************************************************************
* Return the new calendar file URL, for a specified alarm type.
*/
QUrl DirResourceImportDialog::url(KAlarmCal::CalEvent::Type type) const
{
    const DirResourceImportTypeWidget* page = typePage(type);
    return page ? page->url() : QUrl();
}

/******************************************************************************
* Return the resource's display name, for a specified alarm type.
*/
QString DirResourceImportDialog::displayName(KAlarmCal::CalEvent::Type type) const
{
    const DirResourceImportTypeWidget* page = typePage(type);
    return page ? page->displayName() : QString();
}

/******************************************************************************
* Set a validation function to apply to an entered URL.
*/
void DirResourceImportDialog::setUrlValidation(QString (*func)(const QUrl&))
{
    if (mPageActive)
        mPageActive->setUrlValidation(func);
    if (mPageArchived)
        mPageArchived->setUrlValidation(func);
    if (mPageTemplate)
        mPageTemplate->setUrlValidation(func);
}

/******************************************************************************
* When a new page is displayed, set appropriate heights for elements within the
* page, and enable/disable elements according to their status.
*/
void DirResourceImportDialog::pageChanged(KPageWidgetItem* current, KPageWidgetItem* before)
{
    Q_UNUSED(before);

    if (current)
    {
        auto page = static_cast<DirResourceImportWidgetBase*>(current->widget());
        page->setTextSizes();
        auto typePage = qobject_cast<DirResourceImportTypeWidget*>(page);
        if (typePage)
            typePage->validate();
    }
}

/******************************************************************************
* Called when the data entered into an alarm type import page has changed.
* Enable or disable the Next button, depending on whether the entered data is
* valid or not.
*/
void DirResourceImportDialog::typeStatusChanged(bool ok)
{
    auto page = qobject_cast<DirResourceImportTypeWidget*>(currentPage()->widget());
    if (page)
    {
        nextButton()->setEnabled(ok  &&  (page != mLastPage));
        finishButton()->setEnabled(ok  &&  (page == mLastPage));
    }
}

const DirResourceImportTypeWidget* DirResourceImportDialog::typePage(KAlarmCal::CalEvent::Type type) const
{
    if (mAlarmTypes & type)
    {
        switch (type)
        {
            case CalEvent::ACTIVE:    return mPageActive;
            case CalEvent::ARCHIVED:  return mPageArchived;
            case CalEvent::TEMPLATE:  return mPageTemplate;
            default:
                break;
        }
    }
    return nullptr;
}


/*=============================================================================
= Class DirResourceImportWidgetBase
= Base class for page widgets.
=============================================================================*/
DirResourceImportWidgetBase::DirResourceImportWidgetBase(QWidget* parent)
    : QWidget(parent)
{
}


/*=============================================================================
= Class DirResourceImportIntroWidget
= The first page of the directory resource import dialog, which gives general
= information to the user.
=============================================================================*/
DirResourceImportIntroWidget::DirResourceImportIntroWidget(const QString& dirResourceName, const QString& dirResourcePath, KAlarmCal::CalEvent::Types types, QWidget* parent)
    : DirResourceImportWidgetBase(parent)
{
    mUi = new Ui_DirResourceImportIntroWidget;
    mUi->setupUi(this);
    mUi->dirNameLabel->setText(dirResourceName);
    mUi->dirPathLabel->setText(dirResourcePath);

    QStringList typeNames;
    if (types & CalEvent::ACTIVE)
        typeNames += i18nc("@item:intext", "Active alarms");
    if (types & CalEvent::ARCHIVED)
        typeNames += i18nc("@item:intext", "Archived alarms");
    if (types & CalEvent::TEMPLATE)
        typeNames += i18nc("@item:intext", "Alarm templates");
    mUi->dirTypesLabel->setText(typeNames.join(QStringLiteral(", ")));

    if (typeNames.size() > 1)
        mUi->warning1->setVisible(false);
    else
        mUi->warning2->setVisible(false);
}

DirResourceImportIntroWidget::~DirResourceImportIntroWidget()
{
    delete mUi;
}

/******************************************************************************
* Called when the page is displayed, to set appropriate heights for QLabel
* elements with text wrapping, and then remove empty space between widgets.
* By default, if QLabel contains more than one line of text, it often occupies
* more space than required, and calling setMinimumHeight() doesn't affect this.
*/
void DirResourceImportIntroWidget::setTextSizes()
{
    mUi->warning1->setFixedHeight(mUi->warning1->height());
    mUi->warning2->setFixedHeight(mUi->warning2->height());
    mUi->note->setFixedHeight(mUi->note->height());
    setFixedHeight(sizeHint().height());
}


/*=============================================================================
= Class DirResourceImportTypeWidget
= The page of the directory resource import dialog which allows the user to
= specify how to import one alarm type.
=============================================================================*/
DirResourceImportTypeWidget::DirResourceImportTypeWidget(CalEvent::Type alarmType, QWidget* parent)
    : DirResourceImportWidgetBase(parent)
{
    mUi = new Ui_DirResourceImportTypeWidget;
    mUi->setupUi(this);

    const QList<Resource> resources = Resources::allResources<FileResource>(alarmType, Resources::DefaultFirst | Resources::DisplayName);
    if (resources.isEmpty())
    {
        mUi->mergeRadio->setVisible(false);
        mUi->mergeRadio->setEnabled(false);
        mUi->mergeResource->setVisible(false);
    }
    else
    {
        for (const Resource& resource : resources)
            mUi->mergeResource->addItem(resource.displayName(), QVariant(resource.id()));
    }

    connect(mUi->optionGroup, &QButtonGroup::idToggled, this, &DirResourceImportTypeWidget::importTypeSelected);

    mUi->pathRequester->setMode(KFile::File);
    mUi->pathRequester->setFilter(QStringLiteral("*.ics|%1").arg(i18nc("@item:inlistbox File type selection filter", "Calendar files")));
    mUi->statusLabel->setText(QString());
    mUi->pathRequester->setFocus();
    mUi->pathRequester->installEventFilter(this);
    connect(mUi->pathRequester, &KUrlRequester::returnPressed, this, [this]() { validate(); });
    connect(mUi->pathRequester, &KUrlRequester::urlSelected, this, [this]() { validate(); });
    connect(mUi->pathRequester, &KUrlRequester::textChanged, this, [this]() { setStatus(false); });
    connect(mUi->nameText, &QLineEdit::textChanged, this, [this]() { validate(); });

    // Indent fields beneath each radio button option.
    QRadioButton radio(this);
    QStyleOptionButton opt;
    opt.initFrom(&radio);
    int indentWidth = style()->subElementRect(QStyle::SE_RadioButtonIndicator, &opt).width();
    mUi->grid->setColumnMinimumWidth(0, indentWidth);
    mUi->grid->setColumnStretch(1, 1);

    importTypeSelected();
    QTimer::singleShot(0, this, &DirResourceImportTypeWidget::validate);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

DirResourceImportTypeWidget::~DirResourceImportTypeWidget()
{
    delete mUi;
}

/******************************************************************************
* Return the existing resource ID to import into.
*/
ResourceId DirResourceImportTypeWidget::resourceId() const
{
    return mUi->mergeRadio->isChecked() ? mUi->mergeResource->currentData().toLongLong() : -1;
}

/******************************************************************************
* Return the new calendar file URL.
*/
QUrl DirResourceImportTypeWidget::url() const
{
    return mUi->newRadio->isChecked() ? mUi->pathRequester->url() : QUrl();
}

/******************************************************************************
* Return the resource's display name.
*/
QString DirResourceImportTypeWidget::displayName() const
{
    return mUi->newRadio->isChecked() ? mUi->nameText->text() : QString();
}

/******************************************************************************
* Notify the page that it is the last page.
*/
void DirResourceImportTypeWidget::setLastPage()
{
    mLastPage = true;
}

/******************************************************************************
* Set a validation function to apply to an entered URL.
*/
void DirResourceImportTypeWidget::setUrlValidation(QString (*func)(const QUrl&))
{
    mUrlValidationFunc = func;
}

/******************************************************************************
* Called when the page is displayed, to set appropriate heights for QLabel
* elements with text wrapping, and then remove empty space between widgets.
* By default, if QLabel contains more than one line of text, it often occupies
* more space than required, and calling setMinimumHeight() doesn't affect this.
*/
void DirResourceImportTypeWidget::setTextSizes()
{
    const int spacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    mUi->spacer1->changeSize(10, 2*spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);
    mUi->spacer2->changeSize(10, 2*spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);
    mUi->spacer3->changeSize(10, 2*spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
}

bool DirResourceImportTypeWidget::eventFilter(QObject* o, QEvent* e)
{
    if (o == mUi->pathRequester  &&  e->type() == QEvent::FocusOut)
        validate();
    return QWidget::eventFilter(o, e);
}

/******************************************************************************
* Called when an import destination type radio button has been selected.
* Enable or disable the other controls.
*/
void DirResourceImportTypeWidget::importTypeSelected()
{
    const bool importMerge = mUi->mergeRadio->isChecked();
    const bool importNew   = mUi->newRadio->isChecked();
    mUi->mergeResource->setEnabled(importMerge);
    mUi->pathLabel->setEnabled(importNew);
    mUi->pathRequester->setEnabled(importNew);
    mUi->statusLabel->setEnabled(importNew);
    mUi->nameLabel->setEnabled(importNew);
    mUi->nameText->setEnabled(importNew);

    if (importNew)
        validate();
    else if (importMerge  ||  mUi->noRadio->isChecked())
        Q_EMIT status(true);
}

/******************************************************************************
* Validate the current user input. If invalid, disable the OK button.
* Reply = false if invalid, or if waiting for remote validity to be checked.
*/
void DirResourceImportTypeWidget::validate()
{
    if (!mUi->newRadio->isChecked())
        return;

    // Validate URL first, in order to display any error message.
    const QUrl currentUrl = mUi->pathRequester->url();
    if (mUi->pathRequester->text().trimmed().isEmpty() || currentUrl.isEmpty())
    {
        setStatus(false);
        return;
    }
    if (mUrlValidationFunc)
    {
        const QString error = (*mUrlValidationFunc)(currentUrl);
        if (!error.isEmpty())
        {
            setStatus(false, error);
            return;
        }
    }

    if (!currentUrl.isLocalFile())
    {
        // It's a remote file.
        // Check whether the file can be read or written.
        if (mStatJob)
            mStatJob->kill();
        mCheckingDir = false;
        mStatJob = KIO::statDetails(currentUrl, KIO::StatJob::SourceSide, KIO::StatDetail::StatNoDetails, KIO::HideProgressInfo);
        connect(mStatJob, &KIO::StatJob::result, this, &DirResourceImportTypeWidget::slotStatJobResult);

        // Disable the OK button until the file's status is determined.
        setStatus(false, i18nc("@info:status", "Checking file information..."), false);
        return;
    }

    // It's a local file. Check that it doesn't already exist, and that it can be created.
    QFileInfo file(currentUrl.toLocalFile());
    if (file.exists())
    {
        setStatus(false, i18nc("@info:status", "Error! File already exists."));
        return;
    }
    QFileInfo dir(file.path());
    if (!dir.exists())
    {
        setStatus(false, i18nc("@info:status", "Error! Cannot create file (directory does not exist)."));
        return;
    }
    if (!dir.isWritable())
    {
        setStatus(false, i18nc("@info:status", "Error! Cannot create file (directory is not writable)."));
        return;
    }

    if (mUi->nameText->text().trimmed().isEmpty())
    {
        setStatus(false);
        return;
    }

    setStatus(true);
}

/******************************************************************************
* Called when the status of the remote URL has been determined.
* Ensures that the URL doesn't exist, but can be created.
*/
void DirResourceImportTypeWidget::slotStatJobResult(KJob* job)
{
    auto statJob = static_cast<KIO::StatJob*>(job);
    mStatJob = nullptr;

    if (!mCheckingDir)
    {
        // Results from checking the remote file.
        if (job->error() == KIO::ERR_DOES_NOT_EXIST)
        {
            // The file doesn't exist (as expected), so check that the file's
            // directory is writable.
            mCheckingDir = true;
            mStatJob = KIO::statDetails(KIO::upUrl(mUi->pathRequester->url()),
                                        KIO::StatJob::SourceSide,
                                        KIO::StatDetail::StatDefaultDetails,
                                        KIO::HideProgressInfo);
            connect(mStatJob, &KIO::StatJob::result, this, &DirResourceImportTypeWidget::slotStatJobResult);
            return;
        }
        setStatus(false, i18nc("@info:status", "Error! File already exists."));
    }
    else
    {
        // Results from checking the remote file's directory.
        mCheckingDir = false;
        if (job->error())
        {
            if (job->error() == KIO::ERR_DOES_NOT_EXIST)
                setStatus(false, i18nc("@info:status", "Error! Cannot create file (directory does not exist)."));
            else
                setStatus(false, i18nc("@info:status", "Error! Cannot create file (directory is not writable)."));
        }
        else
        {
            KFileItem fi(statJob->statResult(), KIO::upUrl(mUi->pathRequester->url()));
            if (!fi.isDir())
                setStatus(false, i18nc("@info", "Error! Cannot create file (directory does not exist)."));
            else if (!fi.isWritable())
                setStatus(false, i18nc("@info", "Error! Cannot create file (directory is not writable)."));
            else
                setStatus(true);
        }
    }
}

/******************************************************************************
* Set or clear the URL status message, and notify the dialog of the new status.
*/
void DirResourceImportTypeWidget::setStatus(bool ok, const QString& errorMessage, bool errorColour)
{
    if (ok)
        mUi->statusLabel->setText(QString());
    else
    {
        QPalette pal = mUi->pathLabel->palette();
        if (errorColour  &&  !errorMessage.isEmpty())
            pal.setColor(QPalette::WindowText, KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color());
        mUi->statusLabel->setPalette(pal);
        mUi->statusLabel->setText(errorMessage);
    }
    Q_EMIT status(ok);
}

// vim: et sw=4:
