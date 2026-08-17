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
#include "GraphicsPreset.h"

/* The renderer-enum mirrors and kTimeSyncToCpu live in GraphicsPreset.h so
   that GraphicsPresetCores.cpp can static_assert them against ygl.h. */

namespace
{

int resolveCore(const QList<int>& available, const QList<int>& preferred)
{
    for (int i = 0; i < preferred.size(); ++i)
    {
        if (preferred.at(i) >= 0 && available.contains(preferred.at(i)))
            return preferred.at(i);
    }
    /* Nothing preferred is registered: fall back to the first non-dummy core,
       and to the dummy core only if that is genuinely all there is. */
    for (int i = 0; i < available.size(); ++i)
    {
        if (available.at(i) != 0)
            return available.at(i);
    }
    return available.isEmpty() ? 0 : available.first();
}

int videoCoreFor(GraphicsPresetTier tier, const GraphicsPresetCores& cores)
{
    QList<int> preferred;
    if (tier == GraphicsPresetLow)
        preferred << cores.videoOpenGl << cores.videoVulkan;
    else
        preferred << cores.videoVulkan << cores.videoOpenGl;
    return resolveCore(cores.availableVideoCores, preferred);
}

/* Internal render size. High renders at the window's own resolution, which on
   a large display is more than any fixed multiplier would give and on a small
   one is less - the point is that it tracks the screen it is shown on. */
int internalResolutionFor(GraphicsPresetTier tier)
{
    if (tier == GraphicsPresetHigh)
        return kInternalNative;
    if (tier == GraphicsPresetMiddle)
        return kInternalRes4x;
    return kInternalRes2x;
}

/* Rotated background. It tracks the internal resolution rather than carrying a
   setting of its own, which is why the summary shows only one resolution row:
   "fit to emulation" for a High tier that has no fixed multiplier to copy, the
   same multiplier as the rest for Middle, and the cheapest thing that still
   draws for Low, where a rotated plane is the most expensive layer there is. */
int rbgResolutionFor(GraphicsPresetTier tier)
{
    if (tier == GraphicsPresetHigh)
        return kRbgFitToEmulation;
    if (tier == GraphicsPresetMiddle)
        return kRbgRes4x;
    return kRbgOriginal;
}

/* The wording the Settings dialog uses for the same values, so the wizard's
   summary and Settings > Video never disagree about what was chosen. */
QString internalResolutionLabel(int mode)
{
    if (mode == kInternalNative)
        return QString("Native (native resolution of Window)");
    if (mode == kInternalRes4x)
        return QString("4x");
    return QString("2x");
}

int soundCoreFor(const GraphicsPresetCores& cores)
{
    QList<int> preferred;
    preferred << cores.soundSdl << cores.soundOpenAl;
    return resolveCore(cores.availableSoundCores, preferred);
}

int scspSyncFor(GraphicsPresetTier tier)
{
    if (tier == GraphicsPresetHigh)
        return 32;
    if (tier == GraphicsPresetMiddle)
        return 16;
    return 4;
}

/* Name whichever core was actually resolved. A two-way test would report the
   wrong core the moment a preset falls back past its second choice, and the
   summary page is where the user checks what is about to be applied. The
   id >= 0 guard matters: a core that is not compiled in is stored as -1, and
   two absent cores must not compare equal. */
QString videoCoreLabel(int id, const GraphicsPresetCores& cores)
{
    if (id >= 0 && id == cores.videoVulkan)
        return QString("Vulkan Video");
    if (id >= 0 && id == cores.videoOpenGl)
        return QString("OpenGL");
    return QString("Other video core");
}

} /* namespace */

bool graphicsPresetIsAvailable(GraphicsPresetTier tier, const GraphicsPresetCores& cores)
{
    if (tier == GraphicsPresetLow)
        return true;
    return cores.videoVulkan >= 0 && cores.availableVideoCores.contains(cores.videoVulkan);
}

QMap<QString, QVariant> graphicsPresetValues(GraphicsPresetTier tier, const GraphicsPresetCores& cores)
{
    QMap<QString, QVariant> values;
    values.insert("Video/VideoCore", videoCoreFor(tier, cores));
    values.insert("Video/polygon_generation_mode",
        tier == GraphicsPresetHigh ? kPolygonComputeRaster : kPolygonGpuTesseration);
    /* The resolution ladder. High follows the window rather than a fixed
       multiplier, so it keeps up with whatever display it lands on, and the
       rotated background is told to match the rest of the emulation output
       instead of being scaled on its own. */
    values.insert("Video/resolution_mode", internalResolutionFor(tier));
    values.insert("Video/rbg_resolution_mode", rbgResolutionFor(tier));
    /* Bool rather than 0/1: this is what the settings dialog writes, and two
       spellings of the same setting in one ini would be worse than either. */
    values.insert("Video/UseComputeShader", tier != GraphicsPresetLow);
    values.insert("Sound/SoundCore", soundCoreFor(cores));
    values.insert("Sound/ScspSync", scspSyncFor(tier));
    values.insert("Sound/ScspMainMode", kTimeSyncToCpu);
    return values;
}

bool graphicsPresetMatches(GraphicsPresetTier tier, const GraphicsPresetCores& cores, const QMap<QString, QVariant>& current)
{
    const QMap<QString, QVariant> wanted = graphicsPresetValues(tier, cores);
    QMap<QString, QVariant>::const_iterator it;
    for (it = wanted.constBegin(); it != wanted.constEnd(); ++it)
    {
        if (!current.contains(it.key()))
            return false;
        /* A bool read back from an ini file is the string "true", and toInt()
           on that is 0 - comparing as an int would report every enabled
           setting as a mismatch and no preset would ever look current. */
        if (it.value().typeId() == QMetaType::Bool)
        {
            if (current.value(it.key()).toBool() != it.value().toBool())
                return false;
            continue;
        }
        if (current.value(it.key()).toInt() != it.value().toInt())
            return false;
    }
    return true;
}

GraphicsPresetTier graphicsPresetDefaultTier(const GraphicsPresetCores& cores)
{
    return graphicsPresetIsAvailable(GraphicsPresetMiddle, cores)
        ? GraphicsPresetMiddle
        : GraphicsPresetLow;
}

QList<QPair<QString, QString> > graphicsPresetSummary(GraphicsPresetTier tier, const GraphicsPresetCores& cores)
{
    const QMap<QString, QVariant> values = graphicsPresetValues(tier, cores);
    const int videoCore = values.value("Video/VideoCore").toInt();

    QList<QPair<QString, QString> > rows;
    rows << qMakePair(QString("Video Core"), videoCoreLabel(videoCore, cores));
    rows << qMakePair(QString("Polygon Generation"),
        QString(values.value("Video/polygon_generation_mode").toInt() == kPolygonComputeRaster
            ? "Compute Rasterizer" : "GPU Tessellation"));
    /* One resolution row, not two: the rotated background follows the internal
       resolution, so showing both only ever repeated the same choice. */
    rows << qMakePair(QString("Internal Resolution"),
        internalResolutionLabel(values.value("Video/resolution_mode").toInt()));
    rows << qMakePair(QString("Use compute shader for RBG generation"),
        QString(values.value("Video/UseComputeShader").toBool() ? "Enabled" : "Disabled"));
    /* The sound core is not shown. There is only one left to pick from, so a
       row for it would tell the reader nothing about their choice. It is still
       written, so that a preset fully describes the configuration it makes. */
    rows << qMakePair(QString("SCSP sync count per frame"),
        QString::number(values.value("Sound/ScspSync").toInt()));
    rows << qMakePair(QString("Time Synchronization Mode"), QString("Synchronize to CPU"));
    return rows;
}

bool graphicsPresetValueIsTranslatable(const QString& value)
{
    /* Every numeric value this table produces is either a plain count ("32",
       "16", "4") or a resolution multiplier ("4x", "2x"). Neither is a word,
       and neither has any business being looked up in a translation catalog.
       Everything else the table produces -- core names, "Compute Rasterizer",
       "Synchronize to CPU" -- is a word and does get translated. */
    if (value.isEmpty())
        return false;

    int digits = 0;
    while (digits < value.size() && value.at(digits).isDigit())
        ++digits;

    if (digits == 0)
        return true;              /* does not start with a digit: a word */
    if (digits == value.size())
        return false;             /* all digits: a count */
    /* Digits followed by exactly "x": a resolution multiplier. */
    return !(digits + 1 == value.size() && value.at(digits) == QLatin1Char('x'));
}
