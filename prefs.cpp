/*
 *  prefs.cpp  -  program preferences
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include "kalarm.h"

#include <qobjectlist.h>
#include <qlayout.h>
#include <qlabel.h>

#include <kdialog.h>
#include <kglobal.h>
#include <klocale.h>

#include "fontcolour.h"
#include "prefsettings.h"
#include "prefs.h"
#include "prefs.moc"


PrefsBase::PrefsBase( QWidget* parent )
	: KTabCtl( parent )
{
}

PrefsBase::~PrefsBase()
{
}

QSize PrefsBase::sizeHintForWidget( QWidget* widget )
{
	// The size is computed by adding the sizeHint().height() of all
	// widget children and taking the width of the widest child and adding
	// layout()->margin() and layout()->spacing()

	QSize size;
	int numChild = 0;
	QObjectList* l = (QObjectList*)(widget->children());

	for (uint i=0;  i < l->count();  i++)
	{
		QObject* o = l->at(i);
		if (o->isWidgetType())
		{
			numChild += 1;
			QSize s = ((QWidget*)o)->sizeHint();
			if (s.isEmpty())
				s = QSize( 50, 100 ); // Default size
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

void PrefsBase::apply()
{
}

void PrefsBase::restore()
{
}

void PrefsBase::setDefaults()
{
}



GeneralPrefs::GeneralPrefs(QWidget* parent)
	: PrefsBase(parent)
{
	QWidget* page = new QWidget(this );
	QVBoxLayout* layout = new QVBoxLayout(page, 0, KDialog::spacingHint());
	layout->setMargin(KDialog::marginHint());
	m_fontChooser = new FontColourChooser(page, 0, false, QStringList(), true, "Font and Colour", false);
	layout->addWidget(m_fontChooser);

	layout->addStretch(1);
	page->setMinimumSize(sizeHintForWidget(page));

	addTab(page, i18n("Message &Appearance"));
}

GeneralPrefs::~GeneralPrefs()
{
}

void GeneralPrefs::setSettings(GeneralSettings* setts)
{
	m_settings = setts;
	m_fontChooser->setBgColour(m_settings->m_defaultBgColour);
	m_fontChooser->setFont(m_settings->m_messageFont);
}

void GeneralPrefs::restore()
{
}

void GeneralPrefs::apply()
{
	m_settings->m_defaultBgColour = m_fontChooser->bgColour();
	m_settings->m_messageFont = m_fontChooser->font();
	m_settings->saveSettings();
	m_settings->emitSettingsChanged();
}

void GeneralPrefs::setDefaults()
{
	m_fontChooser->setBgColour(GeneralSettings::default_defaultBgColour);
	m_fontChooser->setFont(GeneralSettings::default_messageFont);
}
