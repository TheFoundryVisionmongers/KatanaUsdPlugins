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
    from pxr import Sdf, Sdr
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)


def getShaderSourceTypesFromTarget(target):
    """
    Returns the possible known shader types for a given target.

    @param target: The Katana `target` attribute value for the current shader.
    @type target: C{str}
    @return: A list of "sourceTypes" matching thee render delegate associated with that renderer.
    Empty list if not.
    @rtype: C{list} of C{str}
    """
    # Unfortunately there isn't a nice way to grab the RenderDelegates from Python in order to get
    # the sourceTypes they return. However, we'd still require some kind of map from "target" to
    # the name of the render delegate we should be querying.
    if target ==  "usd":
        return ["glslfx"]
    if target ==  "prman":
        return ["RManCPP", "OSL", "mtlx"]
    if target ==  "arnold":
        return ["arnold", "mtlx"]
    if target ==  "dl":
        return ["OSL"]
    return []


def GetShaderNodeFromRegistry(shaderIdentifier, sourceType=None):
    """
    Method required to get the SdrShadingNode from the SdrRegistry in different
    ways, depending on the usdPlugin.

    @type shaderIdentifier: C{str}
    @type sourceType: C{str} or C{None}
    @rtype: C{Sdr.ShaderNode} or C{None}
    @param shaderIdentifier: The name of the shader to search for in the shading
        registry.
    @param sourceType: The source type of the shader to search for in the shading
        registry.
    @return: The shader from the shader registry which matches the provided
        name and type.
    """
    sdrRegistry = Sdr.Registry()
    # Try and find the shader given the current source type, if that fails, search
    # for a given sourceType given a target we try to convert into the known sourceTypes as
    # per their hydra plug-ins.
    return (
        sdrRegistry.GetNodeByIdentifierAndType(shaderIdentifier, sourceType)
        if sourceType
        else sdrRegistry.GetNodeByIdentifier(shaderIdentifier)
    )


def GetSdrShaderFromShaderTypeAndTarget(katShaderType, target=None):
    """
    Retrieves an SdrShader given a katShaderType and a Katana shader target.
    Will automatically try with the target prefixed to the katShaderType.
    Returns None if we can't find the SdrShader.

    @return: Returns the SdrShader matching the katShaderType and target.
    @rtype: C{Sdr.ShaderNode} or C{None}
    """
    prefixedkatShaderType = None
    if target is not None:
        prefixedkatShaderType = target + ":" + katShaderType
    sourceTypes = getShaderSourceTypesFromTarget(target)
    shader = None
    if sourceTypes:
        for sourceType in sourceTypes:
            shader = GetShaderNodeFromRegistry(katShaderType, sourceType)
            if not shader and prefixedkatShaderType is not None:
                shader = GetShaderNodeFromRegistry(prefixedkatShaderType, sourceType)
            if shader:
                break
    if not shader:
        sdrRegistry = Sdr.Registry()
        sdrNodes = sdrRegistry.GetNodesByIdentifier(katShaderType)
        if sdrNodes:
            return sdrNodes[0]
    return shader


def LocationPathToSdfPath(locationPath, rootPrimName):
    """
    A Helper method to simply validate and create SdfPaths from
    location paths, concatenating the rootPrim name onto the Katana
    location path.

    @type locationPath: C{str}
    @type rootPrimName: C{str}
    @rtype: C{Sdf.Path}
    @param locationPath: The path for the current Katana location.
    @param rootPrimName: The name of the root prim for the C{Usd.Stage}.
    @return: A validated concatenated C{Sdf.Path} to write the C{Prim} to.
    """
    if not locationPath.startswith("/"):
        locationPath = "/" + locationPath
    if rootPrimName:
        locationPath = rootPrimName + locationPath
    # ":" is not supported in an SdfPath
    locationPath = locationPath.replace(":", "_")
    return Sdf.Path(locationPath)


def GetRelativeUsdSdfPath(relativePathMapping, rootName, location):
    """
    To be used with values from attributes like "materialAssign".
    Katana provides the absolute scene graph location path, but since
    the LookFileBakeAPI provides materials under the C{rootName}, as
    relative paths, the abolute paths must be translated to match
    the relative paths provided. If writing out materials from the
    root/materials, or other root paths, this is not needed since these
    are absolute already.
    Child material paths also need to be remapped since they are changed
    to sibling materials. The mappings from the original location in Katana
    and the C{Sdf.Path} in the USD file are stored in the
    C{relativePathMapping}.

    @type relativePathMapping: C{dict} of C{ C{str} : C{Sdf.Path} }
    @type rootName: C{str}
    @type location: C{str}
    @rtype: C{str} C{None}
    @param relativePathMapping: The dictionary of katana relative paths to the
        location of L{rootName}, and their C{Sdf.Path}s.
    @param rootName: Refer to L{LookFilePassData} for more information
    @param location: Katana location path.
    @return: The path to the Prim in the USD Stage, read from the
        C{relativePathMapping} after resolving the relative path.
    """
    if location in relativePathMapping:
        return relativePathMapping[location]
    rfindIndex = len(location)
    # Path separators to ensure the rootName is not matched within a
    # directory or file name. This will require stripping later on.
    rootNamePath = "/" + rootName + "/"
    rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
    if rfindIndex < 0:
        return None
    # Start reading with a closing "/"
    relativePath = location[rfindIndex + len(rootNamePath) - 1:]
    if relativePath in relativePathMapping:
        return relativePathMapping[relativePath]
    # If the location is not found in the first try, there may be
    # duplicate levels in  the hierarchy, so this must be repeated until
    # a valid relative material path is found.
    relativePath = location[rfindIndex:]
    # Start reading with a closing "/"
    rfindIndex = rfindIndex + 1
    while relativePath and relativePath not in relativePathMapping:
        rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
        if rfindIndex < 0:
            # Can no longer find the rootNamePath.
            return None
        relativePath = location[rfindIndex:]
        rfindIndex = rfindIndex + 1
    return relativePathMapping[relativePath]