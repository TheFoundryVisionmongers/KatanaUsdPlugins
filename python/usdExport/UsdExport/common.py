# Copyright (c) 2021 The Foundry Visionmongers Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
# names, trademarks, service marks, or product names of the Licensor
# and its affiliates, except as required to comply with Section 4(c) of
# the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.

import logging

log = logging.getLogger("UsdExport.Common")

try:
    from pxr import Sdr
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

def GetShaderNodeFromRegistry(shaderType):
    """
    Method required to get the SdrShadingNode from the SdrRegistry in different
    ways, depending on the usdPlugin.

    @type shaderType: C{str}
    @rtype: C{Sdr.ShaderNode}
    @param shaderType: The type of the shader to search for in the shading
        registry.
    @return: The shader from the shader registry which matches the provided
        type.
    """
    sdrRegistry = Sdr.Registry()
    shader = sdrRegistry.GetNodeByName(shaderType)
    if not shader:
        # try arnold, that uses identifiers instead of node names
        shader = sdrRegistry.GetShaderNodeByIdentifier(
            "arnold:{}".format(shaderType))

    return shader
    