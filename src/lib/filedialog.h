/*
 *  filedialog.h  -  file save dialogue, with append option & confirm overwrite
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <KFileCustomDialog>
#include <QUrl>
class QCheckBox;

class FileDialog : public KFileCustomDialog
{
        Q_OBJECT
    public:
        FileDialog(const QUrl& startDir, const QString& filter, QWidget* parent = nullptr);
        static QString getSaveFileName(const QUrl& dir = QUrl(), const QString& filter = QString(),
                                       QWidget* parent = nullptr, const QString& caption = QString(), bool* append = nullptr);

    private Q_SLOTS:
        void appendToggled(bool);

    private:
        static QCheckBox* mAppendCheck;
};

#endif

// vim: et sw=4:
