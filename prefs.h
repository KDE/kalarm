/*
 *  prefs.h  -  program preferences
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

#ifndef PREFS_H
#define PREFS_H

#include <qsize.h>
#include <qdatetime.h>
#include <ktabctl.h>
class QCheckBox;
class QRadioButton;
class QSpinBox;
class FontColourChooser;
class Settings;
class TimeSpinBox;


// Base class for each tab in the Preferences dialog
class PrefsBase : public KTabCtl
{
		Q_OBJECT
	public:
		PrefsBase(QWidget* parent);

		QSize        sizeHintForWidget(QWidget*);
		void         setSettings(Settings*);
		virtual void restore() = 0;
		virtual void apply(bool syncToDisc) = 0;
		virtual void setDefaults() = 0;

	protected:
		Settings*    mSettings;
};


// Appearance tab of the Preferences dialog
class AppearancePrefs : public PrefsBase
{
		Q_OBJECT
	public:
		AppearancePrefs(QWidget* parent);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private:
		FontColourChooser*  mFontChooser;
};


// Miscellaneous tab of the Preferences dialog
class MiscPrefs : public PrefsBase
{
		Q_OBJECT
	public:
		MiscPrefs(QWidget* parent);

		virtual void restore();
		virtual void apply(bool syncToDisc);
		virtual void setDefaults();

	private slots:
		void         slotRunModeToggled(bool on);

	private:
		QRadioButton*  mRunInSystemTray;
		QRadioButton*  mRunOnDemand;
		QCheckBox*     mDisableAlarmsIfStopped;
		QCheckBox*     mAutostartTrayIcon1;
		QCheckBox*     mAutostartTrayIcon2;
		QCheckBox*     mConfirmAlarmDeletion;
		QSpinBox*      mDaemonTrayCheckInterval;
		TimeSpinBox*   mStartOfDay;
};

#endif // PREFS_H
