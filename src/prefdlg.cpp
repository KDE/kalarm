/*
 *  prefdlg.cpp  -  program preferences dialog
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "prefdlg.h"
#include "prefdlg_p.h"

#include "alarmtimewidget.h"
#include "editdlg.h"
#include "editdlgtypes.h"
#include "fontcolour.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "latecancel.h"
#include "mainwindow.h"
#include "pluginmanager.h"
#include "preferences.h"
#include "recurrenceedit.h"
#include "resourcescalendar.h"
#include "sounddlg.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "resources/resources.h"
#include "lib/buttongroup.h"
#include "lib/checkbox.h"
#include "lib/colourbutton.h"
#include "lib/config.h"
#include "lib/desktop.h"
#include "lib/label.h"
#include "lib/locale.h"
#include "lib/messagebox.h"
#include "lib/radiobutton.h"
#include "lib/slider.h"
#include "lib/stackedwidgets.h"
#include "lib/timeedit.h"
#include "lib/timespinbox.h"
#include "lib/timezonecombo.h"
#include "lib/tooltip.h"
#include "kalarmcalendar/holidays.h"
#include "kalarmcalendar/identities.h"
#include "audioplugin/audioplugin.h"
#include "config-kalarm.h"
#include "kalarm_debug.h"

#include <KHolidays/HolidayRegion>
using namespace KHolidays;

#ifdef HAVE_TEXT_TO_SPEECH_SUPPORT
#include <TextEditTextToSpeech/TextToSpeech>
#endif

#include <KLocalizedString>
#include <KShell>
#include <KAboutData>
#include <KStandardGuiItem>
#include <QIcon>
#if ENABLE_X11
#include <KX11Extras>
#include <KWindowInfo>
#endif
#include <KWindowSystem>
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
#include <QTimer>
#include <QComboBox>
#include <QLocale>
#include <QLineEdit>

using namespace KCalendarCore;
using namespace KAlarmCal;

static const char PREF_DIALOG_NAME[] = "PrefDialog";

/*=============================================================================
= Class KAlarmPrefDlg
=============================================================================*/

KAlarmPrefDlg*         KAlarmPrefDlg::mInstance = nullptr;
KAlarmPrefDlg::TabType KAlarmPrefDlg::mLastTab  = AnyTab;

/******************************************************************************
* Display the Preferences dialog as a non-modal window.
*/
void KAlarmPrefDlg::display()
{
    if (!mInstance)
    {
        mInstance = new KAlarmPrefDlg;
        QSize s;
        if (Config::readWindowSize(PREF_DIALOG_NAME, s))
            mInstance->resize(s);
        mInstance->restoreTab();
        mInstance->show();
    }
    else
    {
        mInstance->restoreTab();
        // Switch to the virtual desktop which the dialog is in
#if ENABLE_X11
        KWindowInfo info = KWindowInfo(mInstance->winId(), NET::WMGeometry | NET::WMDesktop);
        KX11Extras::setCurrentDesktop(info.desktop());
#endif
        mInstance->setWindowState(mInstance->windowState() & ~Qt::WindowMinimized); // un-minimize it if necessary
        mInstance->raise();
        mInstance->activateWindow();
    }
}

KAlarmPrefDlg::KAlarmPrefDlg()
    : KPageDialog()
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QLatin1StringView("PrefDlg"));    // used by LikeBack
    setWindowTitle(i18nc("@title:window", "Configure"));
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Apply);
    button(QDialogButtonBox::Ok)->setDefault(true);
    button(QDialogButtonBox::RestoreDefaults)->setToolTip(KStandardGuiItem::defaults().toolTip());
    button(QDialogButtonBox::Help)->setToolTip(KStandardGuiItem::help().toolTip());
    button(QDialogButtonBox::Apply)->setToolTip(KStandardGuiItem::apply().toolTip());
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

    mEditPage = new EditPrefTab(mTabScrollGroup);
    mEditPageItem = new KPageWidgetItem(mEditPage, i18nc("@title:tab", "Alarm Defaults"));
    mEditPageItem->setHeader(i18nc("@title", "Default Alarm Edit Settings"));
    mEditPageItem->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    addPage(mEditPageItem);

    mViewPage = new ViewPrefTab(mTabScrollGroup);
    mViewPageItem = new KPageWidgetItem(mViewPage, i18nc("@title:tab", "View"));
    mViewPageItem->setHeader(i18nc("@title", "View Settings"));
    mViewPageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
    addPage(mViewPageItem);

    connect(button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotOk);
    connect(button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotCancel);
    connect(button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotApply);
    connect(button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotDefault);
    connect(button(QDialogButtonBox::Help), &QAbstractButton::clicked, this, &KAlarmPrefDlg::slotHelp);
    connect(this, &KPageDialog::currentPageChanged, this, &KAlarmPrefDlg::slotTabChanged);

    restore(false);
    adjustSize();
}

KAlarmPrefDlg::~KAlarmPrefDlg()
{
    mInstance = nullptr;
}

void KAlarmPrefDlg::slotHelp()
{
    KHelpClient::invokeHelp(QStringLiteral("preferences"));
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotApply()
{
    qCDebug(KALARM_LOG) << "KAlarmPrefDlg::slotApply";
    mValid = true;
    apply();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::apply()
{
    QString errmsg = mEmailPage->validate();
    if (!errmsg.isEmpty())
    {
        setCurrentPage(mEmailPageItem);
        if (KAMessageBox::warningYesNo(this, errmsg) != KMessageBox::ButtonCode::PrimaryAction)
        {
            mValid = false;
            return;
        }
    }
    errmsg = mEditPage->validate();
    if (!errmsg.isEmpty())
    {
        setCurrentPage(mEditPageItem);
        KAMessageBox::error(this, errmsg);
        mValid = false;
        return;
    }
    mValid = mEmailPage->apply(false) && mValid;
    mValid = mViewPage->apply(false)  && mValid;
    mValid = mEditPage->apply(false)  && mValid;
    mValid = mStorePage->apply(false) && mValid;
    mValid = mTimePage->apply(false)  && mValid;
    mValid = mMiscPage->apply(false)  && mValid;
    Preferences::self()->save();
}

// Apply the preferences that are currently selected
void KAlarmPrefDlg::slotOk()
{
    qCDebug(KALARM_LOG) << "KAlarmPrefDlg::slotOk";
    mValid = true;
    apply();
    if (mValid)
        QDialog::accept();
}

void KAlarmPrefDlg::accept()
{
    // accept() is called automatically by KPageDialog when OK button is clicked.
    // Don't accept and close the dialog until slotOk() has finished processing.
}

// Discard the current preferences and close the dialog
void KAlarmPrefDlg::slotCancel()
{
    qCDebug(KALARM_LOG) << "KAlarmPrefDlg::slotCancel";
    restore(false);
    KPageDialog::reject();
}

// Reset all controls to the application defaults
void KAlarmPrefDlg::slotDefault()
{
    switch (KAMessageBox::questionYesNoCancel(this,
                         i18nc("@info", "Reset all tabs to their default values, or only reset the current tab?"),
                         QString(),
                         KGuiItem(i18nc("@action:button Reset ALL tabs", "All")),
                         KGuiItem(i18nc("@action:button Reset the CURRENT tab", "Current"))))
    {
        case KMessageBox::ButtonCode::PrimaryAction:
            restore(true);   // restore all tabs
            break;
        case KMessageBox::ButtonCode::SecondaryAction:
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
    qCDebug(KALARM_LOG) << "KAlarmPrefDlg::restore:" << (defaults ? "defaults" : "");
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
        Config::writeWindowSize(PREF_DIALOG_NAME, re->size());
    KPageDialog::resizeEvent(re);
}

/******************************************************************************
* Called when a new tab is selected, to note which one it is.
*/
void KAlarmPrefDlg::slotTabChanged(KPageWidgetItem* item)
{
    mLastTab = (item == mMiscPageItem)  ? Misc
             : (item == mTimePageItem)  ? Time
             : (item == mStorePageItem) ? Store
             : (item == mEditPageItem)  ? Edit
             : (item == mEmailPageItem) ? Email
             : (item == mViewPageItem)  ? View : AnyTab;
}

/******************************************************************************
* Selects the tab noted in mLastTab.
*/
void KAlarmPrefDlg::restoreTab()
{
    KPageWidgetItem* item = nullptr;
    switch (mLastTab)
    {
        case Misc:   item = mMiscPageItem;   break;
        case Time:   item = mTimePageItem;   break;
        case Store:  item = mStorePageItem;  break;
        case Edit:   item = mEditPageItem;   break;
        case Email:  item = mEmailPageItem;  break;
        case View:   item = mViewPageItem;   break;
        default:
            return;
    }
    setCurrentPage(item);
}


/*=============================================================================
= Class PrefsTabBase
=============================================================================*/
int PrefsTabBase::mIndentWidth = 0;
int PrefsTabBase::mGridIndentWidth = 0;

PrefsTabBase::PrefsTabBase(StackedScrollGroup* scrollGroup)
    : StackedScrollWidget(scrollGroup)
{
    QFrame* topWidget = new QFrame(this);
    setWidget(topWidget);
    mTopLayout = new QVBoxLayout(topWidget);
    mTopLayout->setContentsMargins(
        style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
        style()->pixelMetric(QStyle::PM_LayoutTopMargin),
        style()->pixelMetric(QStyle::PM_LayoutRightMargin),
        style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    if (!mIndentWidth)
    {
        QCheckBox cb(this);
        mIndentWidth = CheckBox::textIndent(&cb);
        mGridIndentWidth = mIndentWidth - style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
    }
}

bool PrefsTabBase::apply(bool syncToDisc)
{
    if (syncToDisc)
        Preferences::self()->save();
    return true;
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
        const int labelCount = mLabels.count();
        QList<int> xpos;
        xpos.reserve(labelCount);
        for (const QLabel* label : std::as_const(mLabels))
        {
            const int x = label->mapTo(this, QPoint(0, 0)).x();
            xpos += x;
            const int w = x + label->sizeHint().width();
            if (w > wid)
                wid = w;
        }
        for (i = 0;  i < labelCount;  ++i)
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
    const int marLeft = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);

    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Run Mode"));
    topLayout()->addWidget(group);
    auto vlayout = new QVBoxLayout(group);

    // Run mode
    auto widget = new QWidget;   // this is to control the QWhatsThis text display area
    vlayout->addWidget(widget);
    auto hbox = new QHBoxLayout(widget);
    hbox->setContentsMargins(0, 0, 0, 0);
    mRunMode = new ButtonGroup(group);
    mRunAuto = new QRadioButton(i18nc("@option:radio", "Run automatically"));
    mRunMode->insertButton(mRunAuto);
    KAlarm::setToolTip(mRunAuto, i18nc("@info:tooltip",
            "Start KAlarm automatically at login. This option must be checked to enable normal functioning of KAlarm."));
    hbox->addWidget(mRunAuto);

    mRunManual = new QRadioButton(i18nc("@option:radio", "Run manually only"));
    mRunMode->insertButton(mRunManual);
    KAlarm::setToolTip(mRunManual, i18nc("@info:tooltip",
            "Only run KAlarm manually, never automatically. KAlarm will only function when you manually start it."));
    hbox->addWidget(mRunManual);

    mRunNone = new QRadioButton(i18nc("@option:radio", "Do not run"));
    mRunMode->insertButton(mRunNone);
    KAlarm::setToolTip(mRunNone, i18nc("@info:tooltip",
            "Stop using KAlarm. If you later run it again, it will then resume normal functioning."));
    hbox->addWidget(mRunNone);

    widget->setWhatsThis(i18nc("@info:whatsthis", "To run <application>KAlarm</application> normally"));
    connect(mRunMode, &ButtonGroup::selectionChanged, this, &MiscPrefTab::slotRunModeChanged);

    // Warn before quitting?
    widget = new QWidget;  // this is for consistent left alignment
    topLayout()->addWidget(widget);
    hbox = new QHBoxLayout(widget);
    hbox->setContentsMargins(marLeft, 0, 0, 0);
    mQuitWarn = new QCheckBox(i18nc("@option:check", "Warn before quitting"));
    topLayout()->addWidget(mQuitWarn);
    mQuitWarn->setWhatsThis(xi18nc("@info:whatsthis", "Check to display a warning prompt before quitting <application>KAlarm</application>."));
    hbox->addWidget(mQuitWarn, 0, Qt::AlignLeft);
    hbox->addStretch();    // left adjust the controls

    // Enable alarm names?
    widget = new QWidget;  // this is for consistent left alignment
    topLayout()->addWidget(widget);
    hbox = new QHBoxLayout(widget);
    hbox->setContentsMargins(marLeft, 0, 0, 0);
    mUseAlarmNames = new QCheckBox(i18nc("@option:check", "Enable alarm names"));
    mUseAlarmNames->setMinimumSize(mUseAlarmNames->sizeHint());
    mUseAlarmNames->setWhatsThis(i18nc("@info:whatsthis", "Check to have the option to give alarms a name. This is a convenience to help you to identify alarms."));
    hbox->addWidget(mUseAlarmNames);
    hbox->addStretch();    // left adjust the controls

    // Confirm alarm deletion?
    widget = new QWidget;  // this is for consistent left alignment
    topLayout()->addWidget(widget);
    hbox = new QHBoxLayout(widget);
    hbox->setContentsMargins(marLeft, 0, 0, 0);
    mConfirmAlarmDeletion = new QCheckBox(i18nc("@option:check", "Confirm alarm deletions"));
    mConfirmAlarmDeletion->setMinimumSize(mConfirmAlarmDeletion->sizeHint());
    mConfirmAlarmDeletion->setWhatsThis(i18nc("@info:whatsthis", "Check to be prompted for confirmation each time you delete an alarm."));
    hbox->addWidget(mConfirmAlarmDeletion);
    hbox->addStretch();    // left adjust the controls

    // Default alarm deferral time
    widget = new QWidget;   // this is to control the QWhatsThis text display area
    topLayout()->addWidget(widget);
    hbox = new QHBoxLayout(widget);
    hbox->setContentsMargins(marLeft, 0, 0, 0);
    QLabel* label = new QLabel(i18nc("@label:spinbox", "Default defer time interval:"));
    hbox->addWidget(label);
    mDefaultDeferTime = new TimeSpinBox(1, 5999);
    mDefaultDeferTime->setMinimumSize(mDefaultDeferTime->sizeHint());
    hbox->addWidget(mDefaultDeferTime);
    widget->setWhatsThis(i18nc("@info:whatsthis",
            "Enter the default time interval (hours & minutes) to defer alarms, used by the Defer Alarm dialog."));
    label->setBuddy(mDefaultDeferTime);
    hbox->addStretch();    // left adjust the controls

    // Terminal window to use for command alarms
    group = new QGroupBox(i18nc("@title:group", "Terminal for Command Alarms"));
    group->setWhatsThis(i18nc("@info:whatsthis", "Choose which application to use when a command alarm is executed in a terminal window"));
    topLayout()->addWidget(group);
    auto grid = new QGridLayout(group);
    int row = 0;

    mXtermType = new ButtonGroup(group);
    int index = 0;
    const auto xtermCommands = Preferences::cmdXTermStandardCommands();
    for (auto it = xtermCommands.constBegin();  it != xtermCommands.constEnd();  ++it, ++index)
    {
        QString cmd = it.value();
        const QStringList args = KShell::splitArgs(cmd);
        auto radio = new QRadioButton(args[0], group);
        radio->setMinimumSize(radio->sizeHint());
        mXtermType->insertButton(radio, it.key());
        cmd.replace(QStringLiteral("%t"), KAboutData::applicationData().displayName());
        cmd.replace(QStringLiteral("%c"), QStringLiteral("<command>"));
        cmd.replace(QStringLiteral("%w"), QStringLiteral("<command; sleep>"));
        cmd.replace(QStringLiteral("%C"), QStringLiteral("[command]"));
        cmd.replace(QStringLiteral("%W"), QStringLiteral("[command; sleep]"));
        radio->setWhatsThis(
                xi18nc("@info:whatsthis", "Check to execute command alarms in a terminal window by <icode>%1</icode>", cmd));
        grid->addWidget(radio, (row = index/3), index % 3, Qt::AlignLeft);
    }

    // QHBox used here doesn't allow the QLineEdit to expand!?
    auto hlayout = new QHBoxLayout();
    grid->addLayout(hlayout, row + 1, 0, 1, 3, Qt::AlignLeft);
    QRadioButton* radio = new QRadioButton(i18nc("@option:radio Other terminal window command", "Other:"), group);
    hlayout->addWidget(radio);
    connect(radio, &QAbstractButton::toggled, this, &MiscPrefTab::slotOtherTerminalToggled);
    mXtermType->insertButton(radio, 0);

    mXtermCommand = new QLineEdit(group);
    mXtermCommand->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    hlayout->addWidget(mXtermCommand);
    const QString wt =
          xi18nc("@info:whatsthis", "Enter the full command line needed to execute a command in your chosen terminal window. "
               "By default the alarm's command string will be appended to what you enter here. "
               "See the <application>KAlarm</application> Handbook for details of special codes to tailor the command line.");
    radio->setWhatsThis(wt);
    mXtermCommand->setWhatsThis(wt);

    const QList<AudioPlugin*> audioPlugins = PluginManager::instance()->audioPlugins();
    if (PluginManager::instance()->akonadiPlugin()  ||  !audioPlugins.isEmpty())
    {
        // Plugins
        group = new QGroupBox(i18nc("@title:group", "Plugins"));
        topLayout()->addWidget(group);
        hlayout = new QHBoxLayout(group);

        if (!audioPlugins.isEmpty())
        {
            auto audioGroup = new QGroupBox(i18nc("@title:group", "Audio Backend"));
            audioGroup->setWhatsThis(i18nc("@info:whatsthis", "Choose which audio backend to use to play sound files"));
            hlayout->addWidget(audioGroup);
            mAudioBackend = new ButtonGroup(audioGroup);
            auto audioLayout = new QVBoxLayout(audioGroup);
            int i = 0;
            for (const auto audioPlugin : audioPlugins)
            {
                radio = new QRadioButton(audioPlugin->name());
                mAudioBackend->insertButton(radio, i++);
                audioLayout->addWidget(radio);
            }
        }
        if (PluginManager::instance()->akonadiPlugin())
        {
            // Akonadi plugin
            mUseAkonadi = new QCheckBox(i18nc("@option:check", "Enable Akonadi"), group);
            KAlarm::setToolTip(mUseAkonadi, i18nc("@info:tooltip",
                  "Enable the use of Akonadi to provide features including birthday import, some email functions, and email address book lookup."));
            mUseAkonadi->setWhatsThis(i18nc("@info:whatsthis",
                  "Enable the use of Akonadi to provide features including birthday import, some email functions, and email address book lookup."));
            hlayout->addWidget(mUseAkonadi, 0, Qt::AlignHCenter | Qt::AlignTop);
        }
    }

    topLayout()->addStretch();    // top adjust the widgets
}

void MiscPrefTab::restore(bool defaults, bool)
{
    mRunMode->blockSignals(true);
    if (defaults  ||  Preferences::runMode() == Preferences::RunMode_Auto)
        mRunAuto->setChecked(true);
    else if (Preferences::runMode() == Preferences::RunMode_Manual)
        mRunManual->setChecked(true);
    else
        mRunNone->setChecked(true);
    mRunMode->blockSignals(false);
    mQuitWarn->setChecked(Preferences::quitWarn());
    mUseAlarmNames->setChecked(Preferences::useAlarmName());
    mConfirmAlarmDeletion->setChecked(Preferences::confirmAlarmDeletion());
    mDefaultDeferTime->setValue(Preferences::defaultDeferTime());
    const auto xtermCmd = Preferences::cmdXTermCommandIndex();
    int id = xtermCmd.first;
    if (id > 0  &&  !mXtermType->button(id))
    {
        // The command is a standard command, but there is no button for it
        // (because the executable doesn't exist on this system). So set it
        // into the 'Other' option.
        id = 0;
    }
    mXtermType->setButton(id);
    mXtermCommand->setEnabled(id == 0);
    mXtermCommand->setText(id == 0 ? xtermCmd.second : QString());
    if (mAudioBackend)
    {
        AudioPlugin* audioPlugin = Preferences::audioPlugin();
        const QList<AudioPlugin*> audioPlugins = PluginManager::instance()->audioPlugins();
        int i = 0;
        for (const auto plugin : audioPlugins)
        {
            if (plugin == audioPlugin)
            {
                QAbstractButton* button = mAudioBackend->button(i);
                if (button)
                    button->setChecked(true);
                break;
            }
            ++i;
        }
    }
    if (mUseAkonadi)
        mUseAkonadi->setChecked(Preferences::useAkonadi());
}

bool MiscPrefTab::apply(bool syncToDisc)
{
    // First validate anything entered in Other X-terminal command
    int xtermID = mXtermType->checkedId();
    if (!xtermID)
    {
        // 'Other' is selected.
        QString cmd = mXtermCommand->text();
        if (cmd.isEmpty())
        {
            mXtermCommand->setFocus();
            const QList<QAbstractButton*> buttons = mXtermType->buttons();
            if (buttons.count() == 2)
            {
                // There are only radio buttons for one standard command, plus 'Other'
                const int id = mXtermType->id(buttons[0]);
                cmd = Preferences::cmdXTermStandardCommand(id);
                const QStringList args = KShell::splitArgs(cmd);
                if (KAMessageBox::warningContinueCancel(topLayout()->parentWidget(), xi18nc("@info", "<para>No command is specified to invoke a terminal window.</para><para>The default command (<command>%1</command>) will be used.</para>", args[0]))
                                != KMessageBox::Continue)
                    return false;
                xtermID = id;
                mXtermType->setButton(id);
            }
            else
            {
                if (KAMessageBox::warningContinueCancel(topLayout()->parentWidget(), xi18nc("@info", "No command is specified to invoke a terminal window"))
                                != KMessageBox::Continue)
                    return false;
                xtermID = -1;
            }
        }
        else
        {
            const QStringList args = KShell::splitArgs(cmd);
            cmd = args.isEmpty() ? QString() : args[0];
            if (QStandardPaths::findExecutable(cmd).isEmpty())
            {
                mXtermCommand->setFocus();
                if (KAMessageBox::warningContinueCancel(topLayout()->parentWidget(), xi18nc("@info", "Command to invoke terminal window not found: <command>%1</command>", cmd))
                                != KMessageBox::Continue)
                    return false;
            }
        }
    }

    if (mQuitWarn->isEnabled())
    {
        const bool b = mQuitWarn->isChecked();
        if (b != Preferences::quitWarn())
            Preferences::setQuitWarn(b);
    }
    Preferences::RunMode mode = mRunAuto->isChecked() ? Preferences::RunMode_Auto
                              : mRunManual->isChecked() ? Preferences::RunMode_Manual
                              : Preferences::RunMode_None;
    if (mode != Preferences::runMode())
        Preferences::setRunMode(mode);
    bool b = mUseAlarmNames->isChecked();
    if (b != Preferences::useAlarmName())
        Preferences::setUseAlarmName(b);
    b = mConfirmAlarmDeletion->isChecked();
    if (b != Preferences::confirmAlarmDeletion())
        Preferences::setConfirmAlarmDeletion(b);
    int i = mDefaultDeferTime->value();
    if (i != Preferences::defaultDeferTime())
        Preferences::setDefaultDeferTime(i);
    const auto xtermCmd = Preferences::cmdXTermCommandIndex();
    if (xtermID != xtermCmd.first  ||  (!xtermID && mXtermCommand->text() != xtermCmd.second))
    {
        if (xtermID > 0)
            Preferences::setCmdXTermCommand(xtermID);
        else
            Preferences::setCmdXTermSpecialCommand(mXtermCommand->text());
    }
    if (mAudioBackend)
    {
        i = mAudioBackend->checkedId();
        const QList<AudioPlugin*> audioPlugins = PluginManager::instance()->audioPlugins();
        if (i >= 0  &&  i < audioPlugins.count())
        {
            if (audioPlugins.at(i) != Preferences::audioPlugin())
                Preferences::setAudioPlugin(audioPlugins.at(i));
        }
    }
    if (mUseAkonadi)
    {
        b = mUseAkonadi->isChecked();
        if (b != Preferences::useAkonadi())
            Preferences::setUseAkonadi(b);
    }
    return PrefsTabBase::apply(syncToDisc);
}

void MiscPrefTab::slotRunModeChanged(QAbstractButton* old, QAbstractButton*)
{
    if (mRunNone->isChecked()
    &&  KAMessageBox::warningYesNo(topLayout()->parentWidget(),
                                   xi18nc("@info", "You should not check this option unless you intend to discontinue use of <application>KAlarm</application>"),
                                   QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
                                  ) != KMessageBox::ButtonCode::PrimaryAction)
    {
        bool blocked = mRunMode->blockSignals(true);
        old->setChecked(true);
        if (!blocked)
            mRunMode->blockSignals(false);
    }
    else if (mRunManual->isChecked()
    &&  KAMessageBox::warningYesNo(topLayout()->parentWidget(),
                                   xi18nc("@info", "This will prevent <application>KAlarm</application> from functioning<nl/>"
                                                   "except if you manually start it."),
                                   QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel()
                                  ) != KMessageBox::ButtonCode::PrimaryAction)
    {
        bool blocked = mRunMode->blockSignals(true);
        old->setChecked(true);
        if (!blocked)
            mRunMode->blockSignals(false);
    }
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
    const int marLeft  = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    const int marRight = style()->pixelMetric(QStyle::PM_LayoutRightMargin);
    const int marTop   = style()->pixelMetric(QStyle::PM_LayoutTopMargin);

    // Default time zone
    auto itemBox = new QHBoxLayout();
    itemBox->setContentsMargins(marLeft, marTop, marRight, 0);
    topLayout()->addLayout(itemBox);

    QWidget* widget = new QWidget; // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    auto box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"));
    box->addWidget(label);
    addAlignedLabel(label);
    mTimeZone = new TimeZoneCombo(widget);
    mTimeZone->setMaxVisibleItems(15);
    box->addWidget(mTimeZone);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
                                "Select the time zone which <application>KAlarm</application> should use "
                                "as its default for displaying and entering dates and times."));
    label->setBuddy(mTimeZone);
    itemBox->addStretch();

    // Holiday region
    itemBox = new QHBoxLayout();
    itemBox->setContentsMargins(marLeft, 0, marRight, 0);
    topLayout()->addLayout(itemBox);

    widget = new QWidget;    // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:listbox", "Holiday region:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mHolidays = new QComboBox();
    mHolidays->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    box->addWidget(mHolidays);
    itemBox->addStretch();
    label->setBuddy(mHolidays);
    widget->setWhatsThis(i18nc("@info:whatsthis",
                               "Select which holiday region to use"));

    const QStringList regions = HolidayRegion::regionCodes();
    QMap<QString, QString> regionsMap;
    for (const QString& regionCode : regions)
    {
        const QString name = HolidayRegion::name(regionCode);
        const QString languageName = QLocale::languageToString(QLocale(HolidayRegion::languageCode(regionCode)).language());
        const QString labelText = languageName.isEmpty() ? name : i18nc("Holiday region, region language", "%1 (%2)", name, languageName);
        regionsMap.insert(labelText, regionCode);
    }

    mHolidays->addItem(i18nc("No holiday region", "None"), QString());
    for (QMapIterator<QString, QString> it(regionsMap);  it.hasNext();  )
    {
        it.next();
        mHolidays->addItem(it.key(), it.value());
    }

    if (KernelWakeAlarm::isAvailable())
    {
        // Wake from suspend in advance of alarm time
        itemBox = new QHBoxLayout();
        itemBox->setContentsMargins(marLeft, 0, marRight, 0);
        topLayout()->addLayout(itemBox);

        widget = new QWidget;   // this is to control the QWhatsThis text display area
        itemBox->addWidget(widget);
        box = new QHBoxLayout(widget);
        box->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(i18nc("@label:spinbox How long before alarm to wake system from suspend", "Wake from suspend before alarm:"));
        addAlignedLabel(label);
        box->addWidget(label);
        mPreWakeSuspend = new SpinBox;
        mPreWakeSuspend->setMaximum(10);   // 10 minutes maximum
        box->addWidget(mPreWakeSuspend);
        label->setBuddy(mPreWakeSuspend);
        widget->setWhatsThis(i18nc("@info:whatsthis",
              "Enter how many minutes before the alarm trigger time to wake the system, for alarms set to wake from suspend. This can be used to ensure that the system is fully restored by the time the alarm triggers."));
        label = new QLabel(i18nc("@label Time unit for user entered number", "minutes"));
        box->addWidget(label);
        itemBox->addStretch();
    }

    // Start-of-day time
    itemBox = new QHBoxLayout();
    itemBox->setContentsMargins(marLeft, 0, marRight, 0);
    topLayout()->addLayout(itemBox);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:spinbox", "Start of day for date-only alarms:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mStartOfDay = new TimeEdit();
    box->addWidget(mStartOfDay);
    label->setBuddy(mStartOfDay);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>The earliest time of day at which a date-only alarm will be triggered.</para>"
          "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->addStretch();

    // Working hours
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Working Hours"));
    topLayout()->addWidget(group);
    QBoxLayout* layout = new QVBoxLayout(group);

    QWidget* daybox = new QWidget(group);   // this is to control the QWhatsThis text display area
    layout->addWidget(daybox);
    auto wgrid = new QGridLayout(daybox);
    wgrid->setContentsMargins(0, 0, 0, 0);
    const QLocale locale;
    for (int i = 0;  i < 7;  ++i)
    {
        int day = Locale::localeDayInWeek_to_weekDay(i);
        mWorkDays[i] = new QCheckBox(locale.dayName(day), daybox);
        wgrid->addWidget(mWorkDays[i], i/3, i%3, Qt::AlignLeft);
    }
    daybox->setWhatsThis(i18nc("@info:whatsthis", "Check the days in the week which are work days"));

    itemBox = new QHBoxLayout();
    itemBox->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(itemBox);

    widget = new QWidget;  // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:spinbox", "Daily start time:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mWorkStart = new TimeEdit();
    box->addWidget(mWorkStart);
    label->setBuddy(mWorkStart);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
            "<para>Enter the start time of the working day.</para>"
            "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->addStretch();

    itemBox = new QHBoxLayout();
    itemBox->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(itemBox);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    itemBox->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:spinbox", "Daily end time:"));
    addAlignedLabel(label);
    box->addWidget(label);
    mWorkEnd = new TimeEdit();
    box->addWidget(mWorkEnd);
    label->setBuddy(mWorkEnd);
    widget->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Enter the end time of the working day.</para>"
          "<para>%1</para>", TimeSpinBox::shiftWhatsThis()));
    itemBox->addStretch();

    // KOrganizer event duration
    group = new QGroupBox(i18nc("@title:group", "KOrganizer"));
    topLayout()->addWidget(group);
    layout = new QVBoxLayout(group);

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    layout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
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
    box->addStretch();    // left adjust the controls

    topLayout()->addStretch();    // top adjust the widgets
}

void TimePrefTab::restore(bool, bool)
{
    KADateTime::Spec timeSpec = Preferences::timeSpec();
    mTimeZone->setTimeZone(timeSpec.type() == KADateTime::TimeZone ? timeSpec.namedTimeZone() : QTimeZone());
    const int ix = Preferences::holidays().isValid() ? mHolidays->findData(Preferences::holidays().regionCode()) : 0;
    mHolidays->setCurrentIndex(ix);
    if (mPreWakeSuspend)
        mPreWakeSuspend->setValue(Preferences::wakeFromSuspendAdvance());
    mStartOfDay->setValue(Preferences::startOfDay());
    mWorkStart->setValue(Preferences::workDayStart());
    mWorkEnd->setValue(Preferences::workDayEnd());
    QBitArray days = Preferences::workDays();
    for (int i = 0;  i < 7;  ++i)
    {
        const bool x = days.testBit(Locale::localeDayInWeek_to_weekDay(i) - 1);
        mWorkDays[i]->setChecked(x);
    }
    mKOrgEventDuration->setValue(Preferences::kOrgEventDuration());
}

bool TimePrefTab::apply(bool syncToDisc)
{
    Preferences::setTimeSpec(mTimeZone->timeZone());
    const QString hol = mHolidays->itemData(mHolidays->currentIndex()).toString();
    if (hol != Preferences::holidays().regionCode())
        Preferences::setHolidayRegion(hol);
    if (mPreWakeSuspend)
    {
        unsigned t = static_cast<unsigned>(mPreWakeSuspend->value());
        if (t != Preferences::wakeFromSuspendAdvance())
            Preferences::setWakeFromSuspendAdvance(t);
    }
    int t = mStartOfDay->value();
    const QTime sodt(t/60, t%60, 0);
    if (sodt != Preferences::startOfDay())
        Preferences::setStartOfDay(sodt);
    t = mWorkStart->value();
    Preferences::setWorkDayStart(QTime(t/60, t%60, 0));
    t = mWorkEnd->value();
    Preferences::setWorkDayEnd(QTime(t/60, t%60, 0));
    QBitArray workDays(7);
    for (int i = 0;  i < 7;  ++i)
        if (mWorkDays[i]->isChecked())
            workDays.setBit(Locale::localeDayInWeek_to_weekDay(i) - 1, true);
    Preferences::setWorkDays(workDays);
    Preferences::setKOrgEventDuration(mKOrgEventDuration->value());
    t = mKOrgEventDuration->value();
    if (t != Preferences::kOrgEventDuration())
        Preferences::setKOrgEventDuration(t);
    return PrefsTabBase::apply(syncToDisc);
}


/*=============================================================================
= Class StorePrefTab
=============================================================================*/

StorePrefTab::StorePrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    // Which resource to save to
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "New Alarms && Templates"));
    topLayout()->addWidget(group);
    auto bgroup = new QButtonGroup(group);
    QBoxLayout* layout = new QVBoxLayout(group);

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
    auto grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, gridIndentWidth());
    mKeepArchived = new QCheckBox(i18nc("@option:check", "Keep alarms after expiry"), group);
    connect(mKeepArchived, &QAbstractButton::toggled, this, &StorePrefTab::slotArchivedToggled);
    mKeepArchived->setWhatsThis(
          i18nc("@info:whatsthis", "Check to archive alarms after expiry or deletion (except deleted alarms which were never triggered)."));
    grid->addWidget(mKeepArchived, 0, 0, 1, 2, Qt::AlignLeft);

    QWidget* widget = new QWidget;
    auto box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    mPurgeArchived = new QCheckBox(i18nc("@option:check", "Discard archived alarms after:"));
    mPurgeArchived->setMinimumSize(mPurgeArchived->sizeHint());
    box->addWidget(mPurgeArchived);
    connect(mPurgeArchived, &QAbstractButton::toggled, this, &StorePrefTab::slotArchivedToggled);
    mPurgeAfter = new SpinBox;
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
    mClearArchived->setWhatsThis((Resources::enabledResources(CalEvent::ARCHIVED, false).count() <= 1)
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
    const int keepDays = Preferences::archivedKeepDays();
    if (!defaults)
        mOldKeepArchived = keepDays;
    setArchivedControls(keepDays);
    mCheckKeepChanges = true;
}

bool StorePrefTab::apply(bool syncToDisc)
{
    const bool b = mAskResource->isChecked();
    if (b != Preferences::askResource())
        Preferences::setAskResource(mAskResource->isChecked());
    const int days = !mKeepArchived->isChecked() ? 0 : mPurgeArchived->isChecked() ? mPurgeAfter->value() : -1;
    if (days != Preferences::archivedKeepDays())
        Preferences::setArchivedKeepDays(days);
    return PrefsTabBase::apply(syncToDisc);
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
    const bool keep = mKeepArchived->isChecked();
    if (keep  &&  !mOldKeepArchived  &&  mCheckKeepChanges
    &&  !Resources::getStandard(CalEvent::ARCHIVED, true).isValid())
    {
        KAMessageBox::error(topLayout()->parentWidget(),
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
    const bool single = Resources::enabledResources(CalEvent::ARCHIVED, false).count() <= 1;
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
    : PrefsTabBase(scrollGroup)
{
    const int marLeft = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);

    QWidget* widget = new QWidget;
    topLayout()->addWidget(widget);
    auto box = new QHBoxLayout(widget);
    box->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    QLabel* label = new QLabel(i18nc("@label", "Email client:"));
    box->addWidget(label);
    mEmailClient = new ButtonGroup(widget);
    const QString kmailOption = i18nc("@option:radio", "KMail");
    const QString sendmailOption = i18nc("@option:radio", "Sendmail");
    if (PluginManager::instance()->akonadiPlugin())
    {
        mKMailButton = new RadioButton(kmailOption);
        mKMailButton->setMinimumSize(mKMailButton->sizeHint());
        box->addWidget(mKMailButton);
        mEmailClient->insertButton(mKMailButton, Preferences::kmail);
    }
    mSendmailButton = new RadioButton(sendmailOption);
    mSendmailButton->setMinimumSize(mSendmailButton->sizeHint());
    box->addWidget(mSendmailButton);
    mEmailClient->insertButton(mSendmailButton, Preferences::sendmail);
    connect(mEmailClient, &ButtonGroup::selectionChanged, this, &EmailPrefTab::slotEmailClientChanged);
    const QString sendText = i18nc("@info", "Choose how to send email when an email alarm is triggered.");
    if (PluginManager::instance()->akonadiPlugin())
        widget->setWhatsThis(xi18nc("@info:whatsthis",
            "<para>%1<list><item><interface>%2</interface>: "
            "The email is sent automatically via <application>KMail</application>.</item>"
            "<item><interface>%3</interface>: "
            "The email is sent automatically. This option will only work if your system is "
            "configured to use <application>sendmail</application> or a sendmail compatible mail transport agent.</item></list></para>",
                                    sendText, kmailOption, sendmailOption));
    else
        widget->setWhatsThis(xi18nc("@info:whatsthis",
            "<para>%1<list><item><interface>%2</interface>: "
            "The email is sent automatically. This option will only work if your system is "
            "configured to use <application>sendmail</application> or a sendmail compatible mail transport agent.</item></list></para>",
                                    sendText, sendmailOption));

    if (PluginManager::instance()->akonadiPlugin())
    {
        widget = new QWidget;  // this is to allow left adjustment
        topLayout()->addWidget(widget);
        box = new QHBoxLayout(widget);
        box->setContentsMargins(marLeft, 0, 0, 0);
        mEmailCopyToKMail = new QCheckBox(xi18nc("@option:check", "Copy sent emails into <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
        mEmailCopyToKMail->setWhatsThis(xi18nc("@info:whatsthis", "After sending an email, store a copy in <application>KMail</application>'s <resource>%1</resource> folder", KAMail::i18n_sent_mail()));
        box->addWidget(mEmailCopyToKMail);
        box->addStretch();    // left adjust the controls
    }

    widget = new QWidget;   // this is to allow left adjustment
    topLayout()->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(marLeft, 0, 0, 0);
    mEmailQueuedNotify = new QCheckBox(i18nc("@option:check", "Notify when remote emails are queued"));
    mEmailQueuedNotify->setWhatsThis(
          i18nc("@info:whatsthis", "Display a notification message whenever an email alarm has queued an email for sending to a remote system. "
               "This could be useful if, for example, you have a dial-up connection, so that you can then ensure that the email is actually transmitted."));
    box->addWidget(mEmailQueuedNotify);
    box->addStretch();    // left adjust the controls

    // Your Email Address group box
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Your Email Address"));
    topLayout()->addWidget(group);
    auto grid = new QGridLayout(group);
    grid->setColumnStretch(2, 1);

    // 'From' email address controls ...
    label = new Label(i18nc("@label 'From' email address", "From:"), group);
    grid->addWidget(label, 1, 0);
    mFromAddressGroup = new ButtonGroup(group);
    connect(mFromAddressGroup, &ButtonGroup::selectionChanged, this, &EmailPrefTab::slotFromAddrChanged);

    // Line edit to enter a 'From' email address
    mFromAddrButton = new RadioButton(group);
    mFromAddressGroup->insertButton(mFromAddrButton, Preferences::MAIL_FROM_ADDR);
    label->setBuddy(mFromAddrButton);
    grid->addWidget(mFromAddrButton, 1, 1);
    mEmailAddress = new QLineEdit(group);
    connect(mEmailAddress, &QLineEdit::textChanged, this, &EmailPrefTab::slotAddressChanged);
    QString whatsThis = i18nc("@info:whatsthis", "Your email address, used to identify you as the sender when sending email alarms.");
    mFromAddrButton->setWhatsThis(whatsThis);
    mEmailAddress->setWhatsThis(whatsThis);
    mFromAddrButton->setFocusWidget(mEmailAddress);
    grid->addWidget(mEmailAddress, 1, 2);

    // 'From' email address to be taken from KMail
    mFromCCentreButton = new RadioButton(xi18nc("@option:radio", "Use default <application>KMail</application> identity"), group);
    mFromAddressGroup->insertButton(mFromCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
    mFromCCentreButton->setWhatsThis(
          xi18nc("@info:whatsthis", "Check to use the default email address set in <application>KMail</application>, to identify you as the sender when sending email alarms."));
    grid->addWidget(mFromCCentreButton, 2, 1, 1, 2, Qt::AlignLeft);

    // 'From' email address to be picked from KMail's identities when the email alarm is configured
    mFromKMailButton = new RadioButton(xi18nc("@option:radio", "Use <application>KMail</application> identities"), group);
    mFromAddressGroup->insertButton(mFromKMailButton, Preferences::MAIL_FROM_KMAIL);
    mFromKMailButton->setWhatsThis(
          xi18nc("@info:whatsthis", "Check to use <application>KMail</application>'s email identities to identify you as the sender when sending email alarms. "
               "For existing email alarms, <application>KMail</application>'s default identity will be used. "
               "For new email alarms, you will be able to pick which of <application>KMail</application>'s identities to use."));
    grid->addWidget(mFromKMailButton, 3, 1, 1, 2, Qt::AlignLeft);

    // 'Bcc' email address controls ...
    grid->setRowMinimumHeight(4, style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    label = new Label(i18nc("@label 'Bcc' email address", "Bcc:"), group);
    grid->addWidget(label, 5, 0);
    mBccAddressGroup = new ButtonGroup(group);
    connect(mBccAddressGroup, &ButtonGroup::selectionChanged, this, &EmailPrefTab::slotBccAddrChanged);

    // Line edit to enter a 'Bcc' email address
    mBccAddrButton = new RadioButton(group);
    mBccAddressGroup->insertButton(mBccAddrButton, Preferences::MAIL_FROM_ADDR);
    label->setBuddy(mBccAddrButton);
    grid->addWidget(mBccAddrButton, 5, 1);
    mEmailBccAddress = new QLineEdit(group);
    whatsThis = xi18nc("@info:whatsthis", "Your email address, used for blind copying email alarms to yourself. "
                     "If you want blind copies to be sent to your account on the computer which <application>KAlarm</application> runs on, you can simply enter your user login name.");
    mBccAddrButton->setWhatsThis(whatsThis);
    mEmailBccAddress->setWhatsThis(whatsThis);
    mBccAddrButton->setFocusWidget(mEmailBccAddress);
    grid->addWidget(mEmailBccAddress, 5, 2);

    // 'Bcc' email address to be taken from KMail
    mBccCCentreButton = new RadioButton(xi18nc("@option:radio", "Use default <application>KMail</application> identity"), group);
    mBccAddressGroup->insertButton(mBccCCentreButton, Preferences::MAIL_FROM_SYS_SETTINGS);
    mBccCCentreButton->setWhatsThis(
          xi18nc("@info:whatsthis", "Check to use the default email address set in <application>KMail</application>, for blind copying email alarms to yourself."));
    grid->addWidget(mBccCCentreButton, 6, 1, 1, 2, Qt::AlignLeft);

    topLayout()->addStretch();    // top adjust the widgets
}

void EmailPrefTab::restore(bool defaults, bool)
{
    mEmailClient->setButton(Preferences::emailClient());
    if (mEmailCopyToKMail)
        mEmailCopyToKMail->setChecked(Preferences::emailCopyToKMail());
    setEmailAddress(Preferences::emailFrom(), Preferences::emailAddress());
    setEmailBccAddress((Preferences::emailBccFrom() == Preferences::MAIL_FROM_SYS_SETTINGS), Preferences::emailBccAddress());
    mEmailQueuedNotify->setChecked(Preferences::emailQueuedNotify());
    if (!defaults)
        mAddressChanged = mBccAddressChanged = false;
}

bool EmailPrefTab::apply(bool syncToDisc)
{
    const int client = mEmailClient->checkedId();
    if (client >= 0  &&  static_cast<Preferences::MailClient>(client) != Preferences::emailClient())
        Preferences::setEmailClient(static_cast<Preferences::MailClient>(client));
    if (mEmailCopyToKMail)
    {
        bool b = mEmailCopyToKMail->isChecked();
        if (b != Preferences::emailCopyToKMail())
            Preferences::setEmailCopyToKMail(b);
    }
    int from = mFromAddressGroup->checkedId();
    QString text = mEmailAddress->text().trimmed();
    if ((from >= 0  &&  static_cast<Preferences::MailFrom>(from) != Preferences::emailFrom())
    ||  text != Preferences::emailAddress())
        Preferences::setEmailAddress(static_cast<Preferences::MailFrom>(from), text);
    bool b = (mBccAddressGroup->checkedButton() == mBccCCentreButton);
    Preferences::MailFrom bfrom = b ? Preferences::MAIL_FROM_SYS_SETTINGS : Preferences::MAIL_FROM_ADDR;;
    text = mEmailBccAddress->text().trimmed();
    if (bfrom != Preferences::emailBccFrom()  ||  text != Preferences::emailBccAddress())
        Preferences::setEmailBccAddress(b, text);
    b = mEmailQueuedNotify->isChecked();
    if (b != Preferences::emailQueuedNotify())
        Preferences::setEmailQueuedNotify(mEmailQueuedNotify->isChecked());
    return PrefsTabBase::apply(syncToDisc);
}

void EmailPrefTab::showEvent(QShowEvent* e)
{
    bool enable = Preferences::useAkonadiIfAvailable();
    if (mKMailButton)
    {
        mKMailButton->setEnabled(enable);
        if (!enable)
            mEmailClient->setButton(Preferences::sendmail);
    }
    if (mEmailCopyToKMail)
        mEmailCopyToKMail->setEnabled(enable);

    PrefsTabBase::showEvent(e);
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

void EmailPrefTab::slotEmailClientChanged(QAbstractButton*, QAbstractButton* button)
{
    if (mEmailCopyToKMail)
        mEmailCopyToKMail->setEnabled(button == mSendmailButton  &&  Preferences::useAkonadiIfAvailable());
}

void EmailPrefTab::slotFromAddrChanged(QAbstractButton*, QAbstractButton* button)
{
    mEmailAddress->setEnabled(button == mFromAddrButton);
    mAddressChanged = true;
}

void EmailPrefTab::slotBccAddrChanged(QAbstractButton*, QAbstractButton* button)
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
    return {};
}

QString EmailPrefTab::validateAddr(ButtonGroup* group, QLineEdit* addr, const QString& msg)
{
    QString errmsg = i18nc("@info", "Are you sure you want to save your changes?");
    switch (group->checkedId())
    {
        case Preferences::MAIL_FROM_SYS_SETTINGS:
            if (!KAMail::controlCentreAddress().isEmpty())
                return {};
            errmsg = xi18nc("@info", "<para>No default email address is currently set in <application>KMail</application>.</para><para>%1</para><para>%2</para>", msg, errmsg);
            break;
        case Preferences::MAIL_FROM_KMAIL:
            if (Identities::identitiesExist())
                return {};
            errmsg = xi18nc("@info", "<para>No <application>KMail</application> identities currently exist.</para><para>%1</para><para>%2</para>", msg, errmsg);
            break;
        case Preferences::MAIL_FROM_ADDR:
            if (!addr->text().trimmed().isEmpty())
                return {};
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
    mTabs->setDocumentMode(true);
    topLayout()->addWidget(mTabs);
    auto tabgroup = new StackedGroup(mTabs);

    auto topGeneral = new StackedGroupWidget(tabgroup);
    auto tgLayout = new QVBoxLayout(topGeneral);
    mTabGeneral = mTabs->addTab(topGeneral, i18nc("@title:tab", "General"));

    auto topTypes = new StackedGroupWidget(tabgroup);
    auto ttLayout = new QVBoxLayout(topTypes);
    ttLayout->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                 0, style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    mTabTypes = mTabs->addTab(topTypes, i18nc("@title:tab Settings specific to alarm type", "Type Specific"));

    auto topFontColour = new StackedGroupWidget(tabgroup);
    auto tfLayout = new QVBoxLayout(topFontColour);
    tfLayout->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                 0, style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    mTabFontColour = mTabs->addTab(topFontColour, i18nc("@title:tab", "Font && Color"));

    // MISCELLANEOUS
    // Alarm message display method
    QWidget* widget = new QWidget;   // this is to control the QWhatsThis text display area
    tgLayout->addWidget(widget);
    auto box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(EditDisplayAlarmDlg::i18n_lbl_DisplayMethod());
    box->addWidget(label);
    mDisplayMethod = new QComboBox();
    mDisplayMethod->addItem(EditDisplayAlarmDlg::i18n_combo_Window());
    mDisplayMethod->addItem(EditDisplayAlarmDlg::i18n_combo_Notify());
    box->addWidget(mDisplayMethod);
    box->addStretch();
    label->setBuddy(mDisplayMethod);
    widget->setWhatsThis(i18nc("@info:whatsthis", "The default setting for the alarm message display method in the alarm edit dialog."));

    // Show in KOrganizer
    mCopyToKOrganizer = new QCheckBox(EditAlarmDlg::i18n_chk_ShowInKOrganizer());
    mCopyToKOrganizer->setMinimumSize(mCopyToKOrganizer->sizeHint());
    mCopyToKOrganizer->setWhatsThis(defsetting.subs(EditAlarmDlg::i18n_chk_ShowInKOrganizer()).toString());
    tgLayout->addWidget(mCopyToKOrganizer);

    // Late cancellation
    widget = new QWidget;
    tgLayout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);
    mLateCancel = new QCheckBox(LateCancelSelector::i18n_chk_CancelIfLate());
    mLateCancel->setMinimumSize(mLateCancel->sizeHint());
    mLateCancel->setWhatsThis(defsetting.subs(LateCancelSelector::i18n_chk_CancelIfLate()).toString());
    box->addWidget(mLateCancel);

    // Recurrence
    widget = new QWidget;   // this is to control the QWhatsThis text display area
    tgLayout->addWidget(widget);
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:listbox", "Recurrence:"));
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
    QWidget* febBox = new QWidget;  // this is to control the QWhatsThis text display area
    tgLayout->addWidget(febBox);
    auto vbox = new QVBoxLayout(febBox);
    vbox->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label", "In non-leap years, repeat yearly February 29th alarms on:"));
    // *** Workaround for the bug that QLabel doesn't align correctly in right-to-left mode. ***
    label->setAlignment((layoutDirection() == Qt::LeftToRight ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignAbsolute);
    label->setWordWrap(true);
    vbox->addWidget(label);
    box = new QHBoxLayout();
    vbox->addLayout(box);
    vbox->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    mFeb29 = new ButtonGroup(febBox);
    widget = new QWidget();
    widget->setFixedWidth(3 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    box->addWidget(widget);
    QRadioButton* radio = new QRadioButton(i18nc("@option:radio", "February 28th"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->insertButton(radio, Preferences::Feb29_Feb28);
    radio = new QRadioButton(i18nc("@option:radio", "March 1st"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->insertButton(radio, Preferences::Feb29_Mar1);
    radio = new QRadioButton(i18nc("@option:radio", "Do not repeat"));
    radio->setMinimumSize(radio->sizeHint());
    box->addWidget(radio);
    mFeb29->insertButton(radio, Preferences::Feb29_None);
    febBox->setWhatsThis(xi18nc("@info:whatsthis",
          "For yearly recurrences, choose what date, if any, alarms due on February 29th should occur in non-leap years."
          "<note>The next scheduled occurrence of existing alarms is not re-evaluated when you change this setting.</note>"));

    tgLayout->addStretch();    // top adjust the widgets

    // DISPLAY ALARMS
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Display Alarms"));
    ttLayout->addWidget(group);
    auto vlayout = new QVBoxLayout(group);

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
    box->setContentsMargins(0, 0, 0, 0);
    label = new QLabel(i18nc("@label:listbox", "Reminder units:"));
    box->addWidget(label);
    mReminderUnits = new QComboBox();
    mReminderUnits->addItem(i18nc("@item:inlistbox", "Minutes"), TimePeriod::Minutes);
    mReminderUnits->addItem(i18nc("@item:inlistbox", "Hours/Minutes"), TimePeriod::HoursMinutes);
    box->addWidget(mReminderUnits);
    label->setBuddy(mReminderUnits);
    widget->setWhatsThis(i18nc("@info:whatsthis", "The default units for the reminder in the alarm edit dialog, for alarms due soon."));
    box->addStretch();    // left adjust the control
    mSpecialActionsButton = new SpecialActionsButton(true);
    box->addWidget(mSpecialActionsButton);

    // SOUND
    QGroupBox* bbox = new QGroupBox(i18nc("@title:group Audio options group", "Sound"));
    ttLayout->addWidget(bbox);
    vlayout = new QVBoxLayout(bbox);

    auto hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->addLayout(hlayout);
    mSound = new QComboBox();
    mSound->addItem(SoundPicker::i18n_combo_None());         // index 0
    mSound->addItem(SoundPicker::i18n_combo_Beep());         // index 1
    mSound->addItem(SoundPicker::i18n_combo_File());         // index 2
#ifdef HAVE_TEXT_TO_SPEECH_SUPPORT
    if (TextEditTextToSpeech::TextToSpeech::self()->isReady())
        mSound->addItem(SoundPicker::i18n_combo_Speak());    // index 3
#endif
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
    box->setContentsMargins(0, 0, 0, 0);
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

    box = new QHBoxLayout();
    box->setContentsMargins(0, 0, 0, 0);
    mSoundVolumeCheckbox = new CheckBox(SoundWidget::i18n_chk_SetVolume(), nullptr);
    connect(mSoundVolumeCheckbox, &CheckBox::toggled, this, [this](bool on) { mSoundVolumeSlider->setEnabled(on); });
    box->addWidget(mSoundVolumeCheckbox);
    mSoundVolumeSlider = new Slider(0, 100, 10, Qt::Horizontal);
    mSoundVolumeCheckbox->setWhatsThis(i18nc("@info:whatsthis", "Select to set the default volume for sound files in the alarm edit dialog."));
    mSoundVolumeSlider->setTickPosition(QSlider::TicksBelow);
    mSoundVolumeSlider->setTickInterval(10);
    mSoundVolumeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    mSoundVolumeSlider->setWhatsThis(i18nc("@info:whatsthis", "The default volume for sound files in the alarm edit dialog."));
    mSoundVolumeSlider->setEnabled(false);
    box->addWidget(mSoundVolumeSlider);
    label = new QLabel;
    mSoundVolumeSlider->setValueLabel(label, QStringLiteral("%1%"), true);
    box->addWidget(label);
    mSoundVolumeCheckbox->setFocusWidget(mSoundVolumeSlider);
    vlayout->addLayout(box);

    // COMMAND ALARMS
    group = new QGroupBox(i18nc("@title:group", "Command Alarms"));
    ttLayout->addWidget(group);
    vlayout = new QVBoxLayout(group);
    hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
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

    // BCC email to sender
    mEmailBcc = new QCheckBox(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf(), group);
    mEmailBcc->setMinimumSize(mEmailBcc->sizeHint());
    mEmailBcc->setWhatsThis(defsetting.subs(EditEmailAlarmDlg::i18n_chk_CopyEmailToSelf()).toString());
    vlayout->addWidget(mEmailBcc, 0, Qt::AlignLeft);

    ttLayout->addStretch();

    // FONT / COLOUR TAB
    mFontChooser = new FontColourChooser(topFontColour, QStringList(), i18nc("@title:group", "Message Font && Color"), true);
    tfLayout->addWidget(mFontChooser);
    tfLayout->setContentsMargins(0, 0, 0, 0);
}

void EditPrefTab::restore(bool, bool allTabs)
{
    int index;
    if (allTabs  ||  mTabs->currentIndex() == mTabGeneral)
    {
        mDisplayMethod->setCurrentIndex(Preferences::defaultDisplayMethod() == Preferences::Display_Window ? 0 : 1);
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
        KAEvent::ExtraActionOptions opts{};
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
        const int volume = Preferences::defaultSoundVolume() * 100;
        mSoundVolumeCheckbox->setChecked(volume >= 0);
        mSoundVolumeSlider->setValue(volume >= 0 ? volume : 100);
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

bool EditPrefTab::apply(bool syncToDisc)
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
#ifdef HAVE_TEXT_TO_SPEECH_SUPPORT
        case 3:  snd = Preferences::Sound_Speak; break;
#endif
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
    const float v = mSoundVolumeCheckbox->isChecked() ? (float)mSoundVolumeSlider->value() / 100 : -1;
    if (v != Preferences::defaultSoundVolume())
        Preferences::setDefaultSoundVolume(v);
    b = mCmdScript->isChecked();
    if (b != Preferences::defaultCmdScript())
        Preferences::setDefaultCmdScript(b);
    Preferences::CmdLogType log = mCmdXterm->isChecked() ? Preferences::Log_Terminal : Preferences::Log_Discard;
    if (log != Preferences::defaultCmdLogType())
        Preferences::setDefaultCmdLogType(log);
    b = mEmailBcc->isChecked();
    if (b != Preferences::defaultEmailBcc())
        Preferences::setDefaultEmailBcc(b);
    Preferences::DisplayMethod disp;
    switch(mDisplayMethod->currentIndex())
    {
        case 1:  disp = Preferences::Display_Notification; break;
        case 0:
        default: disp = Preferences::Display_Window; break;
    }
    if (disp != Preferences::defaultDisplayMethod())
        Preferences::setDefaultDisplayMethod(disp);
    b = mCopyToKOrganizer->isChecked();
    if (b != Preferences::defaultCopyToKOrganizer())
        Preferences::setDefaultCopyToKOrganizer(b);
    const int i = mLateCancel->isChecked() ? 1 : 0;
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
    const int feb29 = mFeb29->checkedId();
    if (feb29 >= 0  &&  static_cast<Preferences::Feb29Type>(feb29) != Preferences::defaultFeb29Type())
        Preferences::setDefaultFeb29Type(static_cast<Preferences::Feb29Type>(feb29));
    QColor colour = mFontChooser->fgColour();
    if (colour != Preferences::defaultFgColour())
        Preferences::setDefaultFgColour(colour);
    colour = mFontChooser->bgColour();
    if (colour != Preferences::defaultBgColour())
        Preferences::setDefaultBgColour(colour);
    const QFont font = mFontChooser->font();
    if (font != Preferences::messageFont())
        Preferences::setMessageFont(font);
    return PrefsTabBase::apply(syncToDisc);
}

void EditPrefTab::slotBrowseSoundFile()
{
    QString defaultDir;
    QString file;
    if (SoundPicker::browseFile(file, defaultDir, mSoundFile->text()))
    {
        if (!file.isEmpty())
            mSoundFile->setText(file);
    }
}

int EditPrefTab::soundIndex(Preferences::SoundType type)
{
    switch (type)
    {
#ifdef HAVE_TEXT_TO_SPEECH_SUPPORT
        case Preferences::Sound_Speak: return 3;
#endif
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
    return {};
}


/*=============================================================================
= Class ViewPrefTab
=============================================================================*/

ViewPrefTab::ViewPrefTab(StackedScrollGroup* scrollGroup)
    : PrefsTabBase(scrollGroup)
{
    mTabs = new QTabWidget();
    mTabs->setDocumentMode(true);
    topLayout()->addWidget(mTabs);

    QWidget* widget = new QWidget;
    auto topGeneral = new QVBoxLayout(widget);
    topGeneral->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                   0, style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    mTabGeneral = mTabs->addTab(widget, i18nc("@title:tab", "General"));

    widget =  new QWidget;
    auto topWindows = new QVBoxLayout(widget);
    topWindows->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                   0, style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    mTabWindows = mTabs->addTab(widget, i18nc("@title:tab", "Alarm Windows"));

    // Run-in-system-tray check box or group.
    static const QString showInSysTrayText = i18nc("@option:check", "Show in system tray");
    static const QString showInSysTrayWhatsThis = xi18nc("@info:whatsthis",
            "<para>Check to show <application>KAlarm</application>'s icon in the system tray."
            " Showing it in the system tray provides easy access and a status indication.</para>");
    if (Preferences::noAutoHideSystemTrayDesktops().contains(Desktop::currentIdentityName()))
    {
        // Run-in-system-tray check box.
        // This desktop type doesn't provide GUI controls to view hidden system tray
        // icons, so don't show options to hide the system tray icon.
        widget = new QWidget;  // this is to allow left adjustment
        topGeneral->addWidget(widget);
        auto box = new QHBoxLayout(widget);
        mShowInSystemTrayCheck = new QCheckBox(showInSysTrayText);
        mShowInSystemTrayCheck->setWhatsThis(showInSysTrayWhatsThis);
        box->addWidget(mShowInSystemTrayCheck);
        box->addStretch();    // left adjust the controls
    }
    else
    {
        // Run-in-system-tray group box
        mShowInSystemTrayGroup = new QGroupBox(showInSysTrayText);
        mShowInSystemTrayGroup->setCheckable(true);
        mShowInSystemTrayGroup->setWhatsThis(showInSysTrayWhatsThis);
        topGeneral->addWidget(mShowInSystemTrayGroup);
        auto grid = new QGridLayout(mShowInSystemTrayGroup);
        grid->setColumnStretch(1, 1);
        grid->setColumnMinimumWidth(0, gridIndentWidth());

        mAutoHideSystemTray = new ButtonGroup(mShowInSystemTrayGroup);
        connect(mAutoHideSystemTray, &ButtonGroup::selectionChanged, this, &ViewPrefTab::slotAutoHideSysTrayChanged);

        QRadioButton* radio = new QRadioButton(i18nc("@option:radio Always show KAlarm icon", "Always show"), mShowInSystemTrayGroup);
        mAutoHideSystemTray->insertButton(radio, 0);
        radio->setWhatsThis(
              xi18nc("@info:whatsthis",
                    "Check to show <application>KAlarm</application>'s icon in the system tray "
                    "regardless of whether alarms are due."));
        grid->addWidget(radio, 0, 0, 1, 2, Qt::AlignLeft);

        radio = new QRadioButton(i18nc("@option:radio", "Automatically hide if no active alarms"), mShowInSystemTrayGroup);
        mAutoHideSystemTray->insertButton(radio, 1);
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
        radio = new QRadioButton(i18nc("@option:radio", "Automatically hide if no alarm due within time period:"), mShowInSystemTrayGroup);
        radio->setWhatsThis(text);
        mAutoHideSystemTray->insertButton(radio, 2);
        grid->addWidget(radio, 2, 0, 1, 2, Qt::AlignLeft);
        mAutoHideSystemTrayPeriod = new TimePeriod(TimePeriod::ShowMinutes, mShowInSystemTrayGroup);
        mAutoHideSystemTrayPeriod->setWhatsThis(text);
        mAutoHideSystemTrayPeriod->setMaximumWidth(mAutoHideSystemTrayPeriod->sizeHint().width());
        grid->addWidget(mAutoHideSystemTrayPeriod, 3, 1, 1, 1, Qt::AlignLeft);
        mShowInSystemTrayGroup->setMaximumHeight(mShowInSystemTrayGroup->sizeHint().height());
    }

    // System tray tooltip group box
    QGroupBox* group = new QGroupBox(i18nc("@title:group", "System Tray Tooltip"));
    topGeneral->addWidget(group);
    auto grid = new QGridLayout(group);
    grid->setColumnStretch(2, 1);
    grid->setColumnMinimumWidth(0, gridIndentWidth());
    grid->setColumnMinimumWidth(1, gridIndentWidth());

    mTooltipShowAlarms = new QCheckBox(i18nc("@option:check", "Show next 24 hours' alarms"), group);
    mTooltipShowAlarms->setMinimumSize(mTooltipShowAlarms->sizeHint());
    connect(mTooltipShowAlarms, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipAlarmsToggled);
    mTooltipShowAlarms->setWhatsThis(
          i18nc("@info:whatsthis", "Specify whether to include in the system tray tooltip, a summary of alarms due in the next 24 hours."));
    grid->addWidget(mTooltipShowAlarms, 0, 0, 1, 3, Qt::AlignLeft);

    widget = new QWidget;
    auto box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
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

    mTooltipShowTime = new QCheckBox(i18nc("@option:check", "Show alarm time"), group);
    mTooltipShowTime->setMinimumSize(mTooltipShowTime->sizeHint());
    connect(mTooltipShowTime, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipTimeToggled);
    mTooltipShowTime->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, the time at which each alarm is due."));
    grid->addWidget(mTooltipShowTime, 2, 1, 1, 2, Qt::AlignLeft);

    mTooltipShowTimeTo = new QCheckBox(i18nc("@option:check", "Show time until alarm"), group);
    mTooltipShowTimeTo->setMinimumSize(mTooltipShowTimeTo->sizeHint());
    connect(mTooltipShowTimeTo, &QAbstractButton::toggled, this, &ViewPrefTab::slotTooltipTimeToToggled);
    mTooltipShowTimeTo->setWhatsThis(i18nc("@info:whatsthis", "Specify whether to show in the system tray tooltip, how long until each alarm is due."));
    grid->addWidget(mTooltipShowTimeTo, 3, 1, 1, 2, Qt::AlignLeft);

    widget = new QWidget; // this is to control the QWhatsThis text display area
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
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
    auto hlayout = new QHBoxLayout(group);
    auto colourLayout = new QVBoxLayout();
    colourLayout->setContentsMargins(0, 0, 0, 0);
    hlayout->addLayout(colourLayout);

    widget = new QWidget;  // to group widgets for QWhatsThis text
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
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
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
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
    grid->setColumnStretch(1, 1);
    if (!KWindowSystem::isPlatformWayland())  // Wayland doesn't allow positioning of windows
    {
        grid->setColumnMinimumWidth(0, gridIndentWidth());

        mWindowPosition = new ButtonGroup(group);
        connect(mWindowPosition, &ButtonGroup::selectionChanged, this, &ViewPrefTab::slotWindowPosChanged);

        const QString whatsthis = xi18nc("@info:whatsthis",
              "<para>Choose how to reduce the chance of alarm messages being accidentally acknowledged:"
              "<list><item>Position alarm message windows as far as possible from the current mouse cursor location, or</item>"
              "<item>Position alarm message windows in the center of the screen, but disable buttons for a short time after the window is displayed.</item></list></para>");
        QRadioButton* radio = new QRadioButton(i18nc("@option:radio", "Position windows far from mouse cursor"), group);
        mWindowPosition->insertButton(radio, 0);
        radio->setWhatsThis(whatsthis);
        grid->addWidget(radio, 0, 0, 1, 2, Qt::AlignLeft);
        radio = new QRadioButton(i18nc("@option:radio", "Center windows, delay activating window buttons"), group);
        mWindowPosition->insertButton(radio, 1);
        radio->setWhatsThis(whatsthis);
        grid->addWidget(radio, 1, 0, 1, 2, Qt::AlignLeft);
    }

    widget = new QWidget;   // this is to control the QWhatsThis text display area
    box = new QHBoxLayout(widget);
    box->setContentsMargins(0, 0, 0, 0);
    mWindowButtonDelayLabel = new QLabel(i18nc("@label:spinbox", "Button activation delay (seconds):"));
    box->addWidget(mWindowButtonDelayLabel);
    mWindowButtonDelay = new QSpinBox();
    mWindowButtonDelay->setRange(1, 10);
    mWindowButtonDelayLabel->setBuddy(mWindowButtonDelay);
    box->addWidget(mWindowButtonDelay);
    if (KWindowSystem::isPlatformWayland())  // Wayland doesn't allow positioning of windows
        widget->setWhatsThis(i18nc("@info:whatsthis",
                            "Enter how long its buttons should remain disabled after the alarm message window is shown, "
                            "to reduce the chance of alarm messages being accidentally acknowledged."));
    else
        widget->setWhatsThis(i18nc("@info:whatsthis",
                            "Enter how long its buttons should remain disabled after the alarm message window is shown."));
    box->addStretch();    // left adjust the controls
    grid->addWidget(widget, 2, 1, Qt::AlignLeft);

    if (KWindowSystem::isPlatformX11())
    {
        grid->setRowMinimumHeight(3, style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));

        mModalMessages = new QCheckBox(i18nc("@option:check", "Message windows have a title bar and take keyboard focus"), group);
        mModalMessages->setMinimumSize(mModalMessages->sizeHint());
        mModalMessages->setWhatsThis(xi18nc("@info:whatsthis",
              "<para>Specify the characteristics of alarm message windows:"
              "<list><item>If checked, the window is a normal window with a title bar, which grabs keyboard input when it is displayed.</item>"
              "<item>If unchecked, the window does not interfere with your typing when "
              "it is displayed, but it has no title bar and cannot be moved or resized.</item></list></para>"));
        grid->addWidget(mModalMessages, 4, 0, 1, 2, Qt::AlignLeft);
    }

    if (topWindows)
        topWindows->addStretch();    // top adjust the widgets
}

void ViewPrefTab::restore(bool, bool allTabs)
{
    if (allTabs  ||  mTabs->currentIndex() == mTabGeneral)
    {
        if (mShowInSystemTrayGroup)
            mShowInSystemTrayGroup->setChecked(Preferences::showInSystemTray());
        else
            mShowInSystemTrayCheck->setChecked(Preferences::showInSystemTray());
        int id;
        const int mins = Preferences::autoHideSystemTray();
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
                const TimePeriod::Units units = secs ? TimePeriod::HoursMinutes
                                              : (days % 7) ? TimePeriod::Days : TimePeriod::Weeks;
                const Duration duration((secs ? secs : days), (secs ? Duration::Seconds : Duration::Days));
                mAutoHideSystemTrayPeriod->setPeriod(duration, false, units);
                break;
            }
        }
        if (mAutoHideSystemTray)
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
        if (mWindowPosition)
            mWindowPosition->setButton(Preferences::messageButtonDelay() ? 1 : 0);
        mWindowButtonDelay->setValue(Preferences::messageButtonDelay());
        if (mModalMessages)
            mModalMessages->setChecked(Preferences::modalMessages());
    }
}

bool ViewPrefTab::apply(bool syncToDisc)
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
    b = mShowInSystemTrayGroup ? mShowInSystemTrayGroup->isChecked() : mShowInSystemTrayCheck->isChecked();
    if (b != Preferences::showInSystemTray())
        Preferences::setShowInSystemTray(b);
    if (b  &&  mAutoHideSystemTray)
    {
        switch (mAutoHideSystemTray->checkedId())
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
    n = mWindowPosition ? mWindowPosition->checkedId() : 1;
    if (n)
        n = mWindowButtonDelay->value();
    if (n != Preferences::messageButtonDelay())
        Preferences::setMessageButtonDelay(n);
    if (mModalMessages)
    {
        b = mModalMessages->isChecked();
        if (b != Preferences::modalMessages())
            Preferences::setModalMessages(b);
    }
    return PrefsTabBase::apply(syncToDisc);
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

void ViewPrefTab::slotAutoHideSysTrayChanged(QAbstractButton*, QAbstractButton* button)
{
    if (mAutoHideSystemTray)
        mAutoHideSystemTrayPeriod->setEnabled(mAutoHideSystemTray->id(button) == 2);
}

void ViewPrefTab::slotWindowPosChanged(QAbstractButton*, QAbstractButton* button)
{
    const bool enable = mWindowPosition ? mWindowPosition->id(button) : true;
    mWindowButtonDelay->setEnabled(enable);
    mWindowButtonDelayLabel->setEnabled(enable);
}

#include "moc_prefdlg_p.cpp"
#include "moc_prefdlg.cpp"

// vim: et sw=4:
