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
#ifndef UISETUPWIZARD_H
#define UISETUPWIZARD_H

#include <QList>
#include <QMap>
#include <QPair>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QWizard>

class Settings;
class UISetupPresetPage;
class UISetupDevicePage;
class UISetupInputPage;
class UISetupGamePage;
class UISetupSummaryPage;

/* First-launch onboarding. Nothing is written to the ini until the user
   presses Finish, so cancelling out leaves an existing installation alone. */
class UISetupWizard : public QWizard
{
    Q_OBJECT

public:
    /* Bumping this re-runs the wizard once for everybody, which is how a
       future revision can introduce a new preset to existing users. */
    static const int kWizardVersion = 1;

    enum PageId
    {
        PagePreset = 0,
        PageDevice = 1,
        PageInput = 2,
        PageGame = 3,
        PageSummary = 4
    };

    explicit UISetupWizard(QWidget* parent = 0);

    /* The ini key holding the wizard version the user last completed. */
    static QString versionKey() { return QString("General/SetupWizardVersion"); }

    /* The decision itself, over an arbitrary settings store. Split out so it
       can be unit tested: shouldRun() reads QtYabause::settings(), which is a
       process-wide singleton backed by the developer's real ini. Defined
       inline so a test can use it without linking the wizard, its four pages
       and all of QtYabause. */
    static bool shouldRunForSettings(QSettings* settings)
    {
        if (!settings)
            return true;
        return settings->value(versionKey(), 0).toInt() < kWizardVersion;
    }

    /* Counterpart of shouldRunForSettings(), for the same reason. */
    static void markDoneForSettings(QSettings* settings)
    {
        if (settings)
            settings->setValue(versionKey(), kWizardVersion);
    }

    static bool shouldRun();
    static void markDone();

    /* The preset page's pending ini key/value pairs. Input is handled
       separately by applyInputChoices(), and the Games page writes straight
       to its own stores via applyChoices(), so this is not every key the
       wizard writes -- only the flat subset the preset page contributes. */
    void collectPending(QMap<QString, QVariant>* out) const;

    /* Translated (label, value) rows describing what the user chose, for the
       summary page. Assembled here because the wizard is what owns the pages
       the answers are spread across -- in particular the Games page, whose
       choices go to two different stores and never pass through
       collectPending(). */
    QList<QPair<QString, QString> > summaryRows() const;

protected:
    void done(int result);
    /* Overridden so a NoDeviceId choice on the device page skips straight to
       the game page: there is nothing for the button page to ask about when
       the port has no device. */
    int nextId() const;

private:
    /* Writes the input port choice for Task 6: what setupInputPlan() decided,
       via the device and button pages' answers. */
    void applyInputChoices(Settings* s);

    UISetupPresetPage* mPresetPage;
    UISetupDevicePage* mDevicePage;
    UISetupInputPage* mInputPage;
    UISetupGamePage* mGamePage;
    UISetupSummaryPage* mSummaryPage;
};

#endif /* UISETUPWIZARD_H */
