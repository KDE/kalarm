/*
 *  dateedit.h  -  date entry widget
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
 *
 *  Based on libkdepim/kdateedit.h.
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
#ifndef DATEEDIT_H
#define DATEEDIT_H

#include <qhbox.h>
#include <qvbox.h>
#include <qdatetime.h>
#include <qcombobox.h>
#include <qmap.h>

class QPushButton;
class QEvent;
class KDatePicker;

/**
* A date editing widget that consists of an editable combo box.
* The combo box contains the date in text form,
* and its arrow button will display a 'popup' style date picker.
*
* This widget also supports advanced features like allowing the user
* to type in the day name to get the date. The following keywords
* are supported (in the native language): tomorrow, yesterday, today,
* monday, tuesday, wednesday, thursday, friday, saturday, sunday.
*
* @author Cornelius Schumacher <schumacher@kde.org>
* @author Mike Pilone <mpilone@slac.com>
* @author David Jarvie <software@astrojar.org.uk>
*/
class DateEdit : public QComboBox
{
    Q_OBJECT
  public:
    DateEdit(QWidget *parent=0, const char *name=0);
    virtual ~DateEdit();

    /** @return True if the date in the text edit is valid,
    * false otherwise. This will not modify the display of the date,
    * but only check for validity.
    */
    bool inputIsValid();

    /** @return The date entered. This will not
    * modify the display of the date, but only return it.
    */
    QDate date() const;

    /** @param handleInvalid If true the date edit accepts invalid dates
    * and displays them as the empty ("") string. It also returns an invalid date.
    * If false (default) invalid dates are not accepted and instead the date
    * of today will be returned.
    */
    void setHandleInvalid(bool handleInvalid);

    void setMinDate(const QDate& d)   { minDate = d; }

  signals:
    /** This signal is emitted whenever the user modifies the date. This
    * may not get emitted until the user presses enter in the line edit or
    * focus leaves the widget (ie: the user confirms their selection).
    */
    void dateChanged(QDate);

  public slots:
    /** Sets the date.
    *
    * @param date The new date to display. This date must be valid or
    * it will not be displayed.
    */
    void setDate(const QDate&);

  protected slots:
    void slotDateSelected(QDate);
    void slotDateEntered(QDate);
    void slotTextChanged(const QString &);
    void lineEnterPressed();

  protected:
    virtual void popup();
#if QT_VERSION < 300
    virtual void mousePressEvent(QMouseEvent*);
#endif
    virtual void focusOutEvent(QFocusEvent*);

  private:
    bool checkMinDate(const QDate&);

    /** Reads the text from the combo box. If the text is a keyword, the
    * word will be translated to a date. If the text is not a keyword, the
    * text will be interpreted as a date.
    */
    QDate readDate() const;

    /** Maps the text that the user can enter to the offset in days from
    * today. For example, the text 'tomorrow' is mapped to +1.
    */
    QMap<QString, int> mKeywordMap;
    bool mTextChanged;
    bool mHandleInvalid;

    KDatePicker *mDatePicker;
    QVBox *mDateFrame;
    QDate  minDate;
    QDate  value;    // last valid date entered
};

#endif // DATEEDIT_H
