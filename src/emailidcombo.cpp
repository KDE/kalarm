/*
 *  emailidcombo.cpp  -  email identity combo box with read-only option
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "emailidcombo.h"

#include <QMouseEvent>
#include <QKeyEvent>

EmailIdCombo::EmailIdCombo(KIdentityManagement::IdentityManager *manager,
                           QWidget *parent)
    : KIdentityManagementWidgets::IdentityCombo(manager, parent) {}

void EmailIdCombo::mousePressEvent(QMouseEvent* e)
{
    if (mReadOnly)
    {
        // Swallow up the event if it's the left button
        if (e->button() == Qt::LeftButton)
            return;
    }
    KIdentityManagementWidgets::IdentityCombo::mousePressEvent(e);
}

void EmailIdCombo::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KIdentityManagementWidgets::IdentityCombo::mouseReleaseEvent(e);
}

void EmailIdCombo::mouseMoveEvent(QMouseEvent* e)
{
    if (!mReadOnly)
        KIdentityManagementWidgets::IdentityCombo::mouseMoveEvent(e);
}

void EmailIdCombo::keyPressEvent(QKeyEvent* e)
{
    if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
        KIdentityManagementWidgets::IdentityCombo::keyPressEvent(e);
}

void EmailIdCombo::keyReleaseEvent(QKeyEvent* e)
{
    if (!mReadOnly)
        KIdentityManagementWidgets::IdentityCombo::keyReleaseEvent(e);
}

// vim: et sw=4:
