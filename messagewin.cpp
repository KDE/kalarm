/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include "kalarm.h"

#include <qfile.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qtextview.h>
#include <qlabel.h>
#include <qwhatsthis.h>

#include <kstddirs.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <kwin.h>
#include <kprocess.h>
#include <kio/netaccess.h>
#include <knotifyclient.h>
#include <kaudioplayer.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "messagewin.h"
#include "messagewin.moc"


static const int MAX_LINE_LENGTH = 80;    // maximum width (in characters) to try to display


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAlarmEvent& evnt, const KAlarmAlarm& alarm, bool reschedule_event)
	: KMainWindow(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose | WGroupLeader),
	  event(evnt),
	  message(alarm.messageIsFileName() ? alarm.fileName() : alarm.message()),
	  font(theApp()->generalSettings()->messageFont()),
	  colour(alarm.colour()),
	  dateTime(alarm.repeatAtLogin() ? evnt.firstAlarm().dateTime() : alarm.dateTime()),
	  eventID(evnt.id()),
//	  audioFile(alarm.audioFile()),
	  alarmID(alarm.id()),
	  flags(alarm.flags()),
	  beep(alarm.beep()),
	  file(alarm.messageIsFileName()),
	  noDefer(alarm.repeatAtLogin()),
	  deferButton(0L),
	  deferHeight(0),
	  restoreHeight(0),
	  rescheduleEvent(reschedule_event),
	  shown(false)
{
	kdDebug() << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWindow"));     // save window sizes etc.
	QSize size = initView();
	if (file  &&  !fileError)
		size = theApp()->readConfigWindowSize("FileMessage", size);
	resize(size);
}

/******************************************************************************
*  Construct the message window for restoration by session management.
*  The window is initialised by readProperties().
*/
MessageWin::MessageWin()
	: KMainWindow(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose),
	  deferHeight(0),
	  rescheduleEvent(false),
	  shown(true)
{
	kdDebug() << "MessageWin::MessageWin()" << endl;
}

MessageWin::~MessageWin()
{
}

/******************************************************************************
*  Construct the message window.
*/
QSize MessageWin::initView()
{
	fileError = false;
	setCaption(i18n("Message"));
	QWidget* topWidget = new QWidget(this, "messageWinTop");
	setCentralWidget(topWidget);
	topWidget->setBackgroundColor(colour);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget, KDialog::marginHint(), KDialog::spacingHint());

	if (dateTime.isValid())
	{
		// Alarm date/time
		QLabel* label = new QLabel(topWidget);
		label->setText(KGlobal::locale()->formatDateTime(dateTime));
		label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		topLayout->addWidget(label);
		QWhatsThis::add(label,
		      i18n("The scheduled date/time for the message\n"
		           "(as opposed to the actual time of display)."));
	}

	if (file)
	{
		// Display the file name
		QLabel* label = new QLabel(topWidget);
		label->setText(message);
		label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		topLayout->addWidget(label);
		QWhatsThis::add(label, i18n("The file whose contents are displayed below"));

		// Display contents of file
		bool opened = false;
		bool dir = false;
		QString tmpFile;
		KURL url;       // don't use KURL(message) since a UNIX file path may not initialise correctly
		url.setPath(message);
		if (KIO::NetAccess::download(url, tmpFile))
		{
			QFile qfile(tmpFile);
			QFileInfo info(qfile);
			if (!(dir = info.isDir())
			&&  qfile.open(IO_ReadOnly|IO_Translate))
			{
				opened = true;
				QTextView* view = new QTextView(this, "fileContents");
				topLayout->addWidget(view);
				QFontMetrics fm = view->fontMetrics();
				QString line;
				int n;
				for (n = 0;  qfile.readLine(line, 4096) > 0;  ++n)
				{
					int nl = line.find('\n');
					if (nl >= 0)
						line = line.left(nl);
					view->append(line);
				}
				qfile.close();
				view->setMinimumSize(view->sizeHint());

				// Set the default size to square, max 20 lines.
				// Note that after the first file has been displayed, this size
				// is overridden by the user-set default stored in the config file.
				// So there is no need to calculate an accurate size.
				int h = fm.lineSpacing() * (n <= 20 ? n : 20) + 2*view->frameWidth();
				view->resize(QSize(h, h).expandedTo(view->sizeHint()));
			}
			KIO::NetAccess::removeTempFile(tmpFile);
		}
		if (!opened)
		{
			// File couldn't be opened
			bool exists = KIO::NetAccess::exists(url);
			QLabel* label = new QLabel(topWidget);
			label->setText(dir ? i18n("Error: File is a directory") : exists ? i18n("Error opening file !!") : i18n("Error: File not found !!"));
			label->setPalette(QPalette(colour, colour));
			label->setFixedSize(label->sizeHint());
			topLayout->addWidget(label);
			fileError = true;
		}
	}
	else
	{
		// Message label
		QLabel* label = new QLabel(topWidget);
		label->setText(message);
		label->setFont(font);
		label->setPalette(QPalette(colour, colour));
		label->setFixedSize(label->sizeHint());
		int spacing = label->fontMetrics().lineSpacing()/2 - KDialog::spacingHint();
		topLayout->addSpacing(spacing);
		topLayout->addWidget(label);
		topLayout->addSpacing(spacing);
	}

	QGridLayout* grid = new QGridLayout(1, 4);
	topLayout->addLayout(grid);
	grid->setColStretch(0, 1);     // keep the buttons right-adjusted in the window

	// OK button
	QPushButton* okButton = new QPushButton(i18n("&OK"), topWidget);
	okButton->setDefault(true);
	connect(okButton, SIGNAL(clicked()), SLOT(close()));
	grid->addWidget(okButton, 0, 1, AlignHCenter);
	QWhatsThis::add(okButton, i18n("Acknowledge the alarm"));

	if (!noDefer)
	{
		// Defer button
		deferButton = new QPushButton(i18n("&Defer..."), topWidget);
		connect(deferButton, SIGNAL(clicked()), SLOT(slotShowDefer()));
		grid->addWidget(deferButton, 0, 2, AlignHCenter);
		QWhatsThis::add(deferButton,
		      i18n("Defer the alarm until later.\n"
		           "You will be prompted to specify when\n"
		           "the alarm should be redisplayed"));
	}

	// KAlarm button
	KIconLoader iconLoader;
	QPixmap pixmap = iconLoader.loadIcon(QString::fromLatin1(PROGRAM_NAME), KIcon::MainToolbar);
	QPushButton* button = new QPushButton(topWidget);
	button->setPixmap(pixmap);
	button->setFixedSize(button->sizeHint());
	connect(button, SIGNAL(clicked()), SLOT(slotKAlarm()));
	grid->addWidget(button, 0, 3, AlignHCenter);
	QWhatsThis::add(button, i18n("Activate KAlarm"));

	// Set the button sizes
	QSize minbutsize = okButton->sizeHint();
	if (!noDefer)
	{
		minbutsize = minbutsize.expandedTo(deferButton->sizeHint());
		deferButton->setFixedSize(minbutsize);
	}
	okButton->setFixedSize(minbutsize);

	topLayout->activate();
	QSize size(minbutsize.width()*3, topLayout->sizeHint().height());
	setMinimumSize(size);

// display Help button in the title bar ????
	KWin::setState(winId(), NET::Modal | NET::Sticky | NET::StaysOnTop);
	KWin::setOnAllDesktops(winId(), true);
	return size;
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWin::saveProperties(KConfig* config)
{
	if (shown)
	{
		config->writeEntry(QString::fromLatin1("EventID"), eventID);
		config->writeEntry(QString::fromLatin1("AlarmID"), alarmID);
		config->writeEntry(QString::fromLatin1("Message"), message);
		config->writeEntry(QString::fromLatin1("File"), file);
		config->writeEntry(QString::fromLatin1("Font"), font);
		config->writeEntry(QString::fromLatin1("Colour"), colour);
		if (dateTime.isValid())
			config->writeEntry(QString::fromLatin1("Time"), dateTime);
		config->writeEntry(QString::fromLatin1("Height"), height() - deferHeight);
		config->writeEntry(QString::fromLatin1("NoDefer"), noDefer);
	}
	else
		config->writeEntry(QString::fromLatin1("AlarmID"), -1);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(KConfig* config)
{
	eventID       = config->readEntry(QString::fromLatin1("EventID"));
	alarmID       = config->readNumEntry(QString::fromLatin1("AlarmID"));
	message       = config->readEntry(QString::fromLatin1("Message"));
	file          = config->readBoolEntry(QString::fromLatin1("File"));
	font          = config->readFontEntry(QString::fromLatin1("Font"));
	colour        = config->readColorEntry(QString::fromLatin1("Colour"));
	QDateTime invalidDateTime;
	dateTime      = config->readDateTimeEntry(QString::fromLatin1("Time"), &invalidDateTime);
	restoreHeight = config->readNumEntry(QString::fromLatin1("Height"));
	noDefer       = config->readBoolEntry(QString::fromLatin1("NoDefer"));
	if (alarmID > 0)
		initView();
}

/******************************************************************************
*  Called when the window is shown.
*  The first time, output any required audio notification, and delete the event
*  from the calendar file.
*/
void MessageWin::showEvent(QShowEvent* se)
{
	KMainWindow::showEvent(se);
	if (!shown)
	{
		if (beep)
		{
			// Beep using two methods, in case the sound card/speakers are switched off or not working
			KNotifyClient::beep();     // beep through the sound card & speakers
			QApplication::beep();      // beep through the internal speaker
		}
		if (!audioFile.isEmpty())
			KAudioPlayer::play(audioFile.latin1());
		if (rescheduleEvent)
			theApp()->rescheduleAlarm(event, alarmID);
		shown = true;
	}
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*/
void MessageWin::resizeEvent(QResizeEvent* re)
{
	if (restoreHeight)
	{
		if (restoreHeight != re->size().height())
		{
			QSize size = re->size();
			size.setHeight(restoreHeight);
			resize(size);
		}
		else if (isVisible())
			restoreHeight = 0;
	}
	else
	{
		if (file  &&  !fileError  &&  !deferHeight)
			theApp()->writeConfigWindowSize("FileMessage", re->size());
		KMainWindow::resizeEvent(re);
	}
}

/******************************************************************************
*  Called when the Defer... button is clicked.
*  Displays the defer message dialog.
*/
void MessageWin::slotShowDefer()
{
	if (!deferHeight)
	{
		delete deferButton;
		QWidget* deferDlg = new QWidget(this);
		QVBoxLayout* wlayout = new QVBoxLayout(deferDlg, KDialog::spacingHint());
		QGridLayout* grid = new QGridLayout(2, 1, KDialog::spacingHint());
		wlayout->addLayout(grid);
		deferTime = new AlarmTimeWidget(true, deferDlg, "deferTime");
		deferTime->setDateTime(QDateTime::currentDateTime().addSecs(60));
		connect(deferTime, SIGNAL(deferred()), SLOT(slotDefer()));
		grid->addWidget(deferTime, 0, 0);
		if (theApp()->getCalendar().getEvent(eventID))
		{
			// The event will only still exist if repetitions are outstanding
			QLabel* warn = new QLabel(deferDlg);
			warn->setText(i18n("Note: deferring this alarm will also defer its repetitions"));
			warn->setFixedSize(warn->sizeHint());
			grid->addWidget(warn, 1, 0);
		}

		QSize s(deferDlg->sizeHint());
		if (s.width() > width())
			resize(s.width(), height());     // this ensures that the background colour extends to edge
		else if (width() > s.width())
			s.setWidth(width());

		// Ensure that the defer dialog doesn't disappear past the bottom of the screen
		int maxHeight = KApplication::desktop()->height() - s.height();
		QRect rect = frameGeometry();
		if (rect.bottom() > maxHeight)
		{
			// Move the window upwards if possible, and resize if necessary
			int bottomShift = rect.bottom() - maxHeight;
			int topShift    = bottomShift;
			if (topShift > rect.top())
				topShift = rect.top();
			rect = geometry();
			rect.setTop(rect.top() - topShift);
			rect.setBottom(rect.bottom() - bottomShift);
			setGeometry(rect);
		}

		deferHeight = s.height();
		if (layout())
			layout()->setEnabled(false);
		deferDlg->setGeometry(0, height(), s.width(), s.height());
		setFixedSize(s.width(), height() + s.height());
		deferDlg->show();
	}
}

/******************************************************************************
*  Called when the Defer button is clicked to defer the alarm.
*/
void MessageWin::slotDefer()
{
	QDateTime dateTime;
	if (deferTime->getDateTime(dateTime))
	{
		// Get the event being deferred. It will only still exist if repetitions are outstanding.
		const Event* kcalEvent = theApp()->getCalendar().getEvent(eventID);
		if (kcalEvent)
		{
			// It's a repeated alarm which may still exist in the calendar file
			KAlarmEvent event(*kcalEvent);
			event.setTime(dateTime);
			theApp()->updateMessage(event, 0L);
		}
		else
		{
			// The event doesn't exist any more, so create a new one
			KAlarmEvent event(dateTime, message, colour, file, flags);
			theApp()->addMessage(event, 0L);
		}
		close();
	}
}

/******************************************************************************
*  Called when the KAlarm button is clicked.
*  Displays the KAlarm main window.
*/
void MessageWin::slotKAlarm()
{
	KProcess proc;
	proc << QString::fromLatin1(PROGRAM_NAME);
	proc.start(KProcess::DontCare);
}
