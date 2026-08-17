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
#include "../QtYabause.h"

/* QtYabause.h already pulls in vidogl.h, sndal.h and sndsdl.h, so VIDCORE_OGL
   / SNDCORE_AL / SNDCORE_SDL are already visible here. The DirectSound core
   was removed in B20 (dfc824dcb) and has no header left to include. ygl.h is
   not part of that chain -- it is included directly below, and only here,
   because GraphicsPreset.h must not drag it (and the GL/windows.h headers
   behind it) into the unit-test target. */
#include "../../ygl.h"

#ifdef HAVE_VULKAN
#include "../../vulkan/VIDVulkanCInterface.h"
#endif

/* Unlike the core structs above, VIDCoreList / SNDCoreList have no shared
   declaration in QtYabause.h -- every .cpp that walks them (UIYabause.cpp,
   UISettings.cpp) declares its own extern, and this file follows the same
   established pattern. Both arrays are defined in QtYabause.cpp. */
extern "C" {
extern SoundInterface_struct* SNDCoreList[];
extern VideoInterface_struct* VIDCoreList[];
}

/* GraphicsPreset.h cannot include ygl.h -- windows.h and the GL headers would
   follow it into the unit test target -- so it mirrors these renderer enum
   values as constexpr. This is the one translation unit that sees both, so
   this is where the two are pinned together. If someone renumbers an enum in
   ygl.h, the build stops here instead of the wizard quietly writing the wrong
   value into the user's ini. */
static_assert(kPolygonGpuTesseration == GPU_TESSERATION,
    "GraphicsPreset.h kPolygonGpuTesseration is out of sync with ygl.h GPU_TESSERATION");
static_assert(kPolygonComputeRaster == COMPUTE_RASTERIZER,
    "GraphicsPreset.h kPolygonComputeRaster is out of sync with ygl.h COMPUTE_RASTERIZER");
static_assert(kInternalNative == RES_NATIVE,
    "GraphicsPreset.h kInternalNative is out of sync with ygl.h RES_NATIVE");
static_assert(kRbgOriginal == RBG_RES_ORIGINAL,
    "GraphicsPreset.h kRbgOriginal is out of sync with ygl.h RBG_RES_ORIGINAL");
static_assert(kRbgFitToEmulation == RBG_RES_FIT_TO_EMULATION,
    "GraphicsPreset.h kRbgFitToEmulation is out of sync with ygl.h RBG_RES_FIT_TO_EMULATION");
static_assert(kInternalRes4x == RES_4x,
    "GraphicsPreset.h kInternalRes4x is out of sync with ygl.h RES_4x");
static_assert(kInternalRes2x == RES_2x,
    "GraphicsPreset.h kInternalRes2x is out of sync with ygl.h RES_2x");
static_assert(kRbgRes2x == RBG_RES_2x,
    "GraphicsPreset.h kRbgRes2x is out of sync with ygl.h RBG_RES_2x");
static_assert(kRbgRes4x == RBG_RES_4x,
    "GraphicsPreset.h kRbgRes4x is out of sync with ygl.h RBG_RES_4x");

GraphicsPresetCores graphicsPresetDetectCores()
{
    GraphicsPresetCores cores;

    /* VIDCORE_OGL is only defined inside vidogl.h under the same guard that
       controls whether the OpenGL renderer was built at all, so the usage
       below has to be guarded too, not just the assignment's meaning. */
#ifdef HAVE_LIBGL
    cores.videoOpenGl = VIDCORE_OGL;
#endif
#ifdef HAVE_VULKAN
    cores.videoVulkan = VIDCORE_VULKAN;
#endif
    cores.soundSdl = SNDCORE_SDL;
#ifdef HAVE_LIBAL
    cores.soundOpenAl = SNDCORE_AL;
#endif

    for (int i = 0; VIDCoreList[i] != NULL; i++)
        cores.availableVideoCores << VIDCoreList[i]->id;
    for (int i = 0; SNDCoreList[i] != NULL; i++)
        cores.availableSoundCores << SNDCoreList[i]->id;

    return cores;
}
