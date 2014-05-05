/*
 *  emailidcombo.h  -  email identity combo box with read-only option
 *  Program:  kalarm
 *  Copyright © 2004,2006 by David Jarvie <djarvie@kde.org>
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

#ifndef EMAILIDCOMBO_H
#define EMAILIDCOMBO_H

#include "combobox.h"
#include <KPIMIdentities/kpimidentities/identitycombo.h>

class QMouseEvent;
class QKeyEvent;


class EmailIdCombo : public KPIMIdentities::IdentityCombo
{
        Q_OBJECT
    public:
        explicit EmailIdCombo(KPIMIdentities::IdentityManager*, QWidget* parent = 0);
        void  setReadOnly(bool ro)    { mReadOnly = ro; }

    protected:
        virtual void mousePressEvent(QMouseEvent*);
        virtual void mouseReleaseEvent(QMouseEvent*);
        virtual void mouseMoveEvent(QMouseEvent*);
        virtual void keyPressEvent(QKeyEvent*);
        virtual void keyReleaseEvent(QKeyEvent*);

    private:
        bool    mReadOnly;      // value cannot be changed
};

#endif // EMAILIDCOMBO_H

// vim: et sw=4:
