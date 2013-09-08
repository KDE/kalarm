/*
 *  editdlg.cpp  -  dialog to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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
#include "editdlg.moc"
#include "editdlg_p.moc"
#include "editdlgtypes.h"

#include "alarmcalendar.h"
#ifdef USE_AKONADI
#include "collectionmodel.h"
#else
#include "alarmresources.h"
#endif
#include "alarmtimewidget.h"
#include "autoqpointer.h"
#include "buttongroup.h"
#include "checkbox.h"
#include "deferdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "latecancel.h"
#include "lineedit.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "packedlayout.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#include "reminder.h"
#include "shellprocess.h"
#include "spinbox.h"
#include "stackedwidgets.h"
#include "templatepickdlg.h"
#include "timeedit.h"
#include "timespinbox.h"

#include <libkdepim/misc/maillistdrag.h>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kfiledialog.h>
#include <kpushbutton.h>
#include <khbox.h>
#include <kvbox.h>
#include <kwindowsystem.h>
#include <kdebug.h>

#include <QLabel>
#include <QDir>
#include <QStyle>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QScrollBar>
#include <QTimer>

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
        r = QString::fromLatin1("%1 / %2").arg(event.recurrenceText()).arg(event.repetitionText());
    else
        r = event.recurrenceText();
    return i18nc("@title:tab", "Recurrence - [%1]", r);
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_chk_ShowInKOrganizer()   { return i18nc("@option:check", "Show in KOrganizer"); }


EditAlarmDlg* EditAlarmDlg::create(bool Template, Type type, QWidget* parent, GetResourceType getResource)
{
    kDebug();
    switch (type)
    {
        case DISPLAY:  return new EditDisplayAlarmDlg(Template, parent, getResource);
        case COMMAND:  return new EditCommandAlarmDlg(Template, parent, getResource);
        case EMAIL:    return new EditEmailAlarmDlg(Template, parent, getResource);
        case AUDIO:    return new EditAudioAlarmDlg(Template, parent, getResource);
        default:  break;
    }
    return 0;
}

EditAlarmDlg* EditAlarmDlg::create(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                                   GetResourceType getResource, bool readOnly)
{
    switch (event->actionTypes())
    {
        case KAEvent::ACT_COMMAND:  return new EditCommandAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_DISPLAY_COMMAND:
        case KAEvent::ACT_DISPLAY:  return new EditDisplayAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_EMAIL:    return new EditEmailAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        case KAEvent::ACT_AUDIO:    return new EditAudioAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
        default:
            break;
    }
    return 0;
}


/******************************************************************************
* Constructor.
* Parameters:
*   Template = true to edit/create an alarm template
*            = false to edit/create an alarm.
*   event   != to initialise the dialog to show the specified event's data.
*/
EditAlarmDlg::EditAlarmDlg(bool Template, KAEvent::SubAction action, QWidget* parent, GetResourceType getResource)
    : KDialog(parent),
      mAlarmType(action),
      mMainPageShown(false),
      mRecurPageShown(false),
      mRecurSetDefaultEndDate(true),
      mTemplateName(0),
      mDeferGroup(0),
      mDeferChangeButton(0),
      mTimeWidget(0),
      mShowInKorganizer(0),
#ifndef USE_AKONADI
      mResource(0),
#endif
      mDeferGroupHeight(0),
      mTemplate(Template),
      mNewAlarm(true),
      mDesiredReadOnly(false),
      mReadOnly(false),
      mShowingMore(true),
      mSavedEvent(0)
{
    init(0, getResource);
}

EditAlarmDlg::EditAlarmDlg(bool Template, const KAEvent* event, bool newAlarm, QWidget* parent,
                           GetResourceType getResource, bool readOnly)
    : KDialog(parent),
      mAlarmType(event->actionSubType()),
      mMainPageShown(false),
      mRecurPageShown(false),
      mRecurSetDefaultEndDate(true),
      mTemplateName(0),
      mDeferGroup(0),
      mDeferChangeButton(0),
      mTimeWidget(0),
      mShowInKorganizer(0),
#ifndef USE_AKONADI
      mResource(0),
#endif
      mDeferGroupHeight(0),
      mEventId(newAlarm ? QString() : event->id()),
      mTemplate(Template),
      mNewAlarm(newAlarm),
      mDesiredReadOnly(readOnly),
      mReadOnly(readOnly),
      mShowingMore(true),
      mSavedEvent(0)
{
    init(event, getResource);
}

void EditAlarmDlg::init(const KAEvent* event, GetResourceType getResource)
{
    switch (getResource)
    {
#ifdef USE_AKONADI
        case RES_USE_EVENT_ID:
            if (event)
            {
                mCollectionItemId = event->itemId();
                break;
            }
            // fall through to RES_PROMPT
        case RES_PROMPT:
            mCollectionItemId = -1;
            break;
        case RES_IGNORE:
        default:
            mCollectionItemId = -2;
            break;
#else
        case RES_USE_EVENT_ID:
            if (event)
            {
                mResourceEventId = event->id();
                break;
            }
            // fall through to RES_PROMPT
        case RES_PROMPT:
            mResourceEventId = QString("");   // empty but non-null
            break;
        case RES_IGNORE:
        default:
            mResourceEventId.clear();         // null
            break;
#endif
    }
}

void EditAlarmDlg::init(const KAEvent* event)
{
    setObjectName(mTemplate ? QLatin1String("TemplEditDlg") : QLatin1String("EditDlg"));    // used by LikeBack
    QString caption;
    if (mReadOnly)
        caption = mTemplate ? i18nc("@title:window", "Alarm Template [read-only]")
                : event->expired() ? i18nc("@title:window", "Archived Alarm [read-only]")
                                   : i18nc("@title:window", "Alarm [read-only]");
    else
        caption = type_caption();
    setCaption(caption);
    setButtons((mReadOnly ? Cancel|Try|Default : mTemplate ? Ok|Cancel|Try|Default : Ok|Cancel|Try|Help|Default));
    setDefaultButton(mReadOnly ? Cancel : Ok);
    setButtonText(Help, i18nc("@action:button", "Load Template..."));
    setButtonIcon(Help, KIcon());
    setButtonIcon(Default, KIcon());
    connect(this, SIGNAL(tryClicked()), SLOT(slotTry()));
    connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));  // More/Less Options button
    connect(this, SIGNAL(helpClicked()), SLOT(slotHelp()));  // Load Template button
    KVBox* mainWidget = new KVBox(this);
    mainWidget->setMargin(0);
    setMainWidget(mainWidget);
    if (mTemplate)
    {
        KHBox* box = new KHBox(mainWidget);
        box->setMargin(0);
        box->setSpacing(spacingHint());
        QLabel* label = new QLabel(i18nc("@label:textbox", "Template name:"), box);
        label->setFixedSize(label->sizeHint());
        mTemplateName = new KLineEdit(box);
        mTemplateName->setReadOnly(mReadOnly);
        connect(mTemplateName, SIGNAL(userTextChanged(QString)), SLOT(contentsChanged()));
        label->setBuddy(mTemplateName);
        box->setWhatsThis(i18nc("@info:whatsthis", "Enter the name of the alarm template"));
        box->setFixedHeight(box->sizeHint().height());
    }
    mTabs = new KTabWidget(mainWidget);
    mTabScrollGroup = new StackedScrollGroup(this, mTabs);

    StackedScrollWidget* mainScroll = new StackedScrollWidget(mTabScrollGroup);
    mTabs->addTab(mainScroll, i18nc("@title:tab", "Alarm"));
    mMainPageIndex = 0;
    PageFrame* mainPage = new PageFrame(mainScroll);
    mainScroll->setWidget(mainPage);   // mainPage becomes the child of mainScroll
    connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
    QVBoxLayout* topLayout = new QVBoxLayout(mainPage);
    topLayout->setMargin(marginHint());
    topLayout->setSpacing(spacingHint());

    // Recurrence tab
    StackedScrollWidget* recurScroll = new StackedScrollWidget(mTabScrollGroup);
    mTabs->addTab(recurScroll, QString());
    mRecurPageIndex = 1;
    KVBox* recurTab = new KVBox();
    recurTab->setMargin(marginHint());
    recurScroll->setWidget(recurTab);   // recurTab becomes the child of recurScroll
    mRecurrenceEdit = new RecurrenceEdit(mReadOnly, recurTab);
    connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
    connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));
    connect(mRecurrenceEdit, SIGNAL(frequencyChanged()), SLOT(slotRecurFrequencyChange()));
    connect(mRecurrenceEdit, SIGNAL(repeatNeedsInitialisation()), SLOT(slotSetSubRepetition()));
    connect(mRecurrenceEdit, SIGNAL(contentsChanged()), SLOT(contentsChanged()));

    // Controls specific to the alarm type
    QGroupBox* actionBox = new QGroupBox(i18nc("@title:group", "Action"), mainPage);
    topLayout->addWidget(actionBox, 1);
    QVBoxLayout* layout = new QVBoxLayout(actionBox);
    layout->setMargin(marginHint());
    layout->setSpacing(spacingHint());

    type_init(actionBox, layout);

    if (!mTemplate)
    {
        // Deferred date/time: visible only for a deferred recurring event.
        mDeferGroup = new QGroupBox(i18nc("@title:group", "Deferred Alarm"), mainPage);
        topLayout->addWidget(mDeferGroup);
        QHBoxLayout* hlayout = new QHBoxLayout(mDeferGroup);
        hlayout->setMargin(marginHint());
        hlayout->setSpacing(spacingHint());
        QLabel* label = new QLabel(i18nc("@label", "Deferred to:"), mDeferGroup);
        label->setFixedSize(label->sizeHint());
        hlayout->addWidget(label);
        mDeferTimeLabel = new QLabel(mDeferGroup);
        hlayout->addWidget(mDeferTimeLabel);

        mDeferChangeButton = new QPushButton(i18nc("@action:button", "Change..."), mDeferGroup);
        mDeferChangeButton->setFixedSize(mDeferChangeButton->sizeHint());
        connect(mDeferChangeButton, SIGNAL(clicked()), SLOT(slotEditDeferral()));
        mDeferChangeButton->setWhatsThis(i18nc("@info:whatsthis", "Change the alarm's deferred time, or cancel the deferral"));
        hlayout->addWidget(mDeferChangeButton);
//??        mDeferGroup->addSpace(0);
    }

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    topLayout->addLayout(hlayout);

    // Date and time entry
    if (mTemplate)
    {
        QGroupBox* templateTimeBox = new QGroupBox(i18nc("@title:group", "Time"), mainPage);
        hlayout->addWidget(templateTimeBox);
        QGridLayout* grid = new QGridLayout(templateTimeBox);
        grid->setMargin(marginHint());
        grid->setSpacing(spacingHint());
        mTemplateTimeGroup = new ButtonGroup(templateTimeBox);
        connect(mTemplateTimeGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotTemplateTimeType(QAbstractButton*)));
        connect(mTemplateTimeGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(contentsChanged()));

        mTemplateDefaultTime = new RadioButton(i18nc("@option:radio", "Default time"), templateTimeBox);
        mTemplateDefaultTime->setFixedSize(mTemplateDefaultTime->sizeHint());
        mTemplateDefaultTime->setReadOnly(mReadOnly);
        mTemplateDefaultTime->setWhatsThis(i18nc("@info:whatsthis", "Do not specify a start time for alarms based on this template. "
                                                "The normal default start time will be used."));
        mTemplateTimeGroup->addButton(mTemplateDefaultTime);
        grid->addWidget(mTemplateDefaultTime, 0, 0, Qt::AlignLeft);

        KHBox* box = new KHBox(templateTimeBox);
        box->setMargin(0);
        box->setSpacing(spacingHint());
        mTemplateUseTime = new RadioButton(i18nc("@option:radio", "Time:"), box);
        mTemplateUseTime->setFixedSize(mTemplateUseTime->sizeHint());
        mTemplateUseTime->setReadOnly(mReadOnly);
        mTemplateUseTime->setWhatsThis(i18nc("@info:whatsthis", "Specify a start time for alarms based on this template."));
        mTemplateTimeGroup->addButton(mTemplateUseTime);
        mTemplateTime = new TimeEdit(box);
        mTemplateTime->setFixedSize(mTemplateTime->sizeHint());
        mTemplateTime->setReadOnly(mReadOnly);
        mTemplateTime->setWhatsThis(i18nc("@info:whatsthis",
              "<para>Enter the start time for alarms based on this template.</para><para>%1</para>",
              TimeSpinBox::shiftWhatsThis()));
        connect(mTemplateTime, SIGNAL(valueChanged(int)), SLOT(contentsChanged()));
        box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
        box->setFixedHeight(box->sizeHint().height());
        grid->addWidget(box, 0, 1, Qt::AlignLeft);

        mTemplateAnyTime = new RadioButton(i18nc("@option:radio", "Date only"), templateTimeBox);
        mTemplateAnyTime->setFixedSize(mTemplateAnyTime->sizeHint());
        mTemplateAnyTime->setReadOnly(mReadOnly);
        mTemplateAnyTime->setWhatsThis(i18nc("@info:whatsthis", "Set the <interface>Any time</interface> option for alarms based on this template."));
        mTemplateTimeGroup->addButton(mTemplateAnyTime);
        grid->addWidget(mTemplateAnyTime, 1, 0, Qt::AlignLeft);

        box = new KHBox(templateTimeBox);
        box->setMargin(0);
        box->setSpacing(spacingHint());
        mTemplateUseTimeAfter = new RadioButton(i18nc("@option:radio", "Time from now:"), box);
        mTemplateUseTimeAfter->setFixedSize(mTemplateUseTimeAfter->sizeHint());
        mTemplateUseTimeAfter->setReadOnly(mReadOnly);
        mTemplateUseTimeAfter->setWhatsThis(i18nc("@info:whatsthis",
                                                  "Set alarms based on this template to start after the specified time "
                                                 "interval from when the alarm is created."));
        mTemplateTimeGroup->addButton(mTemplateUseTimeAfter);
        mTemplateTimeAfter = new TimeSpinBox(1, maxDelayTime, box);
        mTemplateTimeAfter->setValue(1439);
        mTemplateTimeAfter->setFixedSize(mTemplateTimeAfter->sizeHint());
        mTemplateTimeAfter->setReadOnly(mReadOnly);
        connect(mTemplateTimeAfter, SIGNAL(valueChanged(int)), SLOT(contentsChanged()));
        mTemplateTimeAfter->setWhatsThis(i18nc("@info:whatsthis", "<para>%1</para><para>%2</para>",
                                               AlarmTimeWidget::i18n_TimeAfterPeriod(), TimeSpinBox::shiftWhatsThis()));
        box->setFixedHeight(box->sizeHint().height());
        grid->addWidget(box, 1, 1, Qt::AlignLeft);

        hlayout->addStretch();
    }
    else
    {
        mTimeWidget = new AlarmTimeWidget(i18nc("@title:group", "Time"), AlarmTimeWidget::AT_TIME, mainPage);
        connect(mTimeWidget, SIGNAL(dateOnlyToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
        connect(mTimeWidget, SIGNAL(changed(KDateTime)), SLOT(contentsChanged()));
        topLayout->addWidget(mTimeWidget);
    }

    // Optional controls depending on More/Less Options button
    mMoreOptions = new QFrame(mainPage);
    mMoreOptions->setFrameStyle(QFrame::NoFrame);
    topLayout->addWidget(mMoreOptions);
    QVBoxLayout* moreLayout = new QVBoxLayout(mMoreOptions);
    moreLayout->setMargin(0);
    moreLayout->setSpacing(spacingHint());

    // Reminder
    mReminder = createReminder(mMoreOptions);
    if (mReminder)
    {
        mReminder->setFixedSize(mReminder->sizeHint());
        connect(mReminder, SIGNAL(changed()), SLOT(contentsChanged()));
        moreLayout->addWidget(mReminder, 0, Qt::AlignLeft);
        if (mTimeWidget)
            connect(mTimeWidget, SIGNAL(changed(KDateTime)), mReminder, SLOT(setDefaultUnits(KDateTime)));
    }

    // Late cancel selector - default = allow late display
    mLateCancel = new LateCancelSelector(true, mMoreOptions);
    connect(mLateCancel, SIGNAL(changed()), SLOT(contentsChanged()));
    moreLayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

    PackedLayout* playout = new PackedLayout(Qt::AlignJustify);
    playout->setSpacing(2*spacingHint());
    moreLayout->addLayout(playout);

    // Acknowledgement confirmation required - default = no confirmation
    CheckBox* confirmAck = type_createConfirmAckCheckbox(mMoreOptions);
    if (confirmAck)
    {
        confirmAck->setFixedSize(confirmAck->sizeHint());
        connect(confirmAck, SIGNAL(toggled(bool)), SLOT(contentsChanged()));
        playout->addWidget(confirmAck);
    }

    if (theApp()->korganizerEnabled())
    {
        // Show in KOrganizer checkbox
        mShowInKorganizer = new CheckBox(i18n_chk_ShowInKOrganizer(), mMoreOptions);
        mShowInKorganizer->setFixedSize(mShowInKorganizer->sizeHint());
        connect(mShowInKorganizer, SIGNAL(toggled(bool)), SLOT(contentsChanged()));
        mShowInKorganizer->setWhatsThis(i18nc("@info:whatsthis", "Check to copy the alarm into KOrganizer's calendar"));
        playout->addWidget(mShowInKorganizer);
    }

    setButtonWhatsThis(Ok, i18nc("@info:whatsthis", "Schedule the alarm at the specified time."));

    // Hide optional controls
    KConfigGroup config(KGlobal::config(), EDIT_MORE_GROUP);
    showOptions(config.readEntry(EDIT_MORE_KEY, false));

    // Initialise the state of all controls according to the specified event, if any
    initValues(event);
    if (mTemplateName)
        mTemplateName->setFocus();

    if (!mNewAlarm)
    {
        // Save the initial state of all controls so that we can later tell if they have changed
        saveState((event && (mTemplate || !event->isTemplate())) ? event : 0);
        contentsChanged();    // enable/disable OK button
    }

    // Note the current desktop so that the dialog can be shown on it.
    // If a main window is visible, the dialog will by KDE default always appear on its
    // desktop. If the user invokes the dialog via the system tray on a different desktop,
    // that can cause confusion.
    mDesktop = KWindowSystem::currentDesktop();
}

EditAlarmDlg::~EditAlarmDlg()
{
    delete mSavedEvent;
}

/******************************************************************************
* Initialise the dialog controls from the specified event.
*/
void EditAlarmDlg::initValues(const KAEvent* event)
{
    setReadOnly(mDesiredReadOnly);

    mChanged           = false;
    mOnlyDeferred      = false;
    mExpiredRecurrence = false;
    mLateCancel->showAutoClose(false);
    bool deferGroupVisible = false;
    if (event)
    {
        // Set the values to those for the specified event
        if (mTemplate)
            mTemplateName->setText(event->templateName());
        bool recurs = event->recurs();
        if ((recurs || event->repetition())  &&  !mTemplate  &&  event->deferred())
        {
            deferGroupVisible = true;
            mDeferDateTime = event->deferDateTime();
            mDeferTimeLabel->setText(mDeferDateTime.formatLocale());
            mDeferGroup->show();
        }
        if (mTemplate)
        {
            // Editing a template
            int afterTime = event->isTemplate() ? event->templateAfterTime() : -1;
            bool noTime   = !afterTime;
            bool useTime  = !event->mainDateTime().isDateOnly();
            RadioButton* button = noTime          ? mTemplateDefaultTime :
                                  (afterTime > 0) ? mTemplateUseTimeAfter :
                                  useTime         ? mTemplateUseTime : mTemplateAnyTime;
            button->setChecked(true);
            mTemplateTimeAfter->setValue(afterTime > 0 ? afterTime : 1);
            if (!noTime && useTime)
                mTemplateTime->setValue(event->mainDateTime().kDateTime().time());
            else
                mTemplateTime->setValue(0);
        }
        else
        {
            if (event->isTemplate())
            {
                // Initialising from an alarm template: use current date
                KDateTime now = KDateTime::currentDateTime(Preferences::timeZone());
                int afterTime = event->templateAfterTime();
                if (afterTime >= 0)
                {
                    mTimeWidget->setDateTime(now.addSecs(afterTime * 60));
                    mTimeWidget->selectTimeFromNow();
                }
                else
                {
                    KDateTime dt = event->startDateTime().kDateTime();
                    dt.setTimeSpec(Preferences::timeZone());
                    QDate d = now.date();
                    if (!dt.isDateOnly()  &&  now.time() >= dt.time())
                        d = d.addDays(1);     // alarm time has already passed, so use tomorrow
                    dt.setDate(d);
                    mTimeWidget->setDateTime(dt);
                }
            }
            else
            {
                mExpiredRecurrence = recurs && event->mainExpired();
                mTimeWidget->setDateTime(recurs || event->category() == CalEvent::ARCHIVED ? event->startDateTime()
                                         : event->mainExpired() ? event->deferDateTime() : event->mainDateTime());
            }
        }

        KAEvent::SubAction action = event->actionSubType();
        AlarmText altext;
        if (event->commandScript())
            altext.setScript(event->cleanText());
        else
            altext.setText(event->cleanText());
        setAction(action, altext);

        mLateCancel->setMinutes(event->lateCancel(), event->startDateTime().isDateOnly(),
                                TimePeriod::HoursMinutes);
        if (mShowInKorganizer)
            mShowInKorganizer->setChecked(event->copyToKOrganizer());
        type_initValues(event);
        mRecurrenceEdit->set(*event);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
        mTabs->setTabText(mRecurPageIndex, recurText(*event));
    }
    else
    {
        // Set the values to their defaults
        KDateTime defaultTime = KDateTime::currentUtcDateTime().addSecs(60).toTimeSpec(Preferences::timeZone());
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
        type_initValues(0);
        mRecurrenceEdit->setDefaults(defaultTime);   // must be called after mTimeWidget is set up, to ensure correct date-only enabling
        slotRecurFrequencyChange();      // update the Recurrence text
    }
    if (mReminder  &&  mTimeWidget)
        mReminder->setDefaultUnits(mTimeWidget->getDateTime(0, false, false));

    if (!deferGroupVisible  &&  mDeferGroup)
        mDeferGroup->hide();

    bool empty = AlarmCalendar::resources()->events(CalEvent::TEMPLATE).isEmpty();
    enableButton(Help, !empty);   // Load Templates button
}

/******************************************************************************
* Initialise various values in the New Alarm dialogue.
*/
void EditAlarmDlg::setTime(const DateTime& start)
{
    mTimeWidget->setDateTime(start);
}
void EditAlarmDlg::setRecurrence(const KARecurrence& recur, int subRepeatInterval, int subRepeatCount)
{
    KAEvent event;
    event.setTime(mTimeWidget->getDateTime(0, false, false));
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
    mLateCancel->setMinutes(minutes, mTimeWidget->getDateTime(0, false, false).isDateOnly(),
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
    mSavedEvent = 0;
    if (event)
        mSavedEvent = new KAEvent(*event);
    if (mTemplate)
    {
        mSavedTemplateName      = mTemplateName->text();
        mSavedTemplateTimeType  = mTemplateTimeGroup->checkedButton();
        mSavedTemplateTime      = mTemplateTime->time();
        mSavedTemplateAfterTime = mTemplateTimeAfter->value();
    }
    checkText(mSavedTextFileCommandMessage, false);
    if (mTimeWidget)
        mSavedDateTime = mTimeWidget->getDateTime(0, false, false);
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
    QString textFileCommandMessage;
    checkText(textFileCommandMessage, false);
    if (mTemplate)
    {
        if (mSavedTemplateName     != mTemplateName->text()
        ||  mSavedTemplateTimeType != mTemplateTimeGroup->checkedButton()
        ||  (mTemplateUseTime->isChecked()  &&  mSavedTemplateTime != mTemplateTime->time())
        ||  (mTemplateUseTimeAfter->isChecked()  &&  mSavedTemplateAfterTime != mTemplateTimeAfter->value()))
            return true;
    }
    else
    {
        KDateTime dt = mTimeWidget->getDateTime(0, false, false);
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
    if (mSavedEvent  &&  button(Ok))
        button(Ok)->setEnabled(stateChanged() || mDeferDateTime.kDateTime() != mSavedDeferTime);
}

/******************************************************************************
* Get the currently entered dialog data.
* The data is returned in the supplied KAEvent instance.
* Reply = false if the only change has been to an existing deferral.
*/
#ifdef USE_AKONADI
bool EditAlarmDlg::getEvent(KAEvent& event, Akonadi::Collection& collection)
#else
bool EditAlarmDlg::getEvent(KAEvent& event, AlarmResource*& resource)
#endif
{
#ifdef USE_AKONADI
    collection = mCollection;
#else
    resource = mResource;
#endif
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
    KDateTime dt;
    if (!trial)
    {
        if (!mTemplate)
            dt = mAlarmDateTime.effectiveKDateTime();
        else if (mTemplateUseTime->isChecked())
            dt = KDateTime(QDate(2000,1,1), mTemplateTime->time());
    }

    int lateCancel = (trial || !mLateCancel->isEnabled()) ? 0 : mLateCancel->minutes();
    type_setEvent(event, dt, text, lateCancel, trial);

    if (!trial)
    {
        if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
        {
            mRecurrenceEdit->updateEvent(event, !mTemplate);
            KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
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
                        if (remindTime > KDateTime::currentUtcDateTime())
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
            event.setTemplate(mTemplateName->text(), afterTime);
        }
    }
}

/******************************************************************************
* Get the currently specified alarm flag bits.
*/
KAEvent::Flags EditAlarmDlg::getAlarmFlags() const
{
    KAEvent::Flags flags(0);
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
    KDialog::showEvent(se);
    if (!mDeferGroupHeight)
    {
        if (mDeferGroup)
            mDeferGroupHeight = mDeferGroup->height() + spacingHint();
        QSize s;
        if (KAlarm::readConfigWindowSize(mTemplate ? TEMPLATE_DIALOG_NAME : EDIT_DIALOG_NAME, s))
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
}

/******************************************************************************
* Called when the dialog is closed.
*/
void EditAlarmDlg::closeEvent(QCloseEvent* ce)
{
    emit rejected();
    KDialog::closeEvent(ce);
}

/******************************************************************************
* Update the tab sizes (again) and if the resized dialog height is greater
* than the minimum, resize it again. This is necessary because (a) resizing
* tabs doesn't always work properly the first time, and (b) resizing to the
* minimum size hint doesn't always work either.
*/
void EditAlarmDlg::slotResize()
{
    QSize s = mTabScrollGroup->adjustSize(true);
    s = minimumSizeHint();
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
        KAlarm::writeConfigWindowSize(mTemplate ? TEMPLATE_DIALOG_NAME : EDIT_DIALOG_NAME, s);
    }
    KDialog::resizeEvent(re);
}

/******************************************************************************
* Called when any button is clicked.
*/
void EditAlarmDlg::slotButtonClicked(int button)
{
    if (button == Ok)
    {
        if (validate())
            accept();
    }
    else
        KDialog::slotButtonClicked(button);
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
        QString name = mTemplateName->text();
        if (name.isEmpty())
            errmsg = i18nc("@info", "You must enter a name for the alarm template");
        else if (name != mSavedTemplateName)
        {
            if (AlarmCalendar::resources()->templateEvent(name))
                errmsg = i18nc("@info", "Template name is already in use");
        }
        if (!errmsg.isEmpty())
        {
            mTemplateName->setFocus();
            KAMessageBox::sorry(this, errmsg);
            return false;
        }
    }
    else if (mTimeWidget)
    {
        QWidget* errWidget;
        mAlarmDateTime = mTimeWidget->getDateTime(0, !timedRecurrence, false, &errWidget);
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
            KDateTime pre = dt.effectiveKDateTime();
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
                                                  KGlobal::locale()->formatDate(next.date(), KLocale::ShortDate))
                                          : i18nc("@info The parameter is a date/time value",
                                                  "The start date/time does not match the alarm's recurrence pattern, "
                                                  "so it will be adjusted to the date/time of the next recurrence (%1).",
                                                  KGlobal::locale()->formatDateTime(next.kDateTime(), KLocale::ShortDate));
                if (KAMessageBox::warningContinueCancel(this, prompt) != KMessageBox::Continue)
                    return false;
            }
        }

        if (timedRecurrence)
        {
            KAEvent event;
#ifdef USE_AKONADI
            Akonadi::Collection c;
            getEvent(event, c);     // this may adjust mAlarmDateTime
#else
            AlarmResource* r;
            getEvent(event, r);     // this may adjust mAlarmDateTime
#endif
            KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
            bool dateOnly = mAlarmDateTime.isDateOnly();
            if ((dateOnly  &&  mAlarmDateTime.date() < now.date())
            ||  (!dateOnly  &&  mAlarmDateTime.kDateTime() < now))
            {
                // A timed recurrence has an entered start date which
                // has already expired, so we must adjust it.
                if (event.nextOccurrence(now, mAlarmDateTime, KAEvent::ALLOW_FOR_REPETITION) == KAEvent::NO_OCCURRENCE)
                {
                    KAMessageBox::sorry(this, i18nc("@info", "Recurrence has already expired"));
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
            KAMessageBox::sorry(this, errmsg);
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
                KAMessageBox::sorry(this, i18nc("@info", "Reminder period must be less than the recurrence interval, unless <interface>%1</interface> is checked.",
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
                KAMessageBox::sorry(this, i18nc("@info", "The duration of a repetition within the recurrence must be less than the recurrence interval minus any reminder period"));
                mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
                return false;
            }
            if (!recurEvent.repetition().isDaily()
            &&  ((mTemplate && mTemplateAnyTime->isChecked())  ||  (!mTemplate && mAlarmDateTime.isDateOnly())))
            {
                KAMessageBox::sorry(this, i18nc("@info", "For a repetition within the recurrence, its period must be in units of days or weeks for a date-only alarm"));
                mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
                return false;
            }
        }
    }
    if (!checkText(mAlarmMessage))
        return false;

#ifdef USE_AKONADI
    mCollection = Akonadi::Collection();
    // An item ID = -2 indicates that the caller already
    // knows which collection to use.
    if (mCollectionItemId >= -1)
    {
        if (mCollectionItemId >= 0)
        {
            mCollection = AlarmCalendar::resources()->collectionForEvent(mCollectionItemId);
            if (mCollection.isValid())
            {
                CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
                if (!(AkonadiModel::instance()->types(mCollection) & type))
                    mCollection = Akonadi::Collection();   // event may have expired while dialog was open
            }
        }
        bool cancelled = false;
        CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
        if (CollectionControlModel::isWritableEnabled(mCollection, type) <= 0)
            mCollection = CollectionControlModel::destination(type, this, false, &cancelled);
        if (!mCollection.isValid())
        {
            if (!cancelled)
                KAMessageBox::sorry(this, i18nc("@info", "You must select a calendar to save the alarm in"));
            return false;
        }
    }
#else
    mResource = 0;
    // A null resource event ID indicates that the caller already
    // knows which resource to use.
    if (!mResourceEventId.isNull())
    {
        if (!mResourceEventId.isEmpty())
        {
            mResource = AlarmCalendar::resources()->resourceForEvent(mResourceEventId);
            if (mResource)
            {
                CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
                if (mResource->alarmType() != type)
                    mResource = 0;   // event may have expired while dialog was open
            }
        }
        bool cancelled = false;
        if (!mResource  ||  !mResource->writable())
        {
            CalEvent::Type type = mTemplate ? CalEvent::TEMPLATE : CalEvent::ACTIVE;
            mResource = AlarmResources::instance()->destination(type, this, false, &cancelled);
        }
        if (!mResource)
        {
            if (!cancelled)
                KAMessageBox::sorry(this, i18nc("@info", "You must select a calendar to save the alarm in"));
            return false;
        }
    }
#endif
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
        void* result = theApp()->execAlarm(event, event.firstAlarm(), false, false);
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
#ifdef USE_AKONADI
    {
        KAEvent event = dlg->selectedTemplate();
        initValues(&event);
    }
#else
    initValues(dlg->selectedTemplate());
#endif
}

/******************************************************************************
* Called when the More Options or Less Options buttons are clicked.
* Show/hide the optional options and swap the More/Less buttons, and save the
* new setting as the default from now on.
*/
void EditAlarmDlg::slotDefault()
{
    showOptions(!mShowingMore);
    KConfigGroup config(KGlobal::config(), EDIT_MORE_GROUP);
    config.writeEntry(EDIT_MORE_KEY, mShowingMore);
}

/******************************************************************************
* Show/hide the optional options and swap the More/Less buttons.
*/
void EditAlarmDlg::showOptions(bool more)
{
    kDebug() << (more ? "More" : "Less");
    if (more)
    {
        mMoreOptions->show();
        setButtonText(Default, i18nc("@action:button", "Less Options <<"));
    }
    else
    {
        mMoreOptions->hide();
        setButtonText(Default, i18nc("@action:button", "More Options >>"));
    }
    if (mTimeWidget)
        mTimeWidget->showMoreOptions(more);
    type_showOptions(more);
    mRecurrenceEdit->showMoreOptions(more);
    mShowingMore = more;
    QTimer::singleShot(0, this, SLOT(slotResize()));
}

/******************************************************************************
* Called when the Change deferral button is clicked.
*/
void EditAlarmDlg::slotEditDeferral()
{
    if (!mTimeWidget)
        return;
    bool limit = true;
    Repetition repetition = mRecurrenceEdit->subRepetition();
    DateTime start = mSavedEvent->recurs() ? (mExpiredRecurrence ? DateTime() : mSavedEvent->mainDateTime())
                   : mTimeWidget->getDateTime(0, !repetition, !mExpiredRecurrence);
    if (!start.isValid())
    {
        if (!mExpiredRecurrence)
            return;
        limit = false;
    }
    KDateTime now = KDateTime::currentUtcDateTime();
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
            start = repetition.duration(repeatNum).end(start.kDateTime());
        }
    }

    bool deferred = mDeferDateTime.isValid();
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of EditAlarmDlg, and on return from this function).
    AutoQPointer<DeferAlarmDlg> deferDlg = new DeferAlarmDlg((deferred ? mDeferDateTime : DateTime(now.addSecs(60).toTimeSpec(start.timeSpec()))),
                                                             start.isDateOnly(), deferred, this);
    deferDlg->setObjectName(QLatin1String("EditDeferDlg"));    // used by LikeBack
    if (limit)
    {
        // Don't allow deferral past the next recurrence
        int reminder = mReminder ? mReminder->minutes() : 0;
        if (reminder)
        {
            DateTime remindTime = start.addMins(-reminder);
            if (KDateTime::currentUtcDateTime() < remindTime)
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
        if (mTemplateName)
            mTemplateName->setFocus();
        mMainPageShown = true;
    }
    else
    {
        // Set scroll position to top, since it otherwise jumps randomly
        StackedScrollWidget* main = static_cast<StackedScrollWidget*>(mTabs->widget(0));
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
        mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
        KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
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
            mAlarmDateTime = mTimeWidget->getDateTime(0, false, false);
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

// vim: et sw=4:
