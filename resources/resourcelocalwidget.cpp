/*
 *  resourcelocalwidget.cpp  -  configuration widget for a local file calendar resource
 *  Program:  kalarm
 *  Copyright © 2006,2008 by David Jarvie <software@astrojar.org.uk>
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
#include <typeinfo>

#include <QLabel>
#include <QGridLayout>

#include <kurlrequester.h>
#include <klocale.h>
#include <kdebug.h>

#include "resourcelocal.h"
#include "resourcelocalwidget.moc"


ResourceLocalConfigWidget::ResourceLocalConfigWidget(QWidget* parent)
	: ResourceConfigWidget(parent)
{
	QGridLayout* layout = new QGridLayout(this);

	QLabel* label = new QLabel(i18nc("@label:textbox", "Location:"), this);
	layout->addWidget(label, 1, 0);
	mURL = new KUrlRequester(this);
	mURL->setFilter(QString::fromLatin1("*.vcs *.ics|%1").arg(i18nc("@info/plain", "Calendar Files")));
	layout->addWidget(mURL, 1, 1);
}

void ResourceLocalConfigWidget::loadSettings(KRES::Resource* resource)
{
kDebug(KARES_DEBUG)<<typeid(resource).name();
//	KAResourceLocal* res = dynamic_cast<KAResourceLocal*>(resource);
	KAResourceLocal* res = static_cast<KAResourceLocal*>(resource);
	if (!res)
		kError(KARES_DEBUG) << "KAResourceLocal: cast failed";
	else
	{
		ResourceConfigWidget::loadSettings(resource);
		mURL->setUrl(res->fileName());
#ifndef NDEBUG
		kDebug(KARES_DEBUG) << "File" << mURL->url() << " type" << res->typeName();
#endif
	}
}

void ResourceLocalConfigWidget::saveSettings(KRES::Resource* resource)
{
//	KAResourceLocal* res = dynamic_cast<KAResourceLocal*>(resource);
	KAResourceLocal* res = static_cast<KAResourceLocal*>(resource);
	if (!res)
		kDebug(KARES_DEBUG) << "KAResourceLocal: cast failed";
	else
		res->setFileName(mURL->url());
}
