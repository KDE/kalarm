/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <qsize.h>
#include <qptrlist.h>
class QObject;
class QString;
class KAction;
class KActionCollection;
namespace KCal { class Event; }
class KAEvent;
class MainWindow;
class AlarmListView;
class TemplateListView;
class TemplateMenuAction;

namespace KAlarm
{

enum FileType { Unknown, TextPlain, TextFormatted, TextApplication, Image };

MainWindow*         displayMainWindowSelected(const QString& eventID = QString::null);
bool                readConfigWindowSize(const char* window, QSize&);
void                writeConfigWindowSize(const char* window, const QSize&);
FileType            fileType(const QString& mimetype);
KAction*            createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*, const char* name);
TemplateMenuAction* createNewFromTemplateAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*, const char* name);
QPtrList<KAEvent>   templateList();
void                resetDaemon();
void                resetDaemonIfQueued();    // must only be called from KAlarmApp::processQueue()

bool                addEvent(KAEvent&, AlarmListView* selectionView, bool useEventID = false);
bool                addExpiredEvent(KAEvent&);
bool                addTemplate(KAEvent&, TemplateListView* selectionView);
void                modifyEvent(KAEvent& oldEvent, const KAEvent& newEvent, AlarmListView* selectionView);
void                updateEvent(KAEvent&, AlarmListView* selectionView, bool archiveOnDelete = true, bool incRevision = true);
void                updateTemplate(const KAEvent&, TemplateListView* selectionView);
void                deleteEvent(KAEvent&, bool archive = true);
void                deleteTemplate(const KAEvent&);
void                deleteDisplayEvent(const QString& eventID);
bool                reactivateEvent(KAEvent&, AlarmListView* selectionView, bool useEventID = false);
void                enableEvent(KAEvent&, AlarmListView* selectionView, bool enable);

QString             stripAccel(const QString&);

int                 localeFirstDayOfWeek();

/* Given a standard KDE day number, return the day number in the week for the user's locale.
 * Standard day number = 1 (Mon) .. 7 (Sun)
 * Locale day number in week = 0 .. 6
 */
inline int          weekDay_to_localeDayInWeek(int weekDay)  { return (weekDay + 7 - localeFirstDayOfWeek()) % 7; }

/* Given a day number in the week for the user's locale, return the standard KDE day number.
 * 'index' = 0 .. 6
 * Standard day number = 1 (Mon) .. 7 (Sun)
 */
inline int          localeDayInWeek_to_weekDay(int index)  { return (index + localeFirstDayOfWeek() - 1) % 7 + 1; }

} // namespace KAlarm

#endif // FUNCTIONS_H
