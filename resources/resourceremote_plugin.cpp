/*
 *  resourceremote_plugin.cpp  -  KAlarm remote alarm calendar file resource plugin
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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
 *  51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include <kglobal.h>
#include <klocale.h>

#include "resourceremote.h"
#include "resourceremotewidget.h"

extern "C"
{
	void* init_kalarm_remote()
	{
#warning Add .pot files to Makefile.am
#ifndef KALARM_STANDALONE
		KGlobal::locale()->insertCatalog("libkcal");
		KGlobal::locale()->insertCatalog("kres_remote");
#endif
		return new KRES::PluginFactory<KAResourceRemote, ResourceRemoteConfigWidget>();
	}
}
