/*
 *  akonadiplugin.h  -  plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "pluginbase.h"

class SendAkonadiMail;

class AkonadiPlugin : public PluginBase
{
    Q_OBJECT
public:
    explicit AkonadiPlugin(QObject* parent = nullptr, const QList<QVariant>& = {});

    /** Start Akonadi and create birthday model instances.
     *  It returns the BirthdaySortModel as a QSortFilterProxyModel, since
     *  BirthdaySortModel is private to this plugin but is inherited from
     *  QSortFilterProxyModel.
     */
    QSortFilterProxyModel* createBirthdayModels(QWidget* messageParent, QObject* parent = nullptr) override;

    /** Set a new prefix and suffix, and the corresponding selection list. */
    void setPrefixSuffix(QSortFilterProxyModel* birthdaySortModel, const QString& prefix, const QString& suffix, const QStringList& alarmMessageList) override;

    /** Return BirthdayModel enum values. */
    int birthdayModelEnum(BirthdayModelValue) const override;

    /** Send an email using PIM libraries.
     *  @return  empty string if sending initiated successfully, else error message.
     */
    QString sendMail(KMime::Message::Ptr message, const KIdentityManagement::Identity& identity,
                     const QString normalizedFrom, bool keepSentMail, MailSend::JobData& jobdata) override;

    /** Extract dragged and dropped Akonadi RFC822 message data.
     *  @param url      the dropped URL.
     *  @param emailId  updated with the Akonadi email ID.
     *  @return the email message if an Akonadi email has been extracted, else null.
     */
    KMime::Message::Ptr fetchAkonadiEmail(const QUrl& url, qint64& emailId) override;

    /** Get a single selection from the address book. */
    bool getAddressBookSelection(KCalendarCore::Person& person, QWidget* parent = nullptr) override;

    /** Get the Akonadi Collection ID which contains a given email ID. */
    qint64 getCollectionId(qint64 emailId) override;

    /** Delete a KOrganizer event. */
    void deleteEvent(const QString& mimeType, const QString& gid = {}, const QString& uid = {}) override;

    /** Initiate Akonadi resource migration. */
    void initiateAkonadiResourceMigration() override;

    /** Delete a named Akonadi resource.
     *  This should be called after the resource has been migrated.
     */
    void deleteAkonadiResource(const QString& resourceName) override;

private:
    SendAkonadiMail* mSendAkonadiMail {nullptr};
};

// vim: et sw=4:
