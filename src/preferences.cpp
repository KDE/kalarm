/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "preferences.h"

#include "kalarm.h"
#include "kamail.h"
#include "pluginmanager.h"
#include "lib/desktop.h"
#include "lib/messagebox.h"
#include "lib/shellprocess.h"
#include "kalarmcalendar/holidays.h"
#include "kalarmcalendar/identities.h"
#include "kalarm_debug.h"

#include <KIdentityManagementCore/Identity>
#include <KIdentityManagementCore/IdentityManager>

#include <KSharedConfig>
#include <KConfigGroup>
#include <KMessageBox>
#include <KWindowSystem>
#include <KShell>

#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QStandardPaths>
using namespace Qt::Literals::StringLiterals;

#include <time.h>

using namespace KAlarmCal;

//clazy:excludeall=non-pod-global-static

namespace
{

// Config file entry names
const QLatin1StringView GENERAL_GROUP("General");

// Config file entry name for temporary use
const char* TEMP = "Temp";

const QString AUTOSTART_FILE(QStringLiteral("kalarm.autostart.desktop"));

// Values for EmailFrom entry
const QString FROM_SYS_SETTINGS(QStringLiteral("@SystemSettings"));
const QString FROM_KMAIL(QStringLiteral("@KMail"));

// Command strings for executing commands in different types of terminal windows.
// %t = window title parameter
// %c = command to execute in terminal
// %w = command to execute in terminal, with 'sleep 86400' appended
// %C = temporary command file to execute in terminal
// %W = temporary command file to execute in terminal, with 'sleep 86400' appended
const QList<QString> xtermCommands {
    QStringLiteral("xterm -sb -hold -title %t -e %c"),
    QStringLiteral("konsole --noclose -p tabtitle=%t -e ${SHELL:-sh} -c %c"),
    QStringLiteral("gnome-terminal -t %t -e %W"),
    QStringLiteral("eterm --pause -T %t -e %C"),    // some systems use eterm...
    QStringLiteral("Eterm --pause -T %t -e %C"),    // while some use Eterm
    QStringLiteral("rxvt -title %t -e ${SHELL:-sh} -c %w"),
    QStringLiteral("xfce4-terminal -T %t -H -e %c")
};

QList<QString> xtermCommandExes;   // initialised to hold executables in xtermCommands

void splitXTermCommands();

} // namespace

// Config file entry names for notification messages
const QLatin1StringView Preferences::QUIT_WARN("QuitWarn");
const QLatin1StringView Preferences::CONFIRM_ALARM_DELETION("ConfirmAlarmDeletion");
const QLatin1StringView Preferences::EMAIL_QUEUED_NOTIFY("EmailQueuedNotify");
const bool  default_quitWarn             = true;
const bool  default_emailQueuedNotify    = false;
const bool  default_confirmAlarmDeletion = true;

static QString translateXTermPath(const QString& cmdline, bool write);


Preferences*         Preferences::mInstance = nullptr;
bool                 Preferences::mUsingDefaults = false;
Holidays*            Preferences::mHolidays = nullptr;   // always non-null after Preferences initialisation
QString              Preferences::mPreviousVersion;
Preferences::Backend Preferences::mPreviousBackend;


Preferences* Preferences::self()
{
    if (!mInstance)
    {
        // Set the default button for the Quit warning message box to Cancel
        KAMessageBox::setContinueDefault(QUIT_WARN, KMessageBox::Cancel);
        KAMessageBox::setDefaultShouldBeShownContinue(QUIT_WARN, default_quitWarn);
        KAMessageBox::setDefaultShouldBeShownContinue(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
        KAMessageBox::setDefaultShouldBeShownContinue(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);

        mInstance = new Preferences;

        // Ensure that AutoStart corresponds to RunMode
        const bool autostart = (runMode() == RunMode_Auto);
        if (mInstance->self()->base_AutoStart() != autostart)
            mInstance->self()->setBase_AutoStart(autostart);
    }
    return mInstance;
}

Preferences::Preferences()
{
    QObject::connect(this, &Preferences::base_StartOfDayChanged, this, &Preferences::startDayChange);
    QObject::connect(this, &Preferences::base_TimeZoneChanged, this, &Preferences::timeZoneChange);
    QObject::connect(this, &Preferences::base_HolidayRegionChanged, this, &Preferences::holidaysChange);
    QObject::connect(this, &Preferences::base_WorkTimeChanged, this, &Preferences::workTimeChange);

    load();
    // Fetch the KAlarm version and backend which wrote the previous config file
    mPreviousVersion = version();
    mPreviousBackend = backend();
    // Update the KAlarm version in the config file, but don't call
    // writeConfig() here - leave it to be written only if the config file
    // is updated with other data.
    setVersion(QStringLiteral(KALARM_VERSION));
}

/******************************************************************************
* Get whether the Akonadi plugin should be used, if available.
*/
bool Preferences::useAkonadi()
{
    return self()->mUseAkonadiIfAvailable  &&  PluginManager::instance()->akonadiPlugin();
}

/******************************************************************************
* Return the Akonadi plugin to use, or null if not being used or not available.
*/
AkonadiPlugin* Preferences::akonadiPlugin()
{
    if (!self()->mUseAkonadiIfAvailable)
        return nullptr;
    return PluginManager::instance()->akonadiPlugin();
}

void Preferences::setUseAkonadi(bool yes)
{
    if (PluginManager::instance()->akonadiPlugin())
        self()->setUseAkonadiIfAvailable(yes);
}

/******************************************************************************
* Return the Audio plugin to use, or null if none available.
* If the plugin for the current selection is unavailable, an available plugin
* is selected instead.
*/
AudioPlugin* Preferences::audioPlugin()
{
    AudioPlugin* pluginVlc = PluginManager::instance()->audioVlcPlugin();
    AudioPlugin* pluginMpv = PluginManager::instance()->audioMpvPlugin();
    if (!pluginVlc  &&  !pluginMpv)
        return nullptr;
    AudioBackend backend = base_AudioBackend();
    if (backend == Audio_Vlc  &&  !pluginVlc)
    {
        self()->setBase_AudioBackend(Audio_Mpv);
        self()->save();
    }
    if (backend == Audio_Mpv  &&  !pluginMpv)
    {
        self()->setBase_AudioBackend(Audio_Vlc);
        self()->save();
    }
    return (base_AudioBackend() == Audio_Vlc) ? pluginVlc : pluginMpv;
}

/******************************************************************************
* Set the Audio plugin to use.
*/
void Preferences::setAudioPlugin(AudioPlugin* plugin)
{
    if (plugin)
    {
        AudioBackend backend;
        if (plugin == PluginManager::instance()->audioVlcPlugin())
            backend = Audio_Vlc;
        else if (plugin == PluginManager::instance()->audioMpvPlugin())
            backend = Audio_Mpv;
        else
            return;
        self()->setBase_AudioBackend(backend);
    }
}

/******************************************************************************
* Auto hiding of the system tray icon is only allowed on desktops which provide
* GUI controls to show hidden icons.
*/
int Preferences::autoHideSystemTray()
{
    if (noAutoHideSystemTrayDesktops().contains(Desktop::currentIdentityName()))
        return 0;   // never hide
    return self()->mBase_AutoHideSystemTray;
}

/******************************************************************************
* Auto hiding of the system tray icon is only allowed on desktops which provide
* GUI controls to show hidden icons, so while KAlarm is running on such a
* desktop, don't allow changes to the setting.
*/
void Preferences::setAutoHideSystemTray(int timeout)
{
    if (noAutoHideSystemTrayDesktops().contains(Desktop::currentIdentityName()))
        return;
    self()->setBase_AutoHideSystemTray(timeout);
}

/******************************************************************************
* Set the RunMode.
* The AutoStart entry also needs to be set, because it controls autostart via
* the kalarm.autostart.desktop file.
* On KDE desktops, the "X-KDE-autostart-condition" entry in
* kalarm.autostart.desktop references the AutoStart config entry to determine
* whether to autostart KAlarm.
* The kalarm.autostart.desktop entry is
*   X-KDE-autostart-condition=kalarmrc:General:AutoStart:false
* where the parameters are:
*   kalarmrc  : the config file to use
*   General   : the section within the config file
*   AutoStart : the entry which specifies whether to autostart
*   false     : the default value to use, i.e. don't autostart KAlarm unless
*               there is an entry 'AutoStart=true' in kalarmrc [General]
* On non-KDE desktops, the "X-KDE-autostart-condition" entry in
* kalarm.autostart.desktop doesn't have any effect, so that KAlarm will be
* autostarted even if it is set not to autostart. Adding a "Hidden" entry to,
* and removing the "OnlyShowIn=KDE" entry from, a user-modifiable copy of the
* file fixes this.
*/
void Preferences::setRunMode(RunMode mode)
{
    // Find the existing kalarm.autostart.desktop file, and whether it's writable.
    bool existingRO = true;   // whether the existing file is read-only
    QString autostartFile;
    QString configDirRW;
    const QStringList autostartDirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString& dir : autostartDirs)
    {
        const QString file = dir + "/autostart/"_L1 + AUTOSTART_FILE;
        if (QFile::exists(file))
        {
            QFileInfo info(file);
            if (info.isReadable())
            {
                autostartFile = file;
                existingRO = !info.isWritable();
                if (!existingRO)
                    configDirRW = dir;
                break;
            }
        }
    }

    // If the existing file isn't writable, find the path to create a writable copy
    QString autostartFileRW = autostartFile;
    if (existingRO)
    {
        configDirRW = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        autostartFileRW = configDirRW + "/autostart/"_L1 + AUTOSTART_FILE;
        if (configDirRW.isEmpty())
        {
            qCWarning(KALARM_LOG) << "Preferences::setRunMode: No writable autostart file path";
            return;
        }
        if (QFile::exists(autostartFileRW))
        {
            QFileInfo info(autostartFileRW);
            if (!info.isReadable() || !info.isWritable())
            {
                qCWarning(KALARM_LOG) << "Preferences::setRunMode: Autostart file is not read/write:" << autostartFileRW;
                return;
            }
        }
    }

    // Read the existing file and remove any "Hidden=" and "OnlyShowIn=" entries
    bool update = false;
    QStringList lines;
    {
        QFile file(autostartFile);
        if (!file.open(QIODevice::ReadOnly))
        {
            qCWarning(KALARM_LOG) << "Preferences::setRunMode: Error reading autostart file:" << autostartFile;
            return;
        }
        QTextStream stream(&file);
        stream.setAutoDetectUnicode(true);
        lines = stream.readAll().split('\n'_L1);
        for (int i = 0; i < lines.size(); ++i)
        {
            const QString line = lines.at(i).trimmed();
            if (line.isEmpty())
            {
                lines.removeAt(i);
                --i;
            }
            else if (line.startsWith("Hidden="_L1)
                 ||  line.startsWith("OnlyShowIn="_L1))
            {
                lines.removeAt(i);
                update = true;
                --i;
            }
        }
    }

    if (mode == RunMode_Manual)
    {
        // Add a "Hidden" entry to the local kalarm.autostart.desktop file, to
        // prevent autostart from happening.
        lines += QStringLiteral("Hidden=true");
        update = true;
    }
    if (update)
    {
        // Write the updated file
        QFileInfo info(configDirRW + "/autostart"_L1);
        if (!info.exists())
        {
            // First, create the directory for it.
            if (!QDir(configDirRW).mkdir(QStringLiteral("autostart")))
            {
                qCWarning(KALARM_LOG) << "Preferences::setRunMode: Error creating autostart file directory:" << info.filePath();
                return;
            }
        }
        QSaveFile file(autostartFileRW);
        if (!file.open(QIODevice::WriteOnly))
        {
            qCWarning(KALARM_LOG) << "Preferences::setRunMode: Error writing autostart file:" << autostartFileRW;
            return;
        }
        QTextStream stream(&file);
        stream << lines.join('\n'_L1) << "\n";
        // QSaveFile doesn't report a write error when the device is full (see Qt
        // bug 75077), so check that the data can actually be written by flush().
        if (!file.flush()  ||  !file.commit())   // save the file
        {
            qCWarning(KALARM_LOG) << "Preferences::setRunMode: Error writing autostart file:" << autostartFileRW;
            return;
        }
        qCDebug(KALARM_LOG) << "Preferences::setRunMode: Written" << autostartFileRW;
    }

    self()->setBase_RunMode(mode);
    self()->setBase_AutoStart(mode == RunMode_Auto);
}

/******************************************************************************
* Get whether message windows should have a title bar and take keyboard focus.
*/
bool Preferences::modalMessages()
{
    return !KWindowSystem::isPlatformX11() || self()->base_ModalMessages();
}

/******************************************************************************
* Set whether message windows should have a title bar and take keyboard focus.
*/
void Preferences::setModalMessages(bool yes)
{
    if (KWindowSystem::isPlatformX11())
        self()->setBase_ModalMessages(yes);
}

/******************************************************************************
* Get the delay in seconds after a message window is displayed before its
* buttons are activated.
* Reply = 0 for no delay, but position windows as far from cursor as possible.
*/
int Preferences::messageButtonDelay()
{
    const int delay = self()->base_MessageButtonDelay();
    // On Wayland, window positions can't be set, so return a minimum delay of 1 second.
    return (delay <= 0  &&  KWindowSystem::isPlatformWayland()) ? 1 : delay;
}

/******************************************************************************
* Get the user's time zone, or if none has been chosen, the system time zone.
* Reply = time zone, or invalid to use the local time zone.
*/
KADateTime::Spec Preferences::timeSpec()
{
    const QByteArray zoneId = self()->mBase_TimeZone.toLatin1();
    return zoneId.isEmpty() ? KADateTime::LocalZone : KADateTime::Spec(QTimeZone(zoneId));
}

QTimeZone Preferences::timeSpecAsZone()
{
    const QByteArray zoneId = self()->mBase_TimeZone.toLatin1();
    return zoneId.isEmpty() ? QTimeZone::systemTimeZone() : QTimeZone(zoneId);
}

void Preferences::setTimeSpec(const KADateTime::Spec& spec)
{
    self()->setBase_TimeZone(spec.type() == KADateTime::TimeZone ? QString::fromLatin1(spec.namedTimeZone().id()) : QString());
}

void Preferences::timeZoneChange(const QString& zone)
{
    Q_UNUSED(zone);
    Q_EMIT mInstance->timeZoneChanged(timeSpec());
}

const Holidays& Preferences::holidays()
{
    const QString regionCode = self()->mBase_HolidayRegion;
    if (!mHolidays)
        mHolidays = new Holidays(regionCode);
    else if (mHolidays->regionCode() != regionCode)
        mHolidays->setRegion(regionCode);
    return *mHolidays;
}

void Preferences::setHolidayRegion(const QString& regionCode)
{
    self()->setBase_HolidayRegion(regionCode);
}

void Preferences::holidaysChange(const QString& regionCode)
{
    Q_UNUSED(regionCode);
    Q_EMIT mInstance->holidaysChanged(holidays());
}

void Preferences::setStartOfDay(const QTime& t)
{
    if (t != self()->mBase_StartOfDay.time())
    {
        self()->setBase_StartOfDay(QDateTime(QDate(1900,1,1), t));
        Q_EMIT mInstance->startOfDayChanged(t);
    }
}

// Called when the start of day value has changed in the config file
void Preferences::startDayChange(const QDateTime& dt)
{
    Q_EMIT mInstance->startOfDayChanged(dt.time());
}

QBitArray Preferences::workDays()
{
    unsigned days = self()->base_WorkDays();
    QBitArray dayBits(7);
    for (int i = 0;  i < 7;  ++i)
        dayBits.setBit(i, days & (1 << i));
    return dayBits;
}

void Preferences::setWorkDays(const QBitArray& dayBits)
{
    if (dayBits.size() != 7)
    {
        qCWarning(KALARM_LOG) << "Preferences::setWorkDays: Error! 'dayBits' parameter must have 7 elements: actual size" << dayBits.size();
        return;
    }
    unsigned days = 0;
    for (int i = 0;  i < 7;  ++i)
        if (dayBits.testBit(i))
            days |= 1 << i;
    self()->setBase_WorkDays(days);
}

void Preferences::workTimeChange(const QDateTime& start, const QDateTime& end, int days)
{
    QBitArray dayBits(7);
    for (int i = 0;  i < 7;  ++i)
        if (days & (1 << i))
            dayBits.setBit(i);
    Q_EMIT mInstance->workTimeChanged(start.time(), end.time(), dayBits);
}

Preferences::MailClient Preferences::emailClient()
{
    if (!useAkonadi())
        return sendmail;
    return static_cast<MailClient>(self()->mBase_EmailClient);
}

void Preferences::setEmailClient(MailClient client)
{
    if (!useAkonadi())
        client = sendmail;
    self()->setBase_EmailClient(client);
}

bool Preferences::emailCopyToKMail()
{
    if (!useAkonadi())
        return false;
    return self()->mBase_EmailCopyToKMail  &&  static_cast<MailClient>(self()->mBase_EmailClient) == sendmail;
}

void Preferences::setEmailCopyToKMail(bool yes)
{
    if (!useAkonadi())
        yes = false;
    self()->setBase_EmailCopyToKMail(yes);
}

Preferences::MailFrom Preferences::emailFrom()
{
    const QString from = self()->mBase_EmailFrom;
    if (from == FROM_KMAIL)
        return MAIL_FROM_KMAIL;
    if (from == FROM_SYS_SETTINGS)
        return MAIL_FROM_SYS_SETTINGS;
    return MAIL_FROM_ADDR;
}

/******************************************************************************
* Get user's default 'From' email address.
*/
QString Preferences::emailAddress()
{
    const QString from = self()->mBase_EmailFrom;
    if (from == FROM_KMAIL)
        return Identities::identityManager()->defaultIdentity().fullEmailAddr();
    if (from == FROM_SYS_SETTINGS)
        return KAMail::controlCentreAddress();
    return from;
}

void Preferences::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
    QString out;
    switch (from)
    {
        case MAIL_FROM_KMAIL:        out = FROM_KMAIL; break;
        case MAIL_FROM_SYS_SETTINGS: out = FROM_SYS_SETTINGS; break;
        case MAIL_FROM_ADDR:         out = address; break;
        default:  return;
    }
    self()->setBase_EmailFrom(out);
}

Preferences::MailFrom Preferences::emailBccFrom()
{
    const QString from = self()->mBase_EmailBccAddress;
    if (from == FROM_SYS_SETTINGS)
        return MAIL_FROM_SYS_SETTINGS;
    return MAIL_FROM_ADDR;
}

QString Preferences::emailBccAddress()
{
    const QString from = self()->mBase_EmailBccAddress;
    if (from == FROM_SYS_SETTINGS)
        return KAMail::controlCentreAddress();
    return from;
}

bool Preferences::emailBccUseSystemSettings()
{
    return self()->mBase_EmailBccAddress == FROM_SYS_SETTINGS;
}

void Preferences::setEmailBccAddress(bool useSystemSettings, const QString& address)
{
    QString out;
    if (useSystemSettings)
        out = FROM_SYS_SETTINGS;
    else
        out = address;
    self()->setBase_EmailBccAddress(out);
}

QString Preferences::cmdXTermCommand()
{
    QString cmd = translateXTermPath(self()->mBase_CmdXTermCommand, false);
    if (cmd.isEmpty())
    {
        // No terminal command is specified. If there is only one standard
        // terminal executable installed, set it as the default.
        const auto cmds = cmdXTermStandardCommands();
        if (cmds.size() == 1)
        {
            cmd = cmds.constBegin().value();
            self()->setBase_CmdXTermCommand(translateXTermPath(cmd, true));
            self()->save();
        }
    }
    return cmd;
}

std::pair<int, QString> Preferences::cmdXTermCommandIndex()
{
    const QString xtermCmd = cmdXTermCommand();
    if (xtermCmd.isEmpty())
        return std::make_pair(-1, QString());
    int id = 0;
    for (const QString& cmd : xtermCommands)
    {
        ++id;
        if (xtermCmd == cmd)
            return std::make_pair(id, xtermCmd);   // index is 1 greater than the index into xtermCommands
    }
    return std::make_pair(0, xtermCmd);
}

void Preferences::setCmdXTermSpecialCommand(const QString& cmd)
{
    self()->setBase_CmdXTermCommand(translateXTermPath(cmd, true));
}

void Preferences::setCmdXTermCommand(int index)
{
    --index;   // convert to an index into xtermCommands
    if (index >= 0  &&  index < xtermCommands.size())
        setCmdXTermSpecialCommand(xtermCommands[index]);
}

QString Preferences::cmdXTermStandardCommand(int index)
{
    --index;   // convert to an index into xtermCommands
    if (index >= 0  &&  index < xtermCommands.size())
    {
        splitXTermCommands();
        const QString exe = xtermCommandExes.at(index);
        if (!exe.isEmpty()  &&  !QStandardPaths::findExecutable(exe).isEmpty())
            return xtermCommands[index];
    }
    return {};
}

QHash<int, QString> Preferences::cmdXTermStandardCommands()
{
    QHash<int, QString> result;
    splitXTermCommands();
    for (int i = 0, count = xtermCommands.count();  i < count;  ++i)
    {
        const QString exe = xtermCommandExes.at(i);
        if (!exe.isEmpty()  &&  !QStandardPaths::findExecutable(exe).isEmpty())
            result[i + 1] = xtermCommands[i];   // index is 1 greater than the index into xtermCommands
    }
    return result;
}

namespace
{
void splitXTermCommands()
{
    if (xtermCommandExes.isEmpty())
    {
        for (const QString& cmd : xtermCommands)
        {
            const QStringList args = KShell::splitArgs(cmd);
            xtermCommandExes.append(args.isEmpty() ? QString() : args[0]);
        }
    }
}
}

Preferences::SoundType Preferences::defaultSoundType()
{
#ifdef HAVE_TEXT_TO_SPEECH_SUPPORT
    return self()->base_DefaultSoundType();
#else
    SoundType type = self()->base_DefaultSoundType();
    return (type == Sound_Speak) ? Sound_None : type;
#endif
}

void Preferences::setDefaultSoundType(SoundType type)
{
#ifndef HAVE_TEXT_TO_SPEECH_SUPPORT
    if (type == Sound_Speak)
        return;
#endif
    self()->setBase_DefaultSoundType(type);
}

void Preferences::connect(const char* signal, const QObject* receiver, const char* member)
{
    QObject::connect(self(), signal, receiver, member);
}

/******************************************************************************
* Called to allow or suppress output of the specified message dialog, where the
* dialog has a checkbox to turn notification off.
*/
void Preferences::setNotify(const QString& messageID, bool notify)
{
    KAMessageBox::saveDontShowAgainContinue(messageID, !notify);
}

/******************************************************************************
* Return whether the specified message dialog is output, where the dialog has
* a checkbox to turn notification off.
* Reply = false if message has been suppressed (by preferences or by selecting
*               "don't ask again")
*       = true in all other cases.
*/
bool Preferences::notifying(const QString& messageID)
{
    return KAMessageBox::shouldBeShownContinue(messageID);
}

/******************************************************************************
* Translate an X terminal command path to/from config file format.
* Note that only a home directory specification at the start of the path is
* translated, so there's no need to worry about missing out some of the
* executable's path due to quotes etc.
* N.B. Calling KConfig::read/writePathEntry() on the entire command line
*      causes a crash on some systems, so it's necessary to extract the
*      executable path first before processing.
*/
QString translateXTermPath(const QString& cmdline, bool write)
{
    QString params = cmdline;
    QString cmd = ShellProcess::splitCommandLine(params);

    // Translate any home directory specification at the start of the
    // executable's path.
    KConfigGroup group(KSharedConfig::openConfig(), GENERAL_GROUP);
    if (write)
    {
        group.writePathEntry(TEMP, cmd);
        cmd = group.readEntry(TEMP, QString());
    }
    else
    {
        group.writeEntry(TEMP, cmd);
        cmd = group.readPathEntry(TEMP, QString());
    }
    group.deleteEntry(TEMP);
    return cmd + params;
}

#include "moc_preferences.cpp"

// vim: et sw=4:
