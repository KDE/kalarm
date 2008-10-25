/*
 *  newalarmaction.cpp  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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
#include "newalarmaction.moc"

#include "shellprocess.h"

#include <kmenu.h>
#include <kactionmenu.h>
#include <klocale.h>
#include <kstandardshortcut.h>
#include <kdebug.h>


NewAlarmAction::NewAlarmAction(bool templates, const QString& label, QObject* parent)
	: KActionMenu(KIcon("document-new"), label, parent)
{
	setShortcuts(KStandardShortcut::openNew());
	QAction* act = menu()->addAction(KIcon(QLatin1String("window-new")), (templates ? i18nc("@item:inmenu", "&Display Alarm Template") : i18nc("@item:inmenu", "Display Alarm")));
	mTypes[act] = EditAlarmDlg::DISPLAY;
	mCommandAction = menu()->addAction(KIcon(QLatin1String("system-run")), (templates ? i18nc("@item:inmenu", "&Command Alarm Template") : i18nc("@item:inmenu", "Command Alarm")));
	mTypes[mCommandAction] = EditAlarmDlg::COMMAND;
	act = menu()->addAction(KIcon(QLatin1String("mail-message-new")), (templates ? i18nc("@item:inmenu", "&Email Alarm Template") : i18nc("@item:inmenu", "Email Alarm")));
	mTypes[act] = EditAlarmDlg::EMAIL;
	setDelayed(false);
	connect(menu(), SIGNAL(aboutToShow()), SLOT(slotInitMenu()));
	connect(menu(), SIGNAL(triggered(QAction*)), SLOT(slotSelected(QAction*)));
}

/******************************************************************************
*  Called when the action is clicked.
*/
void NewAlarmAction::slotInitMenu()
{
	// Don't allow shell commands in kiosk mode
	mCommandAction->setEnabled(ShellProcess::authorised());
}

/******************************************************************************
*  Called when an alarm type is selected from the New popup menu.
*/
void NewAlarmAction::slotSelected(QAction* action)
{
	QMap<QAction*, EditAlarmDlg::Type>::const_iterator it = mTypes.find(action);
	if (it != mTypes.end())
		emit selected(it.value());
}
