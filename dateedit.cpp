/*
 *  dateedit.cpp  -  date entry widget
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
 *
 *  Based on libkdepim/kdateedit.cpp.
 */
/*
    This file is part of libkdepim.

    Copyright (c) 2002 Cornelius Schumacher <schumacher@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#include <qapplication.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qpixmap.h>
#include <qpushbutton.h>

#include <kdatepicker.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <knotifyclient.h>

#include "dateedit.moc"


DateEdit::DateEdit(QWidget *parent, const char *name)
  : QComboBox(true, parent, name)
{
  setMaxCount(1);       // need at least one entry for popup to work
  value = QDate::currentDate();
  QString today = KGlobal::locale()->formatDate(value, true);
  insertItem(today);
  setCurrentItem(0);
  setCurrentText(today);
  setMinimumSize(sizeHint());

  mDateFrame = new QVBox(0,0,WType_Popup);
  mDateFrame->setFrameStyle(QFrame::PopupPanel | QFrame::Raised);
  mDateFrame->setLineWidth(3);
  mDateFrame->hide();

  mDatePicker = new KDatePicker(mDateFrame,QDate::currentDate());

  connect(lineEdit(),SIGNAL(returnPressed()),SLOT(lineEnterPressed()));
  connect(this,SIGNAL(textChanged(const QString &)),
          SLOT(slotTextChanged(const QString &)));

  connect(mDatePicker,SIGNAL(dateEntered(QDate)),SLOT(slotDateEntered(QDate)));
  connect(mDatePicker,SIGNAL(dateSelected(QDate)),SLOT(slotDateSelected(QDate)));

  // Create the keyword list. This will be used to match against when the user
  // enters information.
  mKeywordMap[i18n("tomorrow")] = 1;
  mKeywordMap[i18n("today")] = 0;
  mKeywordMap[i18n("yesterday")] = -1;

  QString dayName;
  for (int i = 1; i <= 7; ++i)
  {
    dayName = KGlobal::locale()->weekDayName(i).lower();
    mKeywordMap[dayName] = i + 100;
  }

  mTextChanged = false;
  mHandleInvalid = false;
}

DateEdit::~DateEdit()
{
  delete mDateFrame;
}

void DateEdit::setDate(const QDate& newDate)
{
  if (!newDate.isValid() && !mHandleInvalid)
    return;

  QString dateString = "";
  if(newDate.isValid())
    dateString = KGlobal::locale()->formatDate( newDate, true );

  mTextChanged = false;

  // We do not want to generate a signal here, since we explicity setting
  // the date
  bool b = signalsBlocked();
  blockSignals(true);
  setCurrentText(dateString);
  blockSignals(b);

  value = newDate;
}

void DateEdit::setHandleInvalid(bool handleInvalid)
{
  mHandleInvalid = handleInvalid;
}

QDate DateEdit::date() const
{
  QDate date = readDate();

  if (date.isValid() || mHandleInvalid) {
    return date;
  } else {
    KNotifyClient::beep();
    return QDate::currentDate();
  }
}

void DateEdit::popup()
{
  if( mDateFrame->isVisible() ) {
    mDateFrame->hide();
  } else {
    QPoint tmpPoint = mapToGlobal(geometry().bottomRight());
    QSize datepickersize = mDatePicker->sizeHint();

    if ( tmpPoint.x() < 7+datepickersize.width() ) tmpPoint.setX( 7+datepickersize.width() );

    int h = QApplication::desktop()->height();

    if ( tmpPoint.y() + datepickersize.height() > h ) tmpPoint.setY( h - datepickersize.height() );

    mDateFrame->setGeometry(tmpPoint.x()-datepickersize.width()-7, tmpPoint.y(),
     datepickersize.width()+2*mDateFrame->lineWidth(), datepickersize.height()+2*mDateFrame->lineWidth());

    QDate date = readDate();
    if(date.isValid()) {
      mDatePicker->setDate(date);
    } else {
      mDatePicker->setDate(QDate::currentDate());
    }
    mDateFrame->show();
  }
}


// Called when a date has been selected by clicking in the KDatePicker table
void DateEdit::slotDateSelected(QDate newDate)
{
  if (newDate.isValid() || mHandleInvalid)
  {
    if (checkMinDate(newDate))
      mDateFrame->hide();
  }
}

// Called when a date has been entered into the KDatePicker line edit
void DateEdit::slotDateEntered(QDate newDate)
{
  if (newDate.isValid() || mHandleInvalid)
    checkMinDate(newDate);
}

void DateEdit::lineEnterPressed()
{
  QDate newDate = readDate();
  if (newDate.isValid() || currentText().isEmpty())
  {
    // Update the edit. This is needed if the user has entered a
    // word rather than the actual date.
    if (checkMinDate(newDate))
      return;
  }

  // Invalid value - reset to the previous date
  mTextChanged = false;
  setDate(value);
  emit dateChanged(value);
}

// Check a new date against any minimum date. The new date's general
// validity must have been checked by the caller.
// If alright, set the date.
// If too early, display a message.
bool DateEdit::checkMinDate(const QDate& newDate)
{
  if (newDate.isValid() && minDate.isValid() && newDate < minDate)
  {
    QString minString;
    if (minDate == QDate::currentDate())
      minString = i18n("today");
    else
      minString = KGlobal::locale()->formatDate(minDate, true);
    KMessageBox::sorry(this, i18n("Date cannot be earlier than %1").arg(minString));
    return false;
  }
  setDate(newDate);
  emit dateChanged(newDate);
  return true;
}

bool DateEdit::inputIsValid()
{
  return readDate().isValid();
}

QDate DateEdit::readDate() const
{
  QString text = currentText();
  QDate date;

  if (mKeywordMap.contains(text.lower()))
  {
    int i = mKeywordMap[text.lower()];
    if (i >= 100)
    {
      /* A day name has been entered. Convert to offset from today.
       * This uses some math tricks to figure out the offset in days
       * to the next date the given day of the week occurs. There
       * are two cases, that the new day is >= the current day, which means
       * the new day has not occured yet or that the new day < the current day,
       * which means the new day is already passed (so we need to find the
       * day in the next week).
       */
      i -= 100;
      int currentDay = QDate::currentDate().dayOfWeek();
      if (i >= currentDay)
        i -= currentDay;
      else
        i += 7 - currentDay;
    }
    date = QDate::currentDate().addDays(i);
  }
  else
  {
    date = KGlobal::locale()->readDate(text);
  }

  return date;
}

void DateEdit::focusOutEvent(QFocusEvent*)
{
kdDebug(5950)<<"DateEdit::focusOutEvent\n";
  // We only process the focus out event if the text has changed
  // since we got focus
  if (mTextChanged)
  {
    lineEnterPressed();
    mTextChanged = false;
  }
}

void DateEdit::slotTextChanged(const QString &)
{
  mTextChanged = true;
}

#if QT_VERSION < 300
void DateEdit::mousePressEvent(QMouseEvent *e)
{
    if ( e->button() != LeftButton )
        return;
//KDE 2.3.1: area excluding arrow button is:
    QRect editRect = style().comboButtonRect(0, 0, width(), height());
    int xborder = editRect.left();
    int yborder = editRect.top();
    int left = editRect.width() + 2*xborder;
    QRect arrowRect(left, yborder, width() - left - xborder, height() - 2*yborder);
    if (arrowRect.contains(e->pos()))
        popup();
    else
      QComboBox::mousePressEvent(e);
//KDE 3.0: arrow button area is:
//        style().querySubControlMetrics(QStyle::CC_ComboBox, this, QStyle::SC_ComboBoxArrow);
}
#endif
