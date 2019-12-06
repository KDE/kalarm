/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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
#include "eventid.h"

#include <KAlarmCal/KAEvent>
#include <KFile>

#include <QSize>
#include <QString>
#include <QVector>
#include <QMimeType>
#include <QUrl>

using namespace KAlarmCal;

namespace KCal { class Event; }
class QWidget;
class QAction;
class QAction;
class KToggleAction;
class Resource;
class MainWindow;
class AlarmListModel;

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

/** Desktop identity, obtained from XDG_CURRENT_DESKTOP. */
enum class Desktop
{
    Kde,      //!< KDE (KDE 4 and Plasma both identify as "KDE")
    Unity,    //!< Unity
    Other
};

/** Display a main window with the specified event selected */
MainWindow*         displayMainWindowSelected(const QString& eventId);
bool                readConfigWindowSize(const char* window, QSize&, int* splitterWidth = nullptr);
void                writeConfigWindowSize(const char* window, const QSize&, int splitterWidth = -1);
/** Check from its mime type whether a file appears to be a text or image file.
 *  If a text file, its type is distinguished.
 */
FileType            fileType(const QMimeType& mimetype);
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
FileErr             checkFileExists(QString& filename, QUrl&);
bool                showFileErrMessage(const QString& filename, FileErr, FileErr blankError, QWidget* errmsgParent);

/** If a url string is a local file, strip off the 'file:/' prefix. */
QString             pathOrUrl(const QString& url);

bool                browseFile(QString& file, const QString& caption, QString& defaultDir,
                               const QString& initialFile = QString(),
                               const QString& filter = QString(), bool existing = false, QWidget* parent = nullptr);
bool                editNewAlarm(const QString& templateName, QWidget* parent = nullptr);
void                editNewAlarm(EditAlarmDlg::Type, QWidget* parent = nullptr);
void                editNewAlarm(KAEvent::SubAction, QWidget* parent = nullptr, const AlarmText* = nullptr);
void                editNewAlarm(const KAEvent* preset, QWidget* parent = nullptr);
void                editAlarm(KAEvent*, QWidget* parent = nullptr);
bool                editAlarmById(const EventId& eventID, QWidget* parent = nullptr);
void                updateEditedAlarm(EditAlarmDlg*, KAEvent&, Resource&);
void                viewAlarm(const KAEvent*, QWidget* parent = nullptr);
void                editNewTemplate(EditAlarmDlg::Type, QWidget* parent = nullptr);
void                editNewTemplate(const KAEvent* preset, QWidget* parent = nullptr);
void                editTemplate(KAEvent*, QWidget* parent = nullptr);
void                execNewAlarmDlg(EditAlarmDlg*);
/** Create a "New From Template" QAction */
KToggleAction*      createAlarmEnableAction(QObject* parent);
QAction*            createStopPlayAction(QObject* parent);
KToggleAction*      createSpreadWindowsAction(QObject* parent);
/** Returns a list of all alarm templates.
 *  If shell commands are disabled, command alarm templates are omitted.
 */
KAEvent::List       templateList();
void                outputAlarmWarnings(QWidget* parent, const KAEvent* = nullptr);
void                refreshAlarms();
void                refreshAlarmsIfQueued();    // must only be called from KAlarmApp::processQueue()
QString             runKMail();

QStringList         dontShowErrors(const EventId&);
bool                dontShowErrors(const EventId&, const QString& tag);
void                setDontShowErrors(const EventId&, const QStringList& tags = QStringList());
void                setDontShowErrors(const EventId&, const QString& tag);
void                setDontShowErrors(const QString& eventId, const QString& tag);

enum         // 'options' parameter values for addEvent(). May be OR'ed together.
{
    USE_EVENT_ID       = 0x01,   // use event ID if it's provided
    NO_RESOURCE_PROMPT = 0x02,   // don't prompt for resource
    ALLOW_KORG_UPDATE  = 0x04    // allow change to be sent to KOrganizer
};
UpdateResult        addEvent(KAEvent&, Resource* = nullptr, QWidget* msgParent = nullptr, int options = ALLOW_KORG_UPDATE, bool showKOrgErr = true);
UpdateResult        addEvents(QVector<KAEvent>&, QWidget* msgParent = nullptr, bool allowKOrgUpdate = true, bool showKOrgErr = true);
bool                addArchivedEvent(KAEvent&, Resource* = nullptr);
UpdateResult        addTemplate(KAEvent&, Resource* = nullptr, QWidget* msgParent = nullptr);
UpdateResult        modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent = nullptr, bool showKOrgErr = true);
UpdateResult        updateEvent(KAEvent&, QWidget* msgParent = nullptr, bool archiveOnDelete = true);
UpdateResult        updateTemplate(KAEvent&, QWidget* msgParent = nullptr);
UpdateResult        deleteEvent(KAEvent&, bool archive = true, QWidget* msgParent = nullptr, bool showKOrgErr = true);
UpdateResult        deleteEvents(QVector<KAEvent>&, bool archive = true, QWidget* msgParent = nullptr, bool showKOrgErr = true);
UpdateResult        deleteTemplates(const KAEvent::List& events, QWidget* msgParent = nullptr);
inline UpdateResult deleteTemplate(KAEvent& event, QWidget* msgParent = nullptr)
                        { KAEvent::List e;  e += &event;  return deleteTemplates(e, msgParent); }
void                deleteDisplayEvent(const QString& eventID);
UpdateResult        reactivateEvent(KAEvent&, Resource* = nullptr, QWidget* msgParent = nullptr, bool showKOrgErr = true);
UpdateResult        reactivateEvents(QVector<KAEvent>&, QVector<EventId>& ineligibleIDs, Resource* = nullptr, QWidget* msgParent = nullptr, bool showKOrgErr = true);
UpdateResult        enableEvents(QVector<KAEvent>&, bool enable, QWidget* msgParent = nullptr);
QVector<KAEvent>    getSortedActiveEvents(QObject* parent, AlarmListModel** model = nullptr);
void                purgeArchive(int purgeDays);    // must only be called from KAlarmApp::processQueue()
void                displayKOrgUpdateError(QWidget* parent, UpdateError, const UpdateResult& korgError, int nAlarms = 0);
Desktop             currentDesktopIdentity();
QString             currentDesktopIdentityName();
QStringList         checkRtcWakeConfig(bool checkEventExists = false);
void                deleteRtcWakeConfig();
void                cancelRtcWake(QWidget* msgParent, const QString& eventId = QString());
bool                setRtcWakeTime(unsigned triggerTime, QWidget* parent);

/** Return a prompt string to ask the user whether to convert the calendar to the
 *  current format.
 *  @param calendarName    The calendar name
 *  @param calendarVersion The calendar version
 *  @param whole           If true, the whole calendar needs to be converted; else
 *                         only some alarms may need to be converted.
 */
QString             conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole);

#ifndef NDEBUG
void                setTestModeConditions();
void                setSimulatedSystemTime(const KADateTime&);
#endif

} // namespace KAlarm

bool caseInsensitiveLessThan(const QString& s1, const QString& s2);

#endif // FUNCTIONS_H

// vim: et sw=4:
