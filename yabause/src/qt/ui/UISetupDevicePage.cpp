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
#include "UISetupDevicePage.h"
#include "InputDeviceCombo.h"
#include "../InputPortConfig.h"
#include "../Settings.h"

#include <QTimer>

/* The wizard only configures the first pad on the first port. Everything else
   stays in the Settings dialog. */
static const uint kWizardPort = 1;
static const uint kWizardPad = 1;

UISetupDevicePage::UISetupDevicePage(QWidget* parent)
    : QWizardPage(parent)
    , mHotplugTimer(0)
    , mDeviceGeneration(0)
    , mStarted(false)
{
    setupUi(this);

    mHotplugTimer = new QTimer(this);
    mHotplugTimer->setInterval(1000);
    connect(mHotplugTimer, SIGNAL(timeout()), this, SLOT(hotplugTimeout()));
    connect(cbDevice, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceComboChanged(int)));

    QtYabause::retranslateWidget(this);
}

void UISetupDevicePage::initializePage()
{
    setTitle(QtYabause::translate("Controls"));
    setSubTitle(QtYabause::translate("Which device do you want to play with?"));

    /* QWizard runs this again every time the page becomes current, including
       when the user presses Back from the button page. Re-reading the stored
       device then would treat the selection they just made as "stored" and
       deviceChanged() would go false, so the bindings would never be
       cleared. Read it once. */
    if (!mStarted)
    {
        /* SdlDeviceSource asks the peripheral core what is plugged in, and the
           core reports nothing until it has been initialised. On first launch
           the wizard runs before the emulation thread exists, so nothing else
           has done it yet. */
        PerInterface_struct* core = QtYabause::getPERCore(QtYabause::defaultPERCore().id);
        if (core)
            core->Init();

        mStoredDeviceId = InputPortConfig::configuredDevice(QtYabause::settings(), kWizardPort, kWizardPad);
        mStarted = true;
    }

    repopulate();
    updateHint();

#ifdef HAVE_LIBSDL
    /* Sync the generation counter to what is plugged in right now, so the
       first tick of the timer below does not immediately see a "change" and
       repopulate a combo that was just populated. */
    mDeviceGeneration = PERSDLJoyRefreshDevices();
#endif
    mHotplugTimer->start();
}

void UISetupDevicePage::cleanupPage()
{
    /* Called when the user goes Back from this page. See validatePage() for
       why the timer also has to be stopped on the forward path. */
    if (mHotplugTimer->isActive())
        mHotplugTimer->stop();
}

bool UISetupDevicePage::validatePage()
{
    /* cleanupPage() is only invoked by QWizard when the user goes Back from
       this page; it is NOT called when Next or Finish moves the wizard
       forward. Without stopping the timer here too, it would keep polling for
       hot-plug changes and rebuilding cbDevice while the Input/Games/Ready
       pages are on screen. validatePage() runs on both the Next and Finish
       paths, so this closes that gap. */
    if (mHotplugTimer->isActive())
        mHotplugTimer->stop();
    return true;
}

void UISetupDevicePage::repopulate()
{
    InputPortConfig::SdlDeviceSource source;

    /* Keep whatever the user already picked across a hot-plug rebuild; fall
       back to the stored device, and to InputPortConfig's own default (first
       recognised gamepad, else keyboard) when the settings have none. */
    QString current = InputDeviceCombo::selectedDeviceId(cbDevice);
    if (current.isEmpty())
    {
        current = mStoredDeviceId;
        if (current.isEmpty() || current == InputPortConfig::KeyboardDeviceId)
        {
            QString name;
            const QString suggested = InputPortConfig::defaultDeviceForPort(source, kWizardPort, &name);
            if (!suggested.isEmpty())
                current = suggested;
        }
    }

    const QString storedName = QtYabause::settings()
        ->value(InputPortConfig::deviceNameKey(kWizardPort, kWizardPad)).toString();

    InputDeviceCombo::fill(cbDevice, PERPAD,
        InputPortConfig::choicesForPort(source, PERPAD, current, storedName), current);
}

void UISetupDevicePage::updateHint()
{
    InputPortConfig::SdlDeviceSource source;
    const QString current = InputDeviceCombo::selectedDeviceId(cbDevice);
    const QString storedName = QtYabause::settings()
        ->value(InputPortConfig::deviceNameKey(kWizardPort, kWizardPad)).toString();

    /* Recomputed rather than cached from repopulate(): the two calls happen
       together on every path (initializePage(), hotplugTimeout(), and a
       manual selection through deviceComboChanged()), so there is no state to
       thread through, and this keeps the "what kind of device is this" logic
       in exactly one place: InputPortConfig::choicesForPort(). */
    const QList<InputPortConfig::Choice> choices =
        InputPortConfig::choicesForPort(source, PERPAD, current, storedName);

    QString hint;
    for (int i = 0; i < choices.size(); ++i)
    {
        if (choices.at(i).id != current)
            continue;

        switch (choices.at(i).kind)
        {
        case InputPortConfig::ChoiceGamepad:
            hint = QtYabause::translate("This gamepad is recognised. It works without any setup.");
            break;
        case InputPortConfig::ChoiceUnknownGamepad:
            hint = QtYabause::translate("This device cannot be set up automatically. Assign each button on the next page.");
            break;
        case InputPortConfig::ChoiceHostInput:
        case InputPortConfig::ChoiceNone:
        case InputPortConfig::ChoiceDisconnected:
        default:
            break;
        }
        break;
    }

    lHint->setText(hint);
}

void UISetupDevicePage::deviceComboChanged(int index)
{
    if (index < 0)
        return;
    updateHint();
}

void UISetupDevicePage::hotplugTimeout()
{
#ifdef HAVE_LIBSDL
    const int generation = PERSDLJoyRefreshDevices();
    if (generation == mDeviceGeneration)
        return;
    mDeviceGeneration = generation;
    repopulate();
    updateHint();
#endif
}

QString UISetupDevicePage::selectedDeviceId() const
{
    return InputDeviceCombo::selectedDeviceId(cbDevice);
}

QString UISetupDevicePage::selectedDeviceName() const
{
    return InputDeviceCombo::selectedDeviceName(cbDevice);
}

QString UISetupDevicePage::storedDeviceId() const
{
    return mStoredDeviceId;
}

bool UISetupDevicePage::deviceChanged() const
{
    return selectedDeviceId() != mStoredDeviceId;
}
