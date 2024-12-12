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

__all__ = ['BaseUsdExportPlugin']

class BaseUsdExportPlugin(object):
    """
    A plug-in class to assist in setting up your USD Export custom logic.
    Use this as a base class and ensure that you override `WritePrim()`.
    You may register a plug-in to multiple types of locations if it applies
    to many location types. See the `RegisterUsdExportPlugin()` function
    for more details.

    :type priority: `int`
    :cvar priority: An optional value to allow overriding where in the
        plug-in list your plug-in will be run. Specifically useful if you
        know there is an order you wish your own subset of plug-ins to run
        in. Your value must be defined as the default argument to `priority`.
        Must be strictly lower than `sys.maxint`.
    """

    priority = 0

    @staticmethod
    def WritePrim(stage, sdfLocationPath, attrDict):
        """
        The method which must be  overridden in order to write out your own
        custom Usd.Prim information for any custom UsdExport logic.

        @type stage: C{Usd.Stage}
        @type sdfLocationPath: C{Sdf.Path}
        @type attrDict: C{dict} of C{str}:C{FnAttribute.Attribute}
        @param stage: The stage to which the prim belongs.
        @param sdfLocationPath: The SdfPath relating to where this Katana
            location is to be written in the provided stage.  This may
            already contain a valid C{Usd.Prim}, or C{Usd.OverridePrim} or
            a derivative of the two.  You may override the type and
            manipulate, the stage and USD file as required.
        @param attrDict: A dictionary containing as the keys, the primary
            attribute name, with the value being an FnAttribute, which may
            be any of the derived types, GroupAttribute, IntAttribute etc.
            This attrDict contains the local attributes which differ
            between the `orig` input of the `UsdMaterialBake` node, and
            the input currently being written.
        """
        raise NotImplementedError()
