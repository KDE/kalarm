/*
 *  editdlg_p.h  -  private classes for editdlg.cpp
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
    void      enableEmailDrop();
    QSize     sizeHint() const override         { return minimumSizeHint(); }
    QSize     minimumSizeHint() const override  { return minimumSize(); }

protected:
    void      dragEnterEvent(QDragEnterEvent*) override;
    void      dragMoveEvent(QDragMoveEvent*) override;
    void      dropEvent(QDropEvent*) override;

private:
    bool      mEmailDrop {false};
};

class CommandEdit : public QWidget
{
    Q_OBJECT
public:
    explicit CommandEdit(QWidget* parent);
    ~CommandEdit();
    bool      isScript() const;
    void      setScript(bool);
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

// vim: et sw=4:
