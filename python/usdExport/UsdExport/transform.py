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

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from pxr import UsdGeom, Sdf
    from .typeConversionMaps import (ConvertParameterValueToGfType)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

def WriteTransform(stage, xformSdfPath, xformAttrs):
    """
    Writes transform attributes to the specified location. If the location does
    not exist, it will be created. Any existing prim type will be overwritten
    with with 'Xform'.

    @type stage: C{Usd.Stage}
    @type xformSdfPath: C{Sdf.Path}
    @type xformAttrs: C{FnAttribute.GroupAttribute}
    @rtype: C{Usd.Prim}
    @param stage: The USD stage to write the transform to.
    @param xformSdfPath: The path in the stage to write the attributes to.
    @param xformAttrs: The Katana attributes to write to the Usd Stage.
    @return: Returns the prim with the transform attributes applied.
    """
    prim = stage.DefinePrim(xformSdfPath, "Xform")
    interactiveXform = xformAttrs.getChildByName("interactive")
    if interactiveXform:
        xformable = UsdGeom.Xformable(prim)
        # Write translation.
        translateAttr = interactiveXform.getChildByName("translate")
        if translateAttr:
            translateOp = xformable.AddTranslateOp()
            translateOp.Set(ConvertParameterValueToGfType(
                                translateAttr.getData(),
                                Sdf.ValueTypeNames.Double3))
        # Write rotation.
        rotateXAttr = interactiveXform.getChildByName("rotateX")
        rotateYAttr = interactiveXform.getChildByName("rotateY")
        rotateZAttr = interactiveXform.getChildByName("rotateZ")
        if rotateXAttr and rotateYAttr and rotateZAttr:
            rotateOp = xformable.AddRotateXYZOp()
            rotateOp.Set((rotateXAttr.getData()[0],
                          rotateYAttr.getData()[0],
                          rotateZAttr.getData()[0]))
        # Write scale.
        scaleAttr = interactiveXform.getChildByName("scale")
        if scaleAttr:
            scaleOp = xformable.AddScaleOp()
            scaleOp.Set(ConvertParameterValueToGfType(
                            scaleAttr.getData(),
                            Sdf.ValueTypeNames.Double3))

    return prim
