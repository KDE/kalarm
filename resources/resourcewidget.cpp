/*
 *  resourcewidget.cpp  -  base class for resource configuration widgets
 *  Program:  kalarm
 *  Copyright © 2006,2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
//#include <typeinfo>

#include <kmessagebox.h>
#include <klocale.h>

#include "alarmresource.h"
#include "resourcewidget.moc"


ResourceConfigWidget::ResourceConfigWidget(QWidget* parent)
	: KRES::ConfigWidget(parent)
{
	resize(245, 115); 
}

void ResourceConfigWidget::loadSettings(KRES::Resource* resource)
{
//	AlarmResource* res = dynamic_cast<AlarmResource*>(resource);
	AlarmResource* res = static_cast<AlarmResource*>(resource);
	if (res)
		connect(res, SIGNAL(notWritable(AlarmResource*)), SLOT(slotNotWritable(AlarmResource*)));
}

/******************************************************************************
* Called when the user tries to change the resource to read-write, but this is
* not allowed because the resource was not written by KAlarm (or was written by
* a later version of KAlarm).
* Display an error message.
*/
void ResourceConfigWidget::slotNotWritable(AlarmResource* resource)
{
	QString text = i18nc("@info", "Calendar <resource>%1</resource> cannot be made writable since it either was not created by <application>KAlarm</application>, or was created by a newer version of <application>KAlarm</application>", resource->resourceName());
	KMessageBox::sorry(this, text);
}
