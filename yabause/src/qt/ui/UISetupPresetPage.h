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
#ifndef UISETUPPRESETPAGE_H
#define UISETUPPRESETPAGE_H

#include "ui_UISetupPresetPage.h"
#include "GraphicsPreset.h"

#include <QMap>
#include <QVariant>

class UISetupPresetPage : public QWizardPage, public Ui::UISetupPresetPage
{
    Q_OBJECT

public:
    explicit UISetupPresetPage(QWidget* parent = 0);

    void initializePage();
    GraphicsPresetTier selectedTier() const;
    QMap<QString, QVariant> pendingValues() const;

    /* Untranslated (label, value) rows for the selected tier. The summary
       page shows the same rows, so it must not have to re-derive them. */
    QList<QPair<QString, QString> > summaryRows() const;

private slots:
    void tierChanged();

private:
    GraphicsPresetCores mCores;
    /* False until the page has been set up once. QWizard calls
       initializePage() every time the page becomes current, so without this
       the radio button the user picked would be reset to whatever the ini
       says as soon as they press Back from a later page. UISetupInputPage
       carries the same guard for the same reason.

       QWizard::IndependentPages would suppress the repeat call globally, but
       it must not be used here: it also stops cleanupPage() from running and
       calls initializePage() only once per page, which would break
       UISetupInputPage -- that page relies on initializePage() to restart its
       scan timer when the user comes back to it. */
    bool mStarted;
};

#endif /* UISETUPPRESETPAGE_H */
