/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2004-2006 by David Jarvie <software@astrojar.org.uk>
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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/**  @file functions.h - miscellaneous functions */

#include <qsize.h>
#include <qstring.h>

#include "alarmevent.h"

class QObject;
class QWidget;
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

/** Return codes from fileType() */
enum FileType { Unknown, TextPlain, TextFormatted, TextApplication, Image };
/** Return codes from calendar update functions.
 *  The codes are ordered by severity.
 */
enum UpdateStatus {
	UPDATE_OK,          // update succeeded
	UPDATE_KORG_ERR,    // update succeeded, but KOrganizer update failed
	UPDATE_ERROR,       // update failed partially
	UPDATE_FAILED,      // update failed completely
	SAVE_FAILED         // calendar was updated in memory, but save failed
};
/** Error codes supplied as parameter to displayUpdateError() */
enum UpdateError { ERR_ADD, ERR_DELETE, ERR_REACTIVATE, ERR_TEMPLATE };
/** Error codes supplied as parameter to displayKOrgUpdateError() */
enum KOrgUpdateError { KORG_ERR_ADD, KORG_ERR_MODIFY, KORG_ERR_DELETE };


/** Display a main window with the specified event selected */
MainWindow*         displayMainWindowSelected(const QString& eventID = QString::null);
bool                readConfigWindowSize(const char* window, QSize&);
void                writeConfigWindowSize(const char* window, const QSize&);
/** Check from its mime type whether a file appears to be a text or image file.
 *  If a text file, its type is distinguished.
 */
FileType            fileType(const QString& mimetype);
/** Return current KAlarm version number */
int                 Version();
inline int          Version(int major, int minor, int rev)     { return major*10000 + minor*100 + rev; }
int                 getVersionNumber(const QString& version, QString* subVersion = 0);
/** Return which version of KAlarm was the first to use the current calendar/event format */
inline int          currentCalendarVersion()        { return KAEvent::calVersion(); }
inline QString      currentCalendarVersionString()  { return KAEvent::calVersionString(); }
QString             browseFile(const QString& caption, QString& defaultDir, const QString& initialFile = QString::null,
                               const QString& filter = QString::null, int mode = 0, QWidget* parent = 0, const char* name = 0);
bool                edit(const QString& eventID);
bool                editNew(const QString& templateName = QString::null);
/** Create a "New Alarm" KAction */
KAction*            createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*, const char* name);
/** Create a "New From Template" KAction */
TemplateMenuAction* createNewFromTemplateAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*, const char* name);
/** Returns a list of all alarm templates.
 *  If shell commands are disabled, command alarm templates are omitted.
 */
QValueList<KAEvent> templateList();
void                outputAlarmWarnings(QWidget* parent, const KAEvent* = 0);
void                resetDaemon();
void                resetDaemonIfQueued();    // must only be called from KAlarmApp::processQueue()
QString             runKMail(bool minimise);
bool                runProgram(const QCString& program, const QCString& windowName, QCString& dcopName, QString& errorMessage);

UpdateStatus        addEvent(KAEvent&, AlarmListView* selectionView, QWidget* errmsgParent = 0, bool useEventID = false, bool allowKOrgUpdate = true);
bool                addExpiredEvent(KAEvent&);
UpdateStatus        addTemplate(KAEvent&, TemplateListView* selectionView, QWidget* errmsgParent = 0);
UpdateStatus        modifyEvent(KAEvent& oldEvent, const KAEvent& newEvent, AlarmListView* selectionView, QWidget* errmsgParent = 0);
UpdateStatus        updateEvent(KAEvent&, AlarmListView* selectionView, QWidget* errmsgParent = 0, bool archiveOnDelete = true, bool incRevision = true);
UpdateStatus        updateTemplate(const KAEvent&, TemplateListView* selectionView, QWidget* errmsgParent = 0);
UpdateStatus        deleteEvent(KAEvent&, bool archive = true, QWidget* errmsgParent = 0);
UpdateStatus        deleteTemplate(const KAEvent&);
void                deleteDisplayEvent(const QString& eventID);
UpdateStatus        reactivateEvent(KAEvent&, AlarmListView* selectionView, bool useEventID = false);
UpdateStatus        enableEvent(KAEvent&, AlarmListView* selectionView, bool enable);
void                displayUpdateError(QWidget* parent, UpdateStatus, UpdateError, int nAlarms);
void                displayKOrgUpdateError(QWidget* parent, KOrgUpdateError, int nAlarms);

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
