/*
 *  msgevent.cpp  -  the event object for messages
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  djarvie@lineone.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <ctype.h>
#include <qcolor.h>
#include <kdebug.h>

#include "msgevent.h"
using namespace KCal;


void MessageEvent::set(const QDateTime& dateTime, int flags,
                       const QColor& colour, const QString& message)
{
  alarm()->setEnabled(true);        // enable the alarm
  setDtStart(dateTime);
  setDtEnd(dateTime.addDays((flags & LATE_CANCEL) ? 0 : 1));
  alarm()->setTime(dateTime);
  alarm()->setText(message);
  QStringList cats;
  cats.append(colour.name());
  if (flags & BEEP)
    cats.append("BEEP");
  setCategories(cats);
}

QColor MessageEvent::colour() const
{
  const QStringList& cats = categories();
  if (cats.count() > 0)
  {
    QColor color(cats[0]);
    if (color.isValid())
      return color;
  }
  return QColor(255, 255, 255);    // missing/invalid colour - return white
}

int MessageEvent::flags() const
{
  int flags = 0;
  const QStringList& cats = categories();
  for (unsigned int i = 1;  i < cats.count();  ++i)
  {
    if ( cats[i] == "BEEP")
      flags |= BEEP;
  }
  if (!isMultiDay())
    flags |= LATE_CANCEL;
  return flags;
}
