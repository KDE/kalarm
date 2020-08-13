/*
 *  templatelistview.h  -  widget showing list of alarm templates
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TEMPLATELISTVIEW_H
#define TEMPLATELISTVIEW_H

#include "eventlistview.h"


class TemplateListView : public EventListView
{
    Q_OBJECT
public:
    explicit TemplateListView(QWidget* parent = nullptr);

protected Q_SLOTS:
    void initSections() override;
};


class TemplateListDelegate : public EventListDelegate
{
    Q_OBJECT
public:
    explicit TemplateListDelegate(TemplateListView* parent = nullptr)
               : EventListDelegate(parent) {}
    void edit(KAEvent*, EventListView*) override;
};

#endif // TEMPLATELISTVIEW_H

// vim: et sw=4:
