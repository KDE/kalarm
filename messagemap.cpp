/*
 *  kalarmapp.cpp  -  description
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <kdebug.h>
#include "messagemap.h"
#include "kalarmmsg.h"
#include "messagemap.moc"

/******************************************************************************
* Called when a KAlarmMsg process either exits or completes reading its STDIN
* data. The process is deleted from the message map.
*/
void MessageMap::slotDelete(KProcess* proc)
{
	kdDebug() << "MessageMap::slotDelete()" << endl;
	Map::Iterator it = messageMap.find(proc);
	if (it != messageMap.end())
	{
		delete it.data();    // delete the MessageData structure first
		messageMap.remove(it);
	}
}

