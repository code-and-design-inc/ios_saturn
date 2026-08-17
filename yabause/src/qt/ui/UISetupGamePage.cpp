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
#include "UISetupGamePage.h"
#include "UISettings.h"
#include "../QtYabause.h"
#include "../Settings.h"

#include <QFileDialog>
#include <QSettings>

#include "../../cdbase.h" /* CDCORE_ARCH */

UISetupGamePage::UISetupGamePage(QWidget* parent)
    : QWizardPage(parent)
    , mHasDrive(false)
    , mStarted(false)
{
    setupUi(this);

    const QStringList drives = getCdDriveList();
    mHasDrive = !drives.isEmpty();
    lDrive->setVisible(mHasDrive);
    cbDrive->setVisible(mHasDrive);
    if (mHasDrive)
    {
        cbDrive->addItem(QtYabause::translate("Do not use an optical drive"), QString());
        for (int i = 0; i < drives.size(); ++i)
            cbDrive->addItem(drives.at(i), drives.at(i));
    }

    connect(pbBrowse, SIGNAL(clicked()), this, SLOT(browseClicked()));

    QtYabause::retranslateWidget(this);
}

void UISetupGamePage::initializePage()
{
    setTitle(QtYabause::translate("Games"));
    setSubTitle(QtYabause::translate("Tell Yaba Sanshiro where your games are."));

    /* Only prefill once. On the way back from the Summary page the line edit
       already holds the folder the user browsed to, and re-reading the stored
       lastDirectory would wipe it. */
    if (mStarted)
        return;
    mStarted = true;

    QSettings gameList("org.devmiyax", "Yabasanshiro");
    leDirectory->setText(gameList.value("lastDirectory").toString());
}

QString UISetupGamePage::gameDirectory() const
{
    return leDirectory->text();
}

QString UISetupGamePage::cdDrive() const
{
    /* Do not gate this on cbDrive->isVisible(): once the wizard has moved on
       to a later page, this page's widgets are hidden by QWizard even though
       the user's selection is still sitting in the combo box. mHasDrive
       reflects what was detected at construction time and is stable for the
       page's whole lifetime. */
    if (!mHasDrive)
        return QString();
    return cbDrive->itemData(cbDrive->currentIndex()).toString();
}

void UISetupGamePage::applyChoices()
{
    if (!gameDirectory().isEmpty())
    {
        QSettings gameList("org.devmiyax", "Yabasanshiro");
        gameList.setValue("lastDirectory", gameDirectory());
    }

    if (!cdDrive().isEmpty())
    {
        Settings* s = QtYabause::settings();
        s->setValue("General/CdRom", CDCORE_ARCH);
        s->setValue("General/CdRomISO", cdDrive());
    }
}

void UISetupGamePage::browseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this,
        QtYabause::translate("Choose the folder that contains your games"),
        leDirectory->text());
    if (!dir.isNull())
        leDirectory->setText(dir);
}
