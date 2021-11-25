/*
 *  templatedlg.h  -  dialog to create, edit and delete alarm templates
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004, 2006, 2007, 2010 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "editdlg.h"
#include <QDialog>

class QResizeEvent;
class QPushButton;
class NewAlarmAction;
class TemplateListModel;
class TemplateListView;


class TemplateDlg : public QDialog
{
    Q_OBJECT
public:
    static TemplateDlg*  create(QWidget* parent = nullptr);
    ~TemplateDlg() override;

Q_SIGNALS:
    void          emptyToggled(bool notEmpty);

protected:
    void          resizeEvent(QResizeEvent*) override;

private Q_SLOTS:
    void          slotNew(EditAlarmDlg::Type);
    void          slotCopy();
    void          slotEdit();
    void          slotDelete();
    void          slotSelectionChanged();

private:
    explicit TemplateDlg(QWidget* parent);

    static TemplateDlg* mInstance;   // the current instance, to prevent multiple dialogues

    TemplateListModel* mListFilterModel;
    TemplateListView*  mListView;
    QPushButton*       mEditButton;
    QPushButton*       mCopyButton;
    QPushButton*       mDeleteButton;
    NewAlarmAction*    mNewAction;
};

// vim: et sw=4:
