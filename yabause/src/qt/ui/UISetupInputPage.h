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
#ifndef UISETUPINPUTPAGE_H
#define UISETUPINPUTPAGE_H

#include "ui_UISetupInputPage.h"
#include "../QtYabause.h"
#include "UISetupInputLatch.h"

#include <QList>
#include <QMap>
#include <QVariant>

class QTimer;
class QToolButton;
class UISetupDevicePage;

/* Walks the user through the thirteen pad buttons one at a time. The scanning
   itself mirrors UIControllerSetting, but that class is a QDialog and cannot
   also be a QWizardPage, so the loop lives here. */
class UISetupInputPage : public QWizardPage, public Ui::UISetupInputPage
{
    Q_OBJECT

public:
    explicit UISetupInputPage(QWidget* parent = 0);
    ~UISetupInputPage();

    //! The device page is created first and outlives this one; the wizard
    //! wires them together in its constructor.
    void setDevicePage(UISetupDevicePage* page);

    void initializePage();
    void cleanupPage();
    bool validatePage();

    /* The mapping the button page started from (the selected device's
       defaults after a device change, the stored bindings otherwise), and the
       subset of it the user actually pressed an input for. Task 6 feeds both
       into setupInputPlan() to decide what the wizard writes. */
    QMap<u8, u32> startingPoint() const;
    QMap<u8, u32> userAssigned() const;

    /* How many of the pad buttons the user actually bound, and how many there
       are in total. The summary page reports the count instead of dumping the
       raw key codes, which mean nothing to a user. */
    int assignedCount() const;
    int buttonCount() const;

protected:
    void keyPressEvent(QKeyEvent* event);
    /* Bindings are committed on release, not on press - see UISetupInputLatch.
       The keyboard needs the same treatment as the pad: auto-repeat would
       otherwise deliver a held key over and over and bind it to several
       Saturn buttons in a row. */
    void keyReleaseEvent(QKeyEvent* event);
    /* Paints the checked ("current step") pad button the same way
       UIControllerSetting::eventFilter() does for the Settings dialog. Copied
       rather than shared because UIControllerSetting is a QDialog and this is
       a QWizardPage; C++ has no way to inherit from both. */
    bool eventFilter(QObject* object, QEvent* event);

private slots:
    void timerTimeout();
    void previousClicked();
    void skipClicked();
    void skipAllClicked();

private:
    //! True when a key press belongs to this port (host input, and scanning).
    bool acceptsKeyboard() const;
    void startStep(int index);
    void assign(u32 key);
    void stopScanning();
    //! Reads Input/Port/1/Id/1/Controller/<PERPAD>/Key/* back into a map, the
    //! same way YabauseThread::reloadControllers() does for the PERPAD branch.
    QMap<u8, u32> storedBindings() const;

    UISetupDevicePage* mDevicePage;
    PerInterface_struct* mCore;
    QTimer* mTimer;
    int mStep;
    u32 mScanFlags;
    //! Holds the input seen on this step until the user lets go of it.
    UISetupInputLatch mLatch;
    /* False until the page has been set up once. Guards against
       initializePage() wiping the user's assignments when they press Back
       from a later page and return here. */
    bool mStarted;
    QList<u8> mOrder;
    QMap<u8, QString> mNames;
    /* The mapping the current step-through started from, and which device it
       was computed for. Recomputed (and mUserAssigned cleared) only when the
       selected device differs from mStartingPointDeviceId -- see
       initializePage(). */
    QMap<u8, u32> mStartingPoint;
    QString mStartingPointDeviceId;
    //! Only the buttons the user actually pressed an input for, keyed by
    //! PERPAD_*. Skipped buttons are absent and keep mStartingPoint's value.
    QMap<u8, u32> mUserAssigned;
    /* Maps each pad key to the QToolButton positioned over it on the picture
       (tbUp, tbX, ...), so startStep()/assign() can drive the highlight and
       the "done" icon without a long if/else chain. */
    QMap<u8, QToolButton*> mPadButtons;
};

#endif /* UISETUPINPUTPAGE_H */
