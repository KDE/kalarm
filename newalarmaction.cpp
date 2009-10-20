/*
 *  newalarmaction.cpp  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  Copyright © 2007-2009 by David Jarvie <djarvie@kde.org>
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


#define DISP_ICON  QLatin1String("window-new")
#define CMD_ICON   QLatin1String("new-command-alarm")
#define MAIL_ICON  QLatin1String("mail-message-new")
#define AUDIO_ICON QLatin1String("new-audio-alarm")
#define DISP_KEY   QKeySequence(Qt::CTRL + Qt::Key_D)
#define CMD_KEY    QKeySequence(Qt::CTRL + Qt::Key_C)
#define MAIL_KEY   QKeySequence(Qt::CTRL + Qt::Key_M)
#define AUDIO_KEY  QKeySequence(Qt::CTRL + Qt::Key_U)


NewAlarmAction::NewAlarmAction(bool templates, const QString& label, QObject* parent)
	: KActionMenu(KIcon("document-new"), label, parent)
{
	mDisplayAction = new KAction(KIcon(DISP_ICON), (templates ? i18nc("@item:inmenu", "&Display Alarm Template") : i18nc("@action", "New Display Alarm")), parent);
	menu()->addAction(mDisplayAction);
	mTypes[mDisplayAction] = EditAlarmDlg::DISPLAY;
	mCommandAction = new KAction(KIcon(CMD_ICON), (templates ? i18nc("@item:inmenu", "&Command Alarm Template") : i18nc("@action", "New Command Alarm")), parent);
	menu()->addAction(mCommandAction);
	mTypes[mCommandAction] = EditAlarmDlg::COMMAND;
	mEmailAction = new KAction(KIcon(MAIL_ICON), (templates ? i18nc("@item:inmenu", "&Email Alarm Template") : i18nc("@action", "New Email Alarm")), parent);
	menu()->addAction(mEmailAction);
	mTypes[mEmailAction] = EditAlarmDlg::EMAIL;
	mAudioAction = new KAction(KIcon(AUDIO_ICON), (templates ? i18nc("@item:inmenu", "&Audio Alarm Template") : i18nc("@action", "New Audio Alarm")), parent);
	menu()->addAction(mAudioAction);
	mTypes[mAudioAction] = EditAlarmDlg::AUDIO;
	if (!templates)
	{
		mDisplayAction->setShortcut(DISP_KEY);
		mCommandAction->setShortcut(CMD_KEY);
		mEmailAction->setShortcut(MAIL_KEY);
		mAudioAction->setShortcut(AUDIO_KEY);
	}
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
	QMap<QAction*, EditAlarmDlg::Type>::ConstIterator it = mTypes.constFind(action);
	if (it != mTypes.constEnd())
		emit selected(it.value());
}
