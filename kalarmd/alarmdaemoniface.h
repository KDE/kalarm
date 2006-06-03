/*
 *  alarmdaemoniface.h  -  DCOP request interface
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2001,2004,2006 by David Jarvie <software@astrojar.org.uk>
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown <pbrown@kde.org>
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

#ifndef ALARMDAEMONIFACE_H
#define ALARMDAEMONIFACE_H

class QByteArray;
#include <dcopobject.h>


class AlarmDaemonIface : virtual public DCOPObject
{
		K_DCOP
	k_dcop:
		virtual ASYNC enableAutoStart(bool enable) = 0;
		virtual ASYNC enable(bool enabl) = 0;
		virtual ASYNC reloadResource(const QString& id) = 0;
		virtual ASYNC resetResource(const QString& id) = 0;
		virtual ASYNC resourceActive(const QString& id, bool active) = 0;
		virtual ASYNC resourceLocation(const QString& id, const QString& locn, const QString& locn2) = 0;
		virtual ASYNC registerApp(const QByteArray& appName, const QByteArray& dcopObject, bool startClient) = 0;
		virtual ASYNC registerChange(const QByteArray& appName, bool startClient) = 0;
		virtual ASYNC eventHandled(const QString& eventID, bool reload) = 0;
		virtual ASYNC quit() = 0;
};

#endif
