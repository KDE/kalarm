/*
 *  prefdlg.cpp  -  program preferences dialog
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include <qvbox.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kdebug.h>

#include "prefdlg.h"
#include "prefdlg.moc"


#ifdef MISC_PREFS
KAlarmPrefDlg::KAlarmPrefDlg(GeneralSettings* genSets, MiscSettings* miscSets)
#else
KAlarmPrefDlg::KAlarmPrefDlg(GeneralSettings* genSets)
#endif
	: KDialogBase(IconList, i18n("Preferences"), Help | Default | Ok | Apply | Cancel, Ok, 0, 0, true, true)
{
	setIconListAllVisible(true);

	QVBox* frame = addVBoxPage(i18n("General"), i18n("General settings of the KAlarm program"), DesktopIcon("misc"));
	m_generalPage = new GeneralPrefs(frame);
	m_generalPage->setSettings(genSets);

#ifdef MISC_PREFS
	frame = addVBoxPage( i18n("Misc"), i18n("Miscellaneous settings"), DesktopIcon("misc"));
	m_miscPage = new MiscPrefs(frame);
	m_miscPage->setSettings(miscSets);
#endif

	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

void KAlarmPrefDlg::slotDefault()
{
	kdDebug() << "KAlarmPrefDlg::slotDefault()" << endl;
	// restore all defaults in the options...
	m_generalPage->setDefaults();
#ifdef MISC_PREFS
	m_miscPage->setDefaults();
#endif
}

void KAlarmPrefDlg::slotHelp()
{
	// show some help...
	// figure out the current active page
	// and give help for that page
}

void KAlarmPrefDlg::slotApply()
{
	kdDebug() << "KAlarmPrefDlg::slotApply()" << endl;
	// Apply the settings that are currently selected
	m_generalPage->apply();
#ifdef MISC_PREFS
	m_miscPage->apply();
#endif
}

void KAlarmPrefDlg::slotOk()
{
	kdDebug() << "KAlarmPrefDlg::slotOk()" << endl;
	// Apply the settings that are currently selected
	m_generalPage->apply();
#ifdef MISC_PREFS
	m_miscPage->apply();
#endif

	KDialogBase::slotOk();
}

void KAlarmPrefDlg::slotCancel()
{
	kdDebug() << "KAlarmPrefDlg::slotCancel()" << endl;
	// discard the current settings and use the present ones
	m_generalPage->restore();
#ifdef MISC_PREFS
	m_miscPage->restore();
#endif

	KDialogBase::slotCancel();
}
