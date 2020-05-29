// Copyright (c) 2018 The Foundry Visionmongers Ltd. All Rights Reserved.

#include "UsdRenderInfoPlugin.h"

#include "pxr/pxr.h"

using namespace PXR_INTERNAL_NS;

DEFINE_RENDERERINFO_PLUGIN(UsdRenderInfoPlugin)


// The plug-ins will be registered inside the `registerPlugins()` function.
// Katana's plug-in system will inspect all shared objects in pursuit of this
// function. When found, the function will be invoked, and the plug-ins will be
// registered.
void registerPlugins()
{
    REGISTER_PLUGIN(UsdRenderInfoPlugin, "usdRendererInfo", 0, 1);
}
