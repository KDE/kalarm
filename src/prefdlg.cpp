/*
 *  prefdlg.cpp  -  program preferences dialog
 *  Program:  kalarm
 *  Copyright © 2001-2016 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include "alarmcalendar.h"
#include "collectionmodel.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "colourbutton.h"
#include "editdlg.h"
#include "editdlgtypes.h"
#include "fontcolour.h"
#include "functions.h"
#include "itembox.h"
#include "kalarmapp.h"
#include "kalocale.h"
#include "kamail.h"
#include "label.h"
#include "latecancel.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#include "sounddlg.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "stackedwidgets.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "timezonecombo.h"
#include "traywindow.h"
#include "prefdlg_p.h"
#include "prefdlg.h"

#include "config-kalarm.h"

#include <kalarmcal/identities.h>

#include <KHolidays/HolidayRegion>
using namespace KHolidays;

#include <kpimtextedit/texttospeech.h>

#include <kglobal.h>
#include <KLocalizedString>
#include <kshell.h>
#include <klineedit.h>
#include <KAboutData>
#include <kstandardguiitem.h>
#include <ksystemtimezone.h>
#include <qicon.h>
#if KDEPIM_HAVE_X11
#include <kwindowinfo.h>
#include <kwindowsystem.h>
#endif
#include <ktoolinvocation.h>
#include <KHelpClient>

#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QTabWidget>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QComboBox>
#include <QLocale>
#include "kalarm_debug.h"

using namespace KCalCore;
using namespace KAlarmCal;

static const char PREF_DIALOG_NAME[] = "PrefDialog";

// Command strings for executing commands in different types of terminal windows.
// %t = window title parameter
// %c = command to execute in terminal
// %w = command to execute in terminal, with 'sleep 86400' appended
// %C = temporary command file to execute in terminal
// %W = temporary command file to execute in terminal, with 'sleep 86400' appended
static QString xtermCommands[] = {
    QStringLiteral("xterm -sb -hold -title %t -e %c"),
    QStringLiteral("konsole --noclose -p tabtitle=%t -e ${SHELL:-sh} -c %c"),
    QStringLiteral("gnome-terminal -t %t -e %W"),
    QStringLiteral("eterm --pause -T %t -e %C"),    // some systems use eterm...
    QStringLiteral("Eterm --pause -T %t -e %C"),    // while some use Eterm
    QStringLiteral("rxvt -title %t -e ${SHELL:-sh} -c %w"),
    QString()       // end of list indicator - don't change!
};


/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg* KAlarmPrefDlg::mInstance = Q_NULLPTR;

void KAlarmPrefDlg::display()
{
    if (!mInstance)
    {
        mInstance = new KAlarmPrefDlg;
        QSize s;
        if (KAlarm::readConfigWindowSize(PREF_DIALOG_NAME, s))
            mInstance->resize(s);
        mInstance->show();
    }
    else
    {
#if KDEPIM_HAVE_X11
        KWindowInfo info = KWindowInfo(mInstance->winId(), NET::WMGeometry | NET::WMDesktop);
        KWindowSystem::setCurrentDesktop(info.desktop());
#endif
        mInstance->setWindowState(mInstance->windowState() & ~Qt::WindowMinimized); // un-minimize it if necessary
        mInstance->raise();
        mInstance->activateWindow();
    }
}

KAlarmPrefDlg::KAlarmPrefDlg()
    : KPageDialog(),
      mShown(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("PrefDlg"));    // used by LikeBack
    setWindowTitle(i18nc("@title:window", "Configure"));
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Apply);
    button(QDialogButtonBox::Ok)->setDefault(true); 
    setFaceType(List);
    mTabScrollGroup = new StackedScrollGroup(this, this);

    mMiscPage = new MiscPrefTab(mTabScrollGroup);
    mMiscPageItem = new KPageWidgetItem(mMiscPage, i18nc("@title:tab General preferences", "General"));
    mMiscPageItem->setHeader(i18nc("@title General preferences", "General"));
    mMiscPageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-other")));
    addPage(mMiscPageItem);

    mTimePage = new TimePrefTab(mTabScrollGroup);
    mTimePageItem = new KPageWidgetItem(mTimePage, i18nc("@title:tab", "Time & Date"));
    mTimePageItem->setHeader(i18nc("@title", "Time and Date"));
    mTimePageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-time")));
    addPage(mTimePageItem);

    mStorePage = new StorePrefTab(mTabScrollGroup);
    mStorePageItem = new KPageWidgetItem(mStorePage, i18nc("@title:tab", "Storage"));
    mStorePageItem->setHeader(i18nc("@title", "Alarm Storage"));
    mStorePageItem->setIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));
    addPage(mStorePageItem);

    mEmailPage = new EmailPrefTab(mTabScrollGroup);
    mEmailPageItem = new KPageWidgetItem(mEmailPage, i18nc("@title:tab Email preferences", "Email"));
    mEmailPageItem->setHeader(i18nc("@title", "Email Alarm Settings"));
    mEmailPageItem->setIcon(QIcon::fromTheme(QStringLiteral("internet-mail")));
    addPage(mEmailPageItem);

    mViewPage = new ViewPrefTab(mTabScrollGroup);
    mViewPageItem = new KPageWidgetItem(mViewPage, i18nc("@title:tab", "View"));
    mViewPageItem->setHeader(i18nc("@title", "View Settings"));
    mViewPageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
    addPage(mViewPageItem);

    mEditPage = new EditPrefTab(mTabScrollGroup);
    mEditPageItem = new KPageWidgetItem(mEditPage, i18nc("@title:tab", "Edit"));
    mEditPageItem->setHeader(i18nc("@title", "Default Alarm Edit Settings"));
    mEditPageItem->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    addPage(mEditPageItem);
    connect(button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotOk);
    connect(button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotCancel);
    connect(button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotApply);
    connect(button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotDefault);
    connect(button(QDialogButtonBox::Help), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotHelp);
    restore(false);
    adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
    mInstance = Q_NULLPTR;
}

void KAlarmPrefDlg::slotHelp()
{
    KHelpClient::invokeHelp(QStringLiteral("preferences"));
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
    qCDebug(KALARM_LOG);
    QString errmsg = mEmailPage->validate();
    if (!errmsg.isEmpty())
    {
        setCurrentPage(mEmailPageItem);
        if (KAMessageBox::warningYesNo(this, errmsg) != KMessageBox::Yes)
        {
            mValid = false;
            return;
        }
    }
    errmsg = mEditPage->validate();
    if (!errmsg.isEmpty())
    {
        setCurrentPage(mEditPageItem);
        KAMessageBox::sorry(this, errmsg);
        mValid = false;
        return;
    }
    mValid = true;
    mEmailPage->apply(false);
    mViewPage->apply(false);
    mEditPage->apply(false);
    mStorePage->apply(false);
    mTimePage->apply(false);
    mMiscPage->apply(false);
    Preferences::self()->save();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
    qCDebug(KALARM_LOG);
    mValid = true;
    slotApply();
    if (mValid)
        QDialog::accept();
}

// Discard the current preferences and close the dialog
void KAlarmPrefDlg::slotCancel()
{
    qCDebug(KALARM_LOG);
    restore(false);
    KPageDialog::reject();
}

// Reset all controls to the application defaults
void KAlarmPrefDlg::slotDefault()
{
    switch (KAMessageBox::questionYesNoCancel(this, i18nc("@info", "Reset all tabs to their default values, or only reset the current tab?"),
                                              QString(),
                                            KGuiItem(i18nc("@action:button Reset ALL tabs", "&All")),
                         KGuiItem(i18nc("@action:button Reset the CURRENT tab", "C&urrent"))))
    {
        case KMessageBox::Yes:
            restore(true);   // restore all tabs
            break;
        case KMessageBox::No:
            Preferences::self()->useDefaults(true);
            static_cast<PrefsTabBase*>(currentPage()->widget())->restore(true, false);
            Preferences::self()->useDefaults(false);
            break;
        default:
            break;
    }
}

// Discard the current preferences and use the present ones
void KAlarmPrefDlg::restore(bool defaults)
{
    qCDebug(KALARM_LOG) << (defaults ? "defaults" : "");
    if (defaults)
        Preferences::self()->useDefaults(true);
    mEmailPage->restore(defaults, true);
    mViewPage->restore(defaults, true);
    mEditPage->restore(defaults, true);
    mStorePage->restore(defaults, true);
    mTimePage->restore(defaults, true);
    mMiscPage->restore(defaults, true);
    if (defaults)
        Preferences::self()->useDefaults(false);
}

/******************************************************************************
* Return the minimum size for the dialog.
* If the minimum size would be too high to fit the desktop, the tab contents
* are made scrollable.
*/
QSize KAlarmPrefDlg::minimumSizeHint() const
{
    if (!mTabScrollGroup->sized())
    {
        QSize s = mTabScrollGroup->adjustSize();
        if (s.isValid())
        {
            if (mTabScrollGroup->heightReduction())
            {
                s = QSize(s.width(), s.height() - mTabScrollGroup->heightReduction());
                const_cast<KAlarmPrefDlg*>(this)->resize(s);
            }
            return s;
        }
    }
    return KPageDialog::minimumSizeHint();
}

void KAlarmPrefDlg::showEvent(QShowEvent* e)
{
    KPageDialog::showEvent(e);
    if (!mShown)
    {
        mTabScrollGroup->adjustSize(true);
        mShown = true;
    }
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void KAlarmPrefDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(PREF_DIALOG_NAME, re->size());
    KPageDialog::resizeEvent(re);
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/
int PrefsTabBase::mIndentWidth = 0;

PrefsTabBase::PrefsTabBase(StackedScrollGroup* scrollGroup)
    : StackedScrollWidget(scrollGroup),
      mLabelsAligned(false)
{
    QFrame *topWidget = new QFrame(this);
    setWidget(topWidget);
    mTopLayout = new QVBoxLayout(topWidget);
    mTopLayout->setMargin(0);
    mTopLayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    if (!mIndentWidth)
    {
        QRadioButton radio(this);
        QStyleOptionButton opt;
        opt.initFrom(&radio);
        mIndentWidth = style()->subElementRect(QStyle::SE_RadioButtonIndicator, &opt).width();
    }
}

void PrefsTabBase::apply(bool syncToDisc)
{
    if (syncToDisc)
        Preferences::self()->save();
}

void PrefsTabBase::addAlignedLabel(QLabel* label)
{
    mLabels += label;
}

void PrefsTabBase::showEvent(QShowEvent*)
{
    if (!mLabelsAligned)
    {
        int wid = 0;
        int i;
        int end = mLabels.count();
        QList<int> xpos;
        for (i = 0;  i < end;  ++i)
        {
            int x = mLabels[i]->mapTo(this, QPoint(0, 0)).x();
            xpos += x;
            int w = x + mLabels[i]->sizeHint().width();
            if (w > wid)
                wid = w;
        }
        for (i = 0;  i < end;  ++i)
        {
            mLabels[i]->setFixedWidth(wid - xpos[i]);
            mLabels[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        mLabelsAligned = true;
    }
}


/*=============================================================================
= Class MiscPrefTab
=============================================================================*/

MiscPrefTab::MiscPrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Run Mode"));
    topLayout()->addWidget(group);
    QVBoxLayout* vlayout = new QVBoxLayout(group);
    vlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    vlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    // Start at login
    mAutoStart = new QCheckBox(i18nc("@option:check", "Start at login"), group);
    connect(mAutoStart, &QAbstractButton::clicked, this, &MiscPrefTab::slotAutostartClicked);
    mAutoStart->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Automatically start <application>KAlarm</application> whenever you start KDE.</para>"
          "<para>This option should always be checked unless you intend to discontinue use of <application>KAlarm</application>.</para>"));
    vlayout->addWidget(mAutoStart, 0, Qt::AlignLeft);

    mQuitWarn = new QCheckBox(i18nc("@option:check", "Warn before quitting"), group);
    mQuitWarn->setWhatsThis(xi18nc("@info:whatsthis", "Check to display a warning prompt before quitting <application>KAlarm</application>."));
    vlayout->addWidget(mQuitWarn, 0, Qt::AlignLeft);

    // Confirm alarm deletion?
    group = new QGroupBox(i18nc("@title:group", "Deletion"));
    topLayout()->addWidget(group);   // this is to allow left adjustment
    QVBoxLayout *box = new QVBoxLayout(group);
    mConfirmAlarmDeletion = new QCheckBox(i18nc("@option:check", "Confirm alarm deletions"));
    mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
    mConfirmAlarmDeletion->setWhatsThis(i18nc("@info:whatsthis", "Check to be prompted for confirmation each time you delete an alarm."));
    box->addWidget(mConfirmAlarmDeletion);

    // Default alarm deferral time
    QWidget *widget = new QWidget;
    box->addWidget(widget);
    QHBoxLayout *hbox = new QHBoxLayout(widget);
    hbox->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    QLabel* label = new QLabel(i18nc("@label:spinbox", "Default defer time interval:"));
    hbox->addWidget(label);
    mDefaultDeferTime = new TimeSpinBox(1, 5999);
    mDefaultDeferTime->setMinimumSize(mDefaultDeferTime->sizeHint());
    hbox->addWidget(mDefaultDeferTime);
    widget->setWhatsThis(i18nc("@info:whatsthis",
            "Enter the default time interval (hours & minutes) to defer alarms, used by the Defer Alarm dialog."));
    label->setBuddy(mDefaultDeferTime);
    hbox->addStretch(1);    // left adjust the controls

    // Terminal window to use for command alarms
    group = new QGroupBox(i18nc("@title:group", "Terminal for Command Alarms"));
    group->setWhatsThis(i18nc("@info:whatsthis", "Choose which application to use when a command alarm is executed in a terminal window"));
    topLayout()->addWidget(group);
    QGridLayout* grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    int row = 0;

    mXtermType = new ButtonGroup(group);
    int index = 0;
    mXtermFirst = -1;
    for (mXtermCount = 0;  !xtermCommands[mXtermCount].isNull();  ++mXtermCount)
    {
        QString cmd = xtermCommands[mXtermCount];
        QStringList args = KShell::splitArgs(cmd);
        if (args.isEmpty()  ||  QStandardPaths::findExecutable(args[0]).isEmpty())
            continue;
        QRadioButton* radio = new QRadioButton(args[0], group);
        radio->setMinimumSize(radio->sizeHint());
        mXtermType->addButton(radio, mXtermCount);
        if (mXtermFirst < 0)
            mXtermFirst = mXtermCount;   // note the id of the first button
        cmd.replace(QStringLiteral("%t"), KAboutData::applicationData().displayName());
        cmd.replace(QStringLiteral("%c"), QStringLiteral("<command>"));
        cmd.replace(QStringLiteral("%w"), QStringLiteral("<command; sleep>"));
        cmd.replace(QStringLiteral("%C"), QStringLiteral("[command]"));
        cmd.replace(QStringLiteral("%W"), QStringLiteral("[command; sleep]"));
        radio->setWhatsThis(
                xi18nc("@info:whatsthis", "Check to execute command alarms in a terminal window by <icode>%1</icode>", cmd));
        grid->addWidget(radio, (row = index/3), index % 3, Qt::AlignLeft);
        ++index;
    }

    // QHBox used here doesn't allow the QLineEdit to expand!?
    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->addLayout(hlayout, row + 1, 0, 1, 3, Qt::AlignLeft);
    QRadioButton* radio = new QRadioButton(i18nc("@option:radio Other terminal window command", "Other:"), group);
    hlayout->addWidget(radio);
    connect(radio, &QAbstractButton::toggled, this, &MiscPrefTab::slotOtherTerminalToggled);
    mXtermType->addButton(radio, mXtermCount);
    if (mXtermFirst < 0)
        mXtermFirst = mXtermCount;   // note the id of the first button
    mXtermCommand = new QLineEdit(group);
    mXtermCommand->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    hlayout->addWidget(mXtermCommand);
    QString wt = 
          xi18nc("@info:whatsthis", "Enter the full command line needed to execute a command in your chosen terminal window. "
               "By default the alarm's command string will be appended to what you enter here. "
               "See the <application>KAlarm</application> Handbook for details of special codes to tailor the command line.");
    radio->setWhatsThis(wt);
    mXtermCommand->setWhatsThis(wt);

    topLayout()->addStretch();    // top adjust the widgets
}

void MiscPrefTab::restore(bool defaults, bool)
{
    mAutoStart->setChecked(defaults ? true : Preferences::autoStart());
    mQuitWarn->setChecked(Preferences::quitWarn());
    mConfirmAlarmDeletion->setChecked(Preferences::confirmAlarmDeletion());
    mDefaultDeferTime->setValue(Preferences::defaultDeferTime());
    QString xtermCmd = Preferences::cmdXTermCommand();
    int id = mXtermFirst;
    if (!xtermCmd.isEmpty())
    {
        for ( ;  id < mXtermCount;  ++id)
        {
            if (mXtermType->find(id)  &&  xtermCmd == xtermCommands[id])
                break;
        }
    }
    mXtermType->setButton(id);
    mXtermCommand->setEnabled(id == mXtermCount);
    mXtermCommand->setText(id == mXtermCount ? xtermCmd : QString());
}

void MiscPrefTab::apply(bool syncToDisc)
{
    // First validate anything entered in Other X-terminal command
    int xtermID = mXtermType->selectedId();
    if (xtermID >= mXtermCount)
    {
        QString cmd = mXtermCommand->text();
        if (cmd.isEmpty())
            xtermID = -1;       // 'Other' is only acceptable if it's non-blank
        else
        {
            QStringList args = KShell::splitArgs(cmd);
            cmd = args.isEmpty() ? QString() : args[0];
            if (QStandardPaths::findExecutable(cmd).isEmpty())
            {
                mXtermCommand->setFocus();
                if (KAMessageBox::warningContinueCancel(topLayout()->parentWidget(), xi18nc("@info", "Command to invoke terminal window not found: <command>%1</command>", cmd))
                                != KMessageBox::Continue)
                    return;
            }
        }
    }
    if (xtermID < 0)
    {
        xtermID = mXtermFirst;
        mXtermType->setButton(mXtermFirst);
    }

    if (mQuitWarn->isEnabled())
    {
        bool b = mQuitWarn->isChecked();
        if (b != Preferences::quitWarn())
            Preferences::setQuitWarn(b);
    }
    bool b = mAutoStart->isChecked();
    if (b != Preferences::autoStart())
    {
        Preferences::setAutoStart(b);
        Preferences::setAskAutoStart(true);  // cancel any start-at-login prompt suppression
        if (b)
            Preferences::setNoAutoStart(false);
        Preferences::setAutoStartChangedByUser(true);  // prevent prompting the user on quit, about start-at-login
    }
    b = mConfirmAlarmDeletion->isChecked();
    if (b != Preferences::confirmAlarmDeletion())
        Preferences::setConfirmAlarmDeletion(b);
    int i = mDefaultDeferTime->value();
    if (i != Preferences::defaultDeferTime())
        Preferences::setDefaultDeferTime(i);
    QString text = (xtermID < mXtermCount) ? xtermCommands[xtermID] : mXtermCommand->text();
    if (text != Preferences::cmdXTermCommand())
        Preferences::setCmdXTermCommand(text);
    PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::slotAutostartClicked()
{
    if (!mAutoStart->isChecked()
    &&  KAMessageBox::warningYesNo(topLayout()->parentWidget(),
                                   xi18nc("@info", "You should not uncheck this option unless you intend to discontinue use of <application>KAlarm</application>"),
                                   QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
                                  ) != KMessageBox::Yes)
        mAutoStart->setChecked(true);
}

void MiscPrefTab::slotOtherTerminalToggled(bool on)
{
    mXtermCommand->setEnabled(on);
}


/*=============================================================================
= Class TimePrefTab
=============================================================================*/

TimePrefTab::TimePrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    // Default time zone
    ItemBox* itemBox = new ItemBox();
    itemBox->setMargin(0);
    qobject_cast<QBoxLayout*>(topLayout())->addLayout(itemBox);

    QWidget *widget = new QWidget; // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    QHBoxLayout *box =  new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"));
    box->addWidget(label);
    addAlignedLabel(label);
#if 1
    mTimeZone = new TimeZoneCombo(widget);
    mTimeZone->setMaxVisibleItems(15);
    box->addWidget(mTimeZone);
#else
    mTimeZone = new KComboBox(box);
    mTimeZone->setMaxVisibleItems(15);
    const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
    for (KTimeZones::ZoneMap::ConstIterator it = zones.constBegin();  it != zones.constEnd();  ++it)
        mTimeZone->addItem(it.key());
#endif
    widget->setWhatsThis(xi18nc("@info:whatsthis",
                                "Select the time zone which <application>KAlarm</application> should use "
                                "as its default for displaying and entering dates and times."));
    label->setBuddy(mTimeZone);
    itemBox->leftAlign();

    // Holiday region
    itemBox = new ItemBox();
    itemBox->setMargin(0);
    qobject_cast<QBoxLayout*>(topLayout())->addLayout(itemBox);

    widget = new QWidget;    // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:listbox", "Holiday region:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mHolidays = new QComboBox();
    mHolidays->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    box->addWidget(mHolidays);
    itemBox->leftAlign();
    label->setBuddy(mHolidays);
    widget->setWhatsThis(i18nc("@info:whatsthis",
                               "Select which holiday region to use"));

    QStringList regions = HolidayRegion::regionCodes();
    QMap<QString, QString> regionsMap;
    foreach (const QString& regionCode, regions)
    {
        QString name = HolidayRegion::name(regionCode);
        QString languageName = KLocale::global()->languageCodeToName(HolidayRegion::languageCode(regionCode));
        QString label = languageName.isEmpty() ? name : i18nc("Holiday region, region language", "%1 (%2)", name, languageName);
        regionsMap.insert(label, regionCode);
    }

    mHolidays->addItem(i18nc("No holiday region", "None"), QString());
    for (QMapIterator<QString, QString> it(regionsMap);  it.hasNext();  )
    {
        it.next();
        mHolidays->addItem(it.key(), it.value());
    }

    // Start-of-day time
    itemBox = new ItemBox();
    itemBox->setMargin(0);
    qobject_cast<QBoxLayout*>(topLayout())->addLayout(itemBox);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:spinbox", "Start of day for date-only alarms:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mStartOfDay = new TimeEdit();
    box->addWidget(mStartOfDay);
    label->setBuddy(mStartOfDay);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>The earliest time of day at which a date-only alarm will be triggered.</para>"
          "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->leftAlign();

    // Working hours
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Working Hours"));
    topLayout()->addWidget(group);
    QBoxLayout* layout = new QVBoxLayout(group);
    layout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    layout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    QWidget* daybox = new QWidget(group);   // this is to control the QWhatsThis text display area
    layout->addWidget(daybox);
    QGridLayout* wgrid = new QGridLayout(daybox);
    wgrid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    const QLocale locale;
    for (int i = 0;  i < 7;  ++i)
    {
        int day = KAlarm::localeDayInWeek_to_weekDay(i);
        mWorkDays[i] = new QCheckBox(locale.dayName(day), daybox);
        wgrid->addWidget(mWorkDays[i], i/3, i%3, Qt::AlignLeft);
    }
    daybox->setWhatsThis(i18nc("@info:whatsthis", "Check the days in the week which are work days"));

    itemBox = new ItemBox(group);
    itemBox->setMargin(0);
    layout->addLayout(itemBox);

    widget = new QWidget;  // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:spinbox", "Daily start time:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mWorkStart = new TimeEdit();
    box->addWidget(mWorkStart);
    label->setBuddy(mWorkStart);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
            "<para>Enter the start time of the working day.</para>"
            "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->leftAlign();

    itemBox = new ItemBox(group);
    itemBox->setMargin(0);
    layout->addLayout(itemBox);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:spinbox", "Daily end time:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mWorkEnd = new TimeEdit();
    box->addWidget(mWorkEnd);
    label->setBuddy(mWorkEnd);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Enter the end time of the working day.</para>"
          "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->leftAlign();

    // KOrganizer event duration
    group = new QGroupBox(i18nc("@title:group", "KOrganizer"));
    topLayout()->addWidget(group);
    layout = new QVBoxLayout(group);
    layout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    layout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    layout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:spinbox", "KOrganizer event duration:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mKOrgEventDuration = new TimeSpinBox(0, 5999);
    mKOrgEventDuration->setMinimumSize(mKOrgEventDuration->sizeHint());
    box->addWidget(mKOrgEventDuration);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
            "<para>Enter the event duration in hours and minutes, for alarms which are copied to KOrganizer.</para>"
            "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    label->setBuddy(mKOrgEventDuration);
    box->addStretch(1);    // left adjust the controls

    topLayout()->addStretch();    // top adjust the widgets
}

void TimePrefTab::restore(bool, bool)
{
#if 1
    mTimeZone->setTimeZone(Preferences::timeZone());
#else
    int tzindex = 0;
    KTimeZone tz = Preferences::timeZone();
    if (tz.isValid())
    {
        QString zone = tz.name();
        int count = mTimeZone->count();
        while (tzindex < count  &&  mTimeZone->itemText(tzindex) != zone)
            ++tzindex;
        if (tzindex >= count)
            tzindex = 0;
    }
    mTimeZone->setCurrentIndex(tzindex);
#endif
    int i = Preferences::holidays().isValid() ? mHolidays->findData(Preferences::holidays().regionCode()) : 0;
    mHolidays->setCurrentIndex(i);
    mStartOfDay->setValue(Preferences::startOfDay());
    mWorkStart->setValue(Preferences::workDayStart());
    mWorkEnd->setValue(Preferences::workDayEnd());
    QBitArray days = Preferences::workDays();
    for (int i = 0;  i < 7;  ++i)
    {
        bool x = days.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1);
        mWorkDays[i]->setChecked(x);
    }
    mKOrgEventDuration->setValue(Preferences::kOrgEventDuration());
}

void TimePrefTab::apply(bool syncToDisc)
{
#if 1
    KTimeZone tz = mTimeZone->timeZone();
    if (tz.isValid())
        Preferences::setTimeZone(tz);
#else
    KTimeZone tz = KSystemTimeZones::zone(mTimeZone->currentText());
    if (tz.isValid()  &&  tz != Preferences::timeZone())
        Preferences::setTimeZone(tz);
#endif
    QString hol = mHolidays->itemData(mHolidays->currentIndex()).toString();
    if (hol != Preferences::holidays().regionCode())
        Preferences::setHolidayRegion(hol);
    int t = mStartOfDay->value();
    QTime sodt(t/60, t%60, 0);
    if (sodt != Preferences::startOfDay())
        Preferences::setStartOfDay(sodt);
    t = mWorkStart->value();
    Preferences::setWorkDayStart(QTime(t/60, t%60, 0));
    t = mWorkEnd->value();
    Preferences::setWorkDayEnd(QTime(t/60, t%60, 0));
    QBitArray workDays(7);
    for (int i = 0;  i < 7;  ++i)
        if (mWorkDays[i]->isChecked())
            workDays.setBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1, 1);
    Preferences::setWorkDays(workDays);
    Preferences::setKOrgEventDuration(mKOrgEventDuration->value());
    t = mKOrgEventDuration->value();
    if (t != Preferences::kOrgEventDuration())
        Preferences::setKOrgEventDuration(t);
    PrefsTabBase::apply(syncToDisc);
}


/*=============================================================================
= Class StorePrefTab
=============================================================================*/

StorePrefTab::StorePrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup),
      mCheckKeepChanges(false)
{
    // Which resource to save to
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "New Alarms && Templates"));
    topLayout()->addWidget(group);
    QButtonGroup* bgroup = new QButtonGroup(group);
    QBoxLayout* layout = new QVBoxLayout(group);
    layout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    layout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    mDefaultResource = new QRadioButton(i18nc("@option:radio", "Store in default calendar"), group);
    bgroup->addButton(mDefaultResource);
    mDefaultResource->setWhatsThis(i18nc("@info:whatsthis", "Add all new alarms and alarm templates to the default calendars, without prompting."));
    layout->addWidget(mDefaultResource, 0, Qt::AlignLeft);
    mAskResource = new QRadioButton(i18nc("@option:radio", "Prompt for which calendar to store in"), group);
    bgroup->addButton(mAskResource);
    mAskResource->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>When saving a new alarm or alarm template, prompt for which calendar to store it in, if there is more than one active calendar.</para>"
          "<para>Note that archived alarms are always stored in the default archived alarm calendar.</para>"));
    layout->addWidget(mAskResource, 0, Qt::AlignLeft);

    // Archived alarms
    group = new QGroupBox(i18nc("@title:group", "Archived Alarms"));
    topLayout()->addWidget(group);
    QGridLayout* grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, indentWidth());
    mKeepArchived = new QCheckBox(i18nc("@option:check", "Keep alarms after expiry"), group);
    connect(mKeepArchived, &QAbstractButton::toggled, this, &StorePrefTab::slotArchivedToggled);
    mKeepArchived->setWhatsThis(
          i18nc("@info:whatsthis", "Check to archive alarms after expiry or deletion (except deleted alarms which were never triggered)."));
    grid->addWidget(mKeepArchived, 0, 0, 1, 2, Qt::AlignLeft);

    QWidget *widget = new QWidget;
    QHBoxLayout *box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mPurgeArchived = new QCheckBox(i18nc("@option:check", "Discard archived alarms after:"));
    mPurgeArchived->setMinimumSize(mPurgeArchived->sizeHint());
    box->addWidget(mPurgeArchived);
    connect(mPurgeArchived, &QAbstractButton::toggled, this, &StorePrefTab::slotArchivedToggled);
    mPurgeAfter = new SpinBox();
    mPurgeAfter->setMinimum(1);
    mPurgeAfter->setSingleShiftStep(10);
    mPurgeAfter->setMinimumSize(mPurgeAfter->sizeHint());
    box->addWidget(mPurgeAfter);
    mPurgeAfterLabel = new QLabel(i18nc("@label Time unit for user-entered number", "days"));
    mPurgeAfterLabel->setMinimumSize(mPurgeAfterLabel->sizeHint());
    mPurgeAfterLabel->setBuddy(mPurgeAfter);
    box->addWidget(mPurgeAfterLabel);
    widget->setWhatsThis(i18nc("@info:whatsthis", "Uncheck to store archived alarms indefinitely. Check to enter how long archived alarms should be stored."));
    grid->addWidget(widget, 1, 1, Qt::AlignLeft);

    mClearArchived = new QPushButton(i18nc("@action:button", "Clear Archived Alarms"), group);
    connect(mClearArchived, &QAbstractButton::clicked, this, &StorePrefTab::slotClearArchived);
    mClearArchived->setWhatsThis((CollectionControlModel::enabledCollections(CalEvent::ARCHIVED, false).count() <= 1)
            ? i18nc("@info:whatsthis", "Delete all existing archived alarms.")
            : i18nc("@info:whatsthis", "Delete all existing archived alarms (from the default archived alarm calendar only)."));
    grid->addWidget(mClearArchived, 2, 1, Qt::AlignLeft);

    topLayout()->addStretch();    // top adjust the widgets
}

void StorePrefTab::restore(bool defaults, bool)
{
    mCheckKeepChanges = defaults;
    if (Preferences::askResource())
        mAskResource->setChecked(true);
    else
        mDefaultResource->setChecked(true);
    int keepDays = Preferences::archivedKeepDays();
    if (!defaults)
        mOldKeepArchived = keepDays;
    setArchivedControls(keepDays);
    mCheckKeepChanges = true;
}

void StorePrefTab::apply(bool syncToDisc)
{
    bool b = mAskResource->isChecked();
    if (b != Preferences::askResource())
        Preferences::setAskResource(mAskResource->isChecked());
    int days = !mKeepArchived->isChecked() ? 0 : mPurgeArchived->isChecked() ? mPurgeAfter->value() : -1;
    if (days != Preferences::archivedKeepDays())
        Preferences::setArchivedKeepDays(days);
    PrefsTabBase::apply(syncToDisc);
}

void StorePrefTab::setArchivedControls(int purgeDays)
{
    mKeepArchived->setChecked(purgeDays);
    mPurgeArchived->setChecked(purgeDays > 0);
    mPurgeAfter->setValue(purgeDays > 0 ? purgeDays : 0);
    slotArchivedToggled(true);
}

void StorePrefTab::slotArchivedToggled(bool)
{
    bool keep = mKeepArchived->isChecked();
    if (keep  &&  !mOldKeepArchived  &&  mCheckKeepChanges
    &&  !CollectionControlModel::getStandard(CalEvent::ARCHIVED).isValid())
    {
        KAMessageBox::sorry(topLayout()->parentWidget(),
             xi18nc("@info", "<para>A default calendar is required in order to archive alarms, but none is currently enabled.</para>"
                  "<para>If you wish to keep expired alarms, please first use the calendars view to select a default "
                  "archived alarms calendar.</para>"));
        mKeepArchived->setChecked(false);
        return;
    }
    mOldKeepArchived = keep;
    mPurgeArchived->setEnabled(keep);
    mPurgeAfter->setEnabled(keep && mPurgeArchived->isChecked());
    mPurgeAfterLabel->setEnabled(keep);
    mClearArchived->setEnabled(keep);
}

void StorePrefTab::slotClearArchived()
{
    bool single = CollectionControlModel::enabledCollections(CalEvent::ARCHIVED, false).count() <= 1;
    if (KAMessageBox::warningContinueCancel(topLayout()->parentWidget(),
                                            single ? i18nc("@info", "Do you really want to delete all archived alarms?")
                                                   : i18nc("@info", "Do you really want to delete all alarms in the default archived alarm calendar?"))
            != KMessageBox::Continue)
        return;
    theApp()->purgeAll();
}


/*=============================================================================
= Class EmailPrefTab
=============================================================================*/

EmailPrefTab::EmailPrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup),
      mAddressChanged(false),
      mBccAddressChanged(false)
{
    QWidget *widget = new QWidget;
    topLayout()->addWidget(widget);
    QHBoxLayout *box = new QHBoxLayout(widget);
    box->setSpacing(2 * style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    QLabel *label = new QLabel(i18nc("@label", "Email client:"));
    box->addWidget(label);
    mEmailClient = new ButtonGroup(widget);
    QString kmailOption = i18nc("@option:radio", "KMail");
    QString sendmailOption = i18nc("@option:radio", "Sendmail");
    mKMailButton = new RadioButton(kmailOption);
    mKMailButton->setMinimumSize(mKMailButton->sizeHint());
    box->addWidget(mKMailButton);
    mEmailClient->addButton(mKMailButton, Preferences::kmail);
    mSendmailButton = new RadioButton(sendmailOption);
    mSendmailButton->setMinimumSize(mSendmailButton->sizeHint());
    box->addWidget(mSendmailButton);
    mEmailClient->addButton(mSendmailButton, Preferences::sendmail);
    connect(mEmailClient, &ButtonGroup::buttonSet, this, &EmailPrefTab::slotEmailClientChanged);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Choose how to send email when an email alarm is triggered."
          "<list><item><interface>%1</interface>: The email is sent automatically via <application>KMail</application>. <application>KMail</application> is started first if necessary.</item>"
          "<item><interface>%2</interface>: The email is sent automatically. This option will only work if "
          "your system is configured to use <application>sendmail</application> or a sendmail compatible mail transport agent.</item></list></para>",
          kmailOption, sendmailOption));

    widget = new QWidget;  // this is to allow left adjustment
    topLayout()->addWidget(widget);
    box = new QHBoxLayout(widget);
    mEmailCopyToKMail = new QCheckBox(xi18nc("@option:check", "Copy sent emails into <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
    mEmailCopyToKMail->setWhatsThis(xi18nc("@info:whatsthis", "After sending an email, store a copy in <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
    box->addWidget(mEmailCopyToKMail);
    box->setStretchFactor(new QWidget(widget), 1);    // left adjust the controls

    widget = new QWidget;   // this is to allow left adjustment
    topLayout()->addWidget(widget);
    box = new QHBoxLayout(widget);
    mEmailQueuedNotify = new QCheckBox(i18nc("@option:check", "Notify when remote emails are queued"));
    mEmailQueuedNotify->setWhatsThis(
          i18nc("@info:whatsthis", "Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
               "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
    box->addWidget(mEmailQueuedNotify);
    box->setStretchFactor(new QWidget(widget), 1);    // left adjust the controls

    // Your Email Address group box
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Your Email Address"));
    topLayout()->addWidget(group);
    QGridLayout* grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(2, 1);

    // 'From' email address controls ...
    label = new Label(i18nc("@label 'From' email address", "From:"), group);
    grid->addWidget(label, 1, 0);
    mFromAddressGroup = new ButtonGroup(group);
    connect(mFromAddressGroup, &ButtonGroup::buttonSet, this, &EmailPrefTab::slotFromAddrChanged);

    // Line edit to enter a 'From' email address
    mFromAddrButton = new RadioButton(group);
    mFromAddressGroup->addButton(mFromAddrButton, Preferences::MAIL_FROM_ADDR);
    label->setBuddy(mFromAddrButton);
    grid->addWidget(mFromAddrButton, 1, 1);
    mEmailAddress = new QLineEdit(group);
    connect(mEmailAddress, &QLineEdit::textChanged, this, &EmailPrefTab::slotAddressChanged);
    QString whatsThis = i18nc("@info:whatsthis", "Your email address, used to identify you as the sender when sending email alarms.");
    mFromAddrButton->setWhatsThis(whatsThis);
    mEmailAddress->setWhatsThis(whatsThis);
    mFromAddrButton->setFocusWidget(mEmailAddress);
    grid->addWidget(mEmailAddress, 1, 2);

    // 'From' email address to be taken from System Settings
    mFromCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from System Settings"), group);
    mFromAddressGroup->addButton(mFromCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
    mFromCCentreButton->setWhatsThis(
          i18nc("@info:whatsthis", "Check to use the email address set in KDE System Settings, to identify you as the sender when sending email alarms."));
    grid->addWidget(mFromCCentreButton, 2, 1, 1, 2, Qt::AlignLeft);

    // 'From' email address to be picked from KMail's identities when the email alarm is configured
    mFromKMailButton = new RadioButton(xi18nc("@option:radio", "Use <application>KMail</application> identities"), group);
    mFromAddressGroup->addButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
    mFromKMailButton->setWhatsThis(
          xi18nc("@info:whatsthis", "Check to use <application>KMail</application>'s email identities to identify you as the sender when sending email alarms. "
               "For existing email alarms, <application>KMail</application>'s default identity will be used. "
               "For new email alarms, you will be able to pick which of <application>KMail</application>'s identities to use."));
    grid->addWidget(mFromKMailButton, 3, 1, 1, 2, Qt::AlignLeft);

    // 'Bcc' email address controls ...
    grid->setRowMinimumHeight(4, style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new Label(i18nc("@label 'Bcc' email address", "Bcc:"), group);
    grid->addWidget(label, 5, 0);
    mBccAddressGroup = new ButtonGroup(group);
    connect(mBccAddressGroup, &ButtonGroup::buttonSet, this, &EmailPrefTab::slotBccAddrChanged);

    // Line edit to enter a 'Bcc' email address
    mBccAddrButton = new RadioButton(group);
    mBccAddressGroup->addButton(mBccAddrButton, Preferences::MAIL_FROM_ADDR);
    label->setBuddy(mBccAddrButton);
    grid->addWidget(mBccAddrButton, 5, 1);
    mEmailBccAddress = new QLineEdit(group);
    whatsThis = xi18nc("@info:whatsthis", "Your email address, used for blind copying email alarms to yourself. "
                     "If you want blind copies to be sent to your account on the computer which <application>KAlarm</application> runs on, you can simply enter your user login name.");
    mBccAddrButton->setWhatsThis(whatsThis);
    mEmailBccAddress->setWhatsThis(whatsThis);
    mBccAddrButton->setFocusWidget(mEmailBccAddress);
    grid->addWidget(mEmailBccAddress, 5, 2);

    // 'Bcc' email address to be taken from System Settings
    mBccCCentreButton = new RadioButton(i18nc("@option:radio", "Use address from System Settings"), group);
    mBccAddressGroup->addButton(mBccCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
    mBccCCentreButton->setWhatsThis(
          i18nc("@info:whatsthis", "Check to use the email address set in KDE System Settings, for blind copying email alarms to yourself."));
    grid->addWidget(mBccCCentreButton, 6, 1, 1, 2, Qt::AlignLeft);

    topLayout()->addStretch();    // top adjust the widgets
}

void EmailPrefTab::restore(bool defaults, bool)
{
    mEmailClient->setButton(Preferences::emailClient());
    mEmailCopyToKMail->setChecked(Preferences::emailCopyToKMail());
    setEmailAddress(Preferences::emailFrom(), Preferences::emailAddress());
    setEmailBccAddress((Preferences::emailBccFrom() == Preferences::MAIL_FROM_SYS_SETTINGS), Preferences::emailBccAddress());
    mEmailQueuedNotify->setChecked(Preferences::emailQueuedNotify());
    if (!defaults)
        mAddressChanged = mBccAddressChanged = false;
}

void EmailPrefTab::apply(bool syncToDisc)
{
    int client = mEmailClient->selectedId();
    if (client >= 0  &&  static_cast<Preferences::MailClient>(client) != Preferences::emailClient())
        Preferences::setEmailClient(static_cast<Preferences::MailClient>(client));
    bool b = mEmailCopyToKMail->isChecked();
    if (b != Preferences::emailCopyToKMail())
        Preferences::setEmailCopyToKMail(b);
    int from = mFromAddressGroup->selectedId();
    QString text = mEmailAddress->text().trimmed();
    if ((from >= 0  &&  static_cast<Preferences::MailFrom>(from) != Preferences::emailFrom())
    ||  text != Preferences::emailAddress())
        Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(from), text);
    b = (mBccAddressGroup->checkedButton() == mBccCCentreButton);
    Preferences::MailFrom bfrom = b ? Preferences::MAIL_FROM_SYS_SETTINGS : Preferences::MAIL_FROM_ADDR;;
    text = mEmailBccAddress->text().trimmed();
    if (bfrom != Preferences::emailBccFrom()  ||  text != Preferences::emailBccAddress())
        Preferences::setEmailBccAddress(b, text);
    b = mEmailQueuedNotify->isChecked();
    if (b != Preferences::emailQueuedNotify())
        Preferences::setEmailQueuedNotify(mEmailQueuedNotify->isChecked());
    PrefsTabBase::apply(syncToDisc);
}

void EmailPrefTab::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
    mFromAddressGroup->setButton(from);
    mEmailAddress->setText(from == Preferences::MAIL_FROM_ADDR ? address.trimmed() : QString());
}

void EmailPrefTab::setEmailBccAddress(bool useSystemSettings, const QString& address)
{
    mBccAddressGroup->setButton(useSystemSettings ? Preferences::MAIL_FROM_SYS_SETTINGS : Preferences::MAIL_FROM_ADDR);
    mEmailBccAddress->setText(useSystemSettings ? QString() : address.trimmed());
}

void EmailPrefTab::slotEmailClientChanged(QAbstractButton* button)
{
    mEmailCopyToKMail->setEnabled(button == mSendmailButton);
}

void EmailPrefTab::slotFromAddrChanged(QAbstractButton* button)
{
    mEmailAddress->setEnabled(button == mFromAddrButton);
    mAddressChanged = true;
}

void EmailPrefTab::slotBccAddrChanged(QAbstractButton* button)
{
    mEmailBccAddress->setEnabled(button == mBccAddrButton);
    mBccAddressChanged = true;
}

QString EmailPrefTab::validate()
{
    if (mAddressChanged)
    {
        mAddressChanged = false;
        QString errmsg = validateAddr(mFromAddressGroup, mEmailAddress, KAMail::i18n_NeedFromEmailAddress());
        if (!errmsg.isEmpty())
            return errmsg;
    }
    if (mBccAddressChanged)
    {
        mBccAddressChanged = false;
        return validateAddr(mBccAddressGroup, mEmailBccAddress, i18nc("@info", "No valid 'Bcc' email address is specified."));
    }
    return QString();
}

QString EmailPrefTab::validateAddr(ButtonGroup* group, QLineEdit* addr, const QString& msg)
{
    QString errmsg = xi18nc("@info", "<para>%1</para><para>Are you sure you want to save your changes?</para>", msg);
    switch (group->selectedId())
    {
        case Preferences::MAIL_FROM_SYS_SETTINGS:
            if (!KAMail::controlCentreAddress().isEmpty())
                return QString();
            errmsg = i18nc("@info", "No email address is currently set in KDE System Settings. %1", errmsg);
            break;
        case Preferences::MAIL_FROM_KMAIL:
            if (Identities::identitiesExist())
                return QString();
            errmsg = xi18nc("@info", "No <application>KMail</application> identities currently exist. %1", errmsg);
            break;
        case Preferences::MAIL_FROM_ADDR:
            if (!addr->text().trimmed().isEmpty())
                return QString();
            break;
    }
    return errmsg;
}


/*=============================================================================
= Class EditPrefTab
=============================================================================*/

EditPrefTab::EditPrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    KLocalizedString defsetting = kxi18nc("@info:whatsthis", "The default setting for <interface>%1</interface> in the alarm edit dialog.");

    mTabs = new QTabWidget();
    topLayout()->addWidget(mTabs);
    StackedGroupT<QWidget>* tabgroup = new StackedGroupT<QWidget>(mTabs);

    StackedWidgetT<QWidget>* topGeneral = new StackedWidgetT<QWidget>(tabgroup);
    QVBoxLayout *tgLayout = new QVBoxLayout(topGeneral);
    mTabGeneral = mTabs->addTab(topGeneral, i18nc("@title:tab", "General"));

    StackedWidgetT<QWidget>* topTypes = new StackedWidgetT<QWidget>(tabgroup);
    QVBoxLayout *ttLayout = new QVBoxLayout(topTypes);
    mTabTypes = mTabs->addTab(topTypes, i18nc("@title:tab", "Alarm Types"));

    StackedWidgetT<QWidget>* topFontColour = new StackedWidgetT<QWidget>(tabgroup);
    QVBoxLayout *tfLayout = new QVBoxLayout(topFontColour);
    mTabFontColour = mTabs->addTab(topFontColour, i18nc("@title:tab", "Font && Color"));

    // MISCELLANEOUS
    // Show in KOrganizer
    mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_chk_ShowInKOrganizer());
    mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
    mCopyToKOrganizer->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_chk_ShowInKOrganizer()).toString());
    tgLayout->addWidget(mCopyToKOrganizer);

    // Late cancellation
    QWidget *widget = new QWidget;
    tgLayout->addWidget(widget);
    QHBoxLayout *box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(0);
    mLateCancel = new QCheckBox(LateCancelSelector::i18n_chk_CancelIfLate());
    mLateCancel->setMinimumSize(mLateCancel->sizeHint());
    mLateCancel->setWhatsThis(defsetting.subs(LateCancelSelector::i18n_chk_CancelIfLate()).toString());
    box->addWidget(mLateCancel);

    // Recurrence
    widget = new QWidget;   // this is to control the QWhatsThis text display area
    tgLayout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    box->setMargin(0);
    QLabel* label = new QLabel(i18nc("@label:listbox", "Recurrence:"));
    box->addWidget(label);
    mRecurPeriod = new ComboBox();
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_NoRecur());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_AtLogin());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_HourlyMinutely());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Daily());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Weekly());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Monthly());
    mRecurPeriod->addItem(RecurrenceEdit::i18n_combo_Yearly());
    box->addWidget(mRecurPeriod);
    box->addStretch();
    label->setBuddy(mRecurPeriod);
    widget->setWhatsThis(i18nc("@info:whatsthis", "The default setting for the recurrence rule in the alarm edit dialog."));

    // How to handle February 29th in yearly recurrences
    QWidget *febBox = new QWidget;  // this is to control the QWhatsThis text display area
    tgLayout->addWidget(febBox);
    QVBoxLayout *vbox = new QVBoxLayout(febBox);
    vbox->setMargin(0);
    vbox->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label", "In non-leap years, repeat yearly February 29th alarms on:"));
    label->setAlignment(Qt::AlignLeft);
    label->setWordWrap(true);
    vbox->addWidget(label);
    box = new QHBoxLayout();
    vbox->addLayout(box);
    vbox->setMargin(0);
    box->setSpacing(2 * style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mFeb29 = new ButtonGroup(febBox);
    widget = new QWidget();
    widget->setFixedWidth(3 * style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    box->addWidget(widget);
    QRadioButton* radio = new QRadioButton(i18nc("@option:radio", "February 2&8th"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->addButton(radio, Preferences::Feb29_Feb28);
    radio = new QRadioButton(i18nc("@option:radio", "March &1st"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->addButton(radio, Preferences::Feb29_Mar1);
    radio = new QRadioButton(i18nc("@option:radio", "Do not repeat"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->addButton(radio, Preferences::Feb29_None);
    febBox->setWhatsThis(xi18nc("@info:whatsthis",
          "For yearly recurrences, choose what date, if any, alarms due on February 29th should occur in non-leap years."
          "<note>The next scheduled occurrence of existing alarms is not re-evaluated when you change this setting.</note>"));

    tgLayout->addStretch();    // top adjust the widgets

    // DISPLAY ALARMS
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Display Alarms"));
    ttLayout->addWidget(group);
    QVBoxLayout* vlayout = new QVBoxLayout(group);
    vlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    vlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    mConfirmAck = new QCheckBox(EditDisplayAlarmDlg::i18n_chk_ConfirmAck());
    mConfirmAck->setMinimumSize(mConfirmAck->sizeHint());
    mConfirmAck->setWhatsThis(defsetting.subs(EditDisplayAlarmDlg::i18n_chk_ConfirmAck()).toString());
    vlayout->addWidget(mConfirmAck, 0, Qt::AlignLeft);

    mAutoClose = new QCheckBox(LateCancelSelector::i18n_chk_AutoCloseWinLC());
    mAutoClose->setMinimumSize(mAutoClose->sizeHint());
    mAutoClose->setWhatsThis(defsetting.subs(LateCancelSelector::i18n_chk_AutoCloseWin()).toString());
    vlayout->addWidget(mAutoClose, 0, Qt::AlignLeft);

    widget = new QWidget;
    vlayout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    label = new QLabel(i18nc("@label:listbox", "Reminder units:"));
    box->addWidget(label);
    mReminderUnits = new QComboBox();
    mReminderUnits->addItem(i18nc("@item:inlistbox", "Minutes"), TimePeriod::Minutes);
    mReminderUnits->addItem(i18nc("@item:inlistbox", "Hours/Minutes"), TimePeriod::HoursMinutes);
    box->addWidget(mReminderUnits);
    label->setBuddy(mReminderUnits);
    widget->setWhatsThis(i18nc("@info:whatsthis", "The default units for the reminder in the alarm edit dialog, for alarms due soon."));
    box->addStretch(1);    // left adjust the control
    mSpecialActionsButton = new SpecialActionsButton(true);
    box->addWidget(mSpecialActionsButton);

    // SOUND
    QGroupBox* bbox = new QGroupBox(i18nc("@title:group Audio options group", "Sound"));
    ttLayout->addWidget(bbox);
    vlayout = new QVBoxLayout(bbox);
    vlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    vlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setMargin(0);
    vlayout->addLayout(hlayout);
    mSound = new QComboBox();
    mSound->addItem(SoundPicker::i18n_combo_None());         // index 0
    mSound->addItem(SoundPicker::i18n_combo_Beep());         // index 1
    mSound->addItem(SoundPicker::i18n_combo_File());         // index 2
    if (KPIMTextEdit::TextToSpeech::self()->isReady())
        mSound->addItem(SoundPicker::i18n_combo_Speak());  // index 3
    mSound->setMinimumSize(mSound->sizeHint());
    mSound->setWhatsThis(defsetting.subs(SoundPicker::i18n_label_Sound()).toString());
    hlayout->addWidget(mSound);
    hlayout->addStretch();
    mSoundRepeat = new QCheckBox(i18nc("@option:check", "Repeat sound file"));
    mSoundRepeat->setMinimumSize(mSoundRepeat->sizeHint());
    mSoundRepeat->setWhatsThis(
          xi18nc("@info:whatsthis sound file 'Repeat' checkbox", "The default setting for sound file <interface>%1</interface> in the alarm edit dialog.", SoundWidget::i18n_chk_Repeat()));
    hlayout->addWidget(mSoundRepeat);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mSoundFileLabel = new QLabel(i18nc("@label:textbox", "Sound file:"));
    box->addWidget(mSoundFileLabel);
    mSoundFile = new QLineEdit();
    box->addWidget(mSoundFile);
    mSoundFileLabel->setBuddy(mSoundFile);
    mSoundFileBrowse = new QPushButton();
    mSoundFileBrowse->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    connect(mSoundFileBrowse, &QAbstractButton::clicked, this, &EditPrefTab::slotBrowseSoundFile);
    mSoundFileBrowse->setToolTip(i18nc("@info:tooltip", "Choose a sound file"));
    box->addWidget(mSoundFileBrowse);
    widget->setWhatsThis(i18nc("@info:whatsthis", "Enter the default sound file to use in the alarm edit dialog."));
    vlayout->addWidget(widget);

    // COMMAND ALARMS
    group = new QGroupBox(i18nc("@title:group", "Command Alarms"));
    ttLayout->addWidget(group);
    vlayout = new QVBoxLayout(group);
    vlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    vlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    vlayout->addLayout(hlayout);

    mCmdScript = new QCheckBox(EditCommandAlarmDlg::i18n_chk_EnterScript(), group);
    mCmdScript->setMinimumSize(mCmdScript->sizeHint());
    mCmdScript->setWhatsThis(defsetting.subs(EditCommandAlarmDlg::i18n_chk_EnterScript()).toString());
    hlayout->addWidget(mCmdScript);
    hlayout->addStretch();

    mCmdXterm = new QCheckBox(EditCommandAlarmDlg::i18n_chk_ExecInTermWindow(), group);
    mCmdXterm->setMinimumSize(mCmdXterm->sizeHint());
    mCmdXterm->setWhatsThis(defsetting.subs(EditCommandAlarmDlg::i18n_radio_ExecInTermWindow()).toString());
    hlayout->addWidget(mCmdXterm);

    // EMAIL ALARMS
    group = new QGroupBox(i18nc("@title:group", "Email Alarms"));
    ttLayout->addWidget(group);
    vlayout = new QVBoxLayout(group);
    vlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    vlayout->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    // BCC email to sender
    mEmailBcc = new QCheckBox(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf(), group);
    mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
    mEmailBcc->setWhatsThis(defsetting.subs(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf()).toString());
    vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

    ttLayout->addStretch();

    // FONT / COLOUR TAB
    mFontChooser = new FontColourChooser(topFontColour, QStringList(), i18nc("@title:group", "Message Font && Color"), true);
    tfLayout->addWidget(mFontChooser);
}

void EditPrefTab::restore(bool, bool allTabs)
{
    int index;
    if (allTabs  ||  mTabs->currentIndex() == mTabGeneral)
    {
        mCopyToKOrganizer->setChecked(Preferences::defaultCopyToKOrganizer());
        mLateCancel->setChecked(Preferences::defaultLateCancel());
        switch (Preferences::defaultRecurPeriod())
        {
            case Preferences::Recur_Yearly:   index = 6; break;
            case Preferences::Recur_Monthly:  index = 5; break;
            case Preferences::Recur_Weekly:   index = 4; break;
            case Preferences::Recur_Daily:    index = 3; break;
            case Preferences::Recur_SubDaily: index = 2; break;
            case Preferences::Recur_Login:    index = 1; break;
            case Preferences::Recur_None:
            default:                          index = 0; break;
        }
        mRecurPeriod->setCurrentIndex(index);
        mFeb29->setButton(Preferences::defaultFeb29Type());
    }
    if (allTabs  ||  mTabs->currentIndex() == mTabTypes)
    {
        mConfirmAck->setChecked(Preferences::defaultConfirmAck());
        mAutoClose->setChecked(Preferences::defaultAutoClose());
        switch (Preferences::defaultReminderUnits())
        {
            case TimePeriod::Weeks:        index = 3; break;
            case TimePeriod::Days:         index = 2; break;
            default:
            case TimePeriod::HoursMinutes: index = 1; break;
            case TimePeriod::Minutes:      index = 0; break;
        }
        mReminderUnits->setCurrentIndex(index);
        KAEvent::ExtraActionOptions opts(0);
        if (Preferences::defaultExecPreActionOnDeferral())
            opts |= KAEvent::ExecPreActOnDeferral;
        if (Preferences::defaultCancelOnPreActionError())
            opts |= KAEvent::CancelOnPreActError;
        if (Preferences::defaultDontShowPreActionError())
            opts |= KAEvent::DontShowPreActError;
        mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction(), opts);
        mSound->setCurrentIndex(soundIndex(Preferences::defaultSoundType()));
        mSoundFile->setText(Preferences::defaultSoundFile());
        mSoundRepeat->setChecked(Preferences::defaultSoundRepeat());
        mCmdScript->setChecked(Preferences::defaultCmdScript());
        mCmdXterm->setChecked(Preferences::defaultCmdLogType() == Preferences::Log_Terminal);
        mEmailBcc->setChecked(Preferences::defaultEmailBcc());
    }
    if (allTabs  ||  mTabs->currentIndex() == mTabFontColour)
    {
        mFontChooser->setFgColour(Preferences::defaultFgColour());
        mFontChooser->setBgColour(Preferences::defaultBgColour());
        mFontChooser->setFont(Preferences::messageFont());
    }
}

void EditPrefTab::apply(bool syncToDisc)
{
    bool b = mAutoClose->isChecked();
    if (b != Preferences::defaultAutoClose())
        Preferences::setDefaultAutoClose(b);
    b = mConfirmAck->isChecked();
    if (b != Preferences::defaultConfirmAck())
        Preferences::setDefaultConfirmAck(b);
    TimePeriod::Units units;
    switch (mReminderUnits->currentIndex())
    {
        case 3:  units = TimePeriod::Weeks;        break;
        case 2:  units = TimePeriod::Days;         break;
        default:
        case 1:  units = TimePeriod::HoursMinutes; break;
        case 0:  units = TimePeriod::Minutes;      break;
    }
    if (units != Preferences::defaultReminderUnits())
        Preferences::setDefaultReminderUnits(units);
    QString text = mSpecialActionsButton->preAction();
    if (text != Preferences::defaultPreAction())
        Preferences::setDefaultPreAction(text);
    text = mSpecialActionsButton->postAction();
    if (text != Preferences::defaultPostAction())
        Preferences::setDefaultPostAction(text);
    KAEvent::ExtraActionOptions opts = mSpecialActionsButton->options();
    b = opts & KAEvent::ExecPreActOnDeferral;
    if (b != Preferences::defaultExecPreActionOnDeferral())
        Preferences::setDefaultExecPreActionOnDeferral(b);
    b = opts & KAEvent::CancelOnPreActError;
    if (b != Preferences::defaultCancelOnPreActionError())
        Preferences::setDefaultCancelOnPreActionError(b);
    b = opts & KAEvent::DontShowPreActError;
    if (b != Preferences::defaultDontShowPreActionError())
        Preferences::setDefaultDontShowPreActionError(b);
    Preferences::SoundType snd;
    switch (mSound->currentIndex())
    {
        case 3:  snd = Preferences::Sound_Speak; break;
        case 2:  snd = Preferences::Sound_File;  break;
        case 1:  snd = Preferences::Sound_Beep;  break;
        case 0:
        default: snd = Preferences::Sound_None;  break;
    }
    if (snd != Preferences::defaultSoundType())
        Preferences::setDefaultSoundType(snd);
    text = mSoundFile->text();
    if (text != Preferences::defaultSoundFile())
        Preferences::setDefaultSoundFile(text);
    b = mSoundRepeat->isChecked();
    if (b != Preferences::defaultSoundRepeat())
        Preferences::setDefaultSoundRepeat(b);
    b = mCmdScript->isChecked();
    if (b != Preferences::defaultCmdScript())
        Preferences::setDefaultCmdScript(b);
    Preferences::CmdLogType log = mCmdXterm->isChecked() ? Preferences::Log_Terminal : Preferences::Log_Discard;
    if (log != Preferences::defaultCmdLogType())
        Preferences::setDefaultCmdLogType(log);
    b = mEmailBcc->isChecked();
    if (b != Preferences::defaultEmailBcc())
        Preferences::setDefaultEmailBcc(b);
    b = mCopyToKOrganizer->isChecked();
    if (b != Preferences::defaultCopyToKOrganizer())
        Preferences::setDefaultCopyToKOrganizer(b);
    int i = mLateCancel->isChecked() ? 1 : 0;
    if (i != Preferences::defaultLateCancel())
        Preferences::setDefaultLateCancel(i);
    Preferences::RecurType period;
    switch (mRecurPeriod->currentIndex())
    {
        case 6:  period = Preferences::Recur_Yearly;   break;
        case 5:  period = Preferences::Recur_Monthly;  break;
        case 4:  period = Preferences::Recur_Weekly;   break;
        case 3:  period = Preferences::Recur_Daily;    break;
        case 2:  period = Preferences::Recur_SubDaily; break;
        case 1:  period = Preferences::Recur_Login;    break;
        case 0:
        default: period = Preferences::Recur_None;     break;
    }
    if (period != Preferences::defaultRecurPeriod())
        Preferences::setDefaultRecurPeriod(period);
    int feb29 = mFeb29->selectedId();
    if (feb29 >= 0  &&  static_cast<Preferences::Feb29Type>(feb29) != Preferences::defaultFeb29Type())
        Preferences::setDefaultFeb29Type(static_cast<Preferences::Feb29Type>(feb29));
    QColor colour = mFontChooser->fgColour();
    if (colour != Preferences::defaultFgColour())
        Preferences::setDefaultFgColour(colour);
    colour = mFontChooser->bgColour();
    if (colour != Preferences::defaultBgColour())
        Preferences::setDefaultBgColour(colour);
    QFont font = mFontChooser->font();
    if (font != Preferences::messageFont())
        Preferences::setMessageFont(font);
    PrefsTabBase::apply(syncToDisc);
}

void EditPrefTab::slotBrowseSoundFile()
{
    QString defaultDir;
    QString url = SoundPicker::browseFile(defaultDir, mSoundFile->text());
    if (!url.isEmpty())
        mSoundFile->setText(url);
}

int EditPrefTab::soundIndex(Preferences::SoundType type)
{
    switch (type)
    {
        case Preferences::Sound_Speak: return 3;
        case Preferences::Sound_File:  return 2;
        case Preferences::Sound_Beep:  return 1;
        case Preferences::Sound_None:
        default:                       return 0;
    }
}

QString EditPrefTab::validate()
{
    if (mSound->currentIndex() == soundIndex(Preferences::Sound_File)  &&  mSoundFile->text().isEmpty())
    {
        mSoundFile->setFocus();
        return xi18nc("@info", "You must enter a sound file when <interface>%1</interface> is selected as the default sound type", SoundPicker::i18n_combo_File());;
    }
    return QString();
}


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    mTabs = new QTabWidget();
    topLayout()->addWidget(mTabs);

    QWidget *widget = new QWidget;
    QVBoxLayout *topGeneral = new QVBoxLayout(widget);
    topGeneral->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin) / 2);
    topGeneral->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mTabGeneral = mTabs->addTab(widget, i18nc("@title:tab", "General"));

    widget =  new QWidget;
    QVBoxLayout *topWindows = new QVBoxLayout(widget);
    topWindows->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin) / 2);
    topWindows->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mTabWindows = mTabs->addTab(widget, i18nc("@title:tab", "Alarm Windows"));

    // Run-in-system-tray group box
    mShowInSystemTray = new QGroupBox(i18nc("@option:check", "Show in system tray"));
    mShowInSystemTray->setCheckable(true);
    mShowInSystemTray->setWhatsThis(
          xi18nc("@info:whatsthis", "<para>Check to show <application>KAlarm</application>'s icon in the system tray."
               " Showing it in the system tray provides easy access and a status indication.</para>"));
    topGeneral->addWidget(mShowInSystemTray);
    QGridLayout* grid = new QGridLayout(mShowInSystemTray);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, indentWidth());

    mAutoHideSystemTray = new ButtonGroup(mShowInSystemTray);
    connect(mAutoHideSystemTray, &ButtonGroup::buttonSet, this, &ViewPrefTab::slotAutoHideSysTrayChanged);

    QRadioButton* radio = new QRadioButton(i18nc("@option:radio Always show KAlarm icon", "Always show"), mShowInSystemTray);
    mAutoHideSystemTray->addButton(radio, 0);
    radio->setWhatsThis(
          xi18nc("@info:whatsthis",
                "Check to show <application>KAlarm</application>'s icon in the system tray "
                "regardless of whether alarms are due."));
    grid->addWidget(radio, 0, 0, 1, 2, Qt::AlignLeft);

    radio = new QRadioButton(i18nc("@option:radio", "Automatically hide if no active alarms"), mShowInSystemTray);
    mAutoHideSystemTray->addButton(radio, 1);
    radio->setWhatsThis(
          xi18nc("@info:whatsthis",
                "Check to automatically hide <application>KAlarm</application>'s icon in "
                "the system tray if there are no active alarms. When hidden, the icon can "
                "always be made visible by use of the system tray option to show hidden icons."));
    grid->addWidget(radio, 1, 0, 1, 2, Qt::AlignLeft);

    QString text = xi18nc("@info:whatsthis",
                         "Check to automatically hide <application>KAlarm</application>'s icon in the "
                         "system tray if no alarms are due within the specified time period. When hidden, "
                         "the icon can always be made visible by use of the system tray option to show hidden icons.");
    radio = new QRadioButton(i18nc("@option:radio", "Automatically hide if no alarm due within time period:"), mShowInSystemTray);
    radio->setWhatsThis(text);
    mAutoHideSystemTray->addButton(radio, 2);
    grid->addWidget(radio, 2, 0, 1, 2, Qt::AlignLeft);
    mAutoHideSystemTrayPeriod = new TimePeriod(true, mShowInSystemTray);
    mAutoHideSystemTrayPeriod->setWhatsThis(text);
    mAutoHideSystemTrayPeriod->setMaximumWidth(mAutoHideSystemTrayPeriod->sizeHint().width());
    grid->addWidget(mAutoHideSystemTrayPeriod, 3, 1, 1, 1, Qt::AlignLeft);
    mShowInSystemTray->setMaximumHeight(mShowInSystemTray->sizeHint().height());

    // System tray tooltip group box
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "System Tray Tooltip"));
    topGeneral->addWidget(group);
    grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(2, 1);
    grid->setColumnMinimumWidth(0, indentWidth());
    grid->setColumnMinimumWidth(1, indentWidth());

    mTooltipShowAlarms = new QCheckBox(i18nc("@option:check", "Show next &24 hours' alarms"), group);
    mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
    connect(mTooltipShowAlarms, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipAlarmsToggled);
    mTooltipShowAlarms->setWhatsThis(
          i18nc("@info:whatsthis", "Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours."));
    grid->addWidget(mTooltipShowAlarms, 0, 0, 1, 3, Qt::AlignLeft);

    widget = new QWidget;
    QHBoxLayout *box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mTooltipMaxAlarms = new QCheckBox(i18nc("@option:check", "Maximum number of alarms to show:"));
    mTooltipMaxAlarms->setMinimumSize(mTooltipMaxAlarms->sizeHint());
    box->addWidget(mTooltipMaxAlarms);
    connect(mTooltipMaxAlarms, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipMaxToggled);
    mTooltipMaxAlarmCount = new SpinBox(1, 99);
    mTooltipMaxAlarmCount->setSingleShiftStep(5);
    mTooltipMaxAlarmCount->setMinimumSize(mTooltipMaxAlarmCount->sizeHint());
    box->addWidget(mTooltipMaxAlarmCount);
    widget->setWhatsThis(
          i18nc("@info:whatsthis", "Uncheck to display all of the next 24 hours' alarms in the system tray tooltip. "
               "Check to enter an upper limit on the number to be displayed."));
    grid->addWidget(widget, 1, 1, 1, 2, Qt::AlignLeft);

    mTooltipShowTime = new QCheckBox(MainWindow::i18n_chk_ShowAlarmTime(), group);
    mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
    connect(mTooltipShowTime, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipTimeToggled);
    mTooltipShowTime->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, the time at which each alarm is due."));
    grid->addWidget(mTooltipShowTime, 2, 1, 1, 2, Qt::AlignLeft);

    mTooltipShowTimeTo = new QCheckBox(MainWindow::i18n_chk_ShowTimeToAlarm(), group);
    mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
    connect(mTooltipShowTimeTo, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipTimeToToggled);
    mTooltipShowTimeTo->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, how long until each alarm is due."));
    grid->addWidget(mTooltipShowTimeTo, 3, 1, 1, 2, Qt::AlignLeft);

    widget = new QWidget; // this is to control the QWhatsThis text display area
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mTooltipTimeToPrefixLabel = new QLabel(i18nc("@label:textbox", "Prefix:"));
    box->addWidget(mTooltipTimeToPrefixLabel);
    mTooltipTimeToPrefix = new QLineEdit();
    box->addWidget(mTooltipTimeToPrefix);
    mTooltipTimeToPrefixLabel->setBuddy(mTooltipTimeToPrefix);
    widget->setWhatsThis(i18nc("@info:whatsthis", "Enter the text to be displayed in front of the time until the alarm, in the system tray tooltip."));
    grid->addWidget(widget, 4, 2, Qt::AlignLeft);
    group->setMaximumHeight(group->sizeHint().height());

    group = new QGroupBox(i18nc("@title:group", "Alarm List"));
    topGeneral->addWidget(group);
    QHBoxLayout* hlayout = new QHBoxLayout(group);
    hlayout->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    QVBoxLayout* colourLayout = new QVBoxLayout();
    colourLayout->setMargin(0);
    hlayout->addLayout(colourLayout);

    widget = new QWidget;  // to group widgets for QWhatsThis text
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing) / 2);
    colourLayout->addWidget(widget);
    QLabel* label1 = new QLabel(i18nc("@label:listbox", "Disabled alarm color:"));
    box->addWidget(label1);
    box->setStretchFactor(new QWidget(widget), 0);
    mDisabledColour = new ColourButton();
    box->addWidget(mDisabledColour);
    label1->setBuddy(mDisabledColour);
    widget->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for disabled alarms."));

    widget = new QWidget;    // to group widgets for QWhatsThis text
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing) / 2);
    colourLayout->addWidget(widget);
    QLabel* label2 = new QLabel(i18nc("@label:listbox", "Archived alarm color:"));
    box->addWidget(label2);
    box->setStretchFactor(new QWidget(widget), 0);
    mArchivedColour = new ColourButton();
    box->addWidget(mArchivedColour);
    label2->setBuddy(mArchivedColour);
    widget->setWhatsThis(i18nc("@info:whatsthis", "Choose the text color in the alarm list for archived alarms."));
    hlayout->addStretch();

    if (topGeneral)
        topGeneral->addStretch();    // top adjust the widgets


    group = new QGroupBox(i18nc("@title:group", "Alarm Message Windows"));
    topWindows->addWidget(group);
    grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, indentWidth());
    mWindowPosition = new ButtonGroup(group);
    connect(mWindowPosition, &ButtonGroup::buttonSet, this, &ViewPrefTab::slotWindowPosChanged);

    QString whatsthis = xi18nc("@info:whatsthis",
          "<para>Choose how to reduce the chance of alarm messages being accidentally acknowledged:"
          "<list><item>Position alarm message windows as far as possible from the current mouse cursor location, or</item>"
          "<item>Position alarm message windows in the center of the screen, but disable buttons for a short time after the window is displayed.</item></list></para>");
    radio = new QRadioButton(i18nc("@option:radio", "Position windows far from mouse cursor"), group);
    mWindowPosition->addButton(radio, 0);
    radio->setWhatsThis(whatsthis);
    grid->addWidget(radio, 0, 0, 1, 2, Qt::AlignLeft);
    radio = new QRadioButton(i18nc("@option:radio", "Center windows, delay activating window buttons"), group);
    mWindowPosition->addButton(radio, 1);
    radio->setWhatsThis(whatsthis);
    grid->addWidget(radio, 1, 0, 1, 2, Qt::AlignLeft);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    box = new QHBoxLayout(widget);
    box->setMargin(0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    mWindowButtonDelayLabel = new QLabel(i18nc("@label:spinbox", "Button activation delay (seconds):"));
    box->addWidget(mWindowButtonDelayLabel);
    mWindowButtonDelay = new QSpinBox();
    mWindowButtonDelay->setRange(1, 10);
    mWindowButtonDelayLabel->setBuddy(mWindowButtonDelay);
    box->addWidget(mWindowButtonDelay);
    widget->setWhatsThis(i18nc("@info:whatsthis",
                            "Enter how long its buttons should remain disabled after the alarm message window is shown."));
    box->setStretchFactor(new QWidget(widget), 1);    // left adjust the controls
    grid->addWidget(widget, 2, 1, Qt::AlignLeft);

    grid->setRowMinimumHeight(3, style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));

    mModalMessages = new QCheckBox(i18nc("@option:check", "Message windows have a title bar and take keyboard focus"), group);
    mModalMessages->setMinimumSize(mModalMessages->sizeHint());
    mModalMessages->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Specify the characteristics of alarm message windows:"
          "<list><item>If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.</item>"
          "<item>If unchecked, the window does not interfere with your typing when "
          "it is displayed, but it has no title bar and cannot be moved or resized.</item></list></para>"));
    grid->addWidget(mModalMessages, 4, 0, 1, 2, Qt::AlignLeft);

    if (topWindows)
        topWindows->addStretch();    // top adjust the widgets
}

void ViewPrefTab::restore(bool, bool allTabs)
{
    if (allTabs  ||  mTabs->currentIndex() == mTabGeneral)
    {
        mShowInSystemTray->setChecked(Preferences::showInSystemTray());
        int id;
        int mins = Preferences::autoHideSystemTray();
        switch (mins)
        {
            case -1:  id = 1;  break;    // hide if no active alarms
            case 0:   id = 0;  break;    // never hide
            default:
            {
                id = 2;
                int days = 0;
                int secs = 0;
                if (mins % 1440)
                    secs = mins * 60;
                else
                    days = mins / 1440;
                TimePeriod::Units units = secs ? TimePeriod::HoursMinutes
                                        : (days % 7) ? TimePeriod::Days : TimePeriod::Weeks;
                Duration duration((secs ? secs : days), (secs ? Duration::Seconds : Duration::Days));
                mAutoHideSystemTrayPeriod->setPeriod(duration, false, units);
                break;
            }
        }
        mAutoHideSystemTray->setButton(id);
        setTooltip(Preferences::tooltipAlarmCount(),
                   Preferences::showTooltipAlarmTime(),
                   Preferences::showTooltipTimeToAlarm(),
                   Preferences::tooltipTimeToPrefix());
        mDisabledColour->setColor(Preferences::disabledColour());
        mArchivedColour->setColor(Preferences::archivedColour());
    }
    if (allTabs  ||  mTabs->currentIndex() == mTabWindows)
    {
        mWindowPosition->setButton(Preferences::messageButtonDelay() ? 1 : 0);
        mWindowButtonDelay->setValue(Preferences::messageButtonDelay());
        mModalMessages->setChecked(Preferences::modalMessages());
    }
}

void ViewPrefTab::apply(bool syncToDisc)
{
    QColor colour = mDisabledColour->color();
    if (colour != Preferences::disabledColour())
        Preferences::setDisabledColour(colour);
    colour = mArchivedColour->color();
    if (colour != Preferences::archivedColour())
        Preferences::setArchivedColour(colour);
    int n = mTooltipShowAlarms->isChecked() ? -1 : 0;
    if (n  &&  mTooltipMaxAlarms->isChecked())
        n = mTooltipMaxAlarmCount->value();
    if (n != Preferences::tooltipAlarmCount())
        Preferences::setTooltipAlarmCount(n);
    bool b = mTooltipShowTime->isChecked();
    if (b != Preferences::showTooltipAlarmTime())
        Preferences::setShowTooltipAlarmTime(b);
    b = mTooltipShowTimeTo->isChecked();
    if (b != Preferences::showTooltipTimeToAlarm())
        Preferences::setShowTooltipTimeToAlarm(b);
    QString text = mTooltipTimeToPrefix->text();
    if (text != Preferences::tooltipTimeToPrefix())
        Preferences::setTooltipTimeToPrefix(text);
    b = mShowInSystemTray->isChecked();
    if (b != Preferences::showInSystemTray())
        Preferences::setShowInSystemTray(b);
    if (b)
    {
        switch (mAutoHideSystemTray->selectedId())
        {
            case 0:  n = 0;   break;    // never hide
            case 1:  n = -1;  break;    // hide if no active alarms
            case 2:                     // hide if no alarms due within period
                     n = mAutoHideSystemTrayPeriod->period().asSeconds() / 60;
                     break;
        }
        if (n != Preferences::autoHideSystemTray())
            Preferences::setAutoHideSystemTray(n);
    }
    n = mWindowPosition->selectedId();
    if (n)
        n = mWindowButtonDelay->value();
    if (n != Preferences::messageButtonDelay())
        Preferences::setMessageButtonDelay(n);
    b = mModalMessages->isChecked();
    if (b != Preferences::modalMessages())
        Preferences::setModalMessages(b);
    PrefsTabBase::apply(syncToDisc);
}

void ViewPrefTab::setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix)
{
    if (!timeTo)
        time = true;    // ensure that at least one time option is ticked

    // Set the states of the controls without calling signal
    // handlers, since these could change the checkboxes' states.
    mTooltipShowAlarms->blockSignals(true);
    mTooltipShowTime->blockSignals(true);
    mTooltipShowTimeTo->blockSignals(true);

    mTooltipShowAlarms->setChecked(maxAlarms);
    mTooltipMaxAlarms->setChecked(maxAlarms > 0);
    mTooltipMaxAlarmCount->setValue(maxAlarms > 0 ? maxAlarms : 1);
    mTooltipShowTime->setChecked(time);
    mTooltipShowTimeTo->setChecked(timeTo);
    mTooltipTimeToPrefix->setText(prefix);

    mTooltipShowAlarms->blockSignals(false);
    mTooltipShowTime->blockSignals(false);
    mTooltipShowTimeTo->blockSignals(false);

    // Enable/disable controls according to their states
    slotTooltipTimeToToggled(timeTo);
    slotTooltipAlarmsToggled(maxAlarms);
}

void ViewPrefTab::slotTooltipAlarmsToggled(bool on)
{
    mTooltipMaxAlarms->setEnabled(on);
    mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isChecked());
    mTooltipShowTime->setEnabled(on);
    mTooltipShowTimeTo->setEnabled(on);
    on = on && mTooltipShowTimeTo->isChecked();
    mTooltipTimeToPrefix->setEnabled(on);
    mTooltipTimeToPrefixLabel->setEnabled(on);
}

void ViewPrefTab::slotTooltipMaxToggled(bool on)
{
    mTooltipMaxAlarmCount->setEnabled(on && mTooltipMaxAlarms->isEnabled());
}

void ViewPrefTab::slotTooltipTimeToggled(bool on)
{
    if (!on  &&  !mTooltipShowTimeTo->isChecked())
        mTooltipShowTimeTo->setChecked(true);
}

void ViewPrefTab::slotTooltipTimeToToggled(bool on)
{
    if (!on  &&  !mTooltipShowTime->isChecked())
        mTooltipShowTime->setChecked(true);
    on = on && mTooltipShowTimeTo->isEnabled();
    mTooltipTimeToPrefix->setEnabled(on);
    mTooltipTimeToPrefixLabel->setEnabled(on);
}

void ViewPrefTab::slotAutoHideSysTrayChanged(QAbstractButton* button)
{
    mAutoHideSystemTrayPeriod->setEnabled(mAutoHideSystemTray->id(button) == 2);
}

void ViewPrefTab::slotWindowPosChanged(QAbstractButton* button)
{
    bool enable = mWindowPosition->id(button);
    mWindowButtonDelay->setEnabled(enable);
    mWindowButtonDelayLabel->setEnabled(enable);
}
#include "moc_prefdlg_p.cpp"
#include "moc_prefdlg.cpp"

// vim: et sw=4:
