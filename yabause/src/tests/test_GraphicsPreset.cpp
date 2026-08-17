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
/* Unit tests for the graphics preset table and the setup wizard's run/skip
   decision. Both are pure logic with no emulator behind them. */
#include <QtTest/QtTest>

#include <QSettings>
#include <QTemporaryDir>

#include "../qt/ui/GraphicsPreset.h"
#include "../qt/ui/UISetupWizard.h"

/* Core ids used by the tests. They mirror the real ones so a mismatch in
   GraphicsPresetCores.cpp shows up as a failing static_assert, not here. */
static const int kVidVulkan = 4;
static const int kVidOpenGl = 1;
static const int kSndSdl = 1;
static const int kSndOpenAl = 4;

static GraphicsPresetCores fullCores()
{
    GraphicsPresetCores c;
    c.videoVulkan = kVidVulkan;
    c.videoOpenGl = kVidOpenGl;
    c.soundSdl = kSndSdl;
    c.soundOpenAl = kSndOpenAl;
    c.availableVideoCores << kVidOpenGl << kVidVulkan;
    c.availableSoundCores << kSndSdl << kSndOpenAl;
    return c;
}

class TestGraphicsPreset : public QObject
{
    Q_OBJECT

private slots:
    void highPresetMatchesSpec();
    void middlePresetMatchesSpec();
    void lowPresetMatchesSpec();
    void fallsBackWhenVulkanMissing();
    void sdlOnlyBuildResolvesToSdl();
    void openAlOnlyBuildResolvesToOpenAl();
    void fallsBackToAnyRegisteredSoundCore();
    void summaryNamesTheCoreThatWasActuallyChosen();
    void availabilityFollowsVideoCore();
    void defaultTierFollowsVulkan();
    void matchesDetectsIdenticalSettings();
    void matchesRejectsOneDifferentKey();
    void summaryCoversEverySetting();
    void numeralSummaryValuesAreNotTranslated();
    void wizardRunsUntilItsVersionIsRecorded();
    void wizardVersionSurvivesTheIniRoundTrip();
    void markDoneStopsTheWizardFromRunningAgain();
};

void TestGraphicsPreset::highPresetMatchesSpec()
{
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetHigh, fullCores());
    QCOMPARE(v.value("Video/VideoCore").toInt(), kVidVulkan);
    QCOMPARE(v.value("Video/polygon_generation_mode").toInt(), 3);
    /* High follows the window instead of a fixed multiplier: RES_NATIVE, with
       the rotated background told to match the emulation output. */
    QCOMPARE(v.value("Video/resolution_mode").toInt(), 0);
    QCOMPARE(v.value("Video/rbg_resolution_mode").toInt(), 4);
    QCOMPARE(v.value("Sound/SoundCore").toInt(), kSndSdl);
    QCOMPARE(v.value("Sound/ScspSync").toInt(), 32);
    QCOMPARE(v.value("Sound/ScspMainMode").toInt(), 0);
    QVERIFY(v.value("Video/UseComputeShader").toBool());
    QCOMPARE(v.size(), 8);
}

void TestGraphicsPreset::middlePresetMatchesSpec()
{
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetMiddle, fullCores());
    QCOMPARE(v.value("Video/VideoCore").toInt(), kVidVulkan);
    QCOMPARE(v.value("Video/polygon_generation_mode").toInt(), 2);
    QCOMPARE(v.value("Video/resolution_mode").toInt(), 1);   /* 4x */
    QCOMPARE(v.value("Video/rbg_resolution_mode").toInt(), 5); /* 4x */
    QVERIFY(v.value("Video/UseComputeShader").toBool());
    QCOMPARE(v.value("Sound/ScspSync").toInt(), 16);
}

void TestGraphicsPreset::lowPresetMatchesSpec()
{
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetLow, fullCores());
    QCOMPARE(v.value("Video/VideoCore").toInt(), kVidOpenGl);
    QCOMPARE(v.value("Video/polygon_generation_mode").toInt(), 2);
    QCOMPARE(v.value("Video/resolution_mode").toInt(), 2);   /* 2x */
    /* A rotated plane is the most expensive layer there is, so the cheap tier
       leaves it at its original size rather than scaling it with the rest. */
    QCOMPARE(v.value("Video/rbg_resolution_mode").toInt(), 0); /* Original */
    QVERIFY(!v.value("Video/UseComputeShader").toBool());
    QCOMPARE(v.value("Sound/ScspSync").toInt(), 4);
}

void TestGraphicsPreset::fallsBackWhenVulkanMissing()
{
    GraphicsPresetCores c = fullCores();
    c.availableVideoCores.clear();
    c.availableVideoCores << kVidOpenGl;
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetMiddle, c);
    QCOMPARE(v.value("Video/VideoCore").toInt(), kVidOpenGl);
}

void TestGraphicsPreset::sdlOnlyBuildResolvesToSdl()
{
    /* SDL is the only sound core registered (OpenAL is compiled in but not
       registered): SDL must still be picked, since it is first in the
       preferred order. */
    GraphicsPresetCores c = fullCores();
    c.availableSoundCores.clear();
    c.availableSoundCores << kSndSdl;
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetHigh, c);
    QCOMPARE(v.value("Sound/SoundCore").toInt(), kSndSdl);
}

void TestGraphicsPreset::openAlOnlyBuildResolvesToOpenAl()
{
    /* SDL is absent: the ranked loop must keep going and land on OpenAL
       rather than stopping at the first miss. */
    GraphicsPresetCores c = fullCores();
    c.availableSoundCores.clear();
    c.availableSoundCores << kSndOpenAl;
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetHigh, c);
    QCOMPARE(v.value("Sound/SoundCore").toInt(), kSndOpenAl);
}

void TestGraphicsPreset::fallsBackToAnyRegisteredSoundCore()
{
    /* Nothing this build knows by name is registered. The last resort is the
       first non-dummy core in the list, never the dummy core (id 0). */
    const int kSomeOtherCore = 6;
    GraphicsPresetCores c = fullCores();
    c.availableSoundCores.clear();
    c.availableSoundCores << 0 << kSomeOtherCore;
    const QMap<QString, QVariant> v = graphicsPresetValues(GraphicsPresetHigh, c);
    QCOMPARE(v.value("Sound/SoundCore").toInt(), kSomeOtherCore);
}

void TestGraphicsPreset::summaryNamesTheCoreThatWasActuallyChosen()
{
    /* The summary is what the user reads before pressing Finish, so it must
       name the core the preset resolved to, not the one it asked for. */
    GraphicsPresetCores c = fullCores();
    c.availableVideoCores.clear();
    c.availableVideoCores << kVidOpenGl;
    c.availableSoundCores.clear();
    c.availableSoundCores << kSndOpenAl;

    const QList<QPair<QString, QString> > rows =
        graphicsPresetSummary(GraphicsPresetMiddle, c);
    QCOMPARE(rows.at(0).second, QString("OpenGL"));

    /* The sound core no longer has a row - there is only one to choose from,
       so naming it told the reader nothing. It must still be resolved and
       written, though, or the preset would not fully describe what it did. */
    QCOMPARE(graphicsPresetValues(GraphicsPresetMiddle, c)
                 .value("Sound/SoundCore").toInt(), kSndOpenAl);
}

void TestGraphicsPreset::availabilityFollowsVideoCore()
{
    GraphicsPresetCores c = fullCores();
    QVERIFY(graphicsPresetIsAvailable(GraphicsPresetHigh, c));
    QVERIFY(graphicsPresetIsAvailable(GraphicsPresetMiddle, c));
    QVERIFY(graphicsPresetIsAvailable(GraphicsPresetLow, c));

    c.availableVideoCores.clear();
    c.availableVideoCores << kVidOpenGl;
    QVERIFY(!graphicsPresetIsAvailable(GraphicsPresetHigh, c));
    QVERIFY(!graphicsPresetIsAvailable(GraphicsPresetMiddle, c));
    QVERIFY(graphicsPresetIsAvailable(GraphicsPresetLow, c));
}

void TestGraphicsPreset::defaultTierFollowsVulkan()
{
    GraphicsPresetCores c = fullCores();
    QCOMPARE(graphicsPresetDefaultTier(c), GraphicsPresetMiddle);

    c.availableVideoCores.clear();
    c.availableVideoCores << kVidOpenGl;
    QCOMPARE(graphicsPresetDefaultTier(c), GraphicsPresetLow);
}

void TestGraphicsPreset::matchesDetectsIdenticalSettings()
{
    const GraphicsPresetCores c = fullCores();
    QMap<QString, QVariant> current = graphicsPresetValues(GraphicsPresetLow, c);
    current.insert("General/ShowFPS", true); /* unrelated keys must be ignored */
    QVERIFY(graphicsPresetMatches(GraphicsPresetLow, c, current));
    QVERIFY(!graphicsPresetMatches(GraphicsPresetHigh, c, current));
}

void TestGraphicsPreset::matchesRejectsOneDifferentKey()
{
    const GraphicsPresetCores c = fullCores();
    QMap<QString, QVariant> current = graphicsPresetValues(GraphicsPresetHigh, c);
    current.insert("Sound/ScspSync", 16);
    QVERIFY(!graphicsPresetMatches(GraphicsPresetHigh, c, current));
}

void TestGraphicsPreset::summaryCoversEverySetting()
{
    const QList<QPair<QString, QString> > rows =
        graphicsPresetSummary(GraphicsPresetHigh, fullCores());
    /* Six rows, not eight. The rotated background follows the internal
       resolution, so a row of its own only repeated the same choice, and with
       one sound core left to pick from a sound row says nothing at all. */
    QCOMPARE(rows.size(), 6);
    QCOMPARE(rows.at(0).first, QString("Video Core"));
    QCOMPARE(rows.at(0).second, QString("Vulkan Video"));
    QCOMPARE(rows.at(1).second, QString("Compute Rasterizer"));
    /* The same wording Settings > Video uses, so the summary and the settings
       dialog cannot describe one choice differently. */
    QCOMPARE(rows.at(2).second, QString("Native (native resolution of Window)"));
    QCOMPARE(rows.at(3).first, QString("Use compute shader for RBG generation"));
    QCOMPARE(rows.at(3).second, QString("Enabled"));
    QCOMPARE(rows.at(4).second, QString("32"));
    QCOMPARE(rows.at(5).second, QString("Synchronize to CPU"));

    const QList<QPair<QString, QString> > low =
        graphicsPresetSummary(GraphicsPresetLow, fullCores());
    QCOMPARE(low.at(3).second, QString("Disabled"));
}

void TestGraphicsPreset::numeralSummaryValuesAreNotTranslated()
{
    /* A value that is a bare numeral must never be looked up in the
       translation catalog; a catalog entry that happened to share that key
       would silently replace "32" or "4x" with something unrelated. */
    QVERIFY(!graphicsPresetValueIsTranslatable(QString("32")));
    QVERIFY(!graphicsPresetValueIsTranslatable(QString("16")));
    QVERIFY(!graphicsPresetValueIsTranslatable(QString("4")));
    QVERIFY(!graphicsPresetValueIsTranslatable(QString("4x")));
    QVERIFY(!graphicsPresetValueIsTranslatable(QString("2x")));
    QVERIFY(!graphicsPresetValueIsTranslatable(QString()));

    QVERIFY(graphicsPresetValueIsTranslatable(QString("Vulkan Video")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("OpenGL")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("Compute Rasterizer")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("GPU Tessellation")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("SDL Sound Interface")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("OpenAL Sound Interface")));
    QVERIFY(graphicsPresetValueIsTranslatable(QString("Synchronize to CPU")));

    /* Every value the real table produces must fall on one side or the other
       without any of them being mangled: numerals left alone, words kept. */
    const QList<QPair<QString, QString> > rows =
        graphicsPresetSummary(GraphicsPresetHigh, fullCores());
    for (int i = 0; i < rows.size(); ++i)
    {
        const QString& value = rows.at(i).second;
        const bool numeric = (value == "4x" || value == "2x" || value == "32");
        QCOMPARE(graphicsPresetValueIsTranslatable(value), !numeric);
    }
}

void TestGraphicsPreset::wizardRunsUntilItsVersionIsRecorded()
{
    /* kWizardVersion is a static const int with no out-of-line definition, so
       copy it into a local rather than letting QCOMPARE bind a reference. */
    const int version = UISetupWizard::kWizardVersion;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath("wizard.ini"), QSettings::IniFormat);

    s.remove(UISetupWizard::versionKey());
    QVERIFY2(UISetupWizard::shouldRunForSettings(&s),
        "a first launch has no recorded version, so the wizard must run");

    s.setValue(UISetupWizard::versionKey(), version - 1);
    QVERIFY2(UISetupWizard::shouldRunForSettings(&s),
        "an older recorded version must run the wizard again");

    s.setValue(UISetupWizard::versionKey(), version);
    QVERIFY2(!UISetupWizard::shouldRunForSettings(&s),
        "the current version must not run the wizard");

    s.setValue(UISetupWizard::versionKey(), version + 1);
    QVERIFY2(!UISetupWizard::shouldRunForSettings(&s),
        "a version from a newer build must not run the wizard");
}

void TestGraphicsPreset::wizardVersionSurvivesTheIniRoundTrip()
{
    /* QSettings hands back a QString once the value has been through an ini
       file, so the comparison has to be numeric. A QVariant equality test
       would re-run the wizard on every launch. */
    const int version = UISetupWizard::kWizardVersion;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("wizard.ini");

    {
        QSettings writer(path, QSettings::IniFormat);
        writer.setValue(UISetupWizard::versionKey(), version);
        writer.sync();
    }

    QSettings reader(path, QSettings::IniFormat);
    QCOMPARE(reader.value(UISetupWizard::versionKey()).toString(), QString::number(version));
    QVERIFY(!UISetupWizard::shouldRunForSettings(&reader));
}

void TestGraphicsPreset::markDoneStopsTheWizardFromRunningAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath("wizard.ini"), QSettings::IniFormat);

    QVERIFY(UISetupWizard::shouldRunForSettings(&s));
    UISetupWizard::markDoneForSettings(&s);
    QVERIFY(!UISetupWizard::shouldRunForSettings(&s));
}

QTEST_MAIN(TestGraphicsPreset)
#include "test_GraphicsPreset.moc"
