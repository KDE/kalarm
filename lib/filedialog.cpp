/*
 *  filedialog.cpp  -  file save dialogue, with append option & confirm overwrite
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "filedialog.h"
#include "autoqpointer.h"

#include <KFileWidget>
#include <KLocalizedString>
#include <krecentdocument.h>
#include <KUrl>

#include <QCheckBox>
#include <qdebug.h>


QCheckBox* FileDialog::mAppendCheck = 0;


QString FileDialog::getSaveFileName(const KUrl& dir, const QString& filter, QWidget* parent, const QString& caption, bool* append)
{
    bool defaultDir = dir.isEmpty();
    bool specialDir = !defaultDir && dir.scheme() == QLatin1String("kfiledialog");
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<FileDialog> dlg = new FileDialog(specialDir ? dir : KUrl(), filter, parent);
    if (!specialDir && !defaultDir)
    {
        if (!dir.isLocalFile())
            qWarning() << "FileDialog::getSaveFileName called with non-local start dir " << dir;
        dlg->setSelection(dir.isLocalFile() ? dir.toLocalFile() : dir.path());  // may also be a filename
    }
    dlg->setOperationMode(Saving);
    dlg->setMode(KFile::File | KFile::LocalOnly);
    dlg->setConfirmOverwrite(true);
    if (!caption.isEmpty())
        dlg->setWindowTitle(caption);
    mAppendCheck = 0;
    if (append)
    {
        // Show an 'append' option in the dialogue.
        // Note that the dialogue will take ownership of the QCheckBox.
        mAppendCheck = new QCheckBox(i18nc("@option:check", "Append to existing file"), 0);
        connect(mAppendCheck, &QCheckBox::toggled, dlg.data(), &FileDialog::appendToggled);
        dlg->fileWidget()->setCustomWidget(mAppendCheck);
        *append = false;
    }
    dlg->setWindowModality(Qt::WindowModal);
    dlg->exec();
    if (!dlg)
        return QString();   // dialogue was deleted

    QString filename = dlg->selectedFile();
    if (!filename.isEmpty())
    {
        if (append)
            *append = mAppendCheck->isChecked();
        KRecentDocument::add(filename);
    }
    return filename;
}

void FileDialog::appendToggled(bool ticked)
{
    setConfirmOverwrite(!ticked);
}
#include "moc_filedialog.cpp"
// vim: et sw=4:
