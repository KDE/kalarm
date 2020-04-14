/*
 *  lib/file.cpp  -  functions to handle files
 *  Program:  kalarm
 *  Copyright © 2005-2019 David Jarvie <djarvie@kde.org>
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

#include "file.h"

#include "lib/autoqpointer.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KFileItem>

#include <QDir>
#include <QRegExp>
#include <QFileDialog>

namespace File
{

/******************************************************************************
* Check from its mime type whether a file appears to be a text or image file.
* If a text file, its type is distinguished.
* Reply = file type.
*/
FileType fileType(const QMimeType& mimetype)
{
    if (mimetype.inherits(QStringLiteral("text/html")))
        return TextFormatted;
    if (mimetype.inherits(QStringLiteral("application/x-executable")))
        return TextApplication;
    if (mimetype.inherits(QStringLiteral("text/plain")))
        return TextPlain;
    if (mimetype.name().startsWith(QLatin1String("image/")))
        return Image;
    return Unknown;
}

/******************************************************************************
* Check that a file exists and is a plain readable file.
* Updates 'filename' and 'url' even if an error occurs, since 'filename' may
* be needed subsequently by showFileErrMessage().
* 'filename' is in user input format and may be a local file path or URL.
*/
FileErr checkFileExists(QString& filename, QUrl& url, QWidget* messageParent)
{
    // Convert any relative file path to absolute
    // (using home directory as the default).
    // This also supports absolute paths and absolute urls.
    FileErr err = FileErr::None;
    url = QUrl::fromUserInput(filename, QDir::homePath(), QUrl::AssumeLocalFile);
    if (filename.isEmpty())
    {
        url = QUrl();
        err = FileErr::Blank;    // blank file name
    }
    else if (!url.isValid())
        err = FileErr::Nonexistent;
    else if (url.isLocalFile())
    {
        // It's a local file
        filename = url.toLocalFile();
        QFileInfo info(filename);
        if      (info.isDir())        err = FileErr::Directory;
        else if (!info.exists())      err = FileErr::Nonexistent;
        else if (!info.isReadable())  err = FileErr::Unreadable;
    }
    else
    {
        filename = url.toDisplayString();
        auto statJob = KIO::statDetails(url, KIO::StatJob::SourceSide, KIO::StatDetail::StatDefaultDetails);
        KJobWidgets::setWindow(statJob, messageParent);
        if (!statJob->exec())
            err = FileErr::Nonexistent;
        else
        {
            KFileItem fi(statJob->statResult(), url);
            if (fi.isDir())             err = FileErr::Directory;
            else if (!fi.isReadable())  err = FileErr::Unreadable;
        }
    }
    return err;
}

/******************************************************************************
* Display an error message appropriate to 'err'.
* Display a Continue/Cancel error message if 'errmsgParent' non-null.
* Reply = true to continue, false to cancel.
*/
bool showFileErrMessage(const QString& filename, FileErr err, FileErr blankError, QWidget* errmsgParent)
{
    if (err != FileErr::None)
    {
        // If file is a local file, remove "file://" from name
        QString file = filename;
        const QRegExp f(QStringLiteral("^file:/+"));
        if (f.indexIn(file) >= 0)
            file = file.mid(f.matchedLength() - 1);

        QString errmsg;
        switch (err)
        {
            case FileErr::Blank:
                if (blankError == FileErr::BlankDisplay)
                    errmsg = i18nc("@info", "Please select a file to display");
                else if (blankError == FileErr::BlankPlay)
                    errmsg = i18nc("@info", "Please select a file to play");
                else
                    qFatal("showFileErrMessage: Program error");
                KAMessageBox::sorry(errmsgParent, errmsg);
                return false;
            case FileErr::Directory:
                KAMessageBox::sorry(errmsgParent, xi18nc("@info", "<filename>%1</filename> is a folder", file));
                return false;
            case FileErr::Nonexistent:   errmsg = xi18nc("@info", "<filename>%1</filename> not found", file);  break;
            case FileErr::Unreadable:    errmsg = xi18nc("@info", "<filename>%1</filename> is not readable", file);  break;
            case FileErr::NotTextImage:  errmsg = xi18nc("@info", "<filename>%1</filename> appears not to be a text or image file", file);  break;
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
    static const QRegExp localfile(QStringLiteral("^file:/+"));
    return (localfile.indexIn(url) >= 0) ? url.mid(localfile.matchedLength() - 1) : url;
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
                const QString& initialFile, const QString& filter, bool existing, QWidget* parent)
{
    file.clear();
    const QString initialDir = !initialFile.isEmpty() ? QString(initialFile).remove(QRegExp(QLatin1String("/[^/]*$")))
                             : !defaultDir.isEmpty()  ? defaultDir
                             :                          QDir::homePath();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<QFileDialog> fileDlg = new QFileDialog(parent, caption, initialDir, filter);
    fileDlg->setAcceptMode(existing ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
    fileDlg->setFileMode(existing ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
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
