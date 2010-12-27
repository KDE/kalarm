/*
 *  resourceremotewidget.cpp  -  configuration widget for a remote file calendar resource
 *  Program:  kalarm
 *  Copyright © 2006,2008 by David Jarvie <djarvie@kde.org>
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

#include "resourceremote.h"
#include "resourceremotewidget.moc"

#include <kcal/resourcecachedconfig.h>

#include <kurlrequester.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <QLabel>
#include <QGridLayout>


ResourceRemoteConfigWidget::ResourceRemoteConfigWidget(QWidget* parent)
    : ResourceConfigWidget(parent)
{
    QGridLayout* layout = new QGridLayout(this);

    QLabel* label = new QLabel(i18nc("@label:textbox", "Download from:"), this);
    layout->addWidget(label, 1, 0);
    mDownloadUrl = new KUrlRequester(this);
    mDownloadUrl->setMode(KFile::File);
    layout->addWidget(mDownloadUrl, 1, 1);

    label = new QLabel(i18nc("@label:textbox", "Upload to:"), this);
    layout->addWidget(label, 2, 0);
    mUploadUrl = new KUrlRequester(this);
    mUploadUrl->setMode(KFile::File);
    layout->addWidget(mUploadUrl, 2, 1);

    mReloadConfig = new KCal::ResourceCachedReloadConfig(this);
    layout->addWidget(mReloadConfig, 3, 0, 1, 2);

    mSaveConfig = new KCal::ResourceCachedSaveConfig(this);
    layout->addWidget(mSaveConfig, 4, 0, 1, 2);
}

void ResourceRemoteConfigWidget::loadSettings(KRES::Resource* resource)
{
//    KAResourceRemote* res = dynamic_cast<KAResourceRemote*>(resource);
    KAResourceRemote* res = static_cast<KAResourceRemote*>(resource);
    if (!res)
        kError(KARES_DEBUG) << "KAResourceRemote: cast failed";
    else
    {
        ResourceConfigWidget::loadSettings(resource);
        mDownloadUrl->setUrl(res->downloadUrl().url());
        mUploadUrl->setUrl(res->uploadUrl().url());
        mReloadConfig->loadSettings(res);
        mSaveConfig->loadSettings(res);
#ifndef NDEBUG
        kDebug(KARES_DEBUG) << "File" << mDownloadUrl->url() << " type" << res->typeName();
#endif
    }
}

void ResourceRemoteConfigWidget::saveSettings(KRES::Resource* resource)
{
//    KAResourceRemote* res = dynamic_cast<KAResourceRemote*>(resource);
    KAResourceRemote* res = static_cast<KAResourceRemote*>(resource);
    if (!res)
        kDebug(KARES_DEBUG) << "KAResourceRemote: cast failed";
    else
    {
        res->setUrls(mDownloadUrl->url(), mUploadUrl->url());
        mReloadConfig->saveSettings(res);
        mSaveConfig->saveSettings(res);

        if (mUploadUrl->url().isEmpty()  &&  !resource->readOnly())
        {
            KMessageBox::information(this, i18nc("@info", "You have specified no upload URL: the alarm calendar will be read-only."),
                                     "RemoteResourseNoUploadURL");
            resource->setReadOnly(true);
        }
    }
}

// vim: et sw=4:
