/*
 *  lib/file.cpp  -  functions to handle files
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "file.h"

#include "lib/autoqpointer.h"
#include "lib/messagebox.h"

#include <KLocalizedString>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KFileItem>

#include <QDir>
#include <QRegularExpression>
#include <QFileDialog>

namespace File
{

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
Type fileType(const QMimeType& mimetype)
{
    if (mimetype.inherits(QStringLiteral("text/html")))
        return Type::TextFormatted;
    if (mimetype.inherits(QStringLiteral("application/x-executable")))
        return Type::TextApplication;
    if (mimetype.inherits(QStringLiteral("text/plain")))
        return Type::TextPlain;
    if (mimetype.name().startsWith(QLatin1String("image/")))
        return Type::Image;
    return Type::Unknown;
}

/******************************************************************************
* Check that a file exists and is a plain readable file.
* Updates 'filename' and 'url' even if an error occurs, since 'filename' may
* be needed subsequently by showFileErrMessage().
* 'filename' is in user input format and may be a local file path or URL.
*/
Error checkFileExists(QString& filename, QUrl& url, QWidget* messageParent)
{
    // Convert any relative file path to absolute
    // (using home directory as the default).
    // This also supports absolute paths and absolute urls.
    Error err = Error::None;
    url = QUrl::fromUserInput(filename, QDir::homePath(), QUrl::AssumeLocalFile);
    if (filename.isEmpty())
    {
        url = QUrl();
        err = Error::Blank;    // blank file name
    }
    else if (!url.isValid())
        err = Error::Nonexistent;
    else if (url.isLocalFile())
    {
        // It's a local file
        filename = url.toLocalFile();
        QFileInfo info(filename);
        if      (info.isDir())        err = Error::Directory;
        else if (!info.exists())      err = Error::Nonexistent;
        else if (!info.isReadable())  err = Error::Unreadable;
    }
    else
    {
        filename = url.toDisplayString();
        auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatDetail::StatDefaultDetails);
        KJobWidgets::setWindow(statJob, messageParent);
        if (!statJob->exec())
            err = Error::Nonexistent;
        else
        {
            KFileItem fi(statJob->statResult(), url);
            if (fi.isDir())             err = Error::Directory;
            else if (!fi.isReadable())  err = Error::Unreadable;
        }
    }
    return err;
}

/******************************************************************************
* Display an error message appropriate to 'err'.
* Display a Continue/Cancel error message if 'errmsgParent' non-null.
* Reply = true to continue, false to cancel.
*/
bool showFileErrMessage(const QString& filename, Error err, Error blankError, QWidget* errmsgParent)
{
    if (err != Error::None)
    {
        // If file is a local file, remove "file://" from name
        const QString file = pathOrUrl(filename);

        QString errmsg;
        switch (err)
        {
            case Error::Blank:
                if (blankError == Error::BlankDisplay)
                    errmsg = i18nc("@info", "Please select a file to display");
                else if (blankError == Error::BlankPlay)
                    errmsg = i18nc("@info", "Please select a file to play");
                else
                    qFatal("showFileErrMessage: Program error");
                KAMessageBox::error(errmsgParent, errmsg);
                return false;
            case Error::Directory:
                KAMessageBox::error(errmsgParent, xi18nc("@info", "<filename>%1</filename> is a folder", file));
                return false;
            case Error::Nonexistent:   errmsg = xi18nc("@info", "<filename>%1</filename> not found", file);  break;
            case Error::Unreadable:    errmsg = xi18nc("@info", "<filename>%1</filename> is not readable", file);  break;
            case Error::NotTextImage:  errmsg = xi18nc("@info", "<filename>%1</filename> appears not to be a text or image file", file);  break;
            default:
                break;
        }
        if (KAMessageBox::warningContinueCancel(errmsgParent, errmsg)
            == KMessageBox::Cancel)
            return false;
    }
    return true;
}

/******************************************************************************
* If a url string is a local file, strip off the 'file:/' prefix.
*/
QString pathOrUrl(const QString& url)
{
    static const QRegularExpression re(QStringLiteral("^file:/+"));
    const QRegularExpressionMatch match = re.match(url);
    return match.hasMatch() ? url.mid(match.capturedEnd(0) - 1) : url;
}

/******************************************************************************
* Display a modal dialog to choose an existing file, initially highlighting
* any specified file.
* @param file        Updated with the file which was selected, or empty if no file
*                    was selected.
* @param initialFile The file to initially highlight - must be a full path name or URL.
* @param defaultDir The directory to start in if @p initialFile is empty. If empty,
*                   the user's home directory will be used. Updated to the
*                   directory containing the selected file, if a file is chosen.
* @param existing true to return only existing files, false to allow new ones.
* Reply = true if 'file' value can be used.
*       = false if the dialogue was deleted while visible (indicating that
*         the parent widget was probably also deleted).
*/
bool browseFile(QString& file, const QString& caption, QString& defaultDir,
                const QString& initialFile, bool existing, QWidget* parent)
{
    return browseFile(file, caption, defaultDir, {}, initialFile, existing, parent);
}

bool browseFile(QString& file, const QString& caption, QString& defaultDir,
                const QString& fileNameFilter, const QString& initialFile,
                bool existing, QWidget* parent)
{
    file.clear();
    static const QRegularExpression re(QStringLiteral("/[^/]*$"));
    const QString initialDir = !initialFile.isEmpty() ? pathOrUrl(initialFile).remove(re)
                             : !defaultDir.isEmpty()  ? defaultDir
                             :                          QDir::homePath();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<QFileDialog> fileDlg = new QFileDialog(parent, caption, initialDir);
    fileDlg->setAcceptMode(existing ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
    fileDlg->setFileMode(existing ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
    QStringList nameFilters;
    if (!fileNameFilter.isEmpty())
        nameFilters << fileNameFilter;
    nameFilters << QStringLiteral("%1 (*)").arg(i18nc("@item:inlistbox File type", "All files"));
    fileDlg->setNameFilters(nameFilters);
    if (!initialFile.isEmpty())
        fileDlg->selectFile(initialFile);
    if (fileDlg->exec() != QDialog::Accepted)
        return static_cast<bool>(fileDlg);   // return false if dialog was deleted
    const QList<QUrl> urls = fileDlg->selectedUrls();
    if (urls.isEmpty())
        return true;
    const QUrl& url = urls[0];
    defaultDir = url.isLocalFile() ? KIO::upUrl(url).toLocalFile() : url.adjusted(QUrl::RemoveFilename).path();
    bool localOnly = true;
    file = localOnly ? url.toDisplayString(QUrl::PreferLocalFile) : url.toDisplayString();
    return true;
}

} // namespace File

// vim: et sw=4:
