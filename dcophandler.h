/*
 *  dcophandler.h  -  handler for DCOP calls by other applications
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef DCOPHANDLER_H
#define DCOPHANDLER_H

#include <qwidget.h>
#include <dcopobject.h>


class DcopHandler : public QWidget, public DCOPObject
{
		Q_OBJECT
	public:
		DcopHandler(const char* name);
		~DcopHandler()  { }
		virtual bool process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray& replyData);
};

#endif // DCOPHANDLER_H
