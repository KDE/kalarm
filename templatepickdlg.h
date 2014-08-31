/*
 *  templatepickdlg.h  -  dialog to choose an alarm template
 *  Program:  kalarm
 *  Copyright © 2004-2011 by David Jarvie <djarvie@kde.org>
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
#ifndef TEMPLATEPICKDLG_H
#define TEMPLATEPICKDLG_H

#include <kalarmcal/kaevent.h>

#include <QDialog>
class QPushButton;
class QResizeEvent;
namespace KCal { class Event; }
class TemplateListModel;
class TemplateListView;

using namespace KAlarmCal;

class TemplatePickDlg : public QDialog
{
        Q_OBJECT
    public:
        explicit TemplatePickDlg(KAEvent::Actions, QWidget* parent = 0);
        KAEvent        selectedTemplate() const;
    protected:
        virtual void   resizeEvent(QResizeEvent*);
    private slots:
        void           slotSelectionChanged();
    private:
        TemplateListModel* mListFilterModel;
        TemplateListView*  mListView;
        QPushButton*       mOkButton;
};

#endif // TEMPLATEPICKDLG_H

// vim: et sw=4:
