/*
*  editdlg.cpp  -  dialog to create or modify an alarm or alarm template
*  Program:  kalarm
*  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QLabel>
#include <QDir>
#include <QStyle>
#include <QGroupBox>
#include <QPushButton>
#include <QTabWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <khbox.h>
#include <kvbox.h>
#include <kwindowsystem.h>
#include <kdebug.h>

#include <libkdepim/maillistdrag.h>
#include <libkdepim/kvcarddrag.h>
#include <kcal/icaldrag.h>

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "alarmtimewidget.h"
#include "buttongroup.h"
#include "checkbox.h"
#include "colourcombo.h"
#include "deferdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "latecancel.h"
#include "lineedit.h"
#include "mainwindow.h"
#include "packedlayout.h"
#include "preferences.h"
#include "radiobutton.h"
#include "recurrenceedit.h"
#include "reminder.h"
#include "shellprocess.h"
#include "spinbox.h"
#include "templatepickdlg.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "editdlg.moc"
#include "editdlgprivate.moc"
#include "editdlgtypes.h"

using namespace KCal;

static const char EDIT_DIALOG_NAME[] = "EditDialog";
static const int  maxDelayTime = 99*60 + 59;    // < 100 hours

inline QString recurText(const KAEvent& event)
{
	if (event.repeatCount())
		return QString::fromLatin1("%1 / %2").arg(event.recurrenceText()).arg(event.repetitionText());
	else
		return event.recurrenceText();
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString EditAlarmDlg::i18n_chk_ShowInKOrganizer()   { return i18nc("@option:check", "Show in KOrganizer"); }


EditAlarmDlg* EditAlarmDlg::create(bool Template, Type type, bool newAlarm, QWidget* parent, GetResourceType getResource)
{
	kDebug(5950) << "EditAlarmDlg::create()";
	switch (type)
	{
		case DISPLAY:  return new EditDisplayAlarmDlg(Template, newAlarm, parent, getResource);
		case COMMAND:  return new EditCommandAlarmDlg(Template, newAlarm, parent, getResource);
		case EMAIL:    return new EditEmailAlarmDlg(Template, newAlarm, parent, getResource);
	}
	return 0;
}

EditAlarmDlg* EditAlarmDlg::create(bool Template, const KAEvent& event, bool newAlarm, QWidget* parent,
                                   GetResourceType getResource, bool readOnly)
{
	switch (event.action())
	{
		case KAEvent::MESSAGE:
		case KAEvent::FILE:     return new EditDisplayAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
		case KAEvent::COMMAND:  return new EditCommandAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
		case KAEvent::EMAIL:    return new EditEmailAlarmDlg(Template, event, newAlarm, parent, getResource, readOnly);
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
EditAlarmDlg::EditAlarmDlg(bool Template, KAEvent::Action action, QWidget* parent, GetResourceType getResource)
	: KDialog(parent),
	  mAlarmType(action),
	  mMainPageShown(false),
	  mRecurPageShown(false),
	  mRecurSetDefaultEndDate(true),
	  mTemplateName(0),
	  mDeferGroup(0),
	  mTimeWidget(0),
	  mShowInKorganizer(0),
	  mResource(0),
	  mDeferGroupHeight(0),
	  mTemplate(Template),
	  mDesiredReadOnly(false),
	  mReadOnly(false),
	  mSavedEvent(0)
{
	init(0, getResource);
}

EditAlarmDlg::EditAlarmDlg(bool Template, const KAEvent& event, QWidget* parent,
                           GetResourceType getResource, bool readOnly)
	: KDialog(parent),
	  mAlarmType(event.action()),
	  mMainPageShown(false),
	  mRecurPageShown(false),
	  mRecurSetDefaultEndDate(true),
	  mTemplateName(0),
	  mDeferGroup(0),
	  mTimeWidget(0),
	  mShowInKorganizer(0),
	  mResource(0),
	  mDeferGroupHeight(0),
	  mTemplate(Template),
	  mDesiredReadOnly(readOnly),
	  mReadOnly(readOnly),
	  mSavedEvent(0)
{
	init(&event, getResource);
}

void EditAlarmDlg::init(const KAEvent* event, GetResourceType getResource)
{
	switch (getResource)
	{
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
			mResourceEventId.clear();
			break;
	}
}

void EditAlarmDlg::init(const KAEvent* event, bool newAlarm)
{
	setObjectName(mTemplate ? "TemplEditDlg" : "EditDlg");    // used by LikeBack
	QString caption;
	if (mReadOnly)
		caption = event->expired() ? i18nc("@title:window", "Archived Alarm [read-only]")
		                           : i18nc("@title:window", "View Alarm");
	else
		caption = type_caption(newAlarm);
	setCaption(caption);
	setButtons((mReadOnly ? Cancel|Try : mTemplate ? Ok|Cancel|Try : Ok|Cancel|Try|Default));
	setDefaultButton(mReadOnly ? Cancel : Ok);
	setButtonText(Default, i18nc("@action:button", "Load Template..."));
	connect(this, SIGNAL(tryClicked()), SLOT(slotTry()));
	connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
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
		mTemplateName = new QLineEdit(box);
		mTemplateName->setReadOnly(mReadOnly);
		label->setBuddy(mTemplateName);
		box->setWhatsThis(i18nc("@info:whatsthis", "Enter the name of the alarm template"));
		box->setFixedHeight(box->sizeHint().height());
	}
	mTabs = new QTabWidget(mainWidget);
//	mTabs->setMargin(marginHint());

	KVBox* mainPageBox = new KVBox;
	mainPageBox->setMargin(marginHint());
	mTabs->addTab(mainPageBox, i18nc("@title:tab", "&Alarm"));
	mMainPageIndex = 0;
	PageFrame* mainPage = new PageFrame(mainPageBox);
	connect(mainPage, SIGNAL(shown()), SLOT(slotShowMainPage()));
	QVBoxLayout* topLayout = new QVBoxLayout(mainPage);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacingHint());

	// Recurrence tab
	KVBox* recurTab = new KVBox;
	recurTab->setMargin(marginHint());
	mTabs->addTab(recurTab, i18nc("@title:tab", "&Recurrence"));
	mRecurPageIndex = 1;
	mRecurrenceEdit = new RecurrenceEdit(mReadOnly, recurTab);
	connect(mRecurrenceEdit, SIGNAL(shown()), SLOT(slotShowRecurrenceEdit()));
	connect(mRecurrenceEdit, SIGNAL(typeChanged(int)), SLOT(slotRecurTypeChange(int)));
	connect(mRecurrenceEdit, SIGNAL(frequencyChanged()), SLOT(slotRecurFrequencyChange()));
	connect(mRecurrenceEdit, SIGNAL(repeatNeedsInitialisation()), SLOT(slotSetSubRepetition()));

	// Controls specific to the alarm type
	QGroupBox* actionBox = new QGroupBox(i18nc("@title:group", "Action"), mainPage);
	topLayout->addWidget(actionBox, 1);
	QVBoxLayout* layout = new QVBoxLayout(actionBox);
	layout->setMargin(marginHint());
	layout->setSpacing(spacingHint());

	type_init(actionBox, layout);

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
//??	mDeferGroup->addSpace(0);

	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	topLayout->addLayout(hlayout);

	// Recurrence type display
	KHBox* recurBox = new KHBox(mainPage);   // this is to control the QWhatsThis text display area
	recurBox->setMargin(0);
	recurBox->setSpacing(spacingHint());
	label = new QLabel(i18nc("@label", "Recurrence:"), recurBox);
	label->setFixedSize(label->sizeHint());
	mRecurrenceText = new QLabel(recurBox);
	recurBox->setWhatsThis(i18nc("@info:whatsthis",
	      "<para>How often the alarm recurs.</para>"
	      "<para>The times shown are those configured in the Recurrence tab for the recurrence and optional sub-repetition.</para>"));
	recurBox->setFixedHeight(recurBox->sizeHint().height());

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
		box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 0, 1, Qt::AlignLeft);

		mTemplateAnyTime = new RadioButton(i18nc("@option:radio", "Date only"), templateTimeBox);
		mTemplateAnyTime->setFixedSize(mTemplateAnyTime->sizeHint());
		mTemplateAnyTime->setReadOnly(mReadOnly);
		mTemplateAnyTime->setWhatsThis(i18nc("@info:whatsthis", "Set the <interface>Date</interface> option for alarms based on this template."));
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
		mTemplateTimeAfter->setWhatsThis(i18nc("@info:whatsthis", "<para>%1</para><para>%2</para>",
		                                       AlarmTimeWidget::i18n_TimeAfterPeriod(), TimeSpinBox::shiftWhatsThis()));
		box->setFixedHeight(box->sizeHint().height());
		grid->addWidget(box, 1, 1, Qt::AlignLeft);

		recurBox->setParent(templateTimeBox);
		grid->addWidget(recurBox, 2, 0, 1, -1, Qt::AlignLeft);

		hlayout->addStretch();
	}
	else
	{
		mTimeWidget = new AlarmTimeWidget(i18nc("@title:group", "Time"), AlarmTimeWidget::AT_TIME, mainPage, recurBox);
		connect(mTimeWidget, SIGNAL(dateOnlyToggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		topLayout->addWidget(mTimeWidget);
	}

	// Reminder
	mReminder = createReminder(mainPage);
	if (mReminder)
	{
		mReminder->setFixedSize(mReminder->sizeHint());
		topLayout->addWidget(mReminder, 0, Qt::AlignLeft);
	}

	// Late cancel selector - default = allow late display
	mLateCancel = new LateCancelSelector(true, mainPage);
	topLayout->addWidget(mLateCancel, 0, Qt::AlignLeft);

	PackedLayout* playout = new PackedLayout(Qt::AlignJustify);
	playout->setSpacing(2*spacingHint());
	topLayout->addLayout(playout);

	// Acknowledgement confirmation required - default = no confirmation
	CheckBox* confirmAck = type_createConfirmAckCheckbox(mainPage);
	if (confirmAck)
	{
		confirmAck->setFixedSize(confirmAck->sizeHint());
		playout->addWidget(confirmAck);
	}

	if (theApp()->korganizerEnabled())
	{
		// Show in KOrganizer checkbox
		mShowInKorganizer = new CheckBox(i18n_chk_ShowInKOrganizer(), mainPage);
		mShowInKorganizer->setFixedSize(mShowInKorganizer->sizeHint());
		mShowInKorganizer->setWhatsThis(i18nc("@info:whatsthis", "Check to copy the alarm into KOrganizer's calendar"));
		playout->addWidget(mShowInKorganizer);
	}

	setButtonWhatsThis(Ok, i18nc("@info:whatsthis", "Schedule the alarm at the specified time."));

	// Initialise the state of all controls according to the specified event, if any
	initValues(event);
	if (mTemplateName)
		mTemplateName->setFocus();

	// Save the initial state of all controls so that we can later tell if they have changed
	saveState((event && (mTemplate || !event->isTemplate())) ? event : 0);

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
		if ((recurs || event->repeatCount())  &&  !mTemplate  &&  event->deferred())
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
				KDateTime now = KDateTime::currentUtcDateTime();
				int afterTime = event->templateAfterTime();
				if (afterTime >= 0)
				{
					mTimeWidget->setDateTime(now.addSecs(afterTime * 60));
					mTimeWidget->selectTimeFromNow();
				}
				else
				{
					KDateTime dt = event->startDateTime().kDateTime();
					now = now.toTimeSpec(dt);
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
				mTimeWidget->setDateTime(!event->mainExpired() ? event->mainDateTime()
				                         : recurs ? DateTime() : event->deferDateTime());
			}
		}

		KAEvent::Action action = event->action();
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
		mRecurrenceText->setText(recurText(*event));
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
	mLateCancel->setFixedSize(mLateCancel->sizeHint());   // do this after type_initValues()

	if (!deferGroupVisible)
		mDeferGroup->hide();

	bool empty = AlarmCalendar::resources()->events(KCalEvent::TEMPLATE).isEmpty();
	enableButton(Default, !empty);
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
	if (readOnly)
		mDeferChangeButton->hide();
	else
		mDeferChangeButton->show();
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
		||  mTemplateUseTime->isChecked()  &&  mSavedTemplateTime != mTemplateTime->time()
		||  mTemplateUseTimeAfter->isChecked()  &&  mSavedTemplateAfterTime != mTemplateTimeAfter->value())
			return true;
	}
	else
	{
		KDateTime dt = mTimeWidget->getDateTime(0, false, false);
		if (mSavedDateTime.timeSpec() != dt.timeSpec()  ||  mSavedDateTime != dt)
			return true;
	}
	if (mSavedLateCancel       != mLateCancel->minutes()
	||  mShowInKorganizer && mSavedShowInKorganizer != mShowInKorganizer->isChecked()
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
* Get the currently entered dialog data.
* The data is returned in the supplied KAEvent instance.
* Reply = false if the only change has been to an existing deferral.
*/
bool EditAlarmDlg::getEvent(KAEvent& event, AlarmResource*& resource)
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
*  Extract the data in the dialog and set up a KAEvent from it.
*  If 'trial' is true, the event is set up for a simple one-off test, ignoring
*  recurrence, reminder, template etc. data.
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

	type_setEvent(event, dt, text, (trial ? 0 : mLateCancel->minutes()), trial);

	if (!trial)
	{
		if (mRecurrenceEdit->repeatType() != RecurrenceEdit::NO_RECUR)
		{
			mRecurrenceEdit->updateEvent(event, !mTemplate);
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
int EditAlarmDlg::getAlarmFlags() const
{
	return (mShowInKorganizer && mShowInKorganizer->isChecked()                  ? KAEvent::COPY_KORGANIZER : 0)
	     | (mRecurrenceEdit->repeatType() == RecurrenceEdit::AT_LOGIN            ? KAEvent::REPEAT_AT_LOGIN : 0)
	     | ((mTemplate ? mTemplateAnyTime->isChecked() : mAlarmDateTime.isDateOnly()) ? KAEvent::ANY_TIME : 0);
}

/******************************************************************************
*  Called when the dialog is displayed.
*  The first time through, sets the size to the same as the last time it was
*  displayed.
*/
void EditAlarmDlg::showEvent(QShowEvent* se)
{
	if (!mDeferGroupHeight)
	{
		mDeferGroupHeight = mDeferGroup->height() + spacingHint();
		QSize s;
		if (KAlarm::readConfigWindowSize(EDIT_DIALOG_NAME, s))
			s.setHeight(s.height() + (mDeferGroup->isHidden() ? 0 : mDeferGroupHeight));
		else
			s = minimumSize();
		resize(s);
	}
	KWindowSystem::setOnDesktop(winId(), mDesktop);    // ensure it displays on the desktop expected by the user
	KDialog::showEvent(se);
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size (adjusted to ignore the optional height of the deferred
*  time edit widget) in the config file.
*/
void EditAlarmDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
	{
		QSize s = re->size();
		s.setHeight(s.height() - (mDeferGroup->isHidden() ? 0 : mDeferGroupHeight));
		KAlarm::writeConfigWindowSize(EDIT_DIALOG_NAME, s);
	}
	KDialog::resizeEvent(re);
}

/******************************************************************************
*  Called when any button is clicked.
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
*  Called when the OK button is clicked.
*  Validate the input data.
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
			if (AlarmCalendar::resources()->templateEvent(name).valid())
				errmsg = i18nc("@info", "Template name is already in use");
		}
		if (!errmsg.isEmpty())
		{
			mTemplateName->setFocus();
			KMessageBox::sorry(this, errmsg);
			return false;
		}
	}
	else if(mTimeWidget)
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
		if (timedRecurrence)
		{
			KAEvent event;
			AlarmResource* r;
			getEvent(event, r);     // this may adjust mAlarmDateTime
			KDateTime now = KDateTime::currentDateTime(mAlarmDateTime.timeSpec());
			bool dateOnly = mAlarmDateTime.isDateOnly();
			if (dateOnly  &&  mAlarmDateTime.date() < now.date()
			||  !dateOnly  &&  mAlarmDateTime.rawDateTime() < now.dateTime())
			{
				// A timed recurrence has an entered start date which
				// has already expired, so we must adjust it.
				dateOnly = mAlarmDateTime.isDateOnly();
				if ((dateOnly  &&  mAlarmDateTime.date() < now.date()
				     || !dateOnly  &&  mAlarmDateTime.rawDateTime() < now.dateTime())
				&&  event.nextOccurrence(now, mAlarmDateTime, KAEvent::ALLOW_FOR_REPETITION) == KAEvent::NO_OCCURRENCE)
				{
					KMessageBox::sorry(this, i18nc("@info", "Recurrence has already expired"));
					return false;
				}
				if (event.workTimeOnly()  &&  !event.displayDateTime().isValid())
				{
					if (KMessageBox::warningContinueCancel(this, i18nc("@info", "The alarm will never occur during working hours"))
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
			KMessageBox::sorry(this, errmsg);
			return false;
		}
	}
	if (recurType != RecurrenceEdit::NO_RECUR)
	{
		KAEvent recurEvent;
		int longestRecurInterval = -1;
		int reminder = mReminder ? mReminder->minutes() : 0;
		if (reminder  &&  !mReminder->isOnceOnly())
		{
			mRecurrenceEdit->updateEvent(recurEvent, false);
			longestRecurInterval = recurEvent.longestRecurrenceInterval();
			if (longestRecurInterval  &&  reminder >= longestRecurInterval)
			{
				mTabs->setCurrentIndex(mMainPageIndex);
				mReminder->setFocusOnCount();
				KMessageBox::sorry(this, i18nc("@info", "Reminder period must be less than the recurrence interval, unless <interface>%1</interface> is checked."
							     , Reminder::i18n_chk_FirstRecurrenceOnly()));
				return false;
			}
		}
		if (mRecurrenceEdit->subRepeatCount())
		{
			if (longestRecurInterval < 0)
			{
				mRecurrenceEdit->updateEvent(recurEvent, false);
				longestRecurInterval = recurEvent.longestRecurrenceInterval();
			}
			if (recurEvent.repeatCount() * recurEvent.repeatInterval() >= longestRecurInterval - reminder)
			{
				KMessageBox::sorry(this, i18nc("@info", "The duration of a repetition within the recurrence must be less than the recurrence interval minus any reminder period"));
				mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
				return false;
			}
			if (recurEvent.repeatInterval() % 1440
			&&  (mTemplate && mTemplateAnyTime->isChecked()  ||  !mTemplate && mAlarmDateTime.isDateOnly()))
			{
				KMessageBox::sorry(this, i18nc("@info", "For a repetition within the recurrence, its period must be in units of days or weeks for a date-only alarm"));
				mRecurrenceEdit->activateSubRepetition();   // display the alarm repetition dialog again
				return false;
			}
		}
	}
	if (!checkText(mAlarmMessage))
		return false;

	mResource = 0;
	// A null resource event ID indicates that the caller already
	// knows which resource to use.
	if (!mResourceEventId.isNull())
	{
		if (!mResourceEventId.isEmpty())
		{
			mResource = AlarmResources::instance()->resourceForIncidence(mResourceEventId);
			AlarmResource::Type type = mTemplate ? AlarmResource::TEMPLATE : AlarmResource::ACTIVE;
			if (mResource->alarmType() != type)
				mResource = 0;   // event may have expired while dialog was open
		}
		if (!mResource  ||  !mResource->writable())
		{
			KCalEvent::Status type = mTemplate ? KCalEvent::TEMPLATE : KCalEvent::ACTIVE;
			mResource = AlarmResources::instance()->destination(type, this);
		}
		if (!mResource)
		{
			KMessageBox::sorry(this, i18nc("@info", "You must select a resource to save the alarm in"));
			return false;
		}
	}
	return true;
}

/******************************************************************************
*  Called when the Try button is clicked.
*  Display/execute the alarm immediately for the user to check its configuration.
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
		connect(theApp(), SIGNAL(execAlarmSuccess()), SLOT(slotTrySuccess()));
		void* proc = theApp()->execAlarm(event, event.firstAlarm(), false, false);
		if (proc  &&  proc != (void*)-1)
			type_trySuccessMessage((ShellProcess*)proc, text);
	}
}

/******************************************************************************
*  Called when the Try action has completed, successfully.
*/
void EditAlarmDlg::slotTrySuccess()
{
	type_trySuccessMessage(0, QString());
}

/******************************************************************************
*  Called when the Load Template button is clicked.
*  Prompt to select a template and initialise the dialog with its contents.
*/
void EditAlarmDlg::slotDefault()
{
	TemplatePickDlg dlg(this);
	if (dlg.exec() == QDialog::Accepted)
	{
		const Event* kcalEvent = dlg.selectedTemplate();
		if (kcalEvent)
		{
			KAEvent event(kcalEvent);
			initValues(&event);
		}
	}
}

/******************************************************************************
* Called when the Change deferral button is clicked.
*/
void EditAlarmDlg::slotEditDeferral()
{
	if (!mTimeWidget)
		return;
	bool limit = true;
	int repeatInterval;
	int repeatCount = mRecurrenceEdit->subRepeatCount(&repeatInterval);
	DateTime start = mTimeWidget->getDateTime(0, !repeatCount, !mExpiredRecurrence);
	if (!start.isValid())
	{
		if (!mExpiredRecurrence)
			return;
		limit = false;
	}
	KDateTime now = KDateTime::currentUtcDateTime();
	if (limit)
	{
		if (repeatCount  &&  start < now)
		{
			// Sub-repetition - find the time of the next one
			repeatInterval *= 60;
			int repetition = (start.secsTo(now) + repeatInterval - 1) / repeatInterval;
			if (repetition > repeatCount)
			{
				mTimeWidget->getDateTime();    // output the appropriate error message
				return;
			}
			start = start.addSecs(repetition * repeatInterval);
		}
	}

	bool deferred = mDeferDateTime.isValid();
	DeferAlarmDlg deferDlg((deferred ? mDeferDateTime : DateTime(now.addSecs(60))), deferred, this);
	deferDlg.setObjectName("EditDeferDlg");    // used by LikeBack
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
		deferDlg.setLimit(start.addSecs(-60));
	}
	if (deferDlg.exec() == QDialog::Accepted)
	{
		mDeferDateTime = deferDlg.getDateTime();
		mDeferTimeLabel->setText(mDeferDateTime.isValid() ? mDeferDateTime.formatLocale() : QString());
	}
}

/******************************************************************************
*  Called when the main page is shown.
*  Sets the focus widget to the first edit field.
*/
void EditAlarmDlg::slotShowMainPage()
{
	if (!mMainPageShown)
	{
		if (mTemplateName)
			mTemplateName->setFocus();
		mMainPageShown = true;
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
*  Called when the recurrence edit page is shown.
*  The recurrence defaults are set to correspond to the start date.
*  The first time, for a new alarm, the recurrence end date is set according to
*  the alarm start time.
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
*  Called when the recurrence type selection changes.
*  Enables/disables date-only alarms as appropriate.
*  Enables/disables controls depending on at-login setting.
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
		mReminder->setEnabled(!atLogin);
	mLateCancel->setEnabled(!atLogin);
	if (mShowInKorganizer)
		mShowInKorganizer->setEnabled(!atLogin);
	slotRecurFrequencyChange();
}

/******************************************************************************
*  Called when the recurrence frequency selection changes, or the sub-
*  repetition interval changes.
*  Updates the recurrence frequency text.
*/
void EditAlarmDlg::slotRecurFrequencyChange()
{
kDebug()<<"slotRecurFrequencyChange()"<<endl;
	slotSetSubRepetition();
	KAEvent event;
	mRecurrenceEdit->updateEvent(event, false);
	mRecurrenceText->setText(recurText(event));
}

/******************************************************************************
*  Called when the Repetition within Recurrence button has been pressed to
*  display the sub-repetition dialog.
*  Alarm repetition has the following restrictions:
*  1) Not allowed for a repeat-at-login alarm
*  2) For a date-only alarm, the repeat interval must be a whole number of days.
*  3) The overall repeat duration must be less than the recurrence interval.
*/
void EditAlarmDlg::slotSetSubRepetition()
{
	bool dateOnly = mTemplate ? mTemplateAnyTime->isChecked() : mTimeWidget->anyTime();
	mRecurrenceEdit->setSubRepetition((mReminder ? mReminder->minutes() : 0), dateOnly);
}

/******************************************************************************
*  Called when one of the template time radio buttons is clicked,
*  to enable or disable the template time entry spin boxes.
*/
void EditAlarmDlg::slotTemplateTimeType(QAbstractButton*)
{
	mTemplateTime->setEnabled(mTemplateUseTime->isChecked());
	mTemplateTimeAfter->setEnabled(mTemplateUseTimeAfter->isChecked());
}

/******************************************************************************
*  Called when the "Any time" checkbox is toggled in the date/time widget.
*  Sets the advance reminder and late cancel units to days if any time is checked.
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
