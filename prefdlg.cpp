/*
 *  prefdlg.cpp  -  program preferences dialog
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

#include <qvbox.h>

#include <kapplication.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kdebug.h>

#include "prefs.h"
#include "prefdlg.moc"


KAlarmPrefDlg::KAlarmPrefDlg(Settings* sets)
	: KDialogBase(IconList, i18n("Preferences"), Help | Default | Ok | Apply | Cancel, Ok, 0L, 0L, true, true)
{
	setIconListAllVisible(true);

	QVBox* frame = addVBoxPage( i18n("Miscellaneous"), i18n("Miscellaneous"), DesktopIcon("misc"));
	m_miscPage = new MiscPrefs(frame);
	m_miscPage->setSettings(sets);

	frame = addVBoxPage(i18n("Edit Defaults"), i18n("Alarm Edit Defaults"), DesktopIcon("edit"));
	m_defaultPage = new DefaultPrefs(frame);
	m_defaultPage->setSettings(sets);

	frame = addVBoxPage(i18n("Appearance"), i18n("Message Appearance"), DesktopIcon("appearance"));
	m_appearancePage = new AppearancePrefs(frame);
	m_appearancePage->setSettings(sets);

	adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
}

// Restore all defaults in the options...
void KAlarmPrefDlg::slotDefault()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotDefault()" << endl;
	m_appearancePage->setDefaults();
	m_defaultPage->setDefaults();
	m_miscPage->setDefaults();
}

void KAlarmPrefDlg::slotHelp()
{
	kapp->invokeHelp("preferences");
}

// Apply the settings that are currently selected
void KAlarmPrefDlg::slotApply()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotApply()" << endl;
	m_appearancePage->apply(false);
	m_defaultPage->apply(false);
	m_miscPage->apply(true);
}

// Apply the settings that are currently selected
void KAlarmPrefDlg::slotOk()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotOk()" << endl;
	slotApply();
	KDialogBase::slotOk();
}

// Discard the current settings and use the present ones
void KAlarmPrefDlg::slotCancel()
{
	kdDebug(5950) << "KAlarmPrefDlg::slotCancel()" << endl;
	m_appearancePage->restore();
	m_defaultPage->restore();
	m_miscPage->restore();

	KDialogBase::slotCancel();
}
