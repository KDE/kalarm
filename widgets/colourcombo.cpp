/*
 *  colourcombo.cpp  -  colour selection combo box
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
 *
 *  Some code taken from kdelibs/kdeui/kcolorcombo.cpp in the KDE libraries:
 *  Copyright (C) 1997 Martin Jones (mjones@kde.org)
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <qpainter.h>

#include <klocale.h>
#include <kcolordialog.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "colourcombo.h"


ColourCombo::ColourCombo(QWidget* parent, const char* name, const QColor& defaultColour)
	: QComboBox(parent, name),
	  mColourList(theApp()->preferences()->messageColours()),
	  mSelectedColour(defaultColour),
	  mCustomColour(255, 255, 255),
	  mReadOnly(false),
	  mDisabled(false)
{
	addColours();
	connect(this, SIGNAL(activated(int)), SLOT(slotActivated(int)));
	connect(this, SIGNAL(highlighted(int)), SLOT(slotHighlighted(int)));
	connect(theApp()->preferences(), SIGNAL(preferencesChanged()), SLOT(slotPreferencesChanged()));
}

void ColourCombo::setColour(const QColor& colour)
{
	mSelectedColour = colour;
	addColours();
}

/******************************************************************************
*  Set a new colour selection.
*/
void ColourCombo::setColours(const ColourList& colours)
{
	mColourList = colours;
	if (mSelectedColour != mCustomColour
	&&  !mColourList.contains(mSelectedColour))
	{
		// The current colour has been deleted
		mSelectedColour = mColourList.count() ? mColourList.first() : mCustomColour;
	}
	addColours();
}

/******************************************************************************
*  Called when the user changes the preference settings.
*  If the colour list has changed, update the colours displayed.
*/
void ColourCombo::slotPreferencesChanged()
{
	const ColourList& prefColours = theApp()->preferences()->messageColours();
	if (prefColours != mColourList)
		setColours(prefColours);      // update the display with the new colours
}

/******************************************************************************
*  Enable or disable the control.
*  If it is disabled, its colour is set to the dialog background colour.
*/
void ColourCombo::setEnabled(bool enable)
{
	if (enable  &&  mDisabled)
	{
		mDisabled = false;
		setColour(mSelectedColour);
	}
	else if (!enable  &&  !mDisabled)
	{
		mSelectedColour = color();
		int end = count();
		if (end > 1)
		{
			// Add a dialog background colour item
			QPixmap pm = *pixmap(1);
			pm.fill(paletteBackgroundColor());
			insertItem(pm);
			setCurrentItem(end);
		}
		mDisabled = true;
	}
	QComboBox::setEnabled(enable);
}

void ColourCombo::slotActivated(int index)
{
	if (index)
		mSelectedColour = mColourList[index - 1];
	else
	{
		if (KColorDialog::getColor(mCustomColour, this) == QDialog::Accepted)
		{
			QRect rect;
			drawCustomItem(rect, false);
		}
		mSelectedColour = mCustomColour;
	}
	emit activated(mSelectedColour);
}

void ColourCombo::slotHighlighted(int index)
{
	mSelectedColour = index ? mColourList[index - 1] : mCustomColour;
	emit highlighted(mSelectedColour);
}

/******************************************************************************
*  Initialise the items in the combo box to one for each colour in the list.
*/
void ColourCombo::addColours()
{
	clear();

	for (ColourList::const_iterator it = mColourList.begin();  ;  ++it)
	{
		if (it == mColourList.end())
		{
			mCustomColour = mSelectedColour;
			break;
		}
		if (mSelectedColour == *it)
			break;
	}

	QRect rect;
	drawCustomItem(rect, true);

	QPainter painter;
	QPixmap pixmap(rect.width(), rect.height());
	int i = 1;
	for (ColourList::const_iterator it = mColourList.begin();  it != mColourList.end();  ++i, ++it)
	{
		painter.begin(&pixmap);
		QBrush brush(*it);
		painter.fillRect(rect, brush);
		painter.end();

		insertItem(pixmap);
		pixmap.detach();

		if (*it == mSelectedColour.rgb())
			setCurrentItem(i);
	}
}

void ColourCombo::drawCustomItem(QRect& rect, bool insert)
{
	QPen pen;
	if (qGray(mCustomColour.rgb()) < 128)
		pen.setColor(Qt::white);
	else
		pen.setColor(Qt::black);

	QPainter painter;
	QFontMetrics fm = QFontMetrics(painter.font());
	rect.setRect(0, 0, width(), fm.height() + 4);
	QPixmap pixmap(rect.width(), rect.height());

	painter.begin(&pixmap);
	QBrush brush(mCustomColour);
	painter.fillRect(rect, brush);
	painter.setPen(pen);
	painter.drawText(2, fm.ascent() + 2, i18n("Custom..."));
	painter.end();

	if (insert)
		insertItem(pixmap);
	else
		changeItem(pixmap, 0);
	pixmap.detach();
}

void ColourCombo::setReadOnly(bool ro)
{
	mReadOnly = ro;
}

void ColourCombo::resizeEvent(QResizeEvent* re)
{
	QComboBox::resizeEvent(re);
	addColours();
}

void ColourCombo::mousePressEvent(QMouseEvent* e)
{
	if (mReadOnly)
	{
		// Swallow up the event if it's the left button
		if (e->button() == LeftButton)
			return;
	}
	QComboBox::mousePressEvent(e);
}

void ColourCombo::mouseReleaseEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QComboBox::mouseReleaseEvent(e);
}

void ColourCombo::mouseMoveEvent(QMouseEvent* e)
{
	if (!mReadOnly)
		QComboBox::mouseMoveEvent(e);
}

void ColourCombo::keyPressEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QComboBox::keyPressEvent(e);
}

void ColourCombo::keyReleaseEvent(QKeyEvent* e)
{
	if (!mReadOnly)
		QComboBox::keyReleaseEvent(e);
}
#include "colourcombo.moc"
