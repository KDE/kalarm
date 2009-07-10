/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2004-2009 by David Jarvie <djarvie@kde.org>
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

#include <QSize>
#include <QString>
#include <QList>

#include "alarmevent.h"
#include "editdlg.h"
#include <kfile.h>
#include <kmimetype.h>

namespace KCal { class Event; }
class QWidget;
class KActionCollection;
class KToggleAction;
class QAction;
class AlarmResource;
class KAEvent;
class MainWindow;
class AlarmText;
class TemplateMenuAction;

namespace KAlarm
{

/** Return codes from fileType() */
enum FileType { Unknown, TextPlain, TextFormatted, TextApplication, Image };
/** Return codes from calendar update functions.
 *  The codes are ordered by severity, so...
 *  DO NOT CHANGE THE ORDER OF THESE VALUES!
 */
enum UpdateStatus {
	UPDATE_OK,            // update succeeded
	UPDATE_KORG_FUNCERR,  // update succeeded, but KOrganizer reported an error updating
	UPDATE_KORG_ERRSTART, // update succeeded, but KOrganizer update failed (KOrganizer not fully started)
	UPDATE_KORG_ERR,      // update succeeded, but KOrganizer update failed
	UPDATE_ERROR,         // update failed partially
	UPDATE_FAILED,        // update failed completely
	SAVE_FAILED           // calendar was updated in memory, but save failed
};
/** Error codes supplied as parameter to displayUpdateError() */
enum UpdateError { ERR_ADD, ERR_MODIFY, ERR_DELETE, ERR_REACTIVATE, ERR_TEMPLATE };


/** Display a main window with the specified event selected */
MainWindow*         displayMainWindowSelected(const QString& eventID = QString());
bool                readConfigWindowSize(const char* window, QSize&, int* splitterWidth = 0);
void                writeConfigWindowSize(const char* window, const QSize&, int splitterWidth = -1);
/** Check from its mime type whether a file appears to be a text or image file.
 *  If a text file, its type is distinguished.
 */
FileType            fileType(const KMimeType::Ptr& mimetype);
QString             browseFile(const QString& caption, QString& defaultDir, const QString& initialFile = QString(),
                               const QString& filter = QString(), KFile::Modes mode = 0, QWidget* parent = 0);
bool                editNewAlarm(const QString& templateName, QWidget* parent = 0);
void                editNewAlarm(EditAlarmDlg::Type, QWidget* parent = 0);
void                editNewAlarm(KAEventData::Action, QWidget* parent = 0, const AlarmText* = 0);
void                editNewAlarm(const KAEvent* preset, QWidget* parent = 0);
bool                editAlarm(const QString& eventID, QWidget* parent = 0);
void                editAlarm(KAEvent*, QWidget* parent = 0);
void                viewAlarm(const KAEvent*, QWidget* parent = 0);
void                editNewTemplate(EditAlarmDlg::Type, QWidget* parent = 0);
void                editNewTemplate(const KAEvent* preset, QWidget* parent = 0);
void                editTemplate(KAEvent*, QWidget* parent = 0);
void                execNewAlarmDlg(EditAlarmDlg*, bool alreadyExecuted = false);
/** Create a "New From Template" QAction */
TemplateMenuAction* createNewFromTemplateAction(const QString& label, KActionCollection*, const QString& name);
KToggleAction*      createAlarmEnableAction(QObject* parent);
KToggleAction*      createSpreadWindowsAction(QObject* parent);
/** Returns a list of all alarm templates.
 *  If shell commands are disabled, command alarm templates are omitted.
 */
KAEvent::List       templateList();
void                outputAlarmWarnings(QWidget* parent, const KAEvent* = 0);
void                refreshAlarms();
void                refreshAlarmsIfQueued();    // must only be called from KAlarmApp::processQueue()
QString             runKMail(bool minimise);
bool                runProgram(const QString& program, QString& dbusService, const QString& dbusWindowPath, QString& errorMessage);

QStringList         dontShowErrors(const QString& eventId);
bool                dontShowErrors(const QString& eventId, const QString& tag);
void                setDontShowErrors(const QString& eventId, const QStringList& tags = QStringList());
void                setDontShowErrors(const QString& eventId, const QString& tag);

enum         // 'options' parameter values for addEvent(). May be OR'ed together.
{
	USE_EVENT_ID       = 0x01,   // use event ID if it's provided
	NO_RESOURCE_PROMPT = 0x02,   // don't prompt for resource
	ALLOW_KORG_UPDATE  = 0x04    // allow change to be sent to KOrganizer
};
UpdateStatus        addEvent(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0, int options = ALLOW_KORG_UPDATE, bool showKOrgErr = true);
UpdateStatus        addEvents(QList<KAEvent>&, QWidget* msgParent = 0, bool allowKOrgUpdate = true, bool showKOrgErr = true);
bool                addArchivedEvent(KAEvent&, AlarmResource* = 0);
UpdateStatus        addTemplate(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0);
UpdateStatus        modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateStatus        updateEvent(KAEvent&, QWidget* msgParent = 0, bool archiveOnDelete = true);
UpdateStatus        updateTemplate(KAEvent&, QWidget* msgParent = 0);
UpdateStatus        deleteEvent(KAEvent&, bool archive = true, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateStatus        deleteEvents(KAEvent::List&, bool archive = true, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateStatus        deleteTemplates(const QStringList& eventIDs, QWidget* msgParent = 0);
inline UpdateStatus deleteTemplate(const QString& eventID, QWidget* msgParent = 0)
			{ return deleteTemplates(QStringList(eventID), msgParent); }
void                deleteDisplayEvent(const QString& eventID);
UpdateStatus        reactivateEvent(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateStatus        reactivateEvents(KAEvent::List&, QStringList& ineligibleIDs, AlarmResource* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateStatus        enableEvents(KAEvent::List&, bool enable, QWidget* msgParent = 0);
void                purgeArchive(int purgeDays);    // must only be called from KAlarmApp::processQueue()
void                displayUpdateError(QWidget* parent, UpdateStatus, UpdateError, int nAlarms, int nKOrgAlarms = 1, bool showKOrgError = true);
void                displayKOrgUpdateError(QWidget* parent, UpdateError, UpdateStatus korgError, int nAlarms);

bool                convTimeString(const QByteArray& timeString, KDateTime& dateTime, const KDateTime& defaultDt = KDateTime(), bool allowTZ = true);
KDateTime           applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                                  bool haveTime, const KDateTime& defaultDt = KDateTime());
bool                isWorkingTime(const KDateTime&, const KAEvent*);

#ifndef NDEBUG
void                setTestModeConditions();
bool                setSimulatedSystemTime(const QByteArray& timeString);
#endif

} // namespace KAlarm

bool caseInsensitiveLessThan(const QString& s1, const QString& s2);

#endif // FUNCTIONS_H
