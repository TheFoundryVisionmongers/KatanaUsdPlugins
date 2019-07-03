// These files began life as part of the main USD distribution
// https://github.com/PixarAnimationStudios/USD.
// In 2019, Foundry and Pixar agreed Foundry should maintain and curate
// these plug-ins, and they moved to
// https://github.com/TheFoundryVisionmongers/katana-USD
// under the same Modified Apache 2.0 license, as shown below.
// These files began life as part of the main USD distribution
// https://github.com/PixarAnimationStudios/USD.
// In 2019, Foundry and Pixar agreed Foundry should maintain and curate
// these plug-ins, and they moved to
// https://github.com/TheFoundryVisionmongers/katana-USD
// under the same Modified Apache 2.0 license, as shown below.


//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "vtKatana/bootstrap.h"

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/arch/fileSystem.h"

#include <FnLogging/FnLogging.h>
#include <FnAttribute/FnAttribute.h>

#include <mutex> // for std::call_once

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("PxrVtKatanaBootstrap");

void PxrVtKatanaBootstrap()
{
    static std::once_flag once;
    std::call_once(once, []()
    {
        // Path of the katana process (without filename).
        std::string path = TfGetPathName(ArchGetExecutablePath());

        // FnAttribute::Bootstrap() appends 'bin', so remove it here.
        std::string const binPrefix("bin" ARCH_PATH_SEP);
        if (TfStringEndsWith(path, binPrefix))
        {
            path.erase(path.length() - binPrefix.length());
        }

        // Boostrap FnAttribute.
        if (!FnAttribute::Bootstrap(path))
        {
            FnLogError("Failed to bootstrap FnAttribute from Katana at " << path);
            return;
        }
    });
}

PXR_NAMESPACE_CLOSE_SCOPE

