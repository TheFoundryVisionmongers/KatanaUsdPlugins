# Copyright (c) 2020 The Foundry Visionmongers Ltd.
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

from Katana import RenderingAPI, FnAttribute

# From the KatanaUsdPlugins
import UsdKatana

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from pxr import UsdShade, Sdf, Gf, Sdr, Vt, UsdUI, Tf
    # These includes also require fnpxr
    from .common import (GetShaderNodeFromRegistry)
    from .typeConversionMaps import (ValueTypeCastMethods,
                                     ConvertRenderInfoShaderTagsToSdfType,
                                     ConvertToVtVec3fArray,
                                     ConvertParameterValueToGfType)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)


def WriteChildMaterial(stage, materialSdfPath, materialAttribute,
                       parentMaterialSdfPaths):
    """
    Writes a Katana child material as a sibling material, inheriting from the
    parent material paths (the whole hierarchy stored in order) by use of
    BaseMaterials.

    @type stage: C{Usd.Stage}
    @type materialSdfPath: C{Sdf.Path}
    @type materialAttribute: C{FnAttribute.GroupAttribute}
    @type parentMaterialSdfPaths: C{list}
    @rtype: C{UsdShade.Material} or C{None}
    @param stage: The USD stage to write the material to.
    @param materialSdfPath: The SdfPath to write this new child material to.
        Child materials should use the name of the parent material and the name
        of the child material separated by an underscore, e.g:
        ``GrandParentMaterial_ParentMaterial_ChildMaterial``.
        Where the hierarchy of Katana Material locations was:
        GrandParentMaterial
        --ParentMaterial
        ----ChildMaterial
    @param materialAttribute: The material attribute. Usually a sparse group
        attribute, with the change from the parent's material Attribute.
    @param parentMaterialSdfPaths: A list of the parent SdfPaths, in order from
        youngest parent, to oldest. Used to read the attribute types
        from the oldest parent, but specialize/inherit from the youngest
        parent.
    @return: The material created by this method or None if no child material
        was created.
    """
    WriteMaterial(stage, materialSdfPath, materialAttribute)
    # Only define the Material prim if it has not already been defined.
    # We write the 'material.nodes' if they exist first, so if these attributes
    # exist, we may have already created the Material prim.
    materialPrim = stage.GetPrimAtPath(materialSdfPath)
    if materialPrim:
        material = UsdShade.Material(materialPrim)
    else:
        material = UsdShade.Material.Define(stage, materialSdfPath)
    materialPath = material.GetPath()

    # We want to read the attribute data from the oldest parent (as this is
    # where material interfaces are defined), but we want to inherit
    # all of the values from the youngestParent (direct parent location)
    # Again, we cant use USD here, because these aren't parents in USD, because
    # parent->child inheritance is a construct not replicated in USD.
    # So we must pass these through as we create them.
    # For non-grand-children++  oldest and youngest will be the same.
    if len(parentMaterialSdfPaths) < 1:
        return material
    youngestParentSdfPath = parentMaterialSdfPaths[0]
    oldestParentSdfPath = parentMaterialSdfPaths[-1]
    oldestParentMaterial = UsdShade.Material(
        stage.GetPrimAtPath(oldestParentSdfPath))
    youngestParentMaterial = UsdShade.Material(
        stage.GetPrimAtPath(youngestParentSdfPath))
    if oldestParentMaterial and youngestParentMaterial:
        material.SetBaseMaterial(youngestParentMaterial)
        childName = (materialSdfPath.name)[len(youngestParentSdfPath.name)+1:]
        katanaLookAPISchema = UsdKatana.LookAPI(materialPrim)
        katanaLookAPISchema.Apply(materialPrim)
        katanaLookAPISchema.CreatePrimNameAttr(childName)
        if materialAttribute:
            parameters = materialAttribute.getChildByName("parameters")
            # We cant add materialInterfaces to child materials in Katana at
            # present, therefore we only need to check the oldest
            # parent Material.
            if parameters:
                OverwriteMaterialInterfaces(parameters, material,
                                            oldestParentMaterial)
        return material
    return None



def WriteMaterial(stage, materialSdfPath, materialAttribute):
    """
    Converts the given material C{GroupAttribute} into a C{UsdShade.Material}
    along with all the shaders and their connections.

    @type stage: C{Usd.Stage}
    @type materialSdfPath: C{Sdf.Path}
    @type materialAttribute: C{FnAttribute.GroupAttribute}
    @rtype: C{UsdShade.Material} or C{None}
    @param stage: The USD stage to write the material to.
    @param materialSdfPath: The path to write the C{UsdShade.Material} to.
    @param materialAttribute: The ``material`` attribute from Katana's
        scenegraph for the material location.
    @return: The C{UsdShade.Material} created by this function or C{None} if
        no material was created.
    """
    material = UsdShade.Material.Define(stage, materialSdfPath)
    if not materialAttribute:
        return None
    materialNodes = materialAttribute.getChildByName("nodes")
    materialPath = material.GetPath()

    if not materialNodes:
        return None
    CreateEmptyShaders(stage, materialNodes, materialPath)

    layoutAttr = materialAttribute.getChildByName("layout")

    # Now we have defined all the shaders we can connect them with no
    # issues.
    for materialNodeIndex in range(materialNodes.getNumberOfChildren()):
        materialNode = materialNodes.getChildByIndex(materialNodeIndex)
        shaderName = materialNodes.getChildName(materialNodeIndex)
        shaderPath = materialPath.AppendChild(
            Tf.MakeValidIdentifier(shaderName))
        shader = UsdShade.Shader.Get(stage, shaderPath)
        parametersAttr = materialNode.getChildByName("parameters")
        shaderId = str(shader.GetShaderId())

        if parametersAttr:
            AddMaterialParameters(parametersAttr, shaderId, shader)

        connectionsAttr = materialNode.getChildByName("connections")
        if connectionsAttr:
            AddShaderConnections(
                stage, connectionsAttr, materialPath, shader)

        if layoutAttr:
            shaderLayoutAttr = layoutAttr.getChildByName(shaderName)
            if shaderLayoutAttr:
                AddShaderLayout(shaderLayoutAttr, shader)

    interfaces = materialAttribute.getChildByName("interface")
    if interfaces:
        parameters = materialAttribute.getChildByName("parameters")
        AddMaterialInterfaces(stage, parameters, interfaces, material)

    terminals = materialAttribute.getChildByName("terminals")
    if terminals:
        AddTerminals(stage, terminals, material)
    return material


def AddShaderLayout(shaderLayoutAttr, shader):
    """
    Adds UsdUI.NodeGraphNodeAPI attributes to C{shader} prim such as:
    * Position
    * Color
    * View State

    @type shaderLayoutAttr: C{FnAttribute.GroupAttribute}
    @type shader: C{Sdf.Path}
    @param shaderLayoutAttr: Layout group attribute of the shader.
    @param shader: The UsdShader shader object from the Usd Stage.
    """
    nodeGraphAPI = UsdUI.NodeGraphNodeAPI(shader)

    #Add position
    nodePositionAttr = shaderLayoutAttr.getChildByName("position")
    if nodePositionAttr:
        nodePosition = nodePositionAttr.getNearestSample(0)
        nodeGraphAPI.CreatePosAttr(ConvertParameterValueToGfType(
            nodePosition, Sdf.ValueTypeNames.Double2))

    #Add color
    nodeColorAttr = shaderLayoutAttr.getChildByName("color")
    if nodeColorAttr:
        nodeColor = nodeColorAttr.getNearestSample(0)
        nodeGraphAPI.CreateDisplayColorAttr(ConvertParameterValueToGfType(
            nodeColor, Sdf.ValueTypeNames.Color3f))

    #Add expansion state
    nodeViewStateAttr = shaderLayoutAttr.getChildByName("viewState")
    if nodeViewStateAttr:

        usdExpansionStates = ("closed", "minimized", "open")
        nodeViewState = nodeViewStateAttr.getValue()

        if not isinstance(nodeViewState, int) or not 0 <= nodeViewState < 3:
            log.warning('Invalid value for the layout viewState attribute '
                        'of "%s" shader node', shader.GetPath())
            return

        nodeGraphAPI.CreateExpansionStateAttr(ConvertParameterValueToGfType(
            usdExpansionStates[nodeViewState], Sdf.ValueTypeNames.Token))



def CreateEmptyShaders(stage, materialNodes, materialPath):
    """
    Creates all the shader prims for the current material but only fills in the
    type. This is because the connections require both materials to be present.
    The parameters are populated at a later stage, once all shaders for this
    material are created. Called by L{WriteMaterial}.

    @type stage: C{Usd.Stage}
    @type materialNodes: C{FnAttribute.GroupAttribute}
    @type materialPath: C{Sdf.Path}
    @param stage: The C{Usd.Stage} to write the shaders to.
    @param materialNodes: The ``material.nodes`` C{FnAttribute.GroupAttribute}
        from the attributes of the current location.
    @param materialPath: The path to the material in the Usd Stage.
    """
    # We may want the option in the future to write out the shaders from
    # material.layout instead, as this includes nodes which may not be
    # connected.
    for materialNodeIndex in range(materialNodes.getNumberOfChildren()):
        materialNode = materialNodes.getChildByIndex(materialNodeIndex)
        shaderName = Tf.MakeValidIdentifier(
            materialNodes.getChildName(materialNodeIndex))
        shaderPath = materialPath.AppendChild(shaderName)
        shader = UsdShade.Shader.Define(stage, shaderPath)
        shaderIdAttr = materialNode.getChildByName("type")
        shaderId = str(shaderIdAttr.getValue())
        # Special case for Arnold
        targetAttr = materialNode.getChildByName("target")
        if targetAttr.getValue() == "arnold":
            shaderId = f"arnold:{shaderId}"
        shader.SetShaderId(shaderId)


def AddTerminals(stage, terminals, material):
    """
    Adds the terminals onto the USD material. Called by L{WriteMaterial}.

    @type stage: C{Usd.Stage}
    @type terminals: C{FnAttribute.GroupAttribute}
    @type material: The Material to write the terminals to.
    @param stage: The C{Usd.Stage} to write to.
    @param terminals: The ``material.terminals`` attribute
    @param material: The Material to write the terminals to.
    """
    supportedOutputTypes = ["surface", "displacement", "volume"]
    for terminalIndex in range(terminals.getNumberOfChildren()):
        terminalAttr = terminals.getChildByIndex(terminalIndex)
        terminalName = terminals.getChildName(terminalIndex)
        # Ignore the ports as terminals, we fetch these to create the
        # shader port name anyway
        if "Port" in terminalName:
            continue

        portName = terminals.getChildByName(terminalName + "Port")
        # Must have a port name to be able to know what the output of the
        # shader should be called.
        if not portName:
            continue

        # Universal (catch all for unrecognized renderers)
        outputPrefix = ""
        outputType = "surface"

        # If we understand the renderer, use the render context
        # appropriate to it
        if terminalName.startswith("usd"):
            # Don't prefix the output, and leave target type deduction to other means.
            outputType = terminalName[3:].lower()
        elif terminalName.startswith("prman"):
            outputPrefix = "ri:"
            outputType = terminalName[5:].lower()
            if outputType == "bxdf":
                outputType = "surface"
        elif terminalName.startswith("arnold"):
            outputPrefix = "arnold:"
            outputType = terminalName[6:].lower()
        elif terminalName.startswith("dl"):
            outputPrefix = "nsi:"
            outputType = terminalName[2:].lower()

        if outputType not in supportedOutputTypes:
            log.warning('Unable to map terminal "%s" to a supported '
                        'USD shade output type, of either\n%s',
                        terminalName, " ".join(supportedOutputTypes))
            continue

        terminalName = outputPrefix + outputType
        terminalShader = Tf.MakeValidIdentifier(str(terminalAttr.getValue()))
        materialTerminal = material.CreateOutput(terminalName,
                                                 Sdf.ValueTypeNames.Token)
        materialPath = material.GetPath()
        terminalShaderPath = materialPath.AppendChild(terminalShader)
        terminalShader = UsdShade.Shader.Get(stage, terminalShaderPath)
        terminalShaderConnectableApi = UsdShade.ConnectableAPI(terminalShader)
        materialTerminal.ConnectToSource(terminalShaderConnectableApi,
                                         portName.getValue())


def AddMaterialParameters(parametersAttr, shaderId, shader):
    """
    Iterates through all the parameters in C{parametersAttr}. Adds the
    parameters onto the shader, calls L{AddParameterToShader}. Called by
    L{WriteMaterial}.

    @type parametersAttr: C{FnAttribute.GroupAttribute}
    @type shaderId: C{str}
    @type shader: C{Usd.Shader}
    @param parametersAttr: The GroupAttribute relating to
        C{material.nodes.<nodeName>.parameters}.
    @param shaderId: The ``str`` ID of the shader (its type).
    @param shader: The UsdShader shader object from the Usd Stage.
    """
    for paramIndex in range(parametersAttr.getNumberOfChildren()):
        paramName = parametersAttr.getChildName(paramIndex)
        paramAttr = parametersAttr.getChildByIndex(paramIndex)
        AddParameterToShader(paramName, paramAttr, shader, shaderId)


def AddParameterToShader(shaderParamName, paramAttr, shader, shaderId=None,
                         paramName=None, sdfType=None):
    """
    Adds a parameter as an input onto a given Shader or Material prim.

    @type shaderParamName: C{str}
    @type paramAttr: C{FnAttribute.GroupAttriute}
    @type shader: C{UsdShade.Shader}
    @type shaderId: C{str}
    @type paramName: C{str}
    @type sdfType: C{NdrSdfTypeIndicator}
    @rtype: C{Usd.Shader.Input} or C{None}
    @param shaderParamName: The parameter name as it appears on the shader.
        This is used to determine the type of the shader attribute.
    @param paramAttr: The Katana attribute to read the value from, as well
        as timesamples.
    @param shader: The shader or material to add the shaderInput onto. If
        this is not the shader the parameter originally belongs to, and
        is part of a material interface, the shaderId should also be
        provided for the shaderId of the shader the param would have
        originally be set to. This is in order to retrieve the correct
        SdfType.
    @param shaderId: The Id of the shader to search for the SdrShadingNode to
        search for the parameters sdfType. This should be provided if the
        shader provided is not the shader you want to write to is not
        the SdrShaderNode you want to read the inputs and therefore
        sdfTypes from.
    @param paramName: Optional argument. The name to be given to the attribute
        on the shader. This can differ from the L{shaderParamName} in cases of
        material parameter interfaces.
    @param sdfType: Optional argument, This can override any retrieval of
        sdfType from the shader, or shaderId from the Usd SdrRegistry.
        This is useful if you want to use the sdfType from connected
        attributes; this is used for material interfaces.
    @return: The shader input created by this method or C{None} if no shader
        input was created.
    """
    if shaderId is None and sdfType is None:
        shaderId = shader.GetShaderId()
    if paramName is None:
        paramName = shaderParamName
    timeSamples = paramAttr.getNumberOfTimeSamples()
    if timeSamples > 1:
        paramValue = paramAttr.getNearestSample(0)
        # TODO(jp): Get per sample values and save those into USD.
    else:
        paramValue = paramAttr.getNearestSample(0)
    # Check to make sure that either the sdfType has been set or the shaderId
    # has been found using the shader.
    if not shaderId and sdfType is None:
        log.warning('Unable to find shaderId, and sdfType is not provided for '
                    'shaderParamName:%s', shaderParamName)
    if sdfType is None:
        shaderType = _ShaderIdToShaderType(shaderId)
        sdfType = GetShaderAttrSdfType(shaderType, shaderParamName,
                                       isOutput=False)
        if sdfType is None:
            log.debug('Unable to find input port "%s" on input '
                      'of shader ID: %s', shaderParamName, shaderId)
            sdfType = Sdf.ValueTypeNames.Token

        # TODO(jp): Get widget hints for parameter to work out if its
        # an assetID or not. Some strings are meant to be "asset"
        # types in USD. This is if we want some backup type conversion.
        if paramName == "file":
            sdfType = Sdf.ValueTypeNames.Asset
        if paramName == "varname":
            # Used in properties such as the texcoordreader inputs,
            # but the sdftype of 'string' does not work, so force to a token
            sdfType = Sdf.ValueTypeNames.Token
        if paramName in ("ramp_Knots", "ramp_Floats", "ramp_Colors"):
            # Special case for some ramp shaders where neither the sdr registry
            # nor the info shader tags give us the correct sdf type
            sdfType = Sdf.ValueTypeNames.FloatArray
        if sdfType is None:
            log.warning('Unable to find an Sdf conversion of '
                        'shader:%s and param:%s', shaderId, paramName)
            # Best guess because we've been unable to identify any shader node
            # information from the shaderId (is the renderer supported?)
            if "color" in paramName:
                log.warning('Guessed shader:%s and param:%s will '
                            'use datatype color3f', shaderId, paramName)
                sdfType = Sdf.ValueTypeNames.Color3f
    if sdfType:
        shaderInput = shader.CreateInput(paramName, sdfType)
        paramValue = ConvertParameterValueToGfType(paramValue, sdfType)
        shaderInput.Set(paramValue)
        return shaderInput
    return None

def _ShaderIdToShaderType(shaderId):
    idTokens = Sdf.Path.TokenizeIdentifier(shaderId)
    # Only Arnold uses namespaced ShaderID now
    if len(idTokens) == 2:
        return idTokens[1]
    return shaderId

def _EncodePortName(name):
    """
    Encodes a shader port name, returning a string that is safe to output as a
    UsdShade connection.

    @type name: C{str}
    @rtype: C{str}
    @param name: The port name to encode.
    @return: The encoded port name.
    """
    return name.replace(".", ":")


def _DecodePortName(name):
    """
    Decode a shader port name, returning a string that is safe to use to search
    the RenderInfoPlugins.

    @type name: C{str}
    @rtype: C{str}
    @param name: The port name to encode.
    @return: The encoded port name.
    """
    return name.replace(":", ".")


def _GetConnectionInputPort(shaderNode, portName):
    """
    Returns the input port name for a (maybe dot-delimited) port name, or
    C{None} if the input cannot be found.

    @type shaderNode: C{UsdShade.Shader}
    @type portName: C{str}
    @rtype: C{UsdShade.Input} or C{None}
    @param shaderNode: The shader on which to search for ports.
    @param portName: The name of the port to return.
    @return: The found port.
    """
    # Some renderers do not publish the dot-delimited names to the SDR, only
    # making the base name available; in this case, we iteratively remove name
    # components until we find a matching port.
    while portName:
        inputPort = shaderNode.GetInput(portName)
        if inputPort:
            return inputPort
        elif "." in portName:
            portName = portName[0:portName.rindex(".")]
        else:
            break

    return None


def _ReadConnectionsGroup(stage, connectionsAttr, materialPath, shader,
                          connectionPrefix=None):
    """
    Adds the shader connections from C{connectionsAttr}, recursing if necessary
    to handle channel components.

    @type stage: C{Usd.Stage}
    @type connectionsAttr: C{FnAttribute.GroupAttribute}
    @type materialPath: C{Sdf.Path}
    @type shader: C{UsdShade.Shader}
    @type connectionPrefix: C{str} or C{None}
    @rtype: C{list} of C{str}
    @param stage: The USD stage to write into.
    @param connectionsAttr: The C{GroupAttribute} to read connections from.
    @param materialPath: The USD path for the material containing
        ``shaderNode``.
    @param shader: The USD Prim for the shader to add connections to.
    @param connectionPrefix: A string prepended to port names when recursing.
    @return: The port order for ``shaderNode``.
    """
    if not isinstance(connectionsAttr, FnAttribute.GroupAttribute):
        log.warning("Connections attribute for '%s' is malformed.",
                    materialPath)
        return None

    portOrder = []
    for childName, childAttr in connectionsAttr.childList():
        if connectionPrefix:
            connectionName = "%s.%s" % (connectionPrefix, childName)
        else:
            connectionName = childName

        # childAttr may itself be a GroupAttribute, if we have connections to
        # the individual components of ports.
        if isinstance(childAttr, FnAttribute.GroupAttribute):
            portOrder.extend(
                _ReadConnectionsGroup(stage, childAttr, materialPath,
                                      shader, connectionName))
        elif isinstance(childAttr, FnAttribute.StringAttribute):
            connection = childAttr.getValue()

            # Split the connection; the first part is the input port name and
            # the second part is the input shader name.
            splitConnection = connection.split("@")
            if len(splitConnection) != 2:
                log.warning('Connection "%s" is malformed.', connection)
                continue

            outputPortName = _EncodePortName(splitConnection[0])
            outputShaderName = splitConnection[1]
            connectionShaderPath = materialPath.AppendChild(outputShaderName)
            outputShader = UsdShade.Shader.Get(stage, connectionShaderPath)
            if not outputShader or not outputShader.GetSchemaKind():
                continue

            # Currently we cannot be sure that the connectionName such as
            # out.r is in the SdrRegistry.
            portConnectionSdfType = GetShaderAttrSdfType(
                _ShaderIdToShaderType(shader.GetShaderId()), connectionName, isOutput=False)
            if portConnectionSdfType is None:
                    log.warning(
                        'Unable to find input port for connection "%s".',
                        connection)
                    continue
            connectionName = _EncodePortName(connectionName)
            inputPort = shader.CreateInput(
                connectionName, portConnectionSdfType)

            # We need to specify the output type of the source, or it inherits
            # the input type.
            sourceSdfType = GetShaderAttrSdfType(_ShaderIdToShaderType(outputShader.GetShaderId()),
                                                 outputPortName,
                                                 isOutput=True)
            if not sourceSdfType:
                log.warning(
                    'Unable to map type for output on connection "%s", '
                    'assuming "Token".',
                    connection)
                sourceSdfType = Sdf.ValueTypeNames.Token

            outputShaderConnectableApi = UsdShade.ConnectableAPI(outputShader)
            inputPort.ConnectToSource(
                outputShaderConnectableApi, str(outputPortName),
                UsdShade.AttributeType.Output, sourceSdfType)

            # Record the order this port appears (USD connections are stored as
            # a dictionary rather than a list, so this info would be lost).
            portOrder.append(inputPort.GetFullName())
        else:
            log.warning(
                'Connections attribute for "%s" is malformed; expected a '
                'StringAttribute or a GroupAttribute.', materialPath)

    return portOrder


def AddShaderConnections(stage, connectionsAttr, materialPath, shader):
    """
    Adds the connections between the shaders from the
    C{material.nodes.<nodeName>.connections} attribute. The shaders must be
    written first in order to connect to the current shader. The types will
    be read from the Usd shader info from the L{GetShaderAttrSdfType} method.
    Both shaders must be part of the same material. Called by L{WriteMaterial}.

    @type stage: C{Usd.Stage}
    @type connectionsAttr: C{FnAttribute.GroupAttribute}
    @type materialPath: C{Sdf.Path}
    @type shader: C{UsdShade.Shader}
    @param stage: The Usd stage to write to.
    @param connectionsAttr: The material.nodes.<nodeName>.connections Katana
        attribute to read the connection information from.
    @param materialPath: The USD path for the material for this shader and the
        shader it will connect to.
    @param shader: The USD Prim for the shader to add the connection to.
    """
    # In Katana, the order of ports can be important, e.g. NetworkMaterialEdit
    # and Switch nodes.
    portOrder = _ReadConnectionsGroup(
        stage, connectionsAttr, materialPath, shader)
    shader.GetPrim().SetPropertyOrder(portOrder)


def OverwriteMaterialInterfaces(parametersAttr, material, parentMaterial):
    """
    For child materials, this method overwrites the material interface values
    rather than creating anything new from the attributes. Called by
    L{WriteChildMaterial}.

    @type parametersAttr: C{FnAttribute.GroupAttribute}
    @type material: C{UsdShade.Material}
    @type parentMaterial: C{UsdShade.Material}
    @param parametersAttr: The ``material.parameters`` attribute from the
        current Katana location.
    @param material: The child material to write the new values for the
        material interface
    @param parentMaterial: The parent material to read the interface attribute
        information from, including its type. This must be the highest level
        parent material.
    """
    # Not techincally the parent in USD, but it is the parent from Katana.
    parentInputs = parentMaterial.GetInputs()
    for parameterIndex in range(parametersAttr.getNumberOfChildren()):
        parameterName = parametersAttr.getChildName(parameterIndex)
        parameterAttr = parametersAttr.getChildByIndex(parameterIndex)
        # Because the parameter interface may have a group namespace, but
        # the parameterName does not contain this, we need to try and find
        # the related shaderInput.
        matchingParamFullName = None
        paramSdfType = None
        for shaderInput in parentInputs:
            inputParamName = shaderInput.GetBaseName()
            if inputParamName == parameterName:
                matchingParamFullName = inputParamName
                paramSdfType = shaderInput.GetTypeName()
        if matchingParamFullName and paramSdfType:
            AddParameterToShader(parameterName, parameterAttr, material,
                                sdfType=paramSdfType,
                                paramName=matchingParamFullName)


def AddMaterialInterfaces(stage, parametersAttr, interfacesAttr, material):
    """
    Adds the parameter interface attributes onto the material, and add
    connections from the shader to these values. Called by L{WriteMaterial}.

    @type stage: C{Usd.Stage}
    @type parametersAttr: C{FnAttribute.GroupAttribute}
    @type interfacesAttr: C{FnAttribute.GroupAttribute}
    @type material: C{UsdShade.Material}
    @param stage: The Usd Stage to write the new prims and parameters to
    @param parametersAttr: The attribute from materials.parameters. If no
        parametersAttr is provided, this is assumed to then be the default
        value.
    @param interfacesAttr: The attribute from materials.interface
    @param material: The material prim to add the parameter interface to.
    """
    for interfaceIndex in range(interfacesAttr.getNumberOfChildren()):
        interfaceName = interfacesAttr.getChildName(interfaceIndex)
        interfaceAttr = interfacesAttr.getChildByIndex(interfaceIndex)
        hintsAttr = interfaceAttr.getChildByName("hints")
        groupName = None
        if hintsAttr:
            pageAttr = hintsAttr.getChildByName("page")
            if pageAttr:
                groupName = pageAttr.getValue()

        sourceAttr = interfaceAttr.getChildByName("src")
        sourceShaderName, sourceParamName = str(
            sourceAttr.getValue()).split(".")
        sourceShaderPath = material.GetPath().AppendChild(sourceShaderName)
        sourceShader = UsdShade.Shader.Get(stage, sourceShaderPath)
        shaderId = sourceShader.GetShaderId()
        shaderType = _ShaderIdToShaderType(shaderId)
        #Save the parameter name so we can find it in the parameters attribute.
        parameterName = interfaceName

        materialPort = None
        if parametersAttr:
            paramAttr = parametersAttr.getChildByName(parameterName)
            if paramAttr:
                materialPort = AddParameterToShader(
                    sourceParamName, paramAttr, material, shaderId,
                    paramName=interfaceName)

        if not materialPort:
            sdfType = GetShaderAttrSdfType(shaderType, sourceParamName,
                                           isOutput=False)
            #Default to Token Type if not found.
            if sdfType is None:
                log.debug('Unable to find input port "%s" on input '
                          'of shader ID: %s', sourceParamName, shaderId)
                sdfType = Sdf.ValueTypeNames.Token
            materialPort = material.CreateInput(interfaceName, sdfType)

            sourceShaderPort = sourceShader.GetInput(sourceParamName)
            if sourceShaderPort:
                value = sourceShaderPort.Get()
                materialPort.Set(value)
            else:
                shaderNode = GetShaderNodeFromRegistry(shaderType)
                if shaderNode:
                    shaderNodeInput = shaderNode.GetInput(sourceParamName)
                    materialPort.Set(shaderNodeInput.GetDefaultValue())

        if groupName:
            # For now we need to manually replace "." with ":". In the future
            # we should use SetNestedDisplayGroups() when avialiable.
            groupName = groupName.replace(".", ":")
            materialPort.SetDisplayGroup(groupName)

        sourceSdfType = materialPort.GetTypeName()
        sourceShaderPort = sourceShader.CreateInput(sourceParamName,
                                                    sourceSdfType)

        materialConnectableApi = UsdShade.ConnectableAPI(material)
        sourceShaderPort.ConnectToSource(
            materialConnectableApi, interfaceName,
            UsdShade.AttributeType.Input, sourceSdfType)


def GetShaderAttrSdfType(shaderType, shaderAttr, isOutput=False):
    """
    Retrieves the SdfType of the attribute of a given Shader.
    This is retrieved from the Usd Shader Registry C{Sdr.Registry}. The shaders
    attempted to be written must be registered to Usd in order to write
    correctly. If not found in the Shader Registry, try to do our best to
    find the type from the RenderInfoPlugin.

    @type shaderType: C{str}
    @type shaderAttr: C{str}
    @type isOutput: C{bool}
    @rtype: C{Sdf.ValueTypeNames}
    @param shaderType: The name of the shader.
    @param shaderAttr: The name of the attribute on the shader.
    @param isOutput: Whether the attribute is an input or output. C{False}
        by default. Set to True if the attribute is an output.
    @return: The Usd Sdf Type for the attribute of the provided shader or None
        if the shaderType cannot be found or the shaderAttr cannot be found.
    """
    shader = GetShaderNodeFromRegistry(shaderType)
    if shader:
        # From the Docs: Two scenarios can result: an exact mapping from
        # property type to Sdf type, and an inexact mapping. In the first
        # scenario, the first element in the pair will be the cleanly-mapped
        # Sdf type, and the second element, a TfToken, will be empty. In the
        # second scenario, the Sdf type will be set to Token to indicate an
        # unclean mapping, and the second element will be set to the original
        # type returned by  GetType().
        # From USD code: (So we know what an NdrSdfTypeIndicator is in the future)
        # typedef std::pair<SdfValueTypeName, TfToken> NdrSdfTypeIndicator;
        if isOutput:
            shaderOutput = shader.GetOutput(shaderAttr)
            if shaderOutput:
                return shaderOutput.GetTypeAsSdfType()[0]
        else:
            shaderInput = shader.GetInput(shaderAttr)
            if shaderInput:
                return shaderInput.GetTypeAsSdfType()[0]
        log.debug('Unable to read input %s from shader %s in the '
                  'Sdr.Registry; trying to infer type via the '
                  'RenderInfoPlugin RenderInfoPlugin information.',
                  shaderAttr, shaderType)
    else:
        log.debug('Unable to read input shader %s in the Sdr.Registry. '
                  'Trying to infer type via the RenderInfoPlugin '
                  'information', shaderType)

    # We might need to decode a port name if it was encoded from "."
    # delimitors
    shaderAttr = _DecodePortName(shaderAttr)

    # Fallback if we cant find the shaders info in the SdrRegistry
    for renderer in RenderingAPI.RenderPlugins.GetRendererPluginNames(
            True):
        infoPlugin = RenderingAPI.RenderPlugins.GetInfoPlugin(renderer)
        if shaderType in infoPlugin.getRendererObjectNames("shader"):
            if isOutput:
                tags = infoPlugin.getShaderOutputTags(shaderType, shaderAttr)
            else:
                tags = infoPlugin.getShaderInputTags(shaderType, shaderAttr)
            if tags:
                return ConvertRenderInfoShaderTagsToSdfType(tags)
    return None


def WriteMaterialAssign(material, overridePrim):
    """
    Uses the UsdShade.MaterialBindingAPI to bind the material to the
    overridePrim.

    @type material: C{UsdShade.Material}
    @type overridePrim: C{Usd.OverridePrim}
    @param material: The material to bind.
    @param overridePrim: The Prim to bind to.
    """
    UsdShade.MaterialBindingAPI(overridePrim.GetPrim()).Bind(material)
