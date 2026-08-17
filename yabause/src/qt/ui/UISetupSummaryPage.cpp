/*  Copyright 2026 devMiyax

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "UISetupSummaryPage.h"
#include "UISetupWizard.h"
#include "../QtYabause.h"

#include <QHeaderView>
#include <QList>
#include <QPair>
#include <QTableWidgetItem>

UISetupSummaryPage::UISetupSummaryPage(QWidget* parent)
    : QWizardPage(parent)
{
    setupUi(this);
    twSummary->setHorizontalHeaderLabels(QStringList()
        << QtYabause::translate("Setting")
        << QtYabause::translate("Value"));
    twSummary->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    twSummary->verticalHeader()->setVisible(false);
    QtYabause::retranslateWidget(this);
}

void UISetupSummaryPage::initializePage()
{
    setTitle(QtYabause::translate("Ready"));
    setSubTitle(QtYabause::translate("Review your choices before applying them."));

    /* No one-shot guard here, unlike the other pages: this page has no state
       of its own to lose, and it MUST rebuild every time it becomes current
       so that a choice the user changed after going Back is reflected. */
    UISetupWizard* w = qobject_cast<UISetupWizard*>(wizard());
    const QList<QPair<QString, QString> > rows =
        w ? w->summaryRows() : QList<QPair<QString, QString> >();

    twSummary->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        QTableWidgetItem* label = new QTableWidgetItem(rows.at(i).first);
        QTableWidgetItem* value = new QTableWidgetItem(rows.at(i).second);
        label->setFlags(label->flags() ^ Qt::ItemIsEditable);
        value->setFlags(value->flags() ^ Qt::ItemIsEditable);
        twSummary->setItem(i, 0, label);
        twSummary->setItem(i, 1, value);
    }
}
