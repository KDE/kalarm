/*
 *  emailidcombo.h  -  email identity combo box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004, 2006 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "lib/combobox.h"

#include <KIdentityManagementWidgets/IdentityCombo>

class QMouseEvent;
class QKeyEvent;

class EmailIdCombo : public KIdentityManagementWidgets::IdentityCombo
{
    Q_OBJECT
public:
    explicit EmailIdCombo(KIdentityManagement::IdentityManager*, QWidget* parent = nullptr);
    void setReadOnly(bool ro)    { mReadOnly = ro; }

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

private:
    bool    mReadOnly {false};      // value cannot be changed
};

// vim: et sw=4:
