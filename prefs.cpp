/*
 *  prefs.cpp  -  program preferences
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "kalarm.h"

#include <qobjectlist.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qwhatsthis.h>

#include <kdialog.h>
#include <kglobal.h>
#include <klocale.h>

#include "fontcolour.h"
#include "prefsettings.h"
#include "prefs.h"
#include "prefs.moc"


PrefsBase::PrefsBase(QWidget* parent)
	: KTabCtl(parent)
{
}

QSize PrefsBase::sizeHintForWidget(QWidget* widget)
{
	// The size is computed by adding the sizeHint().height() of all
	// widget children and taking the width of the widest child and adding
	// layout()->margin() and layout()->spacing()

	QSize size;
	int numChild = 0;
	QObjectList* list = (QObjectList*)(widget->children());

	for (uint i = 0;  i < list->count();  ++i)
	{
		QObject* obj = list->at(i);
		if (obj->isWidgetType())
		{
			++numChild;
			QSize s = static_cast<QWidget*>(obj)->sizeHint();
			if (s.isEmpty())
				s = QSize(50, 100); // Default size
			size.setHeight(size.height() + s.height());
			if (s.width() > size.width())
				size.setWidth(s.width());
		}
	}

	if (numChild > 0)
	{
		size.setHeight(size.height() + widget->layout()->spacing()*(numChild-1));
		size += QSize(widget->layout()->margin()*2, widget->layout()->margin()*2 + 1);
	}
	else
		size = QSize(1, 1);

	return size;
}

void PrefsBase::setSettings(Settings* setts)
{
	mSettings = setts;
	restore();
}

void PrefsBase::apply(bool syncToDisc)
{
	mSettings->saveSettings(syncToDisc);
	mSettings->emitSettingsChanged();
}



MiscPrefs::MiscPrefs(QWidget* parent)
	: PrefsBase(parent)
{
	QWidget* page = new QWidget(this );
	QVBoxLayout* layout = new QVBoxLayout(page, 0, KDialog::spacingHint());
	layout->setMargin(KDialog::marginHint());

	mAutostartTrayIcon = new QCheckBox(i18n("Autostart system tray icon at login"), page, "autoTray");
	layout->addWidget(mAutostartTrayIcon);
	QWhatsThis::add(mAutostartTrayIcon,
	      i18n("Check to display the system tray icon whenever you start KDE."));

	QGridLayout* grid = new QGridLayout(page, 1, 2, KDialog::spacingHint());
	layout->addLayout(grid);
	QLabel* lbl = new QLabel(i18n("System tray icon update interval [seconds]"), page);
	lbl->setFixedSize(lbl->sizeHint());
	grid->addWidget(lbl, 0, 0, AlignLeft);
	mDaemonTrayCheckInterval = new QSpinBox(1, 9999, 1, page, "daemonCheck");
	grid->addWidget(mDaemonTrayCheckInterval, 0, 1, AlignLeft);
	QWhatsThis::add(mDaemonTrayCheckInterval,
	      i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is running."));

	layout->addStretch(1);
	page->setMinimumSize(sizeHintForWidget(page));

	addTab(page, i18n("&Miscellaneous"));
}

void MiscPrefs::restore()
{
	mAutostartTrayIcon->setChecked(mSettings->mAutostartTrayIcon);
	mDaemonTrayCheckInterval->setValue(mSettings->mDaemonTrayCheckInterval);
}

void MiscPrefs::apply(bool syncToDisc)
{
	mSettings->mAutostartTrayIcon       = mAutostartTrayIcon->isChecked();
	mSettings->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	PrefsBase::apply(syncToDisc);
}

void MiscPrefs::setDefaults()
{
	mAutostartTrayIcon->setChecked(Settings::default_autostartTrayIcon);
	mDaemonTrayCheckInterval->setValue(Settings::default_daemonTrayCheckInterval);
}



AppearancePrefs::AppearancePrefs(QWidget* parent)
	: PrefsBase(parent)
{
	QWidget* page = new QWidget(this );
	QVBoxLayout* layout = new QVBoxLayout(page, 0, KDialog::spacingHint());
	layout->setMargin(KDialog::marginHint());
	mFontChooser = new FontColourChooser(page, 0L, false, QStringList(), true, i18n("Font and Color"), false);
	layout->addWidget(mFontChooser);

	layout->addStretch(1);
	page->setMinimumSize(sizeHintForWidget(page));

	addTab(page, i18n("Message &Appearance"));
}

void AppearancePrefs::restore()
{
	mFontChooser->setBgColour(mSettings->mDefaultBgColour);
	mFontChooser->setFont(mSettings->mMessageFont);
}

void AppearancePrefs::apply(bool syncToDisc)
{
	mSettings->mDefaultBgColour = mFontChooser->bgColour();
	mSettings->mMessageFont     = mFontChooser->font();
	PrefsBase::apply(syncToDisc);
}

void AppearancePrefs::setDefaults()
{
	mFontChooser->setBgColour(Settings::default_defaultBgColour);
	mFontChooser->setFont(Settings::default_messageFont);
}
