/*
 *  templatelistview.h  -  widget showing list of alarm templates
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "eventlistview.h"


class TemplateListView : public EventListView
{
    Q_OBJECT
public:
    explicit TemplateListView(QWidget* parent = nullptr);

protected Q_SLOTS:
    void initSections() override;
private:
    QStyleOptionViewItem listViewOptions() const;
};


class TemplateListDelegate : public EventListDelegate
{
    Q_OBJECT
public:
    explicit TemplateListDelegate(TemplateListView* parent = nullptr)
               : EventListDelegate(parent) {}
    void edit(KAEvent&, EventListView*) override;
};

// vim: et sw=4:
