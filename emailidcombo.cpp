/*
 *  emailidcombo.cpp  -  email identity combo box with read-only option
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

#include "emailidcombo.moc"


EmailIdCombo::EmailIdCombo(KPIM::IdentityManager* manager, QWidget* parent, const char* name)
	: KPIM::IdentityCombo(manager, parent, name),
	  mReadOnly(false)
{ }

void EmailIdCombo::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	KPIM::IdentityCombo::mousePressEvent(e);
}

void EmailIdCombo::mouseReleaseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		KPIM::IdentityCombo::mouseReleaseEvent(e);
}

void EmailIdCombo::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		KPIM::IdentityCombo::mouseMoveEvent(e);
}

void EmailIdCombo::keyPressEvent(QKeyEvent* e)
{
	if (!mReadOnly  ||  e->key() == Qt::Key_Escape)
		KPIM::IdentityCombo::keyPressEvent(e);
}

void EmailIdCombo::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		KPIM::IdentityCombo::keyReleaseEvent(e);
}
