/*
 *  prefsettings.h  -  program preference settings
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#ifndef PREFSETTINGS_H
#define PREFSETTINGS_H

#include <qobject.h>
#include <qcolor.h>
#include <qfont.h>
class QWidget;

class SettingsBase : public QObject
{
	Q_OBJECT
public:
	SettingsBase(QWidget* parent);
	~SettingsBase();

	virtual void loadSettings() = 0;
	virtual void saveSettings() = 0;
	void emitSettingsChanged();

signals:
	void settingsChanged();
};


// Settings configured in the General tab of the Preferences dialog
class GeneralSettings : public SettingsBase
{
	Q_OBJECT
public:
	GeneralSettings(QWidget* parent);
	~GeneralSettings();
	
	QColor defaultBgColour() const   { return m_defaultBgColour; }
	const QFont&  messageFont() const   { return m_messageFont; }
	// some virtual functions that will be overloaded from the base class
	virtual void loadSettings();
	virtual void saveSettings();

	static const QColor default_defaultBgColour;
	static const QFont  default_messageFont;
	QColor m_defaultBgColour;
	QFont  m_messageFont;
};

#endif // PREFSETTINGS_H
