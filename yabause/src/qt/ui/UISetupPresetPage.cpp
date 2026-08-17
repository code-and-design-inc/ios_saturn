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
#include "UISetupPresetPage.h"
#include "../QtYabause.h"
#include "../Settings.h"

#include <QHeaderView>
#include <QTableWidgetItem>

UISetupPresetPage::UISetupPresetPage(QWidget* parent)
    : QWizardPage(parent)
    , mStarted(false)
{
    setupUi(this);

    mCores = graphicsPresetDetectCores();

    twDetails->setHorizontalHeaderLabels(QStringList()
        << QtYabause::translate("Setting")
        << QtYabause::translate("Value"));
    twDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    twDetails->verticalHeader()->setVisible(false);

    const bool vulkanReady = graphicsPresetIsAvailable(GraphicsPresetMiddle, mCores);
    rbHigh->setEnabled(vulkanReady);
    rbMiddle->setEnabled(vulkanReady);
    lUnavailable->setVisible(!vulkanReady);

    connect(rbHigh, SIGNAL(toggled(bool)), this, SLOT(tierChanged()));
    connect(rbMiddle, SIGNAL(toggled(bool)), this, SLOT(tierChanged()));
    connect(rbLow, SIGNAL(toggled(bool)), this, SLOT(tierChanged()));

    QtYabause::retranslateWidget(this);
}

void UISetupPresetPage::initializePage()
{
    setTitle(QtYabause::translate("Graphics"));
    setSubTitle(QtYabause::translate("Choose the preset that matches your PC. You can change any of this later in Settings."));

    /* Only preselect once. On the way back from a later page the user's own
       choice is already in the radio buttons, and re-deriving it from the ini
       would silently discard it. */
    if (mStarted)
        return;
    mStarted = true;

    /* Preselect the tier the user is already on, so an existing installation
       can walk through the wizard without having its settings rewritten. */
    Settings* s = QtYabause::settings();
    QMap<QString, QVariant> current;
    const QStringList keys = graphicsPresetValues(GraphicsPresetHigh, mCores).keys();
    for (int i = 0; i < keys.size(); ++i)
    {
        if (s->contains(keys.at(i)))
            current.insert(keys.at(i), s->value(keys.at(i)));
    }

    GraphicsPresetTier tier = graphicsPresetDefaultTier(mCores);
    if (graphicsPresetMatches(GraphicsPresetHigh, mCores, current))
        tier = GraphicsPresetHigh;
    else if (graphicsPresetMatches(GraphicsPresetMiddle, mCores, current))
        tier = GraphicsPresetMiddle;
    else if (graphicsPresetMatches(GraphicsPresetLow, mCores, current))
        tier = GraphicsPresetLow;

    if (tier == GraphicsPresetHigh && rbHigh->isEnabled())
        rbHigh->setChecked(true);
    else if (tier == GraphicsPresetMiddle && rbMiddle->isEnabled())
        rbMiddle->setChecked(true);
    else
        rbLow->setChecked(true);

    tierChanged();
}

GraphicsPresetTier UISetupPresetPage::selectedTier() const
{
    if (rbHigh->isChecked())
        return GraphicsPresetHigh;
    if (rbMiddle->isChecked())
        return GraphicsPresetMiddle;
    return GraphicsPresetLow;
}

QMap<QString, QVariant> UISetupPresetPage::pendingValues() const
{
    return graphicsPresetValues(selectedTier(), mCores);
}

QList<QPair<QString, QString> > UISetupPresetPage::summaryRows() const
{
    return graphicsPresetSummary(selectedTier(), mCores);
}

void UISetupPresetPage::tierChanged()
{
    const QList<QPair<QString, QString> > rows = summaryRows();
    twDetails->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        /* The label is always a phrase; the value may be a bare numeral such
           as "32" or "4x", which must never go through the catalog -- a
           catalog entry that happened to share that key would silently
           replace it with something unrelated. */
        QTableWidgetItem* label = new QTableWidgetItem(QtYabause::translate(rows.at(i).first));
        QTableWidgetItem* value = new QTableWidgetItem(
            graphicsPresetValueIsTranslatable(rows.at(i).second)
                ? QtYabause::translate(rows.at(i).second)
                : rows.at(i).second);
        label->setFlags(label->flags() ^ Qt::ItemIsEditable);
        value->setFlags(value->flags() ^ Qt::ItemIsEditable);
        twDetails->setItem(i, 0, label);
        twDetails->setItem(i, 1, value);
    }
}
