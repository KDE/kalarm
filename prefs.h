/*
 *  prefs.h  -  program preferences
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#ifndef PREFS_H
#define PREFS_H

#include <qsize.h>
#include <ktabctl.h>
class FontColourChooser;
class GeneralSettings;


// Base class for each tab in the Preferences dialog
class PrefsBase : public KTabCtl
{
	Q_OBJECT
public:
	PrefsBase(QWidget*);
	~PrefsBase();

	QSize        sizeHintForWidget(QWidget*);
	virtual void restore() = 0;
	virtual void apply() = 0;
	virtual void setDefaults() = 0;
};

// General tab of the Preferences dialog
class GeneralPrefs : public PrefsBase
{
	Q_OBJECT
public:
	GeneralPrefs(QWidget*);
	~GeneralPrefs();

	void setSettings(GeneralSettings*);

	virtual void restore();
	virtual void apply();
	virtual void setDefaults();

	GeneralSettings* m_settings;
	FontColourChooser*  m_fontChooser;
};

#endif // PREFS_H
