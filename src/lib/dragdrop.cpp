/*
 *  dragdrop.cpp  -  drag and drop functions
 *  Program:  kalarm
 *  Copyright © 2003-2020 David Jarvie <djarvie@kde.org>
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

#include "dragdrop.h"

#include "kalarm_debug.h"

#include <KAlarmCal/AlarmText>

#include <AkonadiCore/ItemFetchJob>
#include <AkonadiCore/ItemFetchScope>
#include <KMime/Message>

#include <QMimeData>
#include <QUrlQuery>


namespace
{
KAlarmCal::AlarmText kMimeEmailToAlarmText(KMime::Content&, Akonadi::Item::Id);
QString getMailHeader(const char* header, KMime::Content&);
}


namespace DragDrop
{

/******************************************************************************
* Get plain text from a drag-and-drop object.
*/
bool dropPlainText(const QMimeData* data, QString& text)
{
    static const QString TextPlain     = QStringLiteral("text/plain");
    static const QString TextPlainUtf8 = QStringLiteral("text/plain;charset=utf-8");

    if (data->hasFormat(TextPlainUtf8))
        text = QString::fromUtf8(data->data(TextPlainUtf8));
    else if (data->hasFormat(TextPlain))
        text = QString::fromLocal8Bit(data->data(TextPlain));
    else
    {
        text.clear();
        return false;
    }
    return true;
}

/******************************************************************************
* Check whether drag-and-drop data may contain an RFC822 message (Akonadi or not).
*/
bool mayHaveRFC822(const QMimeData* data)
{
    return data->hasFormat(QStringLiteral("message/rfc822"))
       ||  data->hasUrls();
}

/******************************************************************************
* Extract dragged and dropped RFC822 message data.
*/
bool dropRFC822(const QMimeData* data, KAlarmCal::AlarmText& alarmText)
{
    const QByteArray bytes = data->data(QStringLiteral("message/rfc822"));
    if (bytes.isEmpty())
    {
        alarmText.clear();
        return false;
    }

    // Email message(s). Ignore all but the first.
    qCDebug(KALARM_LOG) << "KAlarm::dropRFC822: have email";
    KMime::Content content;
    content.setContent(bytes);
    content.parse();
    alarmText = kMimeEmailToAlarmText(content, -1);
    return true;
}

/******************************************************************************
* Extract dragged and dropped Akonadi RFC822 message data.
*/
bool dropAkonadiEmail(const QMimeData* data, QUrl& url, Akonadi::Item& item, KAlarmCal::AlarmText& alarmText)
{
    alarmText.clear();
    const QList<QUrl> urls = data->urls();
    if (urls.isEmpty())
    {
        url = QUrl();
        item.setId(-1);
        return false;
    }
    url  = urls.at(0);
    item = Akonadi::Item::fromUrl(url);
    if (!item.isValid())
        return false;

    // It's an Akonadi item
    qCDebug(KALARM_LOG) << "KAlarm::dropAkonadiEmail: Akonadi item" << item.id();
    if (QUrlQuery(url).queryItemValue(QStringLiteral("type")) != QLatin1String("message/rfc822"))
        return false;

    // It's an email held in Akonadi
    qCDebug(KALARM_LOG) << "KAlarm::dropAkonadiEmail: Akonadi email";
    Akonadi::ItemFetchJob* job = new Akonadi::ItemFetchJob(item);
    job->fetchScope().fetchFullPayload();
    Akonadi::Item::List items;
    if (job->exec())
        items = job->items();
    if (items.isEmpty())
        qCWarning(KALARM_LOG) << "KAlarm::dropAkonadiEmail: Akonadi item" << item.id() << "not found";
    else
    {
        const Akonadi::Item& it = items.at(0);
        if (!it.isValid()  ||  !it.hasPayload<KMime::Message::Ptr>())
            qCWarning(KALARM_LOG) << "KAlarm::dropAkonadiEmail: invalid email";
        else
        {
            KMime::Message::Ptr message = it.payload<KMime::Message::Ptr>();
            alarmText = kMimeEmailToAlarmText(*message, it.id());
        }
    }
    return true;
}

} // namespace DragDrop

namespace
{

/******************************************************************************
* Convert a KMime email instance to AlarmText.
*/
KAlarmCal::AlarmText kMimeEmailToAlarmText(KMime::Content& content, Akonadi::Item::Id itemId)
{
    QString body;
    if (content.textContent())
        body = content.textContent()->decodedText(true, true);    // strip trailing newlines & spaces
    KAlarmCal::AlarmText alarmText;
    alarmText.setEmail(getMailHeader("To", content),
                       getMailHeader("From", content),
                       getMailHeader("Cc", content),
                       getMailHeader("Date", content),
                       getMailHeader("Subject", content),
                       body,
                       itemId);
    return alarmText;
}

QString getMailHeader(const char* header, KMime::Content& content)
{
    KMime::Headers::Base* hd = content.headerByType(header);
    return hd ? hd->asUnicodeString() : QString();
}

}

// vim: et sw=4:
