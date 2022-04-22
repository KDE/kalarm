/*
 *  pluginbase.h  -  base class for plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"
#include "kalarmplugin_export.h"

#include <KMime/Message>

#include <QObject>

class QUrl;
class QColor;
namespace KMime { class Content; }
namespace KCalendarCore { class Person; }
class QSortFilterProxyModel;

class KALARMPLUGIN_EXPORT PluginBase : public QObject
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

    /** Extract dragged and dropped Akonadi RFC822 message data. */
    virtual KMime::Message::Ptr fetchAkonadiEmail(const QUrl&, qint64& emailId) = 0;

    /** Get a single selection from the address book. */
    virtual bool getAddressBookSelection(KCalendarCore::Person&, QWidget* parent = nullptr) = 0;

    /** Get the Akonadi Collection ID which contains a given email ID. */
    virtual qint64 getCollectionId(qint64 emailId) = 0;

    /** Delete a KOrganizer event. */
    virtual void deleteEvent(const QString& mimeType, const QString& gid = {}, const QString& uid = {}) = 0;

Q_SIGNALS:
    /** Emitted when the birthday contacts model's data has changed. */
    void birthdayModelDataChanged();

protected:
    void setName(const QString& pluginName)  { mName = pluginName; }

private:
    QString mName;
};

// vim: et sw=4:
