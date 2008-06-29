/*
 *  dialogscroll.h  -  dialog scrolling when too high for screen
 *  Program:  kalarm
 *  Copyright © 2008 by David Jarvie <djarvie@kde.org>
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

#ifndef DIALOGSCROLL_H
#define DIALOGSCROLL_H

#include <QScrollArea>
#include <QList>


/*=============================================================================
= Class DialogScroll
= A widget to contain the tab contents, allowing the contents to scroll if
= the dialog is too high to fit the screen.
=============================================================================*/
template <class T>
class DialogScroll : public QScrollArea
{
	public:
		explicit DialogScroll(QWidget* parent = 0);
		~DialogScroll();
		virtual QSize sizeHint() const  { return minimumSizeHint(); }
		virtual QSize minimumSizeHint() const;
		static QSize  initMinimumHeight(T*, QWidget* tabs);
		static void   setSized()        { mSized = true; }
		static bool   sized()           { return mSized; }
	private:
		static QList<DialogScroll<T>*> mTabs;
		static int    mMinHeight;
		static bool   mSized;
};


#include "kalarm.h"
#include "dialogscroll.h"
#include "functions.h"
#include <kdialog.h>
#include <QStyle>

namespace KAlarm { QRect desktopWorkArea(); }

template <class T>
QList<DialogScroll<T>*> DialogScroll<T>::mTabs;
template <class T>
int  DialogScroll<T>::mMinHeight = -1;
template <class T>
bool DialogScroll<T>::mSized = false;

template <class T>
DialogScroll<T>::DialogScroll(QWidget* parent)
	: QScrollArea(parent)
{
	setFrameStyle(QFrame::NoFrame);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setWidgetResizable(true);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	mTabs += this;
}

template <class T>
DialogScroll<T>::~DialogScroll()
{
	mTabs.removeAll(this);
}

template <class T>
QSize DialogScroll<T>::minimumSizeHint() const
{
	if (!widget())
		return QSize();
	QSize s = widget()->minimumSizeHint();
	if (mMinHeight > 0)
		return QSize(s.width() + style()->pixelMetric(QStyle::PM_ScrollBarExtent), mMinHeight);
	return s;
}

/******************************************************************************
* Return the minimum size for the dialog.
* If the minimum size would be too high to fit the desktop, the tab contents
* are made scrollable.
*/
template <class T>
QSize DialogScroll<T>::initMinimumHeight(T* dlg, QWidget* tabs)
{
	if (mSized)
		return QSize();
	int maxHeight = 0;
	for (int i = 0, end = mTabs.count();  i < end;  ++i)
	{
		if (!mTabs[i]->widget())
			return QSize();
		QSize s = mTabs[i]->widget()->minimumSizeHint();
		if (!s.isValid())
			return QSize();
		if (s.height() > maxHeight)
			maxHeight = s.height();
	}
	int decoration = dlg->frameGeometry().height() - dlg->geometry().height();
	if (!decoration)
	{
		// On X11 at least, the window decoration height may not be
		// available, so assume an arbitrary 20 pixels.
		decoration = 20;
	}
	int desk = KAlarm::desktopWorkArea().height();
	// There is no stored size, or the deferral group is visible.
	// Allow the tab contents to be scrolled vertically if that is necessary
	// to avoid the dialog exceeding the screen height.
	QSize s = dlg->KDialog::minimumSizeHint();
	int y = s.height() + decoration - desk;
	if (y > 0)
	{
		mMinHeight = maxHeight - y;
		if (mMinHeight > 0)
		{
			for (int i = 0, end = mTabs.count();  i < end;  ++i)
			{
				mTabs[i]->setMinimumHeight(mMinHeight);
				mTabs[i]->resize(QSize(mTabs[i]->width(), mMinHeight));
			}
		}
		mSized = true;
		tabs->setMinimumHeight(tabs->minimumSizeHint().height());
		s = dlg->KDialog::minimumSizeHint();
		dlg->setMinimumHeight(s.height());
	}
	else
	{
		for (int i = 0, end = mTabs.count();  i < end;  ++i)
			mTabs[i]->setMinimumHeight(maxHeight);
		mSized = true;
	}
	dlg->resize(s);
	return s;
}

#endif // DIALOGSCROLL_H
