/*
 *  templatepickdlg.h  -  dialog to choose an alarm template
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TEMPLATEPICKDLG_H
#define TEMPLATEPICKDLG_H

#include <KAlarmCal/KAEvent>

#include <QDialog>
class QPushButton;
class QResizeEvent;
class TemplateListModel;
class TemplateListView;

using namespace KAlarmCal;

class TemplatePickDlg : public QDialog
{
    Q_OBJECT
public:
    explicit TemplatePickDlg(KAEvent::Actions, QWidget* parent = nullptr);
    KAEvent selectedTemplate() const;
protected:
    void    resizeEvent(QResizeEvent*) override;
private Q_SLOTS:
    void    slotSelectionChanged();
    void    slotDoubleClick();
private:
    TemplateListModel* mListFilterModel;
    TemplateListView*  mListView;
    QPushButton*       mOkButton;
};

#endif // TEMPLATEPICKDLG_H

// vim: et sw=4:
