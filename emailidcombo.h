/*
 *  emailidcombo.h  -  email identity combo box with read-only option
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EMAILIDCOMBO_H
#define EMAILIDCOMBO_H

#include <libkpimidentities/identitycombo.h>
#include "combobox.h"


class EmailIdCombo : public KPIM::IdentityCombo
{
		Q_OBJECT
	public:
		EmailIdCombo(KPIM::IdentityManager*, QWidget* parent = 0, const char* name = 0);
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
