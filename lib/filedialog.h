/*
 *  filedialog.h  -  file save dialogue, with append option & confirm overwrite
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

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <kfiledialog.h>
#include <KUrl>
class QCheckBox;

class FileDialog : public KFileDialog
{
        Q_OBJECT
    public:
        FileDialog(const KUrl& startDir, const QString& filter,
                   QWidget* parent, QWidget* widget = 0)
              : KFileDialog(startDir, filter, parent, widget) {}
        static QString getSaveFileName(const KUrl& dir = KUrl(), const QString& filter = QString(),
                                       QWidget* parent = 0, const QString& caption = QString(), bool* append = 0);

    private slots:
        void appendToggled(bool);

    private:
        static QCheckBox* mAppendCheck;
};

#endif

// vim: et sw=4:
