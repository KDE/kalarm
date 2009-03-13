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

#include "kalarm.h"

#include "filedialog.h"
#include <kabstractfilewidget.h>
#include <klocale.h>
#include <krecentdocument.h>
#include <kdebug.h>
#include <QCheckBox>


QCheckBox* FileDialog::mAppendCheck = 0;


QString FileDialog::getSaveFileName(const KUrl& dir, const QString& filter, QWidget* parent, const QString& caption, bool* append)
{
	bool defaultDir = dir.isEmpty();
	bool specialDir = !defaultDir && dir.protocol() == "kfiledialog";
	FileDialog dlg(specialDir ? dir : KUrl(), filter, parent);
	if (!specialDir && !defaultDir)
	{
		if (!dir.isLocalFile())
			kWarning() << "FileDialog::getSaveFileName called with non-local start dir " << dir;
		dlg.setSelection(dir.path());  // may also be a filename
	}
	dlg.setOperationMode(Saving);
	dlg.setMode(KFile::File | KFile::LocalOnly);
	dlg.setConfirmOverwrite(true);
	if (!caption.isEmpty())
		dlg.setCaption(caption);
	mAppendCheck = 0;
	if (append)
	{
		// Show an 'append' option in the dialogue.
		// Note that the dialogue will take ownership of the QCheckBox.
		mAppendCheck = new QCheckBox(i18nc("@option:check", "Append to existing file"), 0);
		connect(mAppendCheck, SIGNAL(toggled(bool)), &dlg, SLOT(appendToggled(bool)));
		dlg.fileWidget()->setCustomWidget(mAppendCheck);
		*append = false;
	}
	dlg.exec();

	QString filename = dlg.selectedFile();
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
