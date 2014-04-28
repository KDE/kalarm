/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2004-2014 by David Jarvie <djarvie@kde.org>
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

#include "editdlg.h"
#ifdef USE_AKONADI
#include "eventid.h"
#endif

#include <KAlarmCal/kaevent.h>
#ifdef USE_AKONADI
#include <AkonadiCore/collection.h>
#include <AkonadiCore/item.h>
#endif
#include <kfile.h>
#include <kmimetype.h>

#include <QSize>
#include <QString>
#include <QVector>

using namespace KAlarmCal;

namespace KCal { class Event; }
class QWidget;
class QAction;
class KAction;
class KToggleAction;
class AlarmResource;
class MainWindow;
#ifdef USE_AKONADI
class AlarmListModel;
#endif

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
    UPDATE_KORG_ERRINIT,  // update succeeded, but KOrganizer update failed (KOrganizer not started)
    UPDATE_KORG_ERR,      // update succeeded, but KOrganizer update failed
    UPDATE_ERROR,         // update failed partially
    UPDATE_FAILED,        // update failed completely
    SAVE_FAILED           // calendar was updated in memory, but save failed
};
/** Error codes supplied as parameter to displayUpdateError() */
enum UpdateError { ERR_ADD, ERR_MODIFY, ERR_DELETE, ERR_REACTIVATE, ERR_TEMPLATE };

/** Result of calendar update. */
struct UpdateResult
{
    UpdateStatus  status;   // status code
    QString       message;  // error message if any
    UpdateResult() : status(UPDATE_OK) {}
    explicit UpdateResult(UpdateStatus s, const QString& m = QString()) : status(s), message(m) {}
    UpdateResult& operator=(UpdateStatus s)  { status = s; message.clear(); return *this; }
    bool operator==(UpdateStatus s) const  { return status == s; }
    bool operator!=(UpdateStatus s) const  { return status != s; }
    void set(UpdateStatus s) { operator=(s); }
    void set(UpdateStatus s, const QString& m) { status = s; message = m; }
};

/** Display a main window with the specified event selected */
#ifdef USE_AKONADI
MainWindow*         displayMainWindowSelected(Akonadi::Item::Id = -1);
#else
MainWindow*         displayMainWindowSelected(const QString& eventId = QString());
#endif
bool                readConfigWindowSize(const char* window, QSize&, int* splitterWidth = 0);
void                writeConfigWindowSize(const char* window, const QSize&, int splitterWidth = -1);
/** Check from its mime type whether a file appears to be a text or image file.
 *  If a text file, its type is distinguished.
 */
FileType            fileType(const KMimeType::Ptr& mimetype);
/** Check that a file exists and is a plain readable file, optionally a text/image file.
 *  Display a Continue/Cancel error message if 'errmsgParent' non-null.
 */
enum FileErr {
    FileErr_None = 0,
    FileErr_Blank,           // generic blank error
    FileErr_Nonexistent, FileErr_Directory, FileErr_Unreadable, FileErr_NotTextImage,
    FileErr_BlankDisplay,    // blank error to use for file to display
    FileErr_BlankPlay        // blank error to use for file to play
};
FileErr             checkFileExists(QString& filename, KUrl&);
bool                showFileErrMessage(const QString& filename, FileErr, FileErr blankError, QWidget* errmsgParent);

/** If a url string is a local file, strip off the 'file:/' prefix. */
QString             pathOrUrl(const QString& url);

QString             browseFile(const QString& caption, QString& defaultDir, const QString& initialFile = QString(),
                               const QString& filter = QString(), KFile::Modes mode = 0, QWidget* parent = 0);
bool                editNewAlarm(const QString& templateName, QWidget* parent = 0);
void                editNewAlarm(EditAlarmDlg::Type, QWidget* parent = 0);
void                editNewAlarm(KAEvent::SubAction, QWidget* parent = 0, const AlarmText* = 0);
void                editNewAlarm(const KAEvent* preset, QWidget* parent = 0);
void                editAlarm(KAEvent*, QWidget* parent = 0);
#ifdef USE_AKONADI
bool                editAlarmById(const EventId& eventID, QWidget* parent = 0);
void                updateEditedAlarm(EditAlarmDlg*, KAEvent&, Akonadi::Collection&);
#else
bool                editAlarmById(const QString& eventID, QWidget* parent = 0);
void                updateEditedAlarm(EditAlarmDlg*, KAEvent&, AlarmResource*);
#endif
void                viewAlarm(const KAEvent*, QWidget* parent = 0);
void                editNewTemplate(EditAlarmDlg::Type, QWidget* parent = 0);
void                editNewTemplate(const KAEvent* preset, QWidget* parent = 0);
void                editTemplate(KAEvent*, QWidget* parent = 0);
void                execNewAlarmDlg(EditAlarmDlg*);
/** Create a "New From Template" QAction */
KToggleAction*      createAlarmEnableAction(QObject* parent);
KAction*            createStopPlayAction(QObject* parent);
KToggleAction*      createSpreadWindowsAction(QObject* parent);
/** Returns a list of all alarm templates.
 *  If shell commands are disabled, command alarm templates are omitted.
 */
KAEvent::List       templateList();
void                outputAlarmWarnings(QWidget* parent, const KAEvent* = 0);
void                refreshAlarms();
void                refreshAlarmsIfQueued();    // must only be called from KAlarmApp::processQueue()
QString             runKMail(bool minimise);

#ifdef USE_AKONADI
QStringList         dontShowErrors(const EventId&);
bool                dontShowErrors(const EventId&, const QString& tag);
void                setDontShowErrors(const EventId&, const QStringList& tags = QStringList());
void                setDontShowErrors(const EventId&, const QString& tag);
#else
QStringList         dontShowErrors(const QString& eventId);
bool                dontShowErrors(const QString& eventId, const QString& tag);
void                setDontShowErrors(const QString& eventId, const QStringList& tags = QStringList());
#endif
void                setDontShowErrors(const QString& eventId, const QString& tag);

enum         // 'options' parameter values for addEvent(). May be OR'ed together.
{
    USE_EVENT_ID       = 0x01,   // use event ID if it's provided
    NO_RESOURCE_PROMPT = 0x02,   // don't prompt for resource
    ALLOW_KORG_UPDATE  = 0x04    // allow change to be sent to KOrganizer
};
#ifdef USE_AKONADI
UpdateResult        addEvent(KAEvent&, Akonadi::Collection* = 0, QWidget* msgParent = 0, int options = ALLOW_KORG_UPDATE, bool showKOrgErr = true);
#else
UpdateResult        addEvent(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0, int options = ALLOW_KORG_UPDATE, bool showKOrgErr = true);
#endif
UpdateResult        addEvents(QVector<KAEvent>&, QWidget* msgParent = 0, bool allowKOrgUpdate = true, bool showKOrgErr = true);
#ifdef USE_AKONADI
bool                addArchivedEvent(KAEvent&, Akonadi::Collection* = 0);
UpdateResult        addTemplate(KAEvent&, Akonadi::Collection* = 0, QWidget* msgParent = 0);
#else
bool                addArchivedEvent(KAEvent&, AlarmResource* = 0);
UpdateResult        addTemplate(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0);
#endif
UpdateResult        modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        updateEvent(KAEvent&, QWidget* msgParent = 0, bool archiveOnDelete = true);
UpdateResult        updateTemplate(KAEvent&, QWidget* msgParent = 0);
UpdateResult        deleteEvent(KAEvent&, bool archive = true, QWidget* msgParent = 0, bool showKOrgErr = true);
#ifdef USE_AKONADI
UpdateResult        deleteEvents(QVector<KAEvent>&, bool archive = true, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        deleteTemplates(const KAEvent::List& events, QWidget* msgParent = 0);
inline UpdateResult deleteTemplate(KAEvent& event, QWidget* msgParent = 0)
                        { KAEvent::List e;  e += &event;  return deleteTemplates(e, msgParent); }
#else
UpdateResult        deleteEvents(KAEvent::List&, bool archive = true, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        deleteTemplates(const QStringList& eventIDs, QWidget* msgParent = 0);
inline UpdateResult deleteTemplate(const QString& eventID, QWidget* msgParent = 0)
                        { return deleteTemplates(QStringList(eventID), msgParent); }
#endif
void                deleteDisplayEvent(const QString& eventID);
#ifdef USE_AKONADI
UpdateResult        reactivateEvent(KAEvent&, Akonadi::Collection* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        reactivateEvents(QVector<KAEvent>&, QVector<EventId>& ineligibleIDs, Akonadi::Collection* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        enableEvents(QVector<KAEvent>&, bool enable, QWidget* msgParent = 0);
QVector<KAEvent>    getSortedActiveEvents(QObject* parent, AlarmListModel** model = 0);
#else
UpdateResult        reactivateEvent(KAEvent&, AlarmResource* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        reactivateEvents(KAEvent::List&, QStringList& ineligibleIDs, AlarmResource* = 0, QWidget* msgParent = 0, bool showKOrgErr = true);
UpdateResult        enableEvents(KAEvent::List&, bool enable, QWidget* msgParent = 0);
KAEvent::List       getSortedActiveEvents(const KDateTime& startTime = KDateTime(), const KDateTime& endTime = KDateTime());
#endif
void                purgeArchive(int purgeDays);    // must only be called from KAlarmApp::processQueue()
void                displayKOrgUpdateError(QWidget* parent, UpdateError, UpdateResult korgError, int nAlarms = 0);
QStringList         checkRtcWakeConfig(bool checkEventExists = false);
void                deleteRtcWakeConfig();
void                cancelRtcWake(QWidget* msgParent, const QString& eventId = QString());
bool                setRtcWakeTime(unsigned triggerTime, QWidget* parent);

/** Return a prompt string to ask the user whether to convert the calendar to the
 *  current format.
 *  @param whole if true, the whole calendar needs to be converted; else only some
 *               alarms may need to be converted.
 */
QString             conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole);

#ifdef USE_AKONADI
Akonadi::Collection invalidCollection();  // for use as a non-const default parameter
#endif

#ifndef NDEBUG
void                setTestModeConditions();
void                setSimulatedSystemTime(const KDateTime&);
#endif

} // namespace KAlarm

bool caseInsensitiveLessThan(const QString& s1, const QString& s2);

#endif // FUNCTIONS_H

// vim: et sw=4:
