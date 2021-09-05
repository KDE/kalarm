/*
 *  functions.h  -  miscellaneous functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**  @file functions.h - miscellaneous functions */

#include "editdlg.h"
#include "eventid.h"

#include <KAlarmCal/KAEvent>

#include <QString>
#include <QVector>

using namespace KAlarmCal;

namespace KCal { class Event; }
class QWidget;
class QAction;
class KToggleAction;
class Resource;
class MainWindow;
class AlarmListModel;

namespace KAlarm
{

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
    QVector<int>  failed;   // indexes to events whose update failed

    UpdateResult() : status(UPDATE_OK) {}
    explicit UpdateResult(UpdateStatus s, const QString& m = QString()) : status(s), message(m) {}
    UpdateResult& operator=(UpdateStatus s)  { status = s; message.clear(); return *this; }
    bool operator==(UpdateStatus s) const  { return status == s; }
    bool operator!=(UpdateStatus s) const  { return status != s; }
    bool operator>(UpdateStatus s) const   { return status > s; }
    bool operator>=(UpdateStatus s) const  { return status >= s; }
    bool operator<=(UpdateStatus s) const  { return status <= s; }
    bool operator<(UpdateStatus s) const   { return status < s; }
    void set(UpdateStatus s) { operator=(s); }
    void set(UpdateStatus s, const QString& m) { status = s; message = m; }
};

QString i18n_act_StopPlay();      // text of Stop Play action

/** Display a main window with the specified event selected */
MainWindow*         displayMainWindowSelected(const QString& eventId);

bool                editNewAlarm(const QString& templateName, QWidget* parent = nullptr);
void                editNewAlarm(EditAlarmDlg::Type, QWidget* parent = nullptr);
void                editNewAlarm(EditAlarmDlg::Type, QDate startDate, QWidget* parent = nullptr);
void                editNewAlarm(KAEvent::SubAction, QWidget* parent = nullptr, const AlarmText* = nullptr);
void                editNewAlarm(const KAEvent& preset, QWidget* parent = nullptr);
void                editNewAlarm(const KAEvent& preset, const QDate& startDate, QWidget* parent = nullptr);
void                editAlarm(KAEvent&, QWidget* parent = nullptr);
bool                editAlarmById(const EventId& eventID, QWidget* parent = nullptr);
void                updateEditedAlarm(EditAlarmDlg*, KAEvent&, Resource&);
void                viewAlarm(const KAEvent&, QWidget* parent = nullptr);
void                editNewTemplate(EditAlarmDlg::Type, QWidget* parent = nullptr);
void                editNewTemplate(const KAEvent& preset, QWidget* parent = nullptr);
void                editTemplate(KAEvent&, QWidget* parent = nullptr);
void                execNewAlarmDlg(EditAlarmDlg*, QDate startDate = QDate());
/** Create a "New From Template" QAction */
KToggleAction*      createAlarmEnableAction(QObject* parent);
QAction*            createStopPlayAction(QObject* parent);
KToggleAction*      createSpreadWindowsAction(QObject* parent);
/** Returns a list of all alarm templates.
 *  If shell commands are disabled, command alarm templates are omitted.
 */
QVector<KAEvent>    templateList();
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

/** Add a new active (non-archived) alarm to a resource.
 *  @param event      Updated with the actual event ID.
 *  @param resource   Resource to add event to. If invalid, the default resource
 *                    is used or the user is prompted, depending on policy, and
 *                    'resource' is updated with the actual resource used.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the new alarm has been discarded.
 */
UpdateResult addEvent(KAEvent& event, Resource& resource, QWidget* msgParent = nullptr, int options = ALLOW_KORG_UPDATE, bool showKOrgErr = true);

/** Add new active (non-archived) alarms to a resource.
 *  @param events     Updated with the actual event IDs.
 *  @param resource   Resource to add event to. If invalid, the default resource
 *                    is used or the user is prompted, depending on policy, and
 *                    'resource' is updated with the actual resource used.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, all new alarms have been discarded.
 */
UpdateResult addEvents(QVector<KAEvent>& events, Resource& resource, QWidget* msgParent = nullptr, bool allowKOrgUpdate = true, bool showKOrgErr = true);

/** Save the event in the archived calendar.
 *  The event's ID is changed to an archived ID if necessary.
 *  @param event      Updated with the archived event ID.
 *  @param resource   Resource to add event to. If invalid, the default resource
 *                    is used or the user is prompted, depending on policy, and
 *                    'resource' is updated with the actual resource used.
 */
bool addArchivedEvent(KAEvent& event, Resource& resource);

/** Add a new template to a resource.
 *  @param event      Updated with the actual event ID.
 *  @param resource   Resource to add event to. If invalid, the default resource
 *                    is used or the user is prompted, depending on policy, and
 *                    'resource' is updated with the actual resource used.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the new template has been discarded.
 */
UpdateResult addTemplate(KAEvent& event, Resource& resource, QWidget* msgParent = nullptr);

/** Modify an active (non-archived) alarm in a resource.
 *  The new event must have a different event ID from the old one.
 *  @param oldEvent   Event to be replaced. Its resourceId() must give the ID of
 *                    the resource which contains it.
 *  @param newEvent   Modified version of the event. Updated with its new ID if
 *                    it was not supplied with one.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the modification has been discarded.
 */
UpdateResult modifyEvent(KAEvent& oldEvent, KAEvent& newEvent, QWidget* msgParent = nullptr, bool showKOrgErr = true);

/** Update an active (non-archived) alarm.
 *  The new event will have the same event ID as the old one.
 *  The event is not updated in KOrganizer, since this function is called when an
 *  existing alarm is rescheduled (due to recurrence or deferral).
 *  @param event           Event to be replaced. Its resourceId() must give the
 *                         ID of the resource which contains it.
 *  @param msgParent       Parent widget for any calendar selection prompt or
 *                         error message.
 *  @param saveIfReadOnly  If the resource is read-only, whether to try to save
 *                         the resource after updating the event. (Writable
 *                         resources will always be saved.)
 *  @return  Success status; if >= UPDATE_FAILED, the update has been discarded.
 */
UpdateResult updateEvent(KAEvent& event, QWidget* msgParent = nullptr, bool archiveOnDelete = true,
                         bool saveIfReadOnly = true);

/** Update an alarm template.
 *  The new event will have the same event ID as the old one.
 *  @param event      Event to be replaced. Its resourceId() must give the ID of
 *                    the resource which contains it.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the update has been discarded.
 */
UpdateResult updateTemplate(KAEvent& event, QWidget* msgParent = nullptr);

/** Delete an alarm from a resource.
 *  If the event is archived, the event's ID is changed to an archived ID if necessary.
 *  @param event      Event to delete.
 *  @param resource   Resource to delete event from. If invalid, this is
 *                    updated to the resource which contained the event.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the deletion has been discarded.
 */
UpdateResult deleteEvent(KAEvent& event, Resource& resource, bool archive = true, QWidget* msgParent = nullptr, bool showKOrgErr = true);

/** Delete alarms from the resources.
 *  If the events are archived, the events' IDs are changed to archived IDs if necessary.
 *  @param event      Event to delete.
 *  @param resource   Resource to delete event from. If invalid, and all events
 *                    are found in the same resource, this is updated to the
 *                    resource which contained the events.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, all deletions have been discarded.
 */
UpdateResult deleteEvents(QVector<KAEvent>&, Resource& resource, bool archive = true, QWidget* msgParent = nullptr, bool showKOrgErr = true);

/** Delete templates from the resources.
 *  @param event      Event to delete.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, all deletions have been discarded.
 */
UpdateResult deleteTemplates(const KAEvent::List& events, QWidget* msgParent = nullptr);

inline UpdateResult deleteTemplate(KAEvent& event, QWidget* msgParent = nullptr)
                        { KAEvent::List e;  e += &event;  return deleteTemplates(e, msgParent); }
void                deleteDisplayEvent(const QString& eventID);

/** Undelete an archived alarm.
 *  The archive bit is set to ensure that it gets re-archived if deleted again.
 *  @param event      Updated with the restored event.
 *  @param resource   Active alarms resource to restore the event to. If
 *                    invalid, the default resource is used or the user is
 *                    prompted, depending on policy, and 'resource' is updated
 *                    with the actual resource used.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, the reactivated event has been discarded.
 */
UpdateResult reactivateEvent(KAEvent& event, Resource& resource, QWidget* msgParent = nullptr, bool showKOrgErr = true);

/** Undelete archived alarms.
 *  The archive bit is set to ensure that they get re-archived if deleted again.
 *  @param events     Updated to contain the restored events.
 *  @param ineligibleIndexes  Receives the indexes to any ineligible events.
 *  @param resource   Active alarms resource to restore the events to. If
 *                    invalid, the default resource is used or the user is
 *                    prompted, depending on policy, and 'resource' is updated
 *                    with the actual resource used.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if >= UPDATE_FAILED, all reactivated events have been discarded.
 */
UpdateResult reactivateEvents(QVector<KAEvent>& events, QVector<int>& ineligibleIndexes, Resource& resource, QWidget* msgParent = nullptr, bool showKOrgErr = true);

/** Enable or disable alarms.
 *  The new events will have the same event IDs as the old ones.
 *  @param events     Events to be enabled. Each one's resourceId() must give
 *                    the ID of the resource which contains it.
 *  @param enable     Whether to enable or disable the events.
 *  @param msgParent  Parent widget for any calendar selection prompt or error
 *                    message.
 *  @return  Success status; if == UPDATE_FAILED, the enabled status of all
 *           events is unchanged; if == SAVE_FAILED, the enabled status of at
 *           least one event has been successfully changed, but will be lost
 *           when its resource is reloaded.
 */
UpdateResult enableEvents(QVector<KAEvent>& events, bool enable, QWidget* msgParent = nullptr);

/** Return whether an event is read-only.
 *  This depends on whether the event or its resource is read-only.
 */
bool eventReadOnly(const QString& eventId);

QVector<KAEvent>    getSortedActiveEvents(QObject* parent, AlarmListModel** model = nullptr);
void                purgeArchive(int purgeDays);    // must only be called from KAlarmApp::processQueue()

/** Prompt the user for an external calendar file to import alarms from,
 *  and merge them into a resource. If the resource is invalid, the events
 *  will be merged into the default resource for each alarm type (obtained
 *  by calling destination(type)).
 *  The alarms are given new unique event IDs.
 *  @param parent    Parent widget for error message boxes
 *  @param resource  Resource to import into
 *  @return  true if all alarms in the calendar were successfully imported;
 *           false if any alarms failed to be imported.
 */
bool importAlarms(Resource& resource, QWidget* parent);

/** Prompt the user for an external calendar file, and export a list of
 *  alarms to it. If an existing file is chosen, the user has the choice
 *  whether to append or overwrite.
 *  The alarms are given new unique event IDs.
 *  @param events  Events to export
 *  @param parent  Parent widget for error message boxes
 *  @return  true if all alarms in the calendar were successfully exported;
 *           false if any alarms failed to be exported.
 */
bool exportAlarms(const QVector<KAEvent>& events, QWidget* parent);

void                displayKOrgUpdateError(QWidget* parent, UpdateError, const UpdateResult& korgError, int nAlarms = 0);
QStringList         checkRtcWakeConfig(bool checkEventExists = false);
void                deleteRtcWakeConfig();
void                cancelRtcWake(QWidget* msgParent, const QString& eventId = QString());
bool                setRtcWakeTime(unsigned triggerTime, QWidget* parent);

bool                convertTimeString(const QByteArray& timeString, KADateTime& dateTime,
                                  const KADateTime& defaultDt = KADateTime(), bool allowTZ = true);
KADateTime          applyTimeZone(const QString& tzstring, QDate date, QTime time,
                                   bool haveTime, const KADateTime& defaultDt = KADateTime());

#ifndef NDEBUG
void                setTestModeConditions();
void                setSimulatedSystemTime(const KADateTime&);
#endif

} // namespace KAlarm

bool caseInsensitiveLessThan(const QString& s1, const QString& s2);


// vim: et sw=4:
