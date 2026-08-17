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
#ifndef UISETUPGAMEPAGE_H
#define UISETUPGAMEPAGE_H

#include "ui_UISetupGamePage.h"

class UISetupGamePage : public QWizardPage, public Ui::UISetupGamePage
{
    Q_OBJECT

public:
    explicit UISetupGamePage(QWidget* parent = 0);

    void initializePage();
    QString gameDirectory() const;
    QString cdDrive() const;

    /* Writes the two stores this page owns. Called from UISetupWizard::done()
       only when the wizard is accepted. */
    void applyChoices();

private slots:
    void browseClicked();

private:
    /* Whether any optical drive was detected at construction time. Used
       instead of cbDrive->isVisible(), because by the time done() runs the
       user has moved on to the Summary page and this page's widgets are no
       longer visible (QWizard hides pages that are not current), which would
       make an isVisible() check silently discard a valid selection. */
    bool mHasDrive;
    /* False until the page has been set up once. QWizard calls
       initializePage() every time the page becomes current, so without this a
       folder the user browsed to would be overwritten by the stored
       lastDirectory as soon as they press Back from the Summary page.
       UISetupInputPage carries the same guard for the same reason.

       QWizard::IndependentPages would suppress the repeat call globally, but
       it must not be used here: it also stops cleanupPage() from running and
       calls initializePage() only once per page, which would break
       UISetupInputPage -- that page relies on initializePage() to restart its
       scan timer when the user comes back to it. */
    bool mStarted;
};

#endif /* UISETUPGAMEPAGE_H */
