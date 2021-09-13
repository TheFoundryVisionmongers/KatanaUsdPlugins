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

__all__ = [
    'RegisterUsdExportPlugin',
    'UnregisterUsdExportPlugin',
    'GetUsdExportPluginsByType',
    'GetUsdExportPluginClass',
    'GetLocationTypesForPluginName',
    'LoadPluginsFromKatanaResources'
]

# Required until we go full Python3 where dictionaries
# are ordered by insertion by default.
from collections import OrderedDict
import logging

import Utils
import KatanaResources

log = logging.getLogger("UsdExport.pluginRegistry")

__pluginRegistryDict = dict()
__pluginRegistrySorted = dict()

def RegisterUsdExportPlugin(pluginName, locationTypes, pluginClass):
    """
    Registers the plugin with the provided unique pluginName, locationType
    combination into the global UsdExport plugin registry. If a plugin
    was already found with the same pluginName and locationType, this
    plugin will not be registered.

    @type pluginName: C{str}
    @type locationTypes: C{list} of C{str}
    @type pluginClass: C{BaseUsdExportPlugin}
    @rtype: C{bool}
    @param pluginName: The name of the plug-in to register
    @param locationTypes: The types of katana location, matching the katana
        `type` attribute on the location to run this plug-in during
        a UsdMaterialBake.
        Providing an empty string as this argument will allow writing to
        locations which do not have a type attribute on the location,
        to which we will write an OverridePrim.
    @param pluginClass: The pluginClass deriving from C{BaseUsdExportPlugin}
        and implementing WritePrim, and optionally overriding the `priority`
        class variable to run at the given locationType.
    @return: Returns `True` if registering the plug-in was a success,
        otherwise `False`.
    """

    # Validate plugin
    if not locationTypes or not pluginName or not pluginClass:
        return False
    if not hasattr(pluginClass, "WritePrim"):
        log.warning('Unable to register plug-in "%s", plug-in is invalid. '
                    'WritePrim is not present on pluginClass "%s". Please '
                    'check the documentation for how to design the plugin',
                    pluginName, pluginClass)
        return False
    if not hasattr(pluginClass, "priority"):
        log.warning('Unable to register plug-in "%s", plug-in is invalid. '
                    'priority is not present on pluginClass "%s" Please check'
                    ' the documentation for how to design the plug-in',
                    pluginName, pluginClass)
        return False
    if not isinstance(locationTypes, list):
        log.warning('Unable to register plug-in "%s", plug-in is invalid. '
                    'locationTypes argument must be a list. Please check the '
                    'documentation for how to design the plug-in', pluginName)
        return False


    global __pluginRegistryDict
    global __pluginRegistrySorted
    if __pluginRegistryDict is None:
        __pluginRegistryDict = dict()
    for locationType in locationTypes:
        if GetUsdExportPluginClass(pluginName, locationType):
            log.debug('Unable to register plug-in "%s", plug-in has already been'
                      'registered with this name and type "%s"',
                       pluginName, locationType)
        pluginClassesDict = __pluginRegistryDict.get(locationType, None)
        if pluginClassesDict is not None:
            pluginClassesDict[pluginName] = pluginClass
        else:
            pluginClassesDict = OrderedDict()
            pluginClassesDict[pluginName] = pluginClass
            __pluginRegistryDict[locationType] = pluginClassesDict
        __pluginRegistrySorted[locationType] = False
    return True

def UnregisterUsdExportPlugin(pluginName, locationType):
    """
    Removes a plug-in from the UsdExport plug-ins, preventing it from
    running whilst performing a UsdExport.

    @type pluginName: C{str}
    @type locationType: C{str}
    @param pluginName: The name of the plug-in to find.
    @param locationType: The location type of the registered plug-in.
    """
    global __pluginRegistryDict
    pluginClassesDict = __pluginRegistryDict.get(locationType, None)
    if pluginClassesDict is not None and pluginName in pluginClassesDict.keys():
        del pluginClassesDict[pluginName]

def GetUsdExportPluginsByType(locationType):
    """
    Function to retrieve the C{BaseUsdExportPlugin}'s by type.
    It will return the list sorted by the `priority` value on the
    plug-in classes.

    @type locationType: C{str}
    @rtype: C{list} of C{UsdExport.pluginAPI.BaseUsdExportPlugin}
    @param locationType: This method returns all the UsdExport plug-ins we
        need to run at this provided locationType; matching the katana
        locations `type` attribute.
    @return: A list of plug-ins which have been registered to that type.
        Ordered in descending order based on the `priority` class member
        of C{BaseUsdExportPlugin}
    """
    pluginClassesDict = __pluginRegistryDict.get(locationType, None)
    if pluginClassesDict is None:
        return []

    isSorted = __pluginRegistrySorted[locationType]
    # If these are not sorted (due to an insertion since
    # the last time this method was called), this method will sort and assign
    # the sorted flag to ensure re-sorting does not occur on subsequent calls.
    if not isSorted:
        # Sort in descending order (intMax > 0)
        sortedTuples = sorted(pluginClassesDict.items(),
            key=lambda item: item[1].priority, reverse=True)
        __pluginRegistryDict[locationType] = OrderedDict(sortedTuples)
        __pluginRegistrySorted[locationType] = True
    return __pluginRegistryDict[locationType].values()

def GetUsdExportPluginClass(pluginName, locationType):
    """
    Function to return the unique C{BaseUsdExportPlugin} (or derivative)
    based on the provided arguments.

    @type pluginName: C{str}
    @type locationType: C{str}
    @rtype: C{BaseUsdExportPlugin} or C{None}
    @param pluginName: The name of the plugin to find.
    @param locationType: The location type of the registered plugin.
    @return: The C{BaseUsdExportPlugin} registered with this unique
        pluginName and locationType. Returns C{None} if the Plugin with the
        provided name cannot be found.
    """
    pluginClassesDict = __pluginRegistryDict.get(locationType, {})
    exportPluginClass = pluginClassesDict.get(pluginName, None)
    return exportPluginClass

def GetLocationTypesForPluginName(pluginName):
    """
    Searches the registered plugins to find all the location types which
    have a plugin matching the name provided by the `pluginName` argument

    @type pluginName: C{str}
    @rtype: C{list} of C{str}
    @param pluginName: The name of the plugin to find within multiple
        locationTypes
    @return: A list of all the locationTypes plugins with the name matching
        the `pluginName` argument are registered to. Returns an empty list if
        no plugins are registred with the provided C{pluginName}
    """
    locationTypes = []
    for locationType, pluginClassesDict in __pluginRegistryDict.items():
        if pluginName in pluginClassesDict.keys():
            locationTypes.append(locationType)
    return locationTypes


def LoadPluginsFromKatanaResources(reloadCached=False):
    """
    Loads the plugins from the UsdExportPlugins subfolder of all
    folders registered to the KATANA_RESOURCES environment variable.

    @type reloadCached: C{bool}
    @param reloadCached: Decides whether it should reload modules this
        plugin system has already searched and added. By default this is
        set to False.
    """
    paths = KatanaResources.GetSearchPaths("UsdExportPlugins")
    plugins = Utils.Plugins.Load(
        "UsdExport", 1, paths, reloadCached=reloadCached)
    for plugin in plugins:
        if not plugin or len(plugin.data) != 2:
            continue
        RegisterUsdExportPlugin(plugin.name, plugin.data[0], plugin.data[1])
