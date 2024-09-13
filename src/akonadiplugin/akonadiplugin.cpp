/*
 *  akonadiplugin.cpp  -  plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "akonadiplugin.h"

#include "akonadicollectionsearch.h"
#include "akonadiresourcemigrator.h"
#include "birthdaymodel.h"
#include "sendakonadimail.h"
#include "lib/autoqpointer.h"
#include "akonadiplugin_debug.h"

#include <Akonadi/ControlGui>
#include <Akonadi/EmailAddressSelectionDialog>
#include <Akonadi/EntityMimeTypeFilterModel>
#include <Akonadi/Item>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <KCalendarCore/Person>

#include <KPluginFactory>
#include <KLocalizedString>
#include <KDescendantsProxyModel>

#include <QUrlQuery>
using namespace Qt::Literals::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(AkonadiPlugin, "akonadiplugin.json")

AkonadiPlugin::AkonadiPlugin(QObject* parent, const QList<QVariant>& args)
    : PluginBase(parent, args)
{
    setName(QStringLiteral("Akonadi"));
}

/******************************************************************************
* Start Akonadi and create an instance of both birthday models.
*/
QSortFilterProxyModel* AkonadiPlugin::createBirthdayModels(QWidget* messageParent, QObject* parent)
{
    // Start Akonadi server as we need it for the birthday model to access contacts information
    Akonadi::ControlGui::widgetNeedsAkonadi(messageParent);

    BirthdayModel* model = BirthdayModel::instance();
    connect(model, &BirthdayModel::dataChanged, this, &AkonadiPlugin::birthdayModelDataChanged);

    auto descendantsModel = new KDescendantsProxyModel(parent);
    descendantsModel->setSourceModel(model);

    auto mimeTypeFilter = new Akonadi::EntityMimeTypeFilterModel(parent);
    mimeTypeFilter->setSourceModel(descendantsModel);
    mimeTypeFilter->addMimeTypeExclusionFilter(Akonadi::Collection::mimeType());
    mimeTypeFilter->setHeaderGroup(Akonadi::EntityTreeModel::ItemListHeaders);

    BirthdaySortModel* sortModel = new BirthdaySortModel(parent);
    sortModel->setSourceModel(mimeTypeFilter);
    sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    return sortModel;
}

void AkonadiPlugin::setPrefixSuffix(QSortFilterProxyModel* model, const QString& prefix, const QString& suffix, const QStringList& alarmMessageList)
{
    BirthdaySortModel* bmodel = qobject_cast<BirthdaySortModel*>(model);
    if (bmodel)
        bmodel->setPrefixSuffix(prefix, suffix, alarmMessageList);
}

int AkonadiPlugin::birthdayModelEnum(BirthdayModelValue value) const
{
    switch (value)
    {
        case BirthdayModelValue::NameColumn:  return BirthdayModel::NameColumn;
        case BirthdayModelValue::DateColumn:  return BirthdayModel::DateColumn;
        case BirthdayModelValue::DateRole:    return BirthdayModel::DateRole;
        default:  return -1;
    }
}

/******************************************************************************
* Send an email via Akonadi.
*/
QString AkonadiPlugin::sendMail(KMime::Message::Ptr message, const KIdentityManagementCore::Identity& identity,
                                const QString& normalizedFrom, bool keepSentMail, MailSend::JobData& jobdata)
{
    if (!mSendAkonadiMail)
    {
        mSendAkonadiMail = SendAkonadiMail::instance();
        connect(mSendAkonadiMail, &SendAkonadiMail::sent, this, &AkonadiPlugin::emailSent);
        connect(mSendAkonadiMail, &SendAkonadiMail::queued, this, &AkonadiPlugin::emailQueued);
    }
    return mSendAkonadiMail->send(message, identity, normalizedFrom, keepSentMail, jobdata);
}

/******************************************************************************
* Extract dragged and dropped Akonadi RFC822 message data.
*/
KMime::Message::Ptr AkonadiPlugin::fetchAkonadiEmail(const QUrl& url, qint64& emailId)
{
    static_assert(sizeof(Akonadi::Item::Id) == sizeof(emailId), "AkonadiPlugin::fetchAkonadiEmail: parameter is wrong type");

    emailId = -1;
    Akonadi::Item item = Akonadi::Item::fromUrl(url);
    if (!item.isValid())
        return {};

    // It's an Akonadi item
    qCDebug(AKONADIPLUGIN_LOG) << "AkonadiPlugin::fetchAkonadiEmail: Akonadi item" << item.id();
    if (QUrlQuery(url).queryItemValue(QStringLiteral("type")) != "message/rfc822"_L1)
        return {};   // it's not an email

    // It's an email held in Akonadi
    qCDebug(AKONADIPLUGIN_LOG) << "AkonadiPlugin::fetchAkonadiEmail: Akonadi email";
    auto job = new Akonadi::ItemFetchJob(item);
    job->fetchScope().fetchFullPayload();
    Akonadi::Item::List items;
    if (job->exec())
        items = job->items();
    if (items.isEmpty())
        qCWarning(AKONADIPLUGIN_LOG) << "AkonadiPlugin::fetchAkonadiEmail: Akonadi item" << item.id() << "not found";
    else
    {
        const Akonadi::Item& it = items.at(0);
        if (!it.isValid()  ||  !it.hasPayload<KMime::Message::Ptr>())
            qCWarning(AKONADIPLUGIN_LOG) << "AkonadiPlugin::fetchAkonadiEmail: invalid email";
        else
        {
            emailId = it.id();
            return it.payload<KMime::Message::Ptr>();
        }
    }
    return {};
}

bool AkonadiPlugin::getAddressBookSelection(KCalendarCore::Person& person, QWidget* parent)
{
    person = KCalendarCore::Person();

    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of MainWindow, and on return from this function).
    AutoQPointer<Akonadi::EmailAddressSelectionDialog> dlg = new Akonadi::EmailAddressSelectionDialog(parent);
    if (dlg->exec() != QDialog::Accepted)
        return false;

    Akonadi::EmailAddressSelection::List selections = dlg->selectedAddresses();
    if (selections.isEmpty())
        return false;
    person = KCalendarCore::Person(selections.first().name(), selections.first().email());
    return true;
}

qint64 AkonadiPlugin::getCollectionId(qint64 emailId)
{
    static_assert(sizeof(Akonadi::Item::Id) == sizeof(emailId), "AkonadiPlugin::getCollectionId: parameter is wrong type");
    static_assert(sizeof(Akonadi::Collection::Id) == sizeof(qint64), "AkonadiPlugin::getCollectionId: wrong return type");

    Akonadi::ItemFetchJob* job = new Akonadi::ItemFetchJob(Akonadi::Item(emailId));
    job->fetchScope().setAncestorRetrieval(Akonadi::ItemFetchScope::Parent);
    Akonadi::Item::List items;
    if (job->exec())
        items = job->items();
    if (items.isEmpty()  ||  !items.at(0).isValid())
        return -1;
    const Akonadi::Item& it = items.at(0);
    return it.parentCollection().id();
}

void AkonadiPlugin::deleteEvent(const QString& mimeType, const QString& gid, const QString& uid)
{
    new AkonadiCollectionSearch(mimeType, gid, uid, true);  // this auto-deletes when complete
}

void AkonadiPlugin::initiateAkonadiResourceMigration()
{
    AkonadiResourceMigrator* akonadiMigrator = AkonadiResourceMigrator::instance();
    if (akonadiMigrator)
    {
        connect(akonadiMigrator, &AkonadiResourceMigrator::migrationComplete, this, &AkonadiPlugin::akonadiMigrationComplete);
        connect(akonadiMigrator, &AkonadiResourceMigrator::fileResource, this, &AkonadiPlugin::migrateFileResource);
        connect(akonadiMigrator, &AkonadiResourceMigrator::dirResource, this, &AkonadiPlugin::migrateDirResource);
        akonadiMigrator->initiateMigration();
    }
}

void AkonadiPlugin::deleteAkonadiResource(const QString& resourceName)
{
    AkonadiResourceMigrator* akonadiMigrator = AkonadiResourceMigrator::instance();
    if (akonadiMigrator)
        akonadiMigrator->deleteAkonadiResource(resourceName);
}

#include "akonadiplugin.moc"
#include "moc_akonadiplugin.cpp"

// vim: et sw=4:
