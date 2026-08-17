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
#include "UISetupWizard.h"
#include "UISetupPresetPage.h"
#include "UISetupDevicePage.h"
#include "UISetupInputPage.h"
#include "UISetupGamePage.h"
#include "UISetupSummaryPage.h"
#include "UISetupInputPlan.h"
#include "../InputPortConfig.h"
#include "../QtYabause.h"
#include "../Settings.h"

/* The wizard only configures the first pad on the first port, matching
   UISetupInputPage's own kWizardPort/kWizardPad. */
static const uint kWizardPort = 1;
static const uint kWizardPad = 1;

UISetupWizard::UISetupWizard(QWidget* parent)
    : QWizard(parent)
{
    mPresetPage = new UISetupPresetPage(this);
    mDevicePage = new UISetupDevicePage(this);
    mInputPage = new UISetupInputPage(this);
    mGamePage = new UISetupGamePage(this);
    mSummaryPage = new UISetupSummaryPage(this);

    mInputPage->setDevicePage(mDevicePage);

    setPage(PagePreset, mPresetPage);
    setPage(PageDevice, mDevicePage);
    setPage(PageInput, mInputPage);
    setPage(PageGame, mGamePage);
    setPage(PageSummary, mSummaryPage);

    setWindowTitle(QtYabause::translate("Yaba Sanshiro Setup"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);

    /* QWizard's own buttons come from Qt's translation catalogs, which this
       application does not ship. Route them through mini18n like everything
       else. */
    setButtonText(QWizard::NextButton, QtYabause::translate("Next"));
    setButtonText(QWizard::BackButton, QtYabause::translate("Back"));
    setButtonText(QWizard::FinishButton, QtYabause::translate("Finish"));
    setButtonText(QWizard::CancelButton, QtYabause::translate("Cancel"));
}

bool UISetupWizard::shouldRun()
{
    return shouldRunForSettings(QtYabause::settings());
}

void UISetupWizard::markDone()
{
    markDoneForSettings(QtYabause::settings());
}

void UISetupWizard::collectPending(QMap<QString, QVariant>* out) const
{
    if (!out)
        return;
    const QMap<QString, QVariant> preset = mPresetPage->pendingValues();
    QMap<QString, QVariant>::const_iterator it;
    for (it = preset.constBegin(); it != preset.constEnd(); ++it)
        out->insert(it.key(), it.value());

    /* Input is deliberately not collected here: what the wizard writes for
       the device/button pages depends on both of them together
       (setupInputPlan(), applied separately by applyInputChoices()) and is
       not a flat set of ini keys the way the preset page's choices are. */
}

QList<QPair<QString, QString> > UISetupWizard::summaryRows() const
{
    QList<QPair<QString, QString> > rows;

    /* Graphics: exactly the rows the preset page already showed, so the user
       recognises them. The label is always a phrase, but the value can be a
       bare numeral ("32", "4x") that must not be looked up in the catalog. */
    const QList<QPair<QString, QString> > preset = mPresetPage->summaryRows();
    for (int i = 0; i < preset.size(); ++i)
    {
        const QString value = graphicsPresetValueIsTranslatable(preset.at(i).second)
            ? QtYabause::translate(preset.at(i).second)
            : preset.at(i).second;
        rows << qMakePair(QtYabause::translate(preset.at(i).first), value);
    }

    /* Controls: how many buttons were bound, and which device drives the
       port. The raw key codes behind the count are meaningless to a user.
       A NoDeviceId choice binds nothing at all, so "Use the default
       assignment" would be a lie -- the port is inert, not defaulted. */
    const int assigned = mInputPage->assignedCount();
    QString buttonsAssigned;
    if (mDevicePage->selectedDeviceId() == InputPortConfig::NoDeviceId)
        buttonsAssigned = QtYabause::translate("None");
    else if (assigned == 0)
        buttonsAssigned = QtYabause::translate("Use the default assignment");
    else
        buttonsAssigned = QString("%1 / %2").arg(assigned).arg(mInputPage->buttonCount());
    rows << qMakePair(QtYabause::translate("Buttons assigned"), buttonsAssigned);

    rows << qMakePair(QtYabause::translate("Input device"),
        mDevicePage->selectedDeviceName().isEmpty()
            ? QtYabause::translate("Keyboard")
            : mDevicePage->selectedDeviceName());

    /* Games: applyChoices() writes these to two different stores, so they
       never pass through collectPending() and would otherwise be missing from
       the summary entirely. The labels are the ones the Games page itself
       uses; the values are a path and a drive name and are never translated. */
    const QString directory = mGamePage->gameDirectory();
    rows << qMakePair(QtYabause::translate("Folder that contains your CHD or ISO files"),
        directory.isEmpty() ? QtYabause::translate("Not set") : directory);

    const QString drive = mGamePage->cdDrive();
    rows << qMakePair(QtYabause::translate("Optical drive to play discs from"),
        drive.isEmpty() ? QtYabause::translate("Do not use an optical drive") : drive);

    return rows;
}

int UISetupWizard::nextId() const
{
    /* Nothing to assign when the port has no device: the button page would
       show thirteen steps that can never be answered. */
    if (currentId() == PageDevice
        && mDevicePage->selectedDeviceId() == InputPortConfig::NoDeviceId)
        return PageGame;
    return QWizard::nextId();
}

void UISetupWizard::applyInputChoices(Settings* s)
{
    // No Type key yet means the port has never been written by this wizard or
    // the settings dialog. On such a port, writing nothing does not mean
    // "leave it alone" -- YabauseThread::reloadControllers() finds Device
    // empty and calls InputPortConfig::seedPort() instead, which can pick a
    // gamepad even after the user explicitly chose Keyboard here.
    const bool portUnconfigured =
        !s->contains(InputPortConfig::typeKey(kWizardPort, kWizardPad));

    const SetupInputResult plan = setupInputPlan(
        mDevicePage->storedDeviceId(),
        mDevicePage->selectedDeviceId(),
        mDevicePage->selectedDeviceName(),
        mInputPage->startingPoint(),
        mInputPage->userAssigned(),
        portUnconfigured);

    if (!plan.write)
        return;

    if (plan.clearFirst)
    {
        /* Bindings recorded against the previous device stay in the settings
           otherwise: the new device's defaults do not necessarily cover every
           button, and an unrecognised pad has no defaults at all, so the old
           codes would survive and keep firing in game. */
        InputPortConfig::clearBindings(s, kWizardPort, kWizardPad, PERPAD);
        /* A physical pad drives one Saturn port. Taking it for port 1 has to
           release it from port 2, or both ports answer to the same hardware. */
        InputPortConfig::releaseDeviceFromOtherPorts(s, kWizardPort, kWizardPad, plan.deviceId);
    }

    /* All three of these, always. reloadControllers() reads Type to decide
       what to connect (an absent Type is 0 = Unconnected, so the port gets no
       controller), and Device to resolve which physical device the stored
       codes belong to (an absent Device reads as the keyboard, which leaves
       the settings dialog showing "Keyboard" for a port bound to pad codes). */
    s->setValue(InputPortConfig::typeKey(kWizardPort, kWizardPad), PERPAD);
    s->setValue(InputPortConfig::deviceKey(kWizardPort, kWizardPad), plan.deviceId);
    s->setValue(InputPortConfig::deviceNameKey(kWizardPort, kWizardPad), plan.deviceName);

    for (QMap<u8, u32>::const_iterator it = plan.bindings.constBegin();
         it != plan.bindings.constEnd(); ++it)
        s->setValue(InputPortConfig::bindingKey(kWizardPort, kWizardPad, PERPAD, it.key()),
                    (quint32)it.value());
}

void UISetupWizard::done(int result)
{
    if (result == QDialog::Accepted)
    {
        QMap<QString, QVariant> pending;
        collectPending(&pending);

        Settings* s = QtYabause::settings();
        QMap<QString, QVariant>::const_iterator it;
        for (it = pending.constBegin(); it != pending.constEnd(); ++it)
            s->setValue(it.key(), it.value());

        applyInputChoices(s);
        mGamePage->applyChoices();
    }

    /* Marked done either way: a user who cancels should not be asked again on
       every launch. The Help menu can bring the wizard back. */
    markDone();
    QtYabause::settings()->sync();

    QWizard::done(result);
}
