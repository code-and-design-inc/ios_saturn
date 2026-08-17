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
#ifndef UISETUPDEVICEPAGE_H
#define UISETUPDEVICEPAGE_H

#include "ui_UISetupDevicePage.h"
#include "../QtYabause.h"

class QTimer;

/* Lets the user pick which physical device drives Saturn port 1 pad 1,
   before UISetupInputPage walks them through binding its buttons. Split out
   from that page (rather than merged into its top two widgets) because Task 5
   needs to know the device choice before deciding whether the button page can
   be skipped at all. */
class UISetupDevicePage : public QWizardPage, public Ui::UISetupDevicePage
{
    Q_OBJECT

public:
    explicit UISetupDevicePage(QWidget* parent = 0);

    void initializePage();
    void cleanupPage();
    bool validatePage();

    QString selectedDeviceId() const;
    QString selectedDeviceName() const;
    //! What was in the ini when the page opened.
    QString storedDeviceId() const;
    bool deviceChanged() const;

private slots:
    void deviceComboChanged(int index);
    void hotplugTimeout();

private:
    void repopulate();
    void updateHint();

    QTimer* mHotplugTimer;
    int mDeviceGeneration;
    QString mStoredDeviceId;
    /* False until the page has been set up once. Guards against
       initializePage() re-reading the stored device (and so losing track of
       whether the user actually changed it) when the wizard revisits this
       page after Back from a later page. */
    bool mStarted;
};

#endif /* UISETUPDEVICEPAGE_H */
