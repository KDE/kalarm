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
#ifndef MESSAGEMAP_H
#define MESSAGEMAP_H

#include "kalarm.h"

#include <qobject.h>
#include <qmap.h>
class KProcess;
class MessageData;

		class MessageMap : public QObject
		{
				Q_OBJECT
			public:
				MessageMap() { }
				~MessageMap() { }
				void  insert(KProcess& p, MessageData& d) { messageMap.insert(&p, &d); }
			public slots:
				void  slotDelete(KProcess*);
			private:
				typedef QMap<KProcess*, MessageData*> Map;  // KAlarmMsg processes and their data
				Map   messageMap;
		};

#endif
