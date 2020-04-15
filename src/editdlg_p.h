/*
 *  editdlg_p.h  -  private classes for editdlg.cpp
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007-2009 by David Jarvie <djarvie@kde.org>
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

#ifndef EDITDLG_P_H
#define EDITDLG_P_H

#include <KTextEdit>
#include <QFrame>
class QDragEnterEvent;
class QShowEvent;
class CheckBox;
class LineEdit;


class PageFrame : public QFrame
{
        Q_OBJECT
    public:
        explicit PageFrame(QWidget* parent = nullptr) : QFrame(parent) { }

    protected:
        void      showEvent(QShowEvent*) override    { Q_EMIT shown(); }

    Q_SIGNALS:
        void      shown();
};

class TextEdit : public KTextEdit
{
        Q_OBJECT
    public:
        explicit TextEdit(QWidget* parent);
        QSize     sizeHint() const override         { return minimumSizeHint(); }
        QSize     minimumSizeHint() const override  { return minimumSize(); }

    protected:
        void      dragEnterEvent(QDragEnterEvent*) override;
};

class CommandEdit : public QWidget
{
        Q_OBJECT
    public:
        explicit CommandEdit(QWidget* parent);
        bool      isScript() const;
        void      setScript(bool);
        QString   text() const;
        QString   text(EditAlarmDlg*, bool showErrorMessage) const;
        void      setText(const AlarmText&);
        void      setReadOnly(bool);
        QSize     minimumSizeHint() const override;
        QSize     sizeHint() const override   { return minimumSizeHint(); }

    Q_SIGNALS:
        void      scriptToggled(bool);
        void      changed();        // emitted when any changes occur

    private Q_SLOTS:
        void      slotCmdScriptToggled(bool);

    private:
        CheckBox* mTypeScript;      // entering a script
        LineEdit* mCommandEdit;     // command line edit box
        TextEdit* mScriptEdit;      // script edit box
};

#endif // EDITDLG_P_H

// vim: et sw=4:
