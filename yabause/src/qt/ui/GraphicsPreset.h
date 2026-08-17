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
#ifndef GRAPHICSPRESET_H
#define GRAPHICSPRESET_H

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVariant>

/* Mirrors of the renderer enums in ygl.h. They live here, as constexpr, for
   two reasons. This header pulls in nothing but Qt containers, so the unit
   test target never sees windows.h or the GL headers. And constexpr at
   namespace scope is a compile-time constant in every translation unit that
   includes it, which is what lets GraphicsPresetCores.cpp -- the one file
   that DOES include ygl.h -- static_assert them against the real enums.
   A plain `const int` in the .cpp could not do that: namespace-scope const
   has internal linkage, and an extern const int is not a constant
   expression, so the assert would not compile. */
constexpr int kPolygonGpuTesseration = 2; /* GPU_TESSERATION */
constexpr int kPolygonComputeRaster = 3;  /* COMPUTE_RASTERIZER */
constexpr int kInternalNative = 0;        /* RES_NATIVE */
constexpr int kInternalRes4x = 1;         /* RES_4x */
constexpr int kInternalRes2x = 2;         /* RES_2x */
constexpr int kRbgOriginal = 0;           /* RBG_RES_ORIGINAL */
constexpr int kRbgRes2x = 1;              /* RBG_RES_2x */
constexpr int kRbgFitToEmulation = 4;     /* RBG_RES_FIT_TO_EMULATION */
constexpr int kRbgRes4x = 5;              /* RBG_RES_4x */

/* Sound/ScspMainMode index 0 is "Synchronize to CPU" in UISettings.ui. */
constexpr int kTimeSyncToCpu = 0;

/* A preset is a set of ini key/value pairs, not a sequence of widget
   manipulations. Keeping it that way lets the setup wizard, the settings
   dialog and the unit tests all use the same definition. */
enum GraphicsPresetTier
{
    GraphicsPresetHigh = 0,
    GraphicsPresetMiddle = 1,
    GraphicsPresetLow = 2
};

/* Which cores this build knows about, and which of them are actually
   registered in VIDCoreList / SNDCoreList. Vulkan and OpenAL are both
   conditionally compiled, so a preset must never write an id that the running
   binary cannot instantiate. An id of -1 means "not compiled in". */
struct GraphicsPresetCores
{
    GraphicsPresetCores()
        : videoVulkan(-1)
        , videoOpenGl(-1)
        , soundSdl(-1)
        , soundOpenAl(-1)
    {
    }

    int videoVulkan;
    int videoOpenGl;
    int soundSdl;
    int soundOpenAl;
    QList<int> availableVideoCores;
    QList<int> availableSoundCores;
};

/* Defined in GraphicsPresetCores.cpp, which is part of the application only.
   The unit tests build their own GraphicsPresetCores by hand. */
GraphicsPresetCores graphicsPresetDetectCores();

/* False when the tier needs a video core this build cannot provide. */
bool graphicsPresetIsAvailable(GraphicsPresetTier tier, const GraphicsPresetCores& cores);

/* The seven ini keys the tier owns, with cores resolved against what is
   actually registered. */
QMap<QString, QVariant> graphicsPresetValues(GraphicsPresetTier tier, const GraphicsPresetCores& cores);

/* True when every key the tier owns already has the tier's value in current.
   Keys outside the preset are ignored. */
bool graphicsPresetMatches(GraphicsPresetTier tier, const GraphicsPresetCores& cores, const QMap<QString, QVariant>& current);

/* Middle when Vulkan is available, Low otherwise. */
GraphicsPresetTier graphicsPresetDefaultTier(const GraphicsPresetCores& cores);

/* Human readable (label, value) rows for the wizard page. Both strings are
   English; the caller runs the label through QtYabause::translate(). */
QList<QPair<QString, QString> > graphicsPresetSummary(GraphicsPresetTier tier, const GraphicsPresetCores& cores);

/* True when a summary VALUE is a word that belongs in the translation
   catalog, false for the bare numerals the table also produces ("32", "4x").
   Running a numeral through mini18n would swap it for whatever unrelated
   entry happens to share that key, so callers must consult this before
   translating a value. Labels are always translatable. */
bool graphicsPresetValueIsTranslatable(const QString& value);

#endif /* GRAPHICSPRESET_H */
