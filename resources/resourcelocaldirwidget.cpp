/*
 *  resourcelocaldirwidget.cpp  -  configuration widget for local directory calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QLabel>
#include <QGridLayout>

#include <klocale.h>
#include <kurlrequester.h>
#include <kdebug.h>

#include "resourcelocaldir.h"
#include "resourcelocaldirwidget.moc"


ResourceLocalDirConfigWidget::ResourceLocalDirConfigWidget(QWidget* parent)
	: ResourceConfigWidget(parent)
{
	QGridLayout* layout = new QGridLayout(this);

	QLabel* label = new QLabel(i18n("Location:"), this);
	layout->addWidget(label, 1, 0);
	mURL = new KUrlRequester(this);
	mURL->setMode(KFile::Directory | KFile::LocalOnly);
	layout->addWidget(mURL, 1, 1);
}

void ResourceLocalDirConfigWidget::loadSettings(KRES::Resource* resource)
{
//	KAResourceLocalDir* res = dynamic_cast<KAResourceLocalDir*>(resource);
	KAResourceLocalDir* res = static_cast<KAResourceLocalDir*>(resource);
	if (!res)
		kError(KARES_DEBUG) << "ResourceLocalDirConfigWidget::loadSettings(KAResourceLocalDir): cast failed";
	else
	{
		ResourceConfigWidget::loadSettings(resource);
		mURL->setUrl(res->dirName());
		kDebug(KARES_DEBUG) << "ResourceLocalDirConfigWidget::loadSettings(): Directory" << mURL->url();
	}
}

void ResourceLocalDirConfigWidget::saveSettings(KRES::Resource *resource)
{
//	KAResourceLocalDir* res = dynamic_cast<KAResourceLocalDir*>(resource);
	KAResourceLocalDir* res = static_cast<KAResourceLocalDir*>(resource);
	if (!res)
		kDebug(KARES_DEBUG) << "ResourceLocalDirConfigWidget::saveSettings(KAResourceLocalDir): cast failed";
	else
		res->setDirName(mURL->url());
}
