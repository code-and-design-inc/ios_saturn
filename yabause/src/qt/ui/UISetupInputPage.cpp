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
#include "UISetupInputPage.h"
#include "UISetupDevicePage.h"
#include "../InputPortConfig.h"
#include "../Settings.h"

#include <QKeyEvent>
#include <QTimer>
#include <QToolButton>
#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QIcon>
#include <QStringList>

/* The wizard only configures the first pad on the first port. Everything else
   stays in the Settings dialog. */
/* The code the same axis produces travelling the other way, or 0 when the
   input has no opposite. The bit layout lives with the codes themselves in the
   peripheral core; a build without SDL has no axes to worry about. */
static u32 oppositeAxisCode(u32 key)
{
#ifdef HAVE_LIBSDL
    return PERSDLJoyOppositeAxisCode(key);
#else
    Q_UNUSED(key);
    return 0;
#endif
}

static const uint kWizardPort = 1;
static const uint kWizardPad = 1;

UISetupInputPage::UISetupInputPage(QWidget* parent)
    : QWizardPage(parent)
    , mDevicePage(0)
    , mCore(0)
    , mStep(-1)
    , mScanFlags(0)
    , mStarted(false)
{
    setupUi(this);

    mTimer = new QTimer(this);
    mTimer->setInterval(25);

    mOrder << PERPAD_UP << PERPAD_DOWN << PERPAD_LEFT << PERPAD_RIGHT
           << PERPAD_A << PERPAD_B << PERPAD_C
           << PERPAD_X << PERPAD_Y << PERPAD_Z
           << PERPAD_LEFT_TRIGGER << PERPAD_RIGHT_TRIGGER << PERPAD_START;

    mNames[PERPAD_UP] = "Up";
    mNames[PERPAD_DOWN] = "Down";
    mNames[PERPAD_LEFT] = "Left";
    mNames[PERPAD_RIGHT] = "Right";
    mNames[PERPAD_A] = "A";
    mNames[PERPAD_B] = "B";
    mNames[PERPAD_C] = "C";
    mNames[PERPAD_X] = "X";
    mNames[PERPAD_Y] = "Y";
    mNames[PERPAD_Z] = "Z";
    mNames[PERPAD_LEFT_TRIGGER] = "Left trigger";
    mNames[PERPAD_RIGHT_TRIGGER] = "Right trigger";
    mNames[PERPAD_START] = "Start";

    mPadButtons[PERPAD_UP] = tbUp;
    mPadButtons[PERPAD_DOWN] = tbDown;
    mPadButtons[PERPAD_LEFT] = tbLeft;
    mPadButtons[PERPAD_RIGHT] = tbRight;
    mPadButtons[PERPAD_A] = tbA;
    mPadButtons[PERPAD_B] = tbB;
    mPadButtons[PERPAD_C] = tbC;
    mPadButtons[PERPAD_X] = tbX;
    mPadButtons[PERPAD_Y] = tbY;
    mPadButtons[PERPAD_Z] = tbZ;
    mPadButtons[PERPAD_LEFT_TRIGGER] = tbLeftTrigger;
    mPadButtons[PERPAD_RIGHT_TRIGGER] = tbRightTrigger;
    mPadButtons[PERPAD_START] = tbStart;

    QMap<u8, QToolButton*>::iterator padButtonIt;
    for (padButtonIt = mPadButtons.begin(); padButtonIt != mPadButtons.end(); ++padButtonIt)
    {
        QToolButton* button = padButtonIt.value();
        /* Unlike the Settings dialog, the wizard drives which button is
           highlighted; a click here must not be able to pick a different
           one. WA_TransparentForMouseEvents is used instead of just leaving
           the button unconnected because QToolButton::isChecked() still
           flips on click even with no slot attached to it, which would
           desync the highlight from mStep for as long as the click lasted. */
        button->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        button->installEventFilter(this);
    }

    connect(mTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    connect(pbPrevious, SIGNAL(clicked()), this, SLOT(previousClicked()));
    connect(pbSkip, SIGNAL(clicked()), this, SLOT(skipClicked()));
    connect(pbSkipAll, SIGNAL(clicked()), this, SLOT(skipAllClicked()));

    setFocusPolicy(Qt::StrongFocus);
    QtYabause::retranslateWidget(this);
}

UISetupInputPage::~UISetupInputPage()
{
    /* Backstop for stopScanning(): QWizard only calls cleanupPage() on Back and
       validatePage() on Next/Finish. Cancel, Escape, and closing the window all
       go through UISetupWizard::done(), which never touches this page, so
       without this the scan restriction from startStep() would stay pinned to
       one device and the Settings dialog could not bind anything else until
       something unrelated reset it. Same pattern as
       UIControllerSetting::~UIControllerSetting(). */
#ifdef HAVE_LIBSDL
    PERSDLJoySetScanDeviceIndex(PERSDL_SCAN_ANY_DEVICE);
#endif
}

void UISetupInputPage::setDevicePage(UISetupDevicePage* page)
{
    mDevicePage = page;
}

void UISetupInputPage::initializePage()
{
    /* Debug-only guard: Q_ASSERT compiles out in Release, so this catches a
       missing setDevicePage() wire-up in a developer build, not in what ships.
       A Release build with mDevicePage still null would crash on the first
       call below instead. */
    Q_ASSERT(mDevicePage);

    setTitle(QtYabause::translate("Controls"));
    setSubTitle(QtYabause::translate("Press each button on your keyboard or gamepad when it is highlighted."));

    /* Arriving at the page is a clean start: whatever the latch was holding or
       ignoring belonged to the last visit. */
    mLatch.clear();

    /* The peripheral core that does the scanning is fixed -- there is no
       "which core" choice left in the wizard, only "which physical device"
       (UISetupDevicePage). Set it up once; PERSDLJoySetScanDeviceIndex() below
       is what actually restricts it to the chosen device. */
    if (!mCore)
    {
        mCore = QtYabause::getPERCore(QtYabause::defaultPERCore().id);
        if (mCore)
            mCore->Init();
    }

    /* The starting point decides what "skip" means. B20 gives a recognised
       gamepad a full default mapping, so starting from it makes skipping a
       button keep a working binding instead of leaving a hole - a user who
       skips everything still gets a playable pad. When the device did not
       change there is nothing to improve on, so the stored bindings stay.

       QWizard calls initializePage() every time the page becomes current, so
       this runs again when the user presses Back from a later page. Starting
       over would throw away every assignment they already made -- but going
       Back to the device page and picking a different device does need a
       fresh start, hence the deviceId comparison rather than just !mStarted. */
    const QString deviceId = mDevicePage->selectedDeviceId();
    if (!mStarted || deviceId != mStartingPointDeviceId)
    {
        /* Wipe any "done" icon left over from a previous run (a previous
           device's assignments, or a previous run of the wizard entirely, e.g.
           Cancel then Help > Setup wizard again). startStep() below takes care
           of the highlight itself. */
        QMap<u8, QToolButton*>::iterator padButtonIt;
        for (padButtonIt = mPadButtons.begin(); padButtonIt != mPadButtons.end(); ++padButtonIt)
            padButtonIt.value()->setIcon(QIcon());

        mUserAssigned.clear();
        mStartingPointDeviceId = deviceId;

        InputPortConfig::SdlDeviceSource source;
        if (mDevicePage->deviceChanged())
        {
            mStartingPoint = InputPortConfig::defaultMapping(source, deviceId, PERPAD);
        }
        else
        {
            mStartingPoint = storedBindings();
            /* An empty result here does not mean "no buttons bound"; it means
               the port has never been configured at all. InputPortConfig::
               seedPort() is what normally writes the first set of bindings,
               but it only runs from YabauseThread::reloadControllers(), and
               the wizard runs from main.cpp before YabauseThread exists. Fall
               back to the device's own defaults so a fresh install does not
               start the button page from a starting point that has nothing
               in it at all. */
            if (mStartingPoint.isEmpty())
                mStartingPoint = InputPortConfig::defaultMapping(source, deviceId, PERPAD);
        }

        mStarted = true;
        startStep(0);
    }
    else
    {
        startStep(mStep);
    }
}

void UISetupInputPage::cleanupPage()
{
    stopScanning();
}

bool UISetupInputPage::validatePage()
{
    /* cleanupPage() is only invoked by QWizard when the user goes Back from
       this page; it is NOT called when Next or Finish moves the wizard
       forward. Without stopping the timer here too, it would keep polling
       mCore->Scan() every 25 ms while the Games/Ready pages are on screen and
       silently assign() stray input into mUserAssigned. validatePage() runs on
       both the Next and Finish paths, so this closes that gap. */
    stopScanning();
    return true;
}

QMap<u8, u32> UISetupInputPage::startingPoint() const
{
    return mStartingPoint;
}

QMap<u8, u32> UISetupInputPage::userAssigned() const
{
    return mUserAssigned;
}

int UISetupInputPage::assignedCount() const
{
    return mUserAssigned.size();
}

int UISetupInputPage::buttonCount() const
{
    return mOrder.size();
}

QMap<u8, u32> UISetupInputPage::storedBindings() const
{
    QMap<u8, u32> bindings;
    Settings* settings = QtYabause::settings();

    settings->beginGroup(QString(INPUTPORT_GROUP_BINDINGS).arg(kWizardPort).arg(kWizardPad).arg(PERPAD));
    QStringList padKeys = settings->childKeys();
    settings->endGroup();

    QStringList::const_iterator it;
    for (it = padKeys.constBegin(); it != padKeys.constEnd(); ++it)
    {
        const u8 padKey = (u8)it->toUInt();
        bindings.insert(padKey, settings->value(
            InputPortConfig::bindingKey(kWizardPort, kWizardPad, PERPAD, padKey)).toUInt());
    }
    return bindings;
}

void UISetupInputPage::startStep(int index)
{
    stopScanning();

    /* Only the button for the current step (if any) should be highlighted;
       clear every other one first regardless of which branch below runs. */
    QMap<u8, QToolButton*>::iterator padButtonIt;
    for (padButtonIt = mPadButtons.begin(); padButtonIt != mPadButtons.end(); ++padButtonIt)
        padButtonIt.value()->setChecked(false);

    if (index < 0 || index >= mOrder.size())
    {
        mStep = mOrder.size();
        lPrompt->setText(QtYabause::translate("All buttons are set."));
        lProgress->clear();
        pbSkip->setEnabled(false);
        return;
    }

    mStep = index;
    const u8 padKey = mOrder.at(mStep);

    if (mPadButtons.contains(padKey))
        mPadButtons.value(padKey)->setChecked(true);

    /* PERSF_AXIS is deliberately NOT set, not even for L and R.
       PERSDLGameControllerScan() checks it first and then hands back the
       analog code (SDL_GC_AXIS_ANALOG_VALUE) instead of the digital one
       (SDL_GC_AXIS_POS_VALUE). Saturn L and R are digital buttons, and an
       analog binding only reaches them through PerAxisValue(), which presses
       at val >= 0xF0. With an Xbox trigger reporting 0..32767, val spans
       128..255, so 0xF0 lands at 87.5% of the pull - the trigger has to be
       squeezed almost flat before it registers. The digital code fires at
       SDL_GC_AXIS_THRESHOLD instead, about 24%, and it is what this device's
       own default mapping uses, so a hand-assigned trigger now behaves the
       same as one that came from the defaults. */
    mScanFlags = PERSF_KEY | PERSF_BUTTON | PERSF_HAT;

    lPrompt->setText(QtYabause::translate("Press the input for")
        + QString(": %1").arg(QtYabause::translate(mNames.value(padKey))));
    lProgress->setText(QString("%1 / %2").arg(mStep + 1).arg(mOrder.size()));
    pbSkip->setEnabled(true);
    pbPrevious->setEnabled(mStep > 0);

#ifdef HAVE_LIBSDL
    /* One Saturn port is driven by one physical device. Without this the scan
       picks up whatever pad is plugged in, so pressing a button on a second
       controller - or nudging a racing wheel - would be recorded here. */
    const QString deviceId = mDevicePage->selectedDeviceId();
    if (InputPortConfig::isPhysicalDevice(deviceId))
    {
        // An unplugged device resolves to -1, which would mean "any device"; keep it
        // disabled instead so nothing gets bound to the wrong pad.
        const int index = PERSDLJoyGetDeviceIndexForId(deviceId.toLatin1().constData());
        PERSDLJoySetScanDeviceIndex(index < 0 ? PERSDL_SCAN_NO_DEVICE : index);
    }
    else
        PERSDLJoySetScanDeviceIndex(PERSDL_SCAN_NO_DEVICE);
#endif

    if (mCore)
        mCore->Flush();
    mTimer->start();
    setFocus();
}

void UISetupInputPage::stopScanning()
{
    if (mTimer->isActive())
        mTimer->stop();

    /* Drop any half-finished press. Skipping a button while holding an input
       must not carry that input over and bind it to the button after it -
       neither the input itself, nor, for an axis, its return to rest. */
    mLatch.reset(oppositeAxisCode(mLatch.pending()));

#ifdef HAVE_LIBSDL
    /* Restore unrestricted scanning: the Settings dialog's own pad scanning
       shares this same global, and leaving it pinned to the wizard's device
       would make the dialog unable to bind anything after the wizard closes. */
    PERSDLJoySetScanDeviceIndex(PERSDL_SCAN_ANY_DEVICE);
#endif
}

void UISetupInputPage::assign(u32 key)
{
    if (mStep < 0 || mStep >= mOrder.size())
        return;

    const u8 padKey = mOrder.at(mStep);

    mUserAssigned.insert(padKey, key);

    /* Same "done so far" indicator as UIControllerSetting::setPadKey() gives
       the Settings dialog's picture. */
    if (mPadButtons.contains(padKey))
        mPadButtons.value(padKey)->setIcon(QIcon(":/actions/icons/actions/button_ok.png"));

    startStep(mStep + 1);
}

void UISetupInputPage::timerTimeout()
{
    if (!mCore)
        return;

    /* Commit on release. PERSDLJoyScan() reports pad buttons and hats by
       level, so a held button comes back on every 25 ms tick; binding on the
       first sighting used to advance a step per tick and write one press into
       several Saturn buttons. */
    const u32 key = mLatch.update(mCore->Scan(mScanFlags));
    if (key != 0)
    {
        /* An axis reports one direction as it moves and the other as it comes
           back, so binding a trigger to L would otherwise put its release on
           R. Swallow that second half. Done before assign(), which starts the
           next step -- reset() there keeps the one-shot deliberately. */
        mLatch.ignoreOnce(oppositeAxisCode(key));
        assign(key);
    }
}

bool UISetupInputPage::acceptsKeyboard() const
{
    /* A port bound to a gamepad must not also answer to the keyboard: the
       codes would both be stored and both fire in game. */
    return mTimer->isActive()
        && InputPortConfig::bindsHostInput(mDevicePage->selectedDeviceId());
}

void UISetupInputPage::keyPressEvent(QKeyEvent* event)
{
    /* Remember the key but do not bind it yet - keyReleaseEvent() does that,
       so that holding a key cannot walk through several steps on auto-repeat.
       Repeats of a key already held tell us nothing new. Esc still cancels,
       and must reach the wizard on press rather than being swallowed here. */
    if (event->key() != Qt::Key_Escape && !event->isAutoRepeat() && acceptsKeyboard())
    {
        mLatch.update((u32)event->key());
        return;
    }
    QWizardPage::keyPressEvent(event);
}

void UISetupInputPage::keyReleaseEvent(QKeyEvent* event)
{
    /* An auto-repeat release is the synthetic half of a repeat pair, not the
       user letting go, so it must not commit the binding. */
    if (event->key() != Qt::Key_Escape && !event->isAutoRepeat() && acceptsKeyboard())
    {
        const u32 key = mLatch.update(0);
        if (key != 0)
        {
            assign(key);
            return;
        }
    }
    QWizardPage::keyReleaseEvent(event);
}

bool UISetupInputPage::eventFilter(QObject* object, QEvent* event)
{
    /* Copied from UIControllerSetting::eventFilter() (UIControllerSetting.cpp)
       rather than shared: that class draws the same highlight for the
       Settings dialog's controller picture, but it is a QDialog and this is
       a QWizardPage, so it cannot be reused by inheritance. */
    if (event->type() == QEvent::Paint)
    {
        QToolButton* tb = qobject_cast<QToolButton*>(object);

        if (tb)
        {
            if (tb->isChecked())
            {
                QStylePainter sp(tb);
                QStyleOptionToolButton options;

                options.initFrom(tb);
                options.arrowType = Qt::NoArrow;
                options.features = QStyleOptionToolButton::None;
                options.icon = tb->icon();
                options.iconSize = tb->iconSize();
                options.state = QStyle::State_Enabled | QStyle::State_HasFocus | QStyle::State_On | QStyle::State_AutoRaise;

                sp.drawComplexControl(QStyle::CC_ToolButton, options);

                return true;
            }
        }
    }

    return QWizardPage::eventFilter(object, event);
}

void UISetupInputPage::previousClicked()
{
    startStep(mStep - 1);
}

void UISetupInputPage::skipClicked()
{
    startStep(mStep + 1);
}

void UISetupInputPage::skipAllClicked()
{
    startStep(mOrder.size());
}
