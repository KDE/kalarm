/*
 *  filedialog.cpp  -  file save dialogue, with append option & confirm overwrite
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filedialog.h"

#include "autoqpointer.h"
#include "kalarm_debug.h"

#include <KFileWidget>
#include <KLocalizedString>
#include <KRecentDocument>

#include <QCheckBox>


QCheckBox* FileDialog::mAppendCheck = nullptr;


FileDialog::FileDialog(const QUrl& startDir, const QList<KFileFilter>& filters, QWidget* parent)
    : KFileCustomDialog(parent)
{
    fileWidget()->setFilters(filters);
    fileWidget()->setStartDir(startDir);
}

QString FileDialog::getSaveFileName(const QUrl& dir, const QList<KFileFilter>& filters, QWidget* parent, const QString& caption, bool* append)
{
    bool defaultDir = dir.isEmpty();
    bool specialDir = !defaultDir && dir.scheme() == QLatin1String("kfiledialog");
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<FileDialog> dlg = new FileDialog(specialDir ? dir : QUrl(), filters, parent);
    if (!specialDir && !defaultDir)
    {
        if (!dir.isLocalFile())
            qCWarning(KALARM_LOG) << "FileDialog::getSaveFileName called with non-local start dir " << dir;
        dlg->fileWidget()->setSelectedUrl(dir);  // may also be a filename
    }
    dlg->setOperationMode(KFileWidget::Saving);
    dlg->fileWidget()->setMode(KFile::File | KFile::LocalOnly);
    dlg->fileWidget()->setConfirmOverwrite(true);
    if (!caption.isEmpty())
        dlg->setWindowTitle(caption);
    mAppendCheck = nullptr;
    if (append)
    {
        // Show an 'append' option in the dialogue.
        // Note that the dialogue will take ownership of the QCheckBox.
        mAppendCheck = new QCheckBox(i18nc("@option:check", "Append to existing file"), nullptr);
        connect(mAppendCheck, &QCheckBox::toggled, dlg.data(), &FileDialog::appendToggled);
        dlg->setCustomWidget(mAppendCheck);
        *append = false;
    }
    dlg->setWindowModality(Qt::WindowModal);
    dlg->exec();
    if (!dlg)
        return {};   // dialogue was deleted

    QString filename = dlg->fileWidget()->selectedFile();
    if (!filename.isEmpty())
    {
        if (append)
            *append = mAppendCheck->isChecked();
        KRecentDocument::add(QUrl::fromLocalFile(filename));
    }
    return filename;
}

void FileDialog::appendToggled(bool ticked)
{
    fileWidget()->setConfirmOverwrite(!ticked);
}

#include "moc_filedialog.cpp"

// vim: et sw=4:
