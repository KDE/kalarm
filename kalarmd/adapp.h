/*
 *  adapp.h  -  kalarmd application
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ADAPP_H
#define ADAPP_H

#include <kuniqueapplication.h>

class AlarmDaemon;


class AlarmDaemonApp : public KUniqueApplication
{
		Q_OBJECT
	public:
		AlarmDaemonApp();
		virtual int  newInstance();

	private:
		AlarmDaemon* mAd;
};

#endif // ADAPP_H
