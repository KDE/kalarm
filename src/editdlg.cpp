/*
 *  editdlg.cpp  -  dialog to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "editdlg.h"
#include "editdlg_p.h"
#include "editdlgtypes.h"

#include "alarmtimewidget.h"
#include "deferdlg.h"
#include "kalarmapp.h"
#include "latecancel.h"
#include "preferences.h"
#include "recurrenceedit.h"
#include "reminder.h"
#include "resourcescalendar.h"
#include "resources/resources.h"
#include "lib/autoqpointer.h"
#include "lib/buttongroup.h"
#include "lib/checkbox.h"
#include "lib/config.h"
#include "lib/lineedit.h"
#include "lib/messagebox.h"
#include "lib/packedlayout.h"
#include "lib/radiobutton.h"
#include "lib/stackedwidgets.h"
#include "templatepickdlg.h"
#include "lib/timeedit.h"
#include "lib/timespinbox.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KConfig>
#include <KSharedConfig>
#include <KWindowSystem>

#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScrollBar>
#include <QTimer>
#include <QDialogButtonBox>
#include <QLocale>
#include <QStyle>

using namespace KCal;
using namespace KAlarmCal;

static const char EDIT_DIALOG_NAME[] = "EditDialog";
static const char TEMPLATE_DIALOG_NAME[] = "EditTemplateDialog";
static const char EDIT_MORE_GROUP[] = "ShowOpts";
static const char EDIT_MORE_KEY[]   = "EditMore";
static const int  maxDelayTime = 99*60 + 59;    // < 100 hours

inline QString recurText(const KAEvent& event)
{
    QString r;
    if (event.repetition())
        r = QStringLiteral("%1 / %2").arg(event.recurrenceText(), event.repetitionText());
    else
        r = event.recurrenceText();
    return i18nc("@title:tab", "Recurrence - [%1]", r);
}

QList<EditAlarmDlg*> EditAlarmDlg::mWindowList;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_chk_ShowInKOrganizer()   { return i18nc("@option:check", "Show in KOrganizer"); }


EditAlarmDlg* EditAlarmDlg::create(bool Template, Type type, QWidget* parent, GetResourceType getResource)
{
    qCDebug(KALARM_LOG) << "EditAlarmDlg::create";
    switch (type)
    {
        case DISPLAY:  return new EditDisplayAlarmDlg(Template, parent, getResource);
        case COMMAND:  return new EditCommandAlarmDlg(Template, parent, getResource);
        case EMAIL:    return new EditEmailAlarmDlg(Template, parent, getResource);
        case AUDIO:    return new EditAudioAlarmDlg(Template, parent, getResource);
        default:  break;
    }
    return nullptr;
}

EditAlarmDlg* EditAlarmDlg::create(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                   GetResourceType getResource, bool readOnly)
{
    switch (event.actionTypes())
    {
        case KAEvent::ACT_COMMAND:  return new EditCommandAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_DISPLAY_COMMAND:
        case KAEvent::ACT_DISPLAY:  return new EditDisplayAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_EMAIL:    return new EditEmailAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_AUDIO:    return new EditAudioAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        default:
            break;
    }
    return nullptr;
}


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event    to initialise the dialog to show the specified event's data.
*/
EditAlarmDlg::EditAlarmDlg(bool Template, KAEvent::SubAction action, QWidget* parent, GetResourceType getResource)
    : QDialog(parent)
    , mAlarmType(action)
    , mTemplate(Template)
    , mNewAlarm(true)
    , mDesiredReadOnly(false)
    , mReadOnly(false)
{
    init(KAEvent(), getResource);
    mWindowList.append(this);
}

EditAlarmDlg::EditAlarmDlg(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                           GetResourceType getResource, bool readOnly)
    : QDialog(parent)
    , mAlarmType(event.actionSubType())
    , mEventId(newAlarm ? QString() : event.id())
    , mTemplate(Template)
    , mNewAlarm(newAlarm)
    , mDesiredReadOnly(readOnly)
    , mReadOnly(readOnly)
{
    init(event, getResource);
    mWindowList.append(this);
}

void EditAlarmDlg::init(const KAEvent& event, GetResourceType getResource)
{
    switch (getResource)
    {
        case RES_USE_EVENT_ID:
            if (event.isValid())
            {
                mResourceEventId = event.id();
                mUseResourceEventId = true;
                break;
            }
            Q_FALLTHROUGH();   // fall through to RES_PROMPT
        case RES_PROMPT:
            mResourceEventId.clear();
            mUseResourceEventId = true;
            break;
        case RES_IGNORE:
        default:
            mResourceEventId.clear();
            mUseResourceEventId = false;
            break;
    }
}

void EditAlarmDlg::init(const KAEvent& event)
{
    setObjectName(mTemplate ? QStringLiteral("TemplEditDlg") : QStringLiteral("EditDlg"));    // used by LikeBack
    QString caption;
    if (mReadOnly)
        caption = mTemplate ? i18nc("@title:window", "Alarm Template [read-only]")
                : event.expired() ? i18nc("@title:window", "Archived Alarm [read-only]")
                                  : i18nc("@title:window", "Alarm [read-only]");
    else
        caption = type_caption();
    setWindowTitle(caption);

    // Create button box now so that types can work with it, but don't insert
    // it into layout just yet
    mButtonBox = new QDialogButtonBox(this);
    if (mReadOnly)
    {
        mButtonBox->addButton(QDialogButtonBox::Cancel);
        mTryButton = mButtonBox->addButton(i18nc("@action:button", "Try"), QDialogButtonBox::ActionRole);
        mMoreLessButton = mButtonBox->addButton(QDialogButtonBox::RestoreDefaults);
    }
    else if (mTemplate)
    {
        mButtonBox->addButton(QDialogButtonBox::Ok);
        mButtonBox->addButton(QDialogButtonBox::Cancel);
        mTryButton = mButtonBox->addButton(i18nc("@action:button", "Try"), QDialogButtonBox::ActionRole);
        mMoreLessButton = mButtonBox->addButton(QDialogButtonBox::RestoreDefaults);
    }
    else
    {
        mButtonBox->addButton(QDialogButtonBox::Ok);
        mButtonBox->addButton(QDialogButtonBox::Cancel);
        mTryButton = mButtonBox->addButton(i18nc("@action:button", "Try"), QDialogButtonBox::ActionRole);
        mLoadTemplateButton = mButtonBox->addButton(i18nc("@action:button", "Load Template..."),
                                                    QDialogButtonBox::HelpRole);
        mLoadTemplateButton->setToolTip(i18nc("@info:tooltip", "Select an alarm template to preset the alarm"));
        mMoreLessButton = mButtonBox->addButton(QDialogButtonBox::RestoreDefaults);
    }
    connect(mButtonBox, &QDialogButtonBox::clicked,
            this, &EditAlarmDlg::slotButtonClicked);

    if (mButtonBox->button(QDialogButtonBox::Ok))
        mButtonBox->button(QDialogButtonBox::Ok)->setWhatsThis(i18nc("@info:whatsthis", "Schedule the alarm at the specified time."));

    auto mainLayout = new QVBoxLayout(this);
    mTabs = new QTabWidget(this);
    mainLayout->addWidget(mTabs);
    mTabScrollGroup = new StackedScrollGroup(this, mTabs);

    auto mainScroll = new StackedScrollWidget(mTabScrollGroup);
    mTabs->addTab(mainScroll, i18nc("@title:tab", "Alarm"));
    mMainPageIndex = 0;
    auto mainPage = new PageFrame(mainScroll);
    mainScroll->setWidget(mainPage);   // mainPage becomes the child of mainScroll
    connect(mainPage, &PageFrame::shown, this, &EditAlarmDlg::slotShowMainPage);
    auto topLayout = new QVBoxLayout(mainPage);

    // Recurrence tab
    auto recurScroll = new StackedScrollWidget(mTabScrollGroup);
    mTabs->addTab(recurScroll, QString());
    mRecurPageIndex = 1;
    QFrame* recurTab = new QFrame;
    auto recurTabLayout = new QVBoxLayout();
    recurTab->setLayout(recurTabLayout);
    recurScroll->setWidget(recurTab);   // recurTab becomes the child of recurScroll
    mRecurrenceEdit = new RecurrenceEdit(mReadOnly);
    recurTabLayout->addWidget(mRecurrenceEdit);
    connect(mRecurrenceEdit, &RecurrenceEdit::shown, this, &EditAlarmDlg::slotShowRecurrenceEdit);
    connect(mRecurrenceEdit, &RecurrenceEdit::typeChanged, this, &EditAlarmDlg::slotRecurTypeChange);
    connect(mRecurrenceEdit, &RecurrenceEdit::frequencyChanged, this, &EditAlarmDlg::slotRecurFrequencyChange);
    connect(mRecurrenceEdit, &RecurrenceEdit::repeatNeedsInitialisation, this, &EditAlarmDlg::slotSetSubRepetition);
    connect(mRecurrenceEdit, &RecurrenceEdit::contentsChanged, this, &EditAlarmDlg::contentsChanged);

    if (mTemplate  ||  Preferences::useAlarmName())
    {
        // Alarm/template name
        QFrame* frame = new QFrame;
        auto box = new QHBoxLayout();
        frame->setLayout(box);
        box->setContentsMargins(0, 0, 0, 0);
        QLabel* label = new QLabel(mTemplate ? i18nc("@label:textbox", "Template name:") : i18nc("@label:textbox", "Alarm name:"));
        box->addWidget(label);
        mName = new QLineEdit();
        mName->setReadOnly(mReadOnly);
        connect(mName, &QLineEdit::textEdited, this, &EditAlarmDlg::contentsChanged);
        label->setBuddy(mName);
        box->addWidget(mName);
        frame->setWhatsThis(mTemplate ? i18nc("@info:whatsthis", "Enter the name of the alarm template")
                                      : i18nc("@info:whatsthis", "Enter a name to help you identify this alarm. This is optional and need not be unique."));
        topLayout->addWidget(frame);
    }

    // Controls specific to the alarm type
    QGroupBox* actionBox = new QGroupBox(i18nc("@title:group", "Action"), mainPage);
    topLayout->addWidget(actionBox, 1);
    auto vlayout = new QVBoxLayout(actionBox);

    type_init(actionBox, vlayout);

    if (!mTemplate)
    {
        // Deferred date/time: visible only for a deferred recurring event.
        mDeferGroup = new QGroupBox(i18nc("@title:group", "Deferred Alarm"), mainPage);
        topLayout->addWidget(mDeferGroup);
        auto hlayout = new QHBoxLayout(mDeferGroup);
        QLabel* label = new QLabel(i18nc("@label", "Deferred to:"), mDeferGroup);
        hlayout->addWidget(label);
        mDeferTimeLabel = new QLabel(mDeferGroup);
        hlayout->addWidget(mDeferTimeLabel);

        mDeferChangeButton = new QPushButton(i18nc("@action:button", "Change..."), mDeferGroup);
        connect(mDeferChangeButton, &QPushButton::clicked, this, &EditAlarmDlg::slotEditDeferral);
        mDeferChangeButton->setWhatsThis(i18nc("@info:whatsthis", "Change the alarm's deferred time, or cancel the deferral"));
        hlayout->addWidget(mDeferChangeButton);
    }

    auto hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addLayout(hlayout);

    // Date and time entry
    if (mTemplate)
    {
        QGroupBox* templateTimeBox = new QGroupBox(i18nc("@title:group", "Time"), mainPage);
        topLayout->addWidget(templateTimeBox);
        auto grid = new QGridLayout(templateTimeBox);
        mTemplateTimeGroup = new ButtonGroup(templateTimeBox);
        connect(mTemplateTimeGroup, &ButtonGroup::buttonSet, this, &EditAlarmDlg::slotTemplateTimeType);
        connect(mTemplateTimeGroup, &ButtonGroup::buttonSet, this, &EditAlarmDlg::contentsChanged);

        mTemplateDefaultTime = new RadioButton(i18nc("@option:radio", "Default time"), templateTimeBox);
        mTemplateDefaultTime->setReadOnly(mReadOnly);
        mTemplateDefaultTime->setWhatsThis(i18nc("@info:whatsthis", "Do not specify a start time for alarms based on this template. "
                                                "The normal default start time will be used."));
        mTemplateTimeGroup->addButton(mTemplateDefaultTime);
        grid->addWidget(mTemplateDefaultTime, 0, 0, Qt::AlignLeft);

        QWidget* box = new QWidget(templateTimeBox);
        auto layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        mTemplateUseTime = new RadioButton(i18nc("@option:radio", "Time:"), box);
        mTemplateUseTime->setReadOnly(mReadOnly);
        mTemplateUseTime->setWhatsThis(i18nc("@info:whatsthis", "Specify a start time for alarms based on this template."));
        layout->addWidget(mTemplateUseTime);
        mTemplateTimeGroup->addButton(mTemplateUseTime);
        mTemplateTime = new TimeEdit();
        mTemplateTime->setReadOnly(mReadOnly);
        mTemplateTime->setWhatsThis(xi18nc("@info:whatsthis",
              "<para>Enter the start time for alarms based on this template.</para><para>%1</para>",
              TimeSpinBox::shiftWhatsThis()));
        connect(mTemplateTime, &TimeEdit::valueChanged, this, &EditAlarmDlg::contentsChanged);
        layout->addWidget(mTemplateTime);
        layout->addStretch(1);
        grid->addWidget(box, 0, 1, Qt::AlignLeft);

        mTemplateAnyTime = new RadioButton(i18nc("@option:radio", "Date only"), templateTimeBox);
        mTemplateAnyTime->setReadOnly(mReadOnly);
        mTemplateAnyTime->setWhatsThis(xi18nc("@info:whatsthis", "Set the <interface>Any time</interface> option for alarms based on this template."));
        mTemplateTimeGroup->addButton(mTemplateAnyTime);
        grid->addWidget(mTemplateAnyTime, 1, 0, Qt::AlignLeft);

        box = new QWidget(templateTimeBox);
        layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        mTemplateUseTimeAfter = new RadioButton(i18nc("@option:radio", "Time from now:"), box);
        mTemplateUseTimeAfter->setReadOnly(mReadOnly);
        mTemplateUseTimeAfter->setWhatsThis(i18nc("@info:whatsthis",
                                                  "Set alarms based on this template to start after the specified time "
                                                 "interval from when the alarm is created."));
        layout->addWidget(mTemplateUseTimeAfter);
        mTemplateTimeGroup->addButton(mTemplateUseTimeAfter);
        mTemplateTimeAfter = new TimeSpinBox(1, maxDelayTime);
        mTemplateTimeAfter->setValue(1439);
        mTemplateTimeAfter->setReadOnly(mReadOnly);
        connect(mTemplateTimeAfter, &TimeSpinBox::valueChanged, this, &EditAlarmDlg::contentsChanged);
        mTemplateTimeAfter->setWhatsThis(xi18nc("@info:whatsthis", "<para>%1</para><para>%2</para>",
                                               AlarmTimeWidget::i18n_TimeAfterPeriod(), TimeSpinBox::shiftWhatsThis()));
        layout->addWidget(mTemplateTimeAfter);
        grid->addWidget(box, 1, 1, Qt::AlignLeft);

        hlayout->addStretch();
    }
    else
    {
        mTimeWidget = new AlarmTimeWidget(i18nc("@title:group", "Time"), AlarmTimeWidget::AT_TIME, mainPage);
        connect(mTimeWidget, &AlarmTimeWidget::dateOnlyToggled, this, &EditAlarmDlg::slotAnyTimeToggled);
        connect(mTimeWidget, &AlarmTimeWidget::changed, this, &EditAlarmDlg::contentsChanged);
        topLayout->addWidget(mTimeWidget);
    }

    // Optional controls depending on More/Fewer Options button
    mMoreOptions = new QFrame(mainPage);
    mMoreOptions->setFrameStyle(QFrame::NoFrame);
    topLayout->addWidget(mMoreOptions);
    auto moreLayout = new QVBoxLayout(mMoreOptions);
    moreLayout->setContentsMargins(0, 0, 0, 0);

    // Reminder
    mReminder = createReminder(mMoreOptions);
    if (mReminder)
    {
        connect(mReminder, &Reminder::changed, this, &EditAlarmDlg::contentsChanged);
        moreLayout->addWidget(mReminder, 0, Qt::AlignLeft);
        if (mTimeWidget)
            connect(mTimeWidget, &AlarmTimeWidget::changed, mReminder, &Reminder::setDefaultUnits);
    }

    // Late cancel selector - default = allow late display
    mLateCancel = new LateCancelSelector(true, mMoreOptions);
    connect(mLateCancel, &LateCancelSelector::changed, this, &EditAlarmDlg::contentsChanged);
    moreLayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

    PackedLayout* playout = new PackedLayout(Qt::AlignJustify);
    playout->setHorizontalSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    playout->setVerticalSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    moreLayout->addLayout(playout);

    // Acknowledgement confirmation required - default = no confirmation
    CheckBox* confirmAck = type_createConfirmAckCheckbox(mMoreOptions);
    if (confirmAck)
    {
        connect(confirmAck, &CheckBox::toggled, this, &EditAlarmDlg::contentsChanged);
        playout->addWidget(confirmAck);
    }

    if (theApp()->korganizerEnabled())
    {
        // Show in KOrganizer checkbox
        mShowInKorganizer = new CheckBox(i18n_chk_ShowInKOrganizer(), mMoreOptions);
        connect(mShowInKorganizer, &CheckBox::toggled, this, &EditAlarmDlg::contentsChanged);
        mShowInKorganizer->setWhatsThis(i18nc("@info:whatsthis", "Check to copy the alarm into KOrganizer's calendar"));
        playout->addWidget(mShowInKorganizer);
    }

    mainLayout->addWidget(mButtonBox);

    // Hide optional controls
    KConfigGroup config(KSharedConfig::openConfig(), EDIT_MORE_GROUP);
    showOptions(config.readEntry(EDIT_MORE_KEY, false));

    // Initialise the state of all controls according to the specified event, if any
    initValues(event);
    if (mName)
        mName->setFocus();

    if (!mNewAlarm)
    {
        // Save the initial state of all controls so that we can later tell if they have changed
        saveState((event.isValid() && (mTemplate || !event.isTemplate())) ? &event : nullptr);
        contentsChanged();    // enable/disable OK button
    }

    // Note the current desktop so that the dialog can be shown on it.
    // If a main window is visible, the dialog will by KDE default always appear on its
    // desktop. If the user invokes the dialog via the system tray on a different desktop,
    // that can cause confusion.
    mDesktop = KWindowSystem::currentDesktop();

    if (theApp()->windowFocusBroken())
    {
        const QList<QWidget*> children = findChildren<QWidget*>();
        for (QWidget* w : children)
            w->installEventFilter(this);
    }
}

EditAlarmDlg::~EditAlarmDlg()
{
    delete mButtonBox;
    mButtonBox = nullptr;    // prevent text edit contentsChanged() signal triggering a crash
    delete mSavedEvent;
    mWindowList.removeAll(this);
}

/******************************************************************************
* Return the number of instances.
*/
int EditAlarmDlg::instanceCount()
{
    return mWindowList.count();
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditAlarmDlg::initValues(const KAEvent& event)
{
    setReadOnly(mDesiredReadOnly);

    mChanged           = false;
    mOnlyDeferred      = false;
    mExpiredRecurrence = false;
    mLateCancel->showAutoClose(false);
    bool deferGroupVisible = false;
    if (event.isValid())
    {
        // Set the values to those for the specified event
        if (mName)
            mName->setText(event.name());
        bool recurs = event.recurs();
        if ((recurs || event.repetition())  &&  !mTemplate  &&  event.deferred())
        {
            deferGroupVisible = true;
            mDeferDateTime = event.deferDateTime();
            mDeferTimeLabel->setText(mDeferDateTime.formatLocale());
            mDeferGroup->show();
        }
        if (mTemplate)
        {
            // Editing a template
            int afterTime = event.isTemplate() ? event.templateAfterTime() : -1;
            bool noTime   = !afterTime;
            bool useTime  = !event.mainDateTime().isDateOnly();
            RadioButton* button = noTime          ? mTemplateDefaultTime :
                                  (afterTime > 0) ? mTemplateUseTimeAfter :
                                  useTime         ? mTemplateUseTime : mTemplateAnyTime;
            button->setChecked(true);
            mTemplateTimeAfter->setValue(afterTime > 0 ? afterTime : 1);
            if (!noTime && useTime)
                mTemplateTime->setValue(event.mainDateTime().kDateTime().time());
            else
                mTemplateTime->setValue(0);
        }
        else
        {
            if (event.isTemplate())
            {
                // Initialising from an alarm template: use current date
                const KADateTime now = KADateTime::currentDateTime(Preferences::timeSpec());
                int afterTime = event.templateAfterTime();
                if (afterTime >= 0)
                {
                    mTimeWidget->setDateTime(now.addSecs(afterTime * 60));
                    mTimeWidget->selectTimeFromNow();
                }
                else
                {
                    KADateTime dt = event.startDateTime().kDateTime();
                    dt.setTimeSpec(Preferences::timeSpec());
                    QDate d = now.date();
                    if (!dt.isDateOnly()  &&  now.time() >= dt.time())
                        d = d.addDays(1);     // alarm time has already passed, so use tomorrow
                    dt.setDate(d);
                    mTimeWidget->setDateTime(dt);
                }
            }
            else
            {
                mExpiredRecurrence = recurs && event.mainExpired();
                mTimeWidget->setDateTime(recurs || event.category() == CalEvent::ARCHIVED ? event.startDateTime()
                                         : event.mainExpired() ? event.deferDateTime() : event.mainDateTime());
            }
        }

        KAEvent::SubAction action = event.actionSubType();
        AlarmText altext;
        if (event.commandScript())
            altext.setScript(event.cleanText());
        else
            altext.setText(event.cleanText());
        setAction(action, altext);

        mLateCancel->setMinutes(event.lateCancel(), event.startDateTime().isDateOnly(),
                                TimePeriod::HoursMinutes);
        if (mShowInKorganizer)
            mShowInKorganizer->setChecked(event.copyToKOrganizer());
        type_initValues(event);
        mRecurrenceEdit->set(event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
        mTabs->setTabText(mRecurPageIndex, recurText(event));
    }
    else
    {
        // Set the values to their defaults
        const KADateTime defaultTime = KADateTime::currentUtcDateTime().addSecs(60).toTimeSpec(Preferences::timeSpec());
        if (mTemplate)
        {
            mTemplateDefaultTime->setChecked(true);
            mTemplateTime->setValue(0);
            mTemplateTimeAfter->setValue(1);
        }
        else
            mTimeWidget->setDateTime(defaultTime);
        mLateCancel->setMinutes((Preferences::defaultLateCancel() ? 1 : 0), false, TimePeriod::HoursMinutes);
        if (mShowInKorganizer)
            mShowInKorganizer->setChecked(Preferences::defaultCopyToKOrganizer());
        type_initValues(KAEvent());
        mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
        slotRecurFrequencyChange();      // update the Recurrence text
    }
    if (mReminder  &&  mTimeWidget)
        mReminder->setDefaultUnits(mTimeWidget->getDateTime(nullptr, false, false));

    if (!deferGroupVisible  &&  mDeferGroup)
        mDeferGroup->hide();

    bool empty = ResourcesCalendar::events(CalEvent::TEMPLATE).isEmpty();
    if (mLoadTemplateButton)
        mLoadTemplateButton->setEnabled(!empty);
}

/******************************************************************************
* Initialise various values in the New Alarm dialogue.
*/
void EditAlarmDlg::setName(const QString& name)
{
    if (mName)
        mName->setText(name);
}
void EditAlarmDlg::setTime(const DateTime& start)
{
    mTimeWidget->setDateTime(start);
    mTimeWidget->selectTimeFromNow(-1);    // select 'At date/time' option
}
KADateTime EditAlarmDlg::time() const
{
    return mTimeWidget->getDateTime(nullptr, false, false);
}
void EditAlarmDlg::setRecurrence(const KARecurrence& recur, const KCalendarCore::Duration& subRepeatInterval, int subRepeatCount)
{
    KAEvent event;
    event.setTime(mTimeWidget->getDateTime(nullptr, false, false));
    event.setRecurrence(recur);
    event.setRepetition(Repetition(subRepeatInterval, subRepeatCount - 1));
    mRecurrenceEdit->set(event);
}
void EditAlarmDlg::setRepeatAtLogin()
{
    mRecurrenceEdit->setRepeatAtLogin();
}
void EditAlarmDlg::setLateCancel(int minutes)
{
    mLateCancel->setMinutes(minutes, mTimeWidget->getDateTime(nullptr, false, false).isDateOnly(),
                            TimePeriod::HoursMinutes);
}
void EditAlarmDlg::setShowInKOrganizer(bool show)
{
    mShowInKorganizer->setChecked(show);
}

/******************************************************************************
* Set the read-only status of all non-template controls.
*/
void EditAlarmDlg::setReadOnly(bool readOnly)
{
    mReadOnly = readOnly;

    if (mTimeWidget)
        mTimeWidget->setReadOnly(readOnly);
    mLateCancel->setReadOnly(readOnly);
    if (mDeferChangeButton)
    {
        if (readOnly)
            mDeferChangeButton->hide();
        else
            mDeferChangeButton->show();
    }
    if (mShowInKorganizer)
        mShowInKorganizer->setReadOnly(readOnly);
}

/******************************************************************************
* Save the state of all controls.
*/
void EditAlarmDlg::saveState(const KAEvent* event)
{
    delete mSavedEvent;
    mSavedEvent = nullptr;
    if (event)
        mSavedEvent = new KAEvent(*event);
    if (mName)
        mSavedName = mName->text();
    if (mTemplate)
    {
        mSavedTemplateTimeType  = mTemplateTimeGroup->checkedButton();
        mSavedTemplateTime      = mTemplateTime->time();
        mSavedTemplateAfterTime = mTemplateTimeAfter->value();
    }
    checkText(mSavedTextFileCommandMessage, false);
    if (mTimeWidget)
        mSavedDateTime = mTimeWidget->getDateTime(nullptr, false, false);
    mSavedLateCancel       = mLateCancel->minutes();
    if (mShowInKorganizer)
        mSavedShowInKorganizer = mShowInKorganizer->isChecked();
    mSavedRecurrenceType   = mRecurrenceEdit->repeatType();
    mSavedDeferTime        = mDeferDateTime.kDateTime();
}

/******************************************************************************
* Check whether any of the controls has changed state since the dialog was
* first displayed.
* Reply = true if any non-deferral controls have changed, or if it's a new event.
*       = false if no non-deferral controls have changed. In this case,
*         mOnlyDeferred indicates whether deferral controls may have changed.
*/
bool EditAlarmDlg::stateChanged() const
{
    mChanged      = true;
    mOnlyDeferred = false;
    if (!mSavedEvent)
        return true;
    if (mName  &&  mSavedName != mName->text())
        return true;
    QString textFileCommandMessage;
    checkText(textFileCommandMessage, false);
    if (mTemplate)
    {
        if (mSavedTemplateTimeType != mTemplateTimeGroup->checkedButton()
        ||  (mTemplateUseTime->isChecked()  &&  mSavedTemplateTime != mTemplateTime->time())
        ||  (mTemplateUseTimeAfter->isChecked()  &&  mSavedTemplateAfterTime != mTemplateTimeAfter->value()))
            return true;
    }
    else
    {
        const KADateTime dt = mTimeWidget->getDateTime(nullptr, false, false);
        if (mSavedDateTime.timeSpec() != dt.timeSpec()  ||  mSavedDateTime != dt)
            return true;
    }
    if (mSavedLateCancel       != mLateCancel->minutes()
    ||  (mShowInKorganizer && mSavedShowInKorganizer != mShowInKorganizer->isChecked())
    ||  textFileCommandMessage != mSavedTextFileCommandMessage
    ||  mSavedRecurrenceType   != mRecurrenceEdit->repeatType())
        return true;
    if (type_stateChanged())
        return true;
    if (mRecurrenceEdit->stateChanged())
        return true;
    if (mSavedEvent  &&  mSavedEvent->deferred())
        mOnlyDeferred = true;
    mChanged = false;
    return false;
}

/******************************************************************************
* Called whenever any of the controls changes state.
* Enable or disable the OK button depending on whether any controls have a
* different state from their initial state.
*/
void EditAlarmDlg::contentsChanged()
{
    // Don't do anything if it's a new alarm or we're still initialising
    // (i.e. mSavedEvent null).
    if (mSavedEvent  &&  mButtonBox  &&  mButtonBox->button(QDialogButtonBox::Ok))
        mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(stateChanged() || mDeferDateTime.kDateTime() != mSavedDeferTime);
}

/******************************************************************************
* Get the currently entered dialog data.
* The data is returned in the supplied KAEvent instance.
* Reply = false if the only change has been to an existing deferral.
*/
bool EditAlarmDlg::getEvent(KAEvent& event, Resource& resource)
{
    resource = mResource;
    if (mChanged)
    {
        // It's a new event, or the edit controls have changed
        setEvent(event, mAlarmMessage, false);
        return true;
    }

    // Only the deferral time may have changed
    event = *mSavedEvent;
    if (mOnlyDeferred)
    {
        // Just modify the original event, to avoid expired recurring events
        // being returned as rubbish.
        if (mDeferDateTime.isValid())
            event.defer(mDeferDateTime, event.reminderDeferral(), false);
        else
            event.cancelDefer();
    }
    return false;
}

/******************************************************************************
* Extract the data in the dialog and set up a KAEvent from it.
* If 'trial' is true, the event is set up for a simple one-off test, ignoring
* recurrence, reminder, template etc. data.
*/
void EditAlarmDlg::setEvent(KAEvent& event, const QString& text, bool trial)
{
    KADateTime dt;
    if (!trial)
    {
        if (!mTemplate)
            dt = mAlarmDateTime.effectiveKDateTime();
        else if (mTemplateUseTime->isChecked())
            dt = KADateTime(QDate(2000,1,1), mTemplateTime->time());
    }

    int lateCancel = (trial || !mLateCancel->isEnabled()) ? 0 : mLateCancel->minutes();
    type_setEvent(event, dt, (mName ? mName->text() : QString()), text, lateCancel, trial);

    if (!trial)
    {
        if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
        {
            mRecurrenceEdit->updateEvent(event, !mTemplate);
            const KADateTime now = KADateTime::currentDateTime(mAlarmDateTime.timeSpec());
            bool dateOnly = mAlarmDateTime.isDateOnly();
            if ((dateOnly  &&  mAlarmDateTime.date() < now.date())
            ||  (!dateOnly  &&  mAlarmDateTime.kDateTime() < now))
            {
                // A timed recurrence has an entered start date which has
                // already expired, so we must adjust the next repetition.
                event.setNextOccurrence(now);
            }
            mAlarmDateTime = event.startDateTime();
            if (mDeferDateTime.isValid()  &&  mDeferDateTime < mAlarmDateTime)
            {
                bool deferral = true;
                bool deferReminder = false;
                int reminder = mReminder ? mReminder->minutes() : 0;
                if (reminder)
                {
                    DateTime remindTime = mAlarmDateTime.addMins(-reminder);
                    if (mDeferDateTime >= remindTime)
                    {
                        if (remindTime > KADateTime::currentUtcDateTime())
                            deferral = false;    // ignore deferral if it's after next reminder
                        else if (mDeferDateTime > remindTime)
                            deferReminder = true;    // it's the reminder which is being deferred
                    }
                }
                if (deferral)
                    event.defer(mDeferDateTime, deferReminder, false);
            }
        }
        if (mTemplate)
        {
            int afterTime = mTemplateDefaultTime->isChecked() ? 0
                          : mTemplateUseTimeAfter->isChecked() ? mTemplateTimeAfter->value() : -1;
            event.setTemplate(mName->text(), afterTime);
        }
    }
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditAlarmDlg::getAlarmFlags() const
{
    KAEvent::Flags flags{};
    if (mShowInKorganizer && mShowInKorganizer->isEnabled() && mShowInKorganizer->isChecked())
        flags |= KAEvent::COPY_KORGANIZER;
    if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
        flags |= KAEvent::REPEAT_AT_LOGIN;
    if (mTemplate ? mTemplateAnyTime->isChecked() : mAlarmDateTime.isDateOnly())
        flags |= KAEvent::ANY_TIME;
    return flags;
}

/******************************************************************************
* Called when the dialog is displayed.
* The first time through, sets the size to the same as the last time it was
* displayed.
*/
void EditAlarmDlg::showEvent(QShowEvent* se)
{
    QDialog::showEvent(se);
    if (!mDeferGroupHeight)
    {
        if (mDeferGroup)
            mDeferGroupHeight = mDeferGroup->height() + style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
        QSize s;
        if (Config::readWindowSize(mTemplate ? TEMPLATE_DIALOG_NAME : EDIT_DIALOG_NAME, s))
        {
            bool defer = mDeferGroup && !mDeferGroup->isHidden();
            s.setHeight(s.height() + (defer ? mDeferGroupHeight : 0));
            if (!defer)
                mTabScrollGroup->setSized();
            resize(s);
        }
    }
    slotResize();
    KWindowSystem::setOnDesktop(winId(), mDesktop);    // ensure it displays on the desktop expected by the user

    if (theApp()->needWindowFocusFix())
    {
        QApplication::setActiveWindow(this);
        QTimer::singleShot(0, this, &EditAlarmDlg::focusFixTimer);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* Called when the window is first shown, to ensure that it initially becomes
* the active window.
* This is only required on Ubuntu's Unity desktop, which doesn't transfer
* keyboard focus properly between Edit Alarm Dialog windows and MessageWindow
* windows.
*/
void EditAlarmDlg::focusFixTimer()
{
    if (theApp()->needWindowFocusFix()
    &&  QApplication::focusWidget()->window() != this)
    {
        QApplication::setActiveWindow(this);
        QTimer::singleShot(0, this, &EditAlarmDlg::focusFixTimer);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* Called to detect when the mouse is pressed anywhere inside the window.
* Activates this window if a MessageWindow window is also active.
* This is only required on Ubuntu's Unity desktop, which doesn't transfer
* keyboard focus properly between Edit Alarm Dialog windows and MessageWindow
* windows.
*/
bool EditAlarmDlg::eventFilter(QObject*, QEvent* e)
{
    if (theApp()->needWindowFocusFix())
    {
        if (e->type() == QEvent::MouseButtonPress)
            QApplication::setActiveWindow(this);
    }
    return false;
}

/******************************************************************************
* Called when the dialog is closed.
*/
void EditAlarmDlg::closeEvent(QCloseEvent* ce)
{
    Q_EMIT rejected();
    QDialog::closeEvent(ce);
}

/******************************************************************************
* Update the tab sizes (again) and if the resized dialog height is greater
* than the minimum, resize it again. This is necessary because (a) resizing
* tabs doesn't always work properly the first time, and (b) resizing to the
* minimum size hint doesn't always work either.
*/
void EditAlarmDlg::slotResize()
{
    mTabScrollGroup->adjustSize(true);
    const QSize s = minimumSizeHint();
    if (height() > s.height())
    {
        // Resize to slightly greater than the minimum height.
        // This is for some unknown reason necessary, since
        // sometimes resizing to the minimum height fails.
        resize(s.width(), s.height() + 2);
    }
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size (adjusted to ignore the optional height of the deferred
* time edit widget) in the config file.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible() && mDeferGroupHeight)
    {
        QSize s = re->size();
        s.setHeight(s.height() - (!mDeferGroup || mDeferGroup->isHidden() ? 0 : mDeferGroupHeight));
        Config::writeWindowSize(mTemplate ? TEMPLATE_DIALOG_NAME : EDIT_DIALOG_NAME, s);
    }
    QDialog::resizeEvent(re);
}

/******************************************************************************
* Called when any button is clicked.
*/
void EditAlarmDlg::slotButtonClicked(QAbstractButton* button)
{
    if (button == mTryButton)
        slotTry();
    else if (button == mLoadTemplateButton)
        slotHelp();
    else if (button == mMoreLessButton)
        slotDefault();
    else if (button == mButtonBox->button(QDialogButtonBox::Ok))
    {
        if (validate())
            accept();
    }
    else
        reject();
}

/******************************************************************************
* Called when the OK button is clicked.
* Validate the input data.
*/
bool EditAlarmDlg::validate()
{
    if (!stateChanged())
    {
        // No changes have been made except possibly to an existing deferral
        if (!mOnlyDeferred)
            reject();
        return mOnlyDeferred;
    }
    RecurrenceEdit::RepeatType recurType = mRecurrenceEdit->repeatType();
    if (mTimeWidget
    &&  mTabs->currentIndex() == mRecurPageIndex  &&  recurType == RecurrenceEdit::AT_LOGIN)
        mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
    bool timedRecurrence = mRecurrenceEdit->isTimedRepeatType();    // does it recur other than at login?
    if (mTemplate)
    {
        // Check that the template name is not blank and is unique
        QString errmsg;
        const QString name = mName->text();
        if (name.isEmpty())
            errmsg = i18nc("@info", "You must enter a name for the alarm template");
        else if (name != mSavedName)
        {
            if (ResourcesCalendar::templateEvent(name).isValid())
                errmsg = i18nc("@info", "Template name is already in use");
        }
        if (!errmsg.isEmpty())
        {
            mName->setFocus();
            KAMessageBox::error(this, errmsg);
            return false;
        }
    }
    else if (mTimeWidget)
    {
        QWidget* errWidget;
        mAlarmDateTime = mTimeWidget->getDateTime(nullptr, !timedRecurrence, false, &errWidget);
        if (errWidget)
        {
            // It's more than just an existing deferral being changed, so the time matters
            mTabs->setCurrentIndex(mMainPageIndex);
            errWidget->setFocus();
            mTimeWidget->getDateTime();   // display the error message now
            return false;
        }
    }
    if (!type_validate(false))
        return false;

    if (!mTemplate)
    {
        if (mChanged  &&  mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
        {
            // Check whether the start date/time must be adjusted
            // to match the recurrence specification.
            DateTime dt = mAlarmDateTime;   // setEvent() changes mAlarmDateTime
            KAEvent event;
            setEvent(event, mAlarmMessage, false);
            mAlarmDateTime = dt;   // restore
            KADateTime pre = dt.effectiveKDateTime();
            bool dateOnly = dt.isDateOnly();
            if (dateOnly)
                pre = pre.addDays(-1);
            else
                pre = pre.addSecs(-1);
            DateTime next;
            event.nextOccurrence(pre, next, KAEvent::IGNORE_REPETITION);
            if (next != dt)
            {
                QString prompt = dateOnly ? i18nc("@info The parameter is a date value",
                                                  "The start date does not match the alarm's recurrence pattern, "
                                                  "so it will be adjusted to the date of the next recurrence (%1).",
                                                  QLocale().toString(next.date(), QLocale::ShortFormat))
                                          : i18nc("@info The parameter is a date/time value",
                                                  "The start date/time does not match the alarm's recurrence pattern, "
                                                  "so it will be adjusted to the date/time of the next recurrence (%1).",
                                                  QLocale().toString(next.effectiveDateTime(), QLocale::ShortFormat));
                if (KAMessageBox::warningContinueCancel(this, prompt) != KMessageBox::Continue)
                    return false;
            }
        }

        if (timedRecurrence)
        {
            KAEvent event;
            Resource res;
            getEvent(event, res);     // this may adjust mAlarmDateTime
            const KADateTime now = KADateTime::currentDateTime(mAlarmDateTime.timeSpec());
            bool dateOnly = mAlarmDateTime.isDateOnly();
            if ((dateOnly  &&  mAlarmDateTime.date() < now.date())
            ||  (!dateOnly  &&  mAlarmDateTime.kDateTime() < now))
            {
                // A timed recurrence has an entered start date which
                // has already expired, so we must adjust it.
                if (event.nextOccurrence(now, mAlarmDateTime, KAEvent::ALLOW_FOR_REPETITION) == KAEvent::NO_OCCURRENCE)
                {
                    KAMessageBox::error(this, i18nc("@info", "Recurrence has already expired"));
                    return false;
                }
                if (event.workTimeOnly()  &&  !event.nextTrigger(KAEvent::DISPLAY_TRIGGER).isValid())
                {
                    if (KAMessageBox::warningContinueCancel(this, i18nc("@info", "The alarm will never occur during working hours"))
                        != KMessageBox::Continue)
                        return false;
                }
            }
        }
        QString errmsg;
        QWidget* errWidget = mRecurrenceEdit->checkData(mAlarmDateTime.effectiveKDateTime(), errmsg);
        if (errWidget)
        {
            mTabs->setCurrentIndex(mRecurPageIndex);
            errWidget->setFocus();
            KAMessageBox::error(this, errmsg);
            return false;
        }
    }
    if (recurType != RecurrenceEdit::NO_RECUR)
    {
        KAEvent recurEvent;
        int longestRecurMinutes = -1;
        int reminder = mReminder ? mReminder->minutes() : 0;
        if (reminder  &&  !mReminder->isOnceOnly())
        {
            mRecurrenceEdit->updateEvent(recurEvent, false);
            longestRecurMinutes = recurEvent.longestRecurrenceInterval().asSeconds() / 60;
            if (longestRecurMinutes  &&  reminder >= longestRecurMinutes)
            {
                mTabs->setCurrentIndex(mMainPageIndex);
                mReminder->setFocusOnCount();
                KAMessageBox::error(this, xi18nc("@info", "Reminder period must be less than the recurrence interval, unless <interface>%1</interface> is checked.",
                                                Reminder::i18n_chk_FirstRecurrenceOnly()));
                return false;
            }
        }
        if (mRecurrenceEdit->subRepetition())
        {
            if (longestRecurMinutes < 0)
            {
                mRecurrenceEdit->updateEvent(recurEvent, false);
                longestRecurMinutes = recurEvent.longestRecurrenceInterval().asSeconds() / 60;
            }
            if (longestRecurMinutes > 0
            &&  recurEvent.repetition().intervalMinutes() * recurEvent.repetition().count() >= longestRecurMinutes - reminder)
            {
                KAMessageBox::error(this, i18nc("@info", "The duration of a repetition within the recurrence must be less than the recurrence interval minus any reminder period"));
                mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
                return false;
            }
            if (!recurEvent.repetition().isDaily()
            &&  ((mTemplate && mTemplateAnyTime->isChecked())  ||  (!mTemplate && mAlarmDateTime.isDateOnly())))
            {
                KAMessageBox::error(this, i18nc("@info", "For a repetition within the recurrence, its period must be in units of days or weeks for a date-only alarm"));
                mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
                return false;
            }
        }
    }
    if (!checkText(mAlarmMessage))
        return false;

    mResource = Resource();
    // mUseResourceEventId = false indicates that the caller already knows
    // which resource to use.
    if (mUseResourceEventId)
    {
        if (!mResourceEventId.isEmpty())
        {
            mResource = Resources::resourceForEvent(mResourceEventId);
            if (mResource.isValid())
            {
                CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
                if (!(mResource.alarmTypes() & type))
                    mResource = Resource();   // event may have expired while dialog was open
            }
        }
        bool cancelled = false;
        CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
        if (!mResource.isWritable(type))
            mResource = Resources::destination(type, this, Resources::NoDestOption, &cancelled);
        if (!mResource.isValid())
        {
            if (!cancelled)
                KAMessageBox::error(this, i18nc("@info", "You must select a calendar to save the alarm in"));
            return false;
        }
    }
    return true;
}

/******************************************************************************
* Called when the Try button is clicked.
* Display/execute the alarm immediately for the user to check its configuration.
*/
void EditAlarmDlg::slotTry()
{
    QString text;
    if (checkText(text))
    {
        if (!type_validate(true))
            return;
        KAEvent event;
        setEvent(event, text, true);
        if (!mNewAlarm  &&  !stateChanged())
        {
            // It's an existing alarm which hasn't been changed yet:
            // enable KALARM_UID environment variable to be set.
            event.setEventId(mEventId);
        }
        type_aboutToTry();
        void* result = theApp()->execAlarm(event, event.firstAlarm(), KAlarmApp::NoRecordCmdError | KAlarmApp::NoNotifyInhibit);
        type_executedTry(text, result);
    }
}

/******************************************************************************
* Called when the Load Template button is clicked.
* Prompt to select a template and initialise the dialog with its contents.
*/
void EditAlarmDlg::slotHelp()
{
    KAEvent::Actions type;
    switch (mAlarmType)
    {
        case KAEvent::FILE:
        case KAEvent::MESSAGE:  type = KAEvent::ACT_DISPLAY;  break;
        case KAEvent::COMMAND:  type = KAEvent::ACT_COMMAND;  break;
        case KAEvent::EMAIL:    type = KAEvent::ACT_EMAIL;  break;
        case KAEvent::AUDIO:    type = KAEvent::ACT_AUDIO;  break;
        default:
            return;
    }
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of EditAlarmDlg, and on return from this function).
    AutoQPointer<TemplatePickDlg> dlg = new TemplatePickDlg(type, this);
    if (dlg->exec() == QDialog::Accepted)
    {
        KAEvent event = dlg->selectedTemplate();
        initValues(event);
    }
}

/******************************************************************************
* Called when the More Options or Fewer Options buttons are clicked.
* Show/hide the optional options and swap the More/Less buttons, and save the
* new setting as the default from now on.
*/
void EditAlarmDlg::slotDefault()
{
    showOptions(!mShowingMore);
    KConfigGroup config(KSharedConfig::openConfig(), EDIT_MORE_GROUP);
    config.writeEntry(EDIT_MORE_KEY, mShowingMore);
}

/******************************************************************************
* Show/hide the optional options and swap the More/Less buttons.
*/
void EditAlarmDlg::showOptions(bool more)
{
    qCDebug(KALARM_LOG) << "EditAlarmDlg::showOptions:" << (more ? "More" : "Less");
    if (more)
    {
        mMoreOptions->show();
        mMoreLessButton->setText(i18nc("@action:Button", "Fewer Options <<"));
        mMoreLessButton->setToolTip(i18nc("@info:tooltip", "Show fewer options"));
    }
    else
    {
        mMoreOptions->hide();
        mMoreLessButton->setText(i18nc("@action:button", "More Options >>"));
        mMoreLessButton->setToolTip(i18nc("@info:tooltip", "Show more options"));
    }
    if (mTimeWidget)
        mTimeWidget->showMoreOptions(more);
    type_showOptions(more);
    mRecurrenceEdit->showMoreOptions(more);
    mShowingMore = more;
    QTimer::singleShot(0, this, &EditAlarmDlg::slotResize);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Called when the Change deferral button is clicked.
*/
void EditAlarmDlg::slotEditDeferral()
{
    if (!mTimeWidget)
        return;
    bool limit = true;
    const Repetition repetition = mRecurrenceEdit->subRepetition();
    DateTime start = mSavedEvent->recurs() ? (mExpiredRecurrence ? DateTime() : mSavedEvent->mainDateTime())
                   : mTimeWidget->getDateTime(nullptr, !repetition, !mExpiredRecurrence);
    if (!start.isValid())
    {
        if (!mExpiredRecurrence)
            return;
        limit = false;
    }
    const KADateTime now = KADateTime::currentUtcDateTime();
    if (limit)
    {
        if (repetition  &&  start < now)
        {
            // Sub-repetition - find the time of the next one
            int repeatNum = repetition.isDaily()
                           ? (start.daysTo(now) + repetition.intervalDays() - 1) / repetition.intervalDays()
                           : (start.secsTo(now) + repetition.intervalSeconds() - 1) / repetition.intervalSeconds();
            if (repeatNum > repetition.count())
            {
                mTimeWidget->getDateTime();    // output the appropriate error message
                return;
            }
            start = KADateTime(repetition.duration(repeatNum).end(start.qDateTime()));
        }
    }

    bool deferred = mDeferDateTime.isValid();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of EditAlarmDlg, and on return from this function).
    AutoQPointer<DeferAlarmDlg> deferDlg = new DeferAlarmDlg((deferred ? mDeferDateTime : DateTime(now.addSecs(60).toTimeSpec(start.timeSpec()))),
                                                             start.isDateOnly(), deferred, this);
    deferDlg->setObjectName(QStringLiteral("EditDeferDlg"));    // used by LikeBack
    if (limit)
    {
        // Don't allow deferral past the next recurrence
        int reminder = mReminder ? mReminder->minutes() : 0;
        if (reminder)
        {
            DateTime remindTime = start.addMins(-reminder);
            if (KADateTime::currentUtcDateTime() < remindTime)
                start = remindTime;
        }
        deferDlg->setLimit(start.addSecs(-60));
    }
    if (deferDlg->exec() == QDialog::Accepted)
    {
        mDeferDateTime = deferDlg->getDateTime();
        mDeferTimeLabel->setText(mDeferDateTime.isValid() ? mDeferDateTime.formatLocale() : QString());
        contentsChanged();
    }
}

/******************************************************************************
* Called when the main page is shown.
* Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
    if (!mMainPageShown)
    {
        if (mName)
            mName->setFocus();
        mMainPageShown = true;
    }
    else
    {
        // Set scroll position to top, since it otherwise jumps randomly
        auto main = static_cast<StackedScrollWidget*>(mTabs->widget(0));
        main->verticalScrollBar()->setValue(0);
    }
    if (mTimeWidget)
    {
        if (!mReadOnly  &&  mRecurPageShown  &&  mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
            mTimeWidget->setDateTime(mRecurrenceEdit->endDateTime());
        if (mReadOnly  ||  mRecurrenceEdit->isTimedRepeatType())
            mTimeWidget->setMinDateTime();             // don't set a minimum date/time
        else
            mTimeWidget->setMinDateTimeIsCurrent();    // set the minimum date/time to track the clock
    }
}

/******************************************************************************
* Called when the recurrence edit page is shown.
* The recurrence defaults are set to correspond to the start date.
* The first time, for a new alarm, the recurrence end date is set according to
* the alarm start time.
*/
void EditAlarmDlg::slotShowRecurrenceEdit()
{
    mRecurPageIndex = mTabs->currentIndex();
    if (!mReadOnly  &&  !mTemplate)
    {
        mAlarmDateTime = mTimeWidget->getDateTime(nullptr, false, false);
        const KADateTime now = KADateTime::currentDateTime(mAlarmDateTime.timeSpec());
        bool expired = (mAlarmDateTime.effectiveKDateTime() < now);
        if (mRecurSetDefaultEndDate)
        {
            mRecurrenceEdit->setDefaultEndDate(expired ? now.date() : mAlarmDateTime.date());
            mRecurSetDefaultEndDate = false;
        }
        mRecurrenceEdit->setStartDate(mAlarmDateTime.date(), now.date());
        if (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN)
            mRecurrenceEdit->setEndDateTime(expired ? now : mAlarmDateTime.kDateTime());
    }
    mRecurPageShown = true;
}

/******************************************************************************
* Called when the recurrence type selection changes.
* Enables/disables date-only alarms as appropriate.
* Enables/disables controls depending on at-login setting.
*/
void EditAlarmDlg::slotRecurTypeChange(int repeatType)
{
    bool atLogin = (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN);
    if (!mTemplate)
    {
        bool recurs = (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR);
        if (mDeferGroup)
            mDeferGroup->setEnabled(recurs);
        mTimeWidget->enableAnyTime(!recurs || repeatType != RecurrenceEdit::SUBDAILY);
        if (atLogin)
        {
            mAlarmDateTime = mTimeWidget->getDateTime(nullptr, false, false);
            mRecurrenceEdit->setEndDateTime(mAlarmDateTime.kDateTime());
        }
        if (mReminder)
            mReminder->enableOnceOnly(recurs && !atLogin);
    }
    if (mReminder)
        mReminder->setAfterOnly(atLogin);
    mLateCancel->setEnabled(!atLogin);
    if (mShowInKorganizer)
        mShowInKorganizer->setEnabled(!atLogin);
    slotRecurFrequencyChange();
}

/******************************************************************************
* Called when the recurrence frequency selection changes, or the sub-
* repetition interval changes.
* Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
    slotSetSubRepetition();
    KAEvent event;
    mRecurrenceEdit->updateEvent(event, false);
    mTabs->setTabText(mRecurPageIndex, recurText(event));
}

/******************************************************************************
* Called when the Repetition within Recurrence button has been pressed to
* display the sub-repetition dialog.
* Alarm repetition has the following restrictions:
* 1) Not allowed for a repeat-at-login alarm
* 2) For a date-only alarm, the repeat interval must be a whole number of days.
* 3) The overall repeat duration must be less than the recurrence interval.
*/
void EditAlarmDlg::slotSetSubRepetition()
{
    bool dateOnly = mTemplate ? mTemplateAnyTime->isChecked() : mTimeWidget->anyTime();
    mRecurrenceEdit->setSubRepetition((mReminder ? mReminder->minutes() : 0), dateOnly);
}

/******************************************************************************
* Called when one of the template time radio buttons is clicked,
* to enable or disable the template time entry spin boxes.
*/
void EditAlarmDlg::slotTemplateTimeType(QAbstractButton*)
{
    mTemplateTime->setEnabled(mTemplateUseTime->isChecked());
    mTemplateTimeAfter->setEnabled(mTemplateUseTimeAfter->isChecked());
}

/******************************************************************************
* Called when the "Any time" checkbox is toggled in the date/time widget.
* Sets the advance reminder and late cancel units to days if any time is checked.
*/
void EditAlarmDlg::slotAnyTimeToggled(bool anyTime)
{
    if (mReminder  &&  mReminder->isReminder())
        mReminder->setDateOnly(anyTime);
    mLateCancel->setDateOnly(anyTime);
}

bool EditAlarmDlg::dateOnly() const
{
    return mTimeWidget ? mTimeWidget->anyTime() : mTemplateAnyTime->isChecked();
}

bool EditAlarmDlg::isTimedRecurrence() const
{
    return mRecurrenceEdit->isTimedRepeatType();
}

void EditAlarmDlg::showMainPage()
{
    mTabs->setCurrentIndex(mMainPageIndex);
}

#include "moc_editdlg.cpp"
#include "moc_editdlg_p.cpp"

// vim: et sw=4:
