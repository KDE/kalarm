/*
 *  lib/file.h  -  functions to handle files
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIB_FILE_H
#define LIB_FILE_H

#include <QString>
class QMimeType;
class QUrl;

class QWidget;

namespace File
{

/** Return codes from fileType() */
enum FileType { Unknown, TextPlain, TextFormatted, TextApplication, Image };

/** Check from its mime type whether a file appears to be a text or image file.
 *  If a text file, its type is distinguished.
 */
FileType fileType(const QMimeType& mimetype);

enum class FileErr
{
    None = 0,
    Blank,           // generic blank error
    Nonexistent, Directory, Unreadable, NotTextImage,
    BlankDisplay,    // blank error to use for file to display
    BlankPlay        // blank error to use for file to play
};

/** Check that a file exists and is a plain readable file, optionally a text/image file.
 *  Display a Continue/Cancel error message if 'errmsgParent' non-null.
 *  @param filename       The file path; updated to absolute path.
 *  @param url            Updated to the file's URL.
 *  @param messageParent  Parent widget for error messages etc.
 */
FileErr checkFileExists(QString& filename, QUrl& url, QWidget* messageParent);

bool showFileErrMessage(const QString& filename, FileErr, FileErr blankError, QWidget* errmsgParent);

/** If a url string is a local file, strip off the 'file:/' prefix. */
QString pathOrUrl(const QString& url);

bool browseFile(QString& file, const QString& caption, QString& defaultDir,
                const QString& initialFile = QString(),
                const QString& filter = QString(), bool existing = false, QWidget* parent = nullptr);

}

#endif // LIB_FILE_H

// vim: et sw=4:
