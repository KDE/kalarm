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

#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qwhatsthis.h>

#include <kstddirs.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdialog.h>
#include <kwin.h>
#include <knotifyclient.h>
#include <kaudioplayer.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "messagewin.h"
#include "messagewin.moc"


/******************************************************************************
*  Construct the message window.
*/
MessageWin::MessageWin(const MessageEvent& event, bool delete_event)
	: KMainWindow(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose),
	  message(event.message()),
	  font(theApp()->generalSettings()->messageFont()),
	  colour(event.colour()),
	  dateTime(event.dateTime()),
	  eventID(event.VUID()),
	  audioFile(event.alarm()->audioFile()),
	  beep(event.beep()),
	  deleteEvent(delete_event),
	  shown(false)
{
	kdDebug() << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWindow"));     // save window sizes etc.
	QSize size = initView();
	resize(size);
}

/******************************************************************************
*  Construct the message window for restoration by session management.
*/
MessageWin::MessageWin()
	: KMainWindow(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose),
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
	setCaption(i18n("Message"));
	QWidget* topWidget = new QWidget(this, "MessageWinTop");
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

	// OK button
	QPushButton* button = new QPushButton(i18n("ABCDEF"), topWidget);
	QSize butsize = button->sizeHint();
	delete button;

	button = new QPushButton(i18n("&OK"), topWidget);
	button->setFixedSize(butsize);
	button->setDefault(true);
	connect(button, SIGNAL(clicked()), SLOT(close()));
	topLayout->addWidget(button);

	topLayout->activate();
	QSize size(butsize.width()*3, topLayout->sizeHint().height());
	setMinimumSize(size);
	
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
	config->writeEntry("Message", message);
	config->writeEntry("Font", font);
	config->writeEntry("Colour", colour);
	if (dateTime.isValid())
		config->writeEntry("Time", dateTime);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(KConfig* config)
{
	message  = config->readEntry("Message");
	font     = config->readFontEntry("Font");
	colour   = config->readColorEntry("Colour");
	QDateTime invalidDateTime;
	dateTime = config->readDateTimeEntry("Time", &invalidDateTime);
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
		if (deleteEvent)
			KAlarmApp::getInstance()->deleteMessage(eventID);
		shown = true;
	}
}
