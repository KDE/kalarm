/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <qsize.h>
class QObject;
class QString;
class KAction;
class KActionCollection;
namespace KCal { class Event; }
class KAEvent;
class KAlarmMainWindow;
class AlarmListView;

namespace KAlarm
{

enum FileType { Unknown, TextPlain, TextFormatted, TextApplication, Image };

KAlarmMainWindow*  displayMainWindowSelected(const QString& eventID = QString::null);
QSize              readConfigWindowSize(const char* window, const QSize& defaultSize);
void               writeConfigWindowSize(const char* window, const QSize&);
FileType           fileType(const QString& mimetype);
KAction*           createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*, const char* name);
void               resetDaemon();
void               resetDaemonIfQueued();    // must only be called from KAlarmApp::processQueue()

const KCal::Event* getEvent(const QString& eventID);
bool               addEvent(const KAEvent&, AlarmListView* selectionView, bool useEventID = false);
void               modifyEvent(KAEvent& oldEvent, const KAEvent& newEvent, AlarmListView* selectionView);
void               updateEvent(KAEvent&, AlarmListView* selectionView, bool archiveOnDelete = true);
void               deleteEvent(KAEvent&, bool archive = true);
void               deleteDisplayEvent(const QString& eventID);
void               undeleteEvent(KAEvent&, AlarmListView* selectionView);
void               archiveEvent(KAEvent&);

int                localeFirstDayOfWeek();

/* Given a standard KDE day number, return the day number in the week for the user's locale.
 * Standard day number = 1 (Mon) .. 7 (Sun)
 * Locale day number in week = 0 .. 6
 */
inline int         weekDay_to_localeDayInWeek(int weekDay)  { return (weekDay + 7 - localeFirstDayOfWeek()) % 7; }

/* Given a day number in the week for the user's locale, return the standard KDE day number.
 * 'index' = 0 .. 6
 * Standard day number = 1 (Mon) .. 7 (Sun)
 */
inline int         localeDayInWeek_to_weekDay(int index)  { return (index + localeFirstDayOfWeek() - 1) % 7 + 1; }

} // namespace KAlarm

#endif // FUNCTIONS_H
