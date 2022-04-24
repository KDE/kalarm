/*
 *  pluginbase.h  -  base class for plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"
#include "kalarmpluginlib_export.h"

#include <KMime/Message>

#include <QObject>

class QUrl;
class QColor;
namespace KMime { class Content; }
namespace KCalendarCore { class Person; }
namespace KIdentityManagement { class Identity; }
namespace KAlarmCal { class KAEvent; }
namespace MailSend { struct JobData; }
class QSortFilterProxyModel;

class KALARMPLUGINLIB_EXPORT PluginBase : public QObject
{
    Q_OBJECT
public:
    explicit PluginBase(QObject* parent = nullptr, const QList<QVariant>& = {});
    ~PluginBase() override;

    QString name() const  { return mName; }

    /** Create birthday model instances. */
    virtual QSortFilterProxyModel* createBirthdayModels(QWidget* messageParent, QObject* parent = nullptr) = 0;

    /** Set a new prefix and suffix, and the corresponding selection list. */
    virtual void setPrefixSuffix(QSortFilterProxyModel* birthdaySortModel, const QString& prefix, const QString& suffix, const QStringList& alarmMessageList) = 0;

    enum class BirthdayModelValue { NameColumn, DateColumn, DateRole };
    /** Return BirthdayModel enum values. */
    virtual int birthdayModelEnum(BirthdayModelValue) const = 0;

    /** Send an email using PIM libraries.
     *  @return  empty string if sending initiated successfully, else error message.
     */
    virtual QString sendMail(KMime::Message::Ptr message, const KIdentityManagement::Identity& identity,
                             const QString& normalizedFrom, bool keepSentMail, MailSend::JobData& jobdata) = 0;

    /** Extract dragged and dropped Akonadi RFC822 message data. */
    virtual KMime::Message::Ptr fetchAkonadiEmail(const QUrl&, qint64& emailId) = 0;

    /** Get a single selection from the address book. */
    virtual bool getAddressBookSelection(KCalendarCore::Person&, QWidget* parent = nullptr) = 0;

    /** Get the Akonadi Collection ID which contains a given email ID. */
    virtual qint64 getCollectionId(qint64 emailId) = 0;

    /** Delete a KOrganizer event. */
    virtual void deleteEvent(const QString& mimeType, const QString& gid = {}, const QString& uid = {}) = 0;

    /** Initiate Akonadi resource migration. */
    virtual void initiateAkonadiResourceMigration() = 0;

    /** Delete a named Akonadi resource.
     *  This should be called after the resource has been migrated.
     */
    virtual void deleteAkonadiResource(const QString& resourceName) = 0;

Q_SIGNALS:
    /** Emitted when the birthday contacts model's data has changed. */
    void birthdayModelDataChanged();

    /** Emitted when an error has occurred sending an email. */
    void emailSent(const MailSend::JobData&, const QStringList& errmsgs, bool sendError);

    /** Emitted when an error has occurred sending an email. */
    void emailQueued(const KAlarmCal::KAEvent&);

    /** Emitted when Akonadi resource migration has completed.
     *  @param migrated  true if Akonadi migration was required.
     */
    void akonadiMigrationComplete(bool migrated);

    /** Emitted when a file resource needs to be migrated. */
    void migrateFileResource(const QString& resourceName, const QUrl& location, KAlarmCal::CalEvent::Types alarmTypes,
                             const QString& displayName, const QColor& backgroundColour,
                             KAlarmCal::CalEvent::Types enabledTypes, KAlarmCal::CalEvent::Types standardTypes,
                             bool readOnly);

    /** Emitted when a directory resource needs to be migrated. */
    void migrateDirResource(const QString& resourceName, const QString& path, KAlarmCal::CalEvent::Types alarmTypes,
                            const QString& displayName, const QColor& backgroundColour,
                            KAlarmCal::CalEvent::Types enabledTypes, KAlarmCal::CalEvent::Types standardTypes,
                            bool readOnly);


protected:
    void setName(const QString& pluginName)  { mName = pluginName; }

private:
    QString mName;
};

// vim: et sw=4:
