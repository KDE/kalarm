/*
 *  birthdaydlg.cpp  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "birthdaydlg.h"

#include "editdlgtypes.h"
#include "fontcolourbutton.h"
#include "latecancel.h"
#include "preferences.h"
#include "reminder.h"
#include "repetitionbutton.h"
#include "resourcescalendar.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "lib/checkbox.h"
#include "lib/shellprocess.h"
#include "akonadiplugin/akonadiplugin.h"

#include <KLocalizedString>
#include <KConfigGroup>
#include <KStandardAction>
#include <KActionCollection>
#include <KSharedConfig>

#include <QAction>
#include <QGroupBox>
#include <QLabel>
#include <QTreeView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSortFilterProxyModel>


BirthdayDlg::BirthdayDlg(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("BirthdayDlg"));    // used by LikeBack
    setWindowTitle(i18nc("@title:window", "Import Birthdays From KAddressBook"));

    auto mainLayout = new QVBoxLayout(this);
    QWidget* mainWidget = new QWidget;
    mainLayout->addWidget(mainWidget);

    auto topLayout = new QVBoxLayout(mainWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);

    if (Preferences::useAlarmName())
    {
        auto hlayout = new QHBoxLayout();
        hlayout->setContentsMargins(0, 0, 0, 0);
        topLayout->addLayout(hlayout);
        QLabel* label = new QLabel(i18nc("@label:textbox", "Alarm name:"), mainWidget);
        hlayout->addWidget(label);
        mName = new KLineEdit(mainWidget);
        mName->setMinimumSize(mName->sizeHint());
        label->setBuddy(mName);
        mName->setWhatsThis(i18nc("@info:whatsthis", "Enter a name to help you identify this alarm. This is optional and need not be unique."));
        hlayout->addWidget(mName);
    }

    // Prefix and suffix to the name in the alarm text
    // Get default prefix and suffix texts from config file
    KConfigGroup config(KSharedConfig::openConfig(), "General");
    mPrefixText = config.readEntry("BirthdayPrefix", i18nc("@info", "Birthday: "));
    mSuffixText = config.readEntry("BirthdaySuffix");

    QGroupBox* textGroup = new QGroupBox(i18nc("@title:group", "Alarm Text"), mainWidget);
    topLayout->addWidget(textGroup);
    auto grid = new QGridLayout(textGroup);
    QLabel* label = new QLabel(i18nc("@label:textbox", "Prefix:"), textGroup);
    grid->addWidget(label, 0, 0);
    mPrefix = new BLineEdit(mPrefixText, textGroup);
    mPrefix->setMinimumSize(mPrefix->sizeHint());
    label->setBuddy(mPrefix);
    connect(mPrefix, &BLineEdit::focusLost, this, &BirthdayDlg::slotTextLostFocus);
    mPrefix->setWhatsThis(i18nc("@info:whatsthis",
          "Enter text to appear before the person's name in the alarm message, "
          "including any necessary trailing spaces."));
    grid->addWidget(mPrefix, 0, 1);

    label = new QLabel(i18nc("@label:textbox", "Suffix:"), textGroup);
    grid->addWidget(label, 1, 0);
    mSuffix = new BLineEdit(mSuffixText, textGroup);
    mSuffix->setMinimumSize(mSuffix->sizeHint());
    label->setBuddy(mSuffix);
    connect(mSuffix, &BLineEdit::focusLost, this, &BirthdayDlg::slotTextLostFocus);
    mSuffix->setWhatsThis(i18nc("@info:whatsthis",
          "Enter text to appear after the person's name in the alarm message, "
          "including any necessary leading spaces."));
    grid->addWidget(mSuffix, 1, 1);

    QGroupBox* group = new QGroupBox(i18nc("@title:group", "Select Birthdays"), mainWidget);
    topLayout->addWidget(group);
    auto layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);

    AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
    if (!akonadiPlugin)
        return;   // no error message - this constructor should only be called if using Akonadi plugin
    mBirthdaySortModel = akonadiPlugin->createBirthdayModels(mainWidget, this);
    connect(akonadiPlugin, &AkonadiPlugin::birthdayModelDataChanged, this, &BirthdayDlg::resizeViewColumns);
    setSortModelSelectionList();

    mBirthdayModel_NameColumn = akonadiPlugin->birthdayModelEnum(AkonadiPlugin::BirthdayModelValue::NameColumn);
    mBirthdayModel_DateColumn = akonadiPlugin->birthdayModelEnum(AkonadiPlugin::BirthdayModelValue::DateColumn);
    mBirthdayModel_DateRole   = akonadiPlugin->birthdayModelEnum(AkonadiPlugin::BirthdayModelValue::DateRole);

    mListView = new QTreeView(group);
    mListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mListView->setModel(mBirthdaySortModel);
    mListView->setRootIsDecorated(false);    // don't show expander icons
    mListView->setSortingEnabled(true);
    mListView->sortByColumn(mBirthdayModel_NameColumn, mListView->header()->sortIndicatorOrder());
    mListView->setAllColumnsShowFocus(true);
    mListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mListView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mListView->setTextElideMode(Qt::ElideRight);
    mListView->header()->setSectionResizeMode(mBirthdayModel_NameColumn, QHeaderView::Stretch);
    mListView->header()->setSectionResizeMode(mBirthdayModel_DateColumn, QHeaderView::ResizeToContents);
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &BirthdayDlg::slotSelectionChanged);
    mListView->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Select birthdays to set alarms for.<nl/>"
          "This list shows all birthdays in <application>KAddressBook</application> except those for which alarms already exist.</para>"
          "<para>You can select multiple birthdays at one time by dragging the mouse over the list, "
          "or by clicking the mouse while pressing Ctrl or Shift.</para>"));
    layout->addWidget(mListView);

    group = new QGroupBox(i18nc("@title:group", "Alarm Configuration"), mainWidget);
    topLayout->addWidget(group);
    auto groupLayout = new QVBoxLayout(group);

    // Sound checkbox and file selector
    auto hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->addLayout(hlayout);
    mSoundPicker = new SoundPicker(group);
    hlayout->addWidget(mSoundPicker);
    hlayout->addSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hlayout->addStretch();

    // Font and colour choice button and sample text
    mFontColourButton = new FontColourButton(group);
    mFontColourButton->setMaximumHeight(mFontColourButton->sizeHint().height() * 3/2);
    hlayout->addWidget(mFontColourButton);
    connect(mFontColourButton, &FontColourButton::selected, this, &BirthdayDlg::setColours);

    // How much advance warning to give
    mReminder = new Reminder(i18nc("@info:whatsthis", "Check to display a reminder in advance of or after the birthday."),
                             i18nc("@info:whatsthis", "Enter the number of days before or after each birthday to display a reminder. "
                                  "This is in addition to the alarm which is displayed on the birthday."),
                             i18nc("@info:whatsthis", "Select whether the reminder should be triggered before or after the birthday."),
                             false, false, group);
    mReminder->setMaximum(0, 364);
    mReminder->setMinutes(0, true);
    groupLayout->addWidget(mReminder, 0, Qt::AlignLeft);

    // Acknowledgement confirmation required - default = no confirmation
    hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    groupLayout->addLayout(hlayout);
    mConfirmAck = EditDisplayAlarmDlg::createConfirmAckCheckbox(group);
    hlayout->addWidget(mConfirmAck);
    hlayout->addSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hlayout->addStretch();

    if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
    {
        // Special actions button
        mSpecialActionsButton = new SpecialActionsButton(false, group);
        hlayout->addWidget(mSpecialActionsButton);
    }

    // Late display checkbox - default = allow late display
    hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    groupLayout->addLayout(hlayout);
    mLateCancel = new LateCancelSelector(false, group);
    hlayout->addWidget(mLateCancel);
    hlayout->addStretch();

    // Sub-repetition button
    mSubRepetition = new RepetitionButton(i18nc("@action:button", "Sub-Repetition"), false, group);
    mSubRepetition->set(Repetition(), true, 364*24*60);
    mSubRepetition->setWhatsThis(i18nc("@info:whatsthis", "Set up an additional alarm repetition"));
    hlayout->addWidget(mSubRepetition);

    // Set the values to their defaults
    setColours(Preferences::defaultFgColour(), Preferences::defaultBgColour());
    mFontColourButton->setFont(Preferences::messageFont(), true);
    mFontColourButton->setBgColour(Preferences::defaultBgColour());
    mFontColourButton->setFgColour(Preferences::defaultFgColour());
    mLateCancel->setMinutes(Preferences::defaultLateCancel(), true, TimePeriod::Days);
    mConfirmAck->setChecked(Preferences::defaultConfirmAck());
    mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
                      Preferences::defaultSoundVolume(), -1, 0, Preferences::defaultSoundRepeat());
    if (mSpecialActionsButton)
    {
        KAEvent::ExtraActionOptions opts{};
        if (Preferences::defaultExecPreActionOnDeferral())
            opts |= KAEvent::ExecPreActOnDeferral;
        if (Preferences::defaultCancelOnPreActionError())
            opts |= KAEvent::CancelOnPreActError;
        if (Preferences::defaultDontShowPreActionError())
            opts |= KAEvent::DontShowPreActError;
        mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction(), opts);
    }


    mButtonBox = new QDialogButtonBox(this);
    mButtonBox->addButton(QDialogButtonBox::Ok);
    mButtonBox->addButton(QDialogButtonBox::Cancel);
    connect(mButtonBox, &QDialogButtonBox::accepted,
            this, &BirthdayDlg::slotOk);
    connect(mButtonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    mainLayout->addWidget(mButtonBox);


    KActionCollection* actions = new KActionCollection(this);
    KStandardAction::selectAll(mListView, &QTreeView::selectAll, actions);
    KStandardAction::deselect(mListView, &QTreeView::clearSelection, actions);
    actions->addAssociatedWidget(mListView);
    const auto lstActions = actions->actions();
    for (QAction* action : lstActions)
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false); // only enable OK button when something is selected
}

/******************************************************************************
* Return a list of events for birthdays chosen.
*/
QList<KAEvent> BirthdayDlg::events() const
{
    QList<KAEvent> list;
    const QModelIndexList indexes = mListView->selectionModel()->selectedRows();     //clazy:exclude=inefficient-qlist
    const int count = indexes.count();
    if (!count)
        return list;
    list.reserve(count);
    const QDate today = KADateTime::currentLocalDate();
    const KADateTime todayStart(today, KADateTime::LocalZone);
    const int thisYear = today.year();
    const int reminder = mReminder->minutes();
    for (const QModelIndex& index : indexes)
    {
        const QModelIndex nameIndex     = index.model()->index(index.row(), 0);
        const QModelIndex birthdayIndex = index.model()->index(index.row(), 1);
        const QString name = nameIndex.data(Qt::DisplayRole).toString();
        QDate date = birthdayIndex.data(mBirthdayModel_DateRole).toDate();
        date.setDate(thisYear, date.month(), date.day());
        if (date <= today)
            date.setDate(thisYear + 1, date.month(), date.day());
        KAEvent event(KADateTime(date, KADateTime::LocalZone),
                      (mName ? mName->text() : QString()),
                      mPrefix->text() + name + mSuffix->text(),
                      mFontColourButton->bgColour(), mFontColourButton->fgColour(),
                      mFontColourButton->font(), KAEvent::MESSAGE, mLateCancel->minutes(),
                      mFlags, true);
        float fadeVolume;
        int   fadeSecs;
        const float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
        const int   repeatPause = mSoundPicker->repeatPause();
        event.setAudioFile(mSoundPicker->file().toDisplayString(), volume, fadeVolume, fadeSecs, repeatPause);
        const QList<int> months(1, date.month());
        event.setRecurAnnualByDate(1, months, 0, KARecurrence::defaultFeb29Type(), -1, QDate());
        event.setRepetition(mSubRepetition->repetition());
        event.setNextOccurrence(todayStart);
        if (reminder)
            event.setReminder(reminder, false);
        if (mSpecialActionsButton)
            event.setActions(mSpecialActionsButton->preAction(),
                             mSpecialActionsButton->postAction(),
                             mSpecialActionsButton->options());
        event.endChanges();
        list.append(event);
    }
    return list;
}

/******************************************************************************
* Called when the OK button is selected to import the selected birthdays.
*/
void BirthdayDlg::slotOk()
{
    // Save prefix and suffix texts to use as future defaults
    KConfigGroup config(KSharedConfig::openConfig(), "General");
    config.writeEntry("BirthdayPrefix", mPrefix->text());
    config.writeEntry("BirthdaySuffix", mSuffix->text());
    config.sync();

    mFlags = KAEvent::ANY_TIME;
    if (mSoundPicker->sound() == Preferences::Sound_Beep) mFlags |= KAEvent::BEEP;
    if (mSoundPicker->repeatPause() >= 0)                 mFlags |= KAEvent::REPEAT_SOUND;
    if (mConfirmAck->isChecked())                         mFlags |= KAEvent::CONFIRM_ACK;
    if (mFontColourButton->defaultFont())                 mFlags |= KAEvent::DEFAULT_FONT;
    QDialog::accept();
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the OK button depending on whether anything is selected.
*/
void BirthdayDlg::slotSelectionChanged()
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(mListView->selectionModel()->hasSelection());
}

/******************************************************************************
* Called when the font/color button has been clicked.
* Set the colors in the message text entry control.
*/
void BirthdayDlg::setColours(const QColor& fgColour, const QColor& bgColour)
{
    QPalette pal = mPrefix->palette();
    pal.setColor(mPrefix->backgroundRole(), bgColour);
    pal.setColor(mPrefix->foregroundRole(), fgColour);
    mPrefix->setPalette(pal);
    mSuffix->setPalette(pal);
}

/******************************************************************************
* Called when the data has changed in the birthday list.
* Resize the date column.
*/
void BirthdayDlg::resizeViewColumns()
{
    mListView->resizeColumnToContents(mBirthdayModel_DateColumn);
}

/******************************************************************************
* Called when the prefix or suffix text has lost keyboard focus.
* If the text has changed, re-evaluates the selection list according to the new
* birthday alarm text format.
*/
void BirthdayDlg::slotTextLostFocus()
{
    const QString prefix = mPrefix->text();
    const QString suffix = mSuffix->text();
    if (prefix != mPrefixText  ||  suffix != mSuffixText)
    {
        // Text has changed - re-evaluate the selection list
        mPrefixText = prefix;
        mSuffixText = suffix;
        setSortModelSelectionList();
    }
}

/******************************************************************************
* Re-evaluates the selection list according to the birthday alarm text format.
*/
void BirthdayDlg::setSortModelSelectionList()
{
    AkonadiPlugin* akonadiPlugin = Preferences::akonadiPlugin();
    if (!akonadiPlugin)
        return;

    QStringList alarmMessageList;
    const QList<KAEvent> activeEvents = ResourcesCalendar::events(CalEvent::ACTIVE);
    for (const KAEvent& event : activeEvents)
    {
        if (event.actionSubType() == KAEvent::MESSAGE
        &&  event.recurType() == KARecurrence::ANNUAL_DATE
        &&  (mPrefixText.isEmpty()  ||  event.message().startsWith(mPrefixText)))
            alarmMessageList.append(event.message());
    }
    akonadiPlugin->setPrefixSuffix(mBirthdaySortModel, mPrefixText, mSuffixText, alarmMessageList);
}

#include "moc_birthdaydlg.cpp"

// vim: et sw=4:
