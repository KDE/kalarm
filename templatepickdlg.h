/*
 *  templatepickdlg.h  -  dialog to choose an alarm template
 *  Program:  kalarm
 *  Copyright © 2004,2006-2008,2010 by David Jarvie <djarvie@kde.org>
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

#include "kaevent.h"
#include <kdialog.h>

class QResizeEvent;
namespace KCal { class Event; }
#ifdef USE_AKONADI
class TemplateListModel;
#else
class TemplateListFilterModel;
#endif
class TemplateListView;


class TemplatePickDlg : public KDialog
{
        Q_OBJECT
    public:
        explicit TemplatePickDlg(KAEvent::Actions, QWidget* parent = 0);
#ifdef USE_AKONADI
        KAEvent        selectedTemplate() const;
#else
        const KAEvent* selectedTemplate() const;
#endif
    protected:
        virtual void   resizeEvent(QResizeEvent*);
    private slots:
        void           slotSelectionChanged();
    private:
#ifdef USE_AKONADI
        TemplateListModel* mListFilterModel;
#else
        TemplateListFilterModel* mListFilterModel;
#endif
        TemplateListView*  mListView;
};

#endif // TEMPLATEPICKDLG_H

// vim: et sw=4:
