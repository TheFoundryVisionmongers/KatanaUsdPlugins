# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import logging

from Katana import RenderingAPI, FnAttribute
import PyFnAttribute

# From the KatanaUsdPlugins
import UsdKatana

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from fnpxr import Usd, UsdShade, Sdf, Gf, Ndr, Sdr
    # These includes also require fnpxr
    from .typeConversionMaps import (valueTypeCastMethods,
                                     convertRenderInfoShaderTagsToSdfType)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)


def WriteChildMaterial(stage, materialSdfPath, materialAttribute,
                       parentMaterialSdfPaths):
    """
    @param stage: The USD stage to write the material to.
    @param materialSdfPath: The SdfPath to write this new child material to.
        Child materials should use the name of the parent material and the name
        of the child material separated by an underscore, e.g:
        GrandParentMaterial_ParentMaterial_ChildMaterial
        Where the hierarchy of Katana Material locations was:
        GrandParentMaterial
        --ParentMaterial
        ----ChildMaterial
    @param materialAttribute: The material attribute. Usually a sparse group
        attribute, with the change from the parent's material Attribute.
    @param parentMaterialSdfPaths: A list of the parent SdfPaths, in order from
        youngest parent, to oldest. We use this to read the attribute types
        from the oldest parent, but specialize/inherit from the youngest
        parent.
    @type stage: C{Usd.Stage}
    @type materialSdfPath: C{Sdf.Path}
    @type materialAttribute: C{FnAttribute.GroupAttribute}
    @type parentMaterialSdfPaths: C{list}
    """
    if not materialAttribute:
        return None
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
        parameters = materialAttribute.getChildByName("parameters")
        # We cant add materialInterfaces to child materials in Katana at
        # present, therefore we only need to check the oldest parent Material.
        if parameters:
            OverWriteMaterialInterfaces(stage, parameters, materialPath,
                                        material, oldestParentMaterial)
        return material



def WriteMaterial(stage, materialSdfPath, materialAttribute):
    """
    https://graphics.pixar.com/usd/docs/api/usd_shade_page_front.html
        The entry point to writing a material and all its shaders into the Usd
        Stage.
        Returns material.
    """
    material = UsdShade.Material.Define(stage, materialSdfPath)
    if not materialAttribute:
        return
    materialNodes = materialAttribute.getChildByName("nodes")
    materialPath = material.GetPath()

    if not materialNodes:
        return
    CreateEmptyShaders(stage, materialNodes, materialPath)

    # Now we have defined all the shaders we can connect them with no
    # issues.
    for materialNodeIndex in xrange(materialNodes.getNumberOfChildren()):
        materialNode = materialNodes.getChildByIndex(materialNodeIndex)
        shaderName = materialNodes.getChildName(materialNodeIndex)
        shaderPath = materialPath.AppendChild(shaderName)
        shader = UsdShade.Shader.Get(stage, shaderPath)
        parametersAttr = materialNode.getChildByName("parameters")
        shaderId = str(shader.GetShaderId())

        if parametersAttr:
            WriteMaterialParameters(parametersAttr, shaderId, shader)
        connectionsAttr = materialNode.getChildByName("connections")
        if connectionsAttr:
            WriteShaderConnections(
                stage, connectionsAttr, materialPath, shader)

    interfaces = materialAttribute.getChildByName("interface")
    if interfaces:
        parameters = materialAttribute.getChildByName("parameters")
        WriteMaterialInterfaces(stage, parameters, interfaces, material)

    terminals = materialAttribute.getChildByName("terminals")
    if terminals:
        WriteTerminals(stage, terminals, materialPath, material)
    return material


def CreateEmptyShaders(stage, materialNodes, materialPath):
    """ This method creates all the shader prims for the current material
        but only fills in the type.  This is because the connections require
        both materials to be present, so we fill in parameters at a later
        stage, once all shaders for this material are created.
        :TODO: We may want the option in the future to write out the shaders
        from material.layout instead, as this includes nodes which may not be
        connected.
        @param stage: The Usd.Stage to write the shaders to.
        @param materialNodes: The material.nodes GroupAttribute from the
            attributes of the current location.
        :materialPath: SdfPath, the path to the material in the Usd Stage.
    """
    for materialNodeIndex in xrange(materialNodes.getNumberOfChildren()):
        materialNode = materialNodes.getChildByIndex(materialNodeIndex)
        shaderName = materialNodes.getChildName(materialNodeIndex)
        shaderPath = materialPath.AppendChild(shaderName)
        shader = UsdShade.Shader.Define(stage, shaderPath)
        shaderIdAttr = materialNode.getChildByName("type")
        shaderId = str(shaderIdAttr.getValue())
        shader.SetShaderId(shaderId)


def WriteTerminals(stage, terminals, materialPath, material):
    """ Write the terminals onto the Material outputs.
    """
    supported_output_types = ["surface", "displacement", "volume"]

    #:TODO: We might want to base this on the actual outputs from the material
    # definitions from USD itself, rather than from Katana, using Katana to
    # just try and match up, and if there is a match between these terminals,
    # and the outputs on a shader, use that.
    for terminalIndex in xrange(terminals.getNumberOfChildren()):
        terminalAttr = terminals.getChildByIndex(terminalIndex)
        terminalName = terminals.getChildName(terminalIndex)
        # We dont want to export the ports as terminals..
        if "Port" in terminalName:
            continue

        # universal (catch all for unrecognised renderers)
        output_prefix = ""
        output_type = "surface"

        # if we understand the renderer, use the render context
        # appropriate to it
        if terminalName.startswith("usd"):
            output_prefix = "glslfx:"
            output_type = terminalName[3:].lower()
        elif terminalName.startswith("prman"):
            output_prefix = "ri:"
            output_type = terminalName[5:].lower()
            if output_type == "bxdf":
                output_type = "surface"
        elif terminalName.startswith("arnold"):
            output_prefix = "arnold:"
            output_type = terminalName[6:].lower()
        elif terminalName.startswith("dl"):
            output_prefix = "nsi:"
            output_type = terminalName[2:].lower()

        if output_type not in supported_output_types:
            log.warning("Unable to map terminal '%s' to a supported "
                "USD shade output type, of either\n%s",
                terminalName, " ".join(supported_output_types))
            continue

        terminalName = output_prefix + output_type

        terminalShader = str(terminalAttr.getValue())
        materialTerminal = material.CreateOutput(terminalName,
                                                 Sdf.ValueTypeNames.Token)

        terminalShaderPath = materialPath.AppendChild(terminalShader)
        terminalShader = UsdShade.Shader.Get(stage, terminalShaderPath)
        # :TODO: Make this work based on the target attribute from the shader.
        # We will have to link to the attributes from Katana for this, as there
        # isnt a place for this on the shader itself (shaders are targetless
        # in USD)
        materialTerminal.ConnectToSource(terminalShader, terminalName)


def WriteMaterialParameters(parametersAttr, shaderId, shader):
    """ Writing the parameters section for each shader.

        @param parametersAttr: The GroupAttribute relating to
            material.nodes.<nodeName>.parameters
        @param shaderId: The `str` ID of the shader (its type)
        @param shader: The UsdShader shader object from the Usd Stage.
        @type parametersAttr: C{FnAttribute.GroupAttribute}
        @type shaderId: C{str}
        @type shader: C{Usd.Shader}
    """
    for paramIndex in xrange(parametersAttr.getNumberOfChildren()):
        paramName = parametersAttr.getChildName(paramIndex)
        paramAttr = parametersAttr.getChildByIndex(paramIndex)
        addParameterToShader(paramName, paramAttr, shader, shaderId)


def addParameterToShader(shaderParamName, paramAttr, shader, shaderId=None,
                         paramName=None, sdfType=None):
    """ Adds a parameter as an input onto a given Shader or Material prim

        @param shaderParamName: The parameter name as it appears on the shader.
            This is used to determine the
        @param paramAttr: The Katana attribute to read the value from, as well
             as timesamples.
        @param shader: The shader or material to add the shaderInput onto.  If
            this is not the shader the parameter originally belongs to, and
            is part of a material interface, the shaderId should also be
            provided for the shaderId of the shader the param would have
            originally be set to. This is in order to retrieve the correct
            SdfType.
        @shaderId: The Id of the shader to search for the SdrShadingNode to
            search for the parameters sdfType.  This should be provided if the
            shader provided is not the shader you want to write to is not
            the SdrShaderNode you want to read the inputs and therefore
            sdfTypes from.
        @param sdfType: Optional argument, This can override any retrieval of
            sdfType from the shader, or shaderId from the Usd SdrRegistry.
            This is useful if you want to use the sdfType from connected
            attriubutes as we do for material interfaces.
        @type shaderParamName: C{str}
        @type paramAttr: C{FnAttribute.GroupAttriute}
        @type shader: C{UsdShade.Shader}
        @type shaderId: C{str}
        @type sdfType: C{SdfTypeIndicator}
        @return: C{Usd.Shader.Input} created by this method.
    """
    if shaderId is None and sdfType is None:
        shaderId = shader.GetShaderId()
    if paramName is None:
        paramName = shaderParamName
    timeSamples = paramAttr.getNumberOfTimeSamples()
    if timeSamples > 1:
        paramValue = paramAttr.getNearestSample(0)
        pass  # TODO: Get per sample values and save those into USD.
    else:
        paramValue = paramAttr.getNearestSample(0)
    # Check to make sure that either the sdfType has been set or the shaderId
    # has been found using the shader.
    if not shaderId and sdfType is None:
        log.warning("Unable to find shaderId, and sdfType is not provided for"
        " shaderParamName:{}".format(
            shaderParamName))
    if sdfType is None:
        sdfType = GetShaderAttrSdfType(shaderId, shaderParamName,
                                       isOutput=False)
        #TODO: Get widget hints for parameter to work out if its
        # an assetID or not. Some strngs are meant to be "asset"
        # types in USD.  This is if we want some backup type conversion.
        if paramName == "file":
            sdfType = Sdf.ValueTypeNames.Asset
        if paramName == "varname":
            # used in properties such as the texcoordreader inputs,
            # but the sdftype of 'string' does not work, so force to a token
            sdfType = Sdf.ValueTypeNames.Token
        if sdfType == None:
            log.warning("Unable to find an Sdf conversion of "
                "shader:%s and param:%s", shaderId, paramName)
            # best guess because we've been unable to identify any shader node
            # information from the shaderId (is the renderer supported?)
            if "color" in paramName:
                log.warning("Guessed shader:%s and param:%s will "
                    "use datatype color3f", shaderId, paramName)
                sdfType = Sdf.ValueTypeNames.Color3f
    if sdfType:
        if sdfType.type.pythonClass:
            gfCast = sdfType.type.pythonClass
        else:
            gfCast = valueTypeCastMethods.get(sdfType)
        input = shader.CreateInput(paramName, sdfType)
        if gfCast:
            if isinstance(paramValue,  PyFnAttribute.ConstVector):
                # Convert Katana's PyFnAttribute.ConstVector to a python list
                paramValue = [v for v in paramValue]
            if isinstance(paramValue, list):
                if len(paramValue) == 1:
                    paramValue = gfCast(paramValue[0])
                elif hasattr(gfCast, "dimension"):
                    if gfCast.dimension == 2:
                        paramValue = gfCast(paramValue[0], paramValue[1])
                    elif gfCast.dimension == 3:
                        paramValue = gfCast(paramValue[0], paramValue[1],
                            paramValue[2])
                    elif gfCast.dimension == 4:
                        paramValue = gfCast(paramValue[0], paramValue[1],
                            paramValue[2], paramValue[3])
                    elif gfCast.dimension == (2, 2):
                        paramValue = gfCast(
                            paramValue[0], paramValue[1],
                            paramValue[2], paramValue[3])
                    elif gfCast.dimension == (3, 3):
                        paramValue = gfCast(
                            paramValue[0], paramValue[1], paramValue[2],
                            paramValue[3], paramValue[4], paramValue[5],
                            paramValue[6], paramValue[7], paramValue[8])
                    elif gfCast.dimension == (4, 4):
                        pv = paramValue
                        paramValue = gfCast(
                            pv[0], pv[1], pv[2], pv[3],
                            pv[4], pv[5], pv[6], pv[7],
                            pv[8], pv[9], pv[10], pv[11],
                            pv[12], pv[13], pv[14], pv[15])
                elif gfCast in [Gf.Quath, Gf.Quatf, Gf.Quatd]:
                    paramValue = gfCast(paramValue[0], paramValue[1],
                        paramValue[2], paramValue[3])
        else:
            paramValue = gfCast(paramValue)
    input.Set(paramValue)
    return input


def WriteShaderConnections(stage, connectionsAttr, materialPath, shader):
    """ Writing the connections from the material.nodes.<nodeName>.connections
        This will haveto read the types from the Usd shader info from the
        shader Registry (Sdr).
    """
    shaderNode = GetShaderNodeFromRegistry(shader.GetShaderId())

    if not shaderNode:
        log.warning(
            "Unable to write shadingNode connections for path {0},"
            "cannot find shaderID {1}".format(
                materialPath, shader.GetShaderId()))
        return
    for connectionIndex in xrange(connectionsAttr.getNumberOfChildren()):
        connectionName = connectionsAttr.getChildName(connectionIndex)
        connectionAttr = connectionsAttr.getChildByIndex(connectionIndex)
        # Split the connection, first part is the name of the attribute,
        # second is the shader it comes from
        splitConnection = str(connectionAttr.getValue()).split("@")
        inputShaderPortName = splitConnection[0]
        inputShaderName = splitConnection[-1]

        connectionshaderPath = materialPath.AppendChild(inputShaderName)
        inputShader = UsdShade.Shader.Get(stage, connectionshaderPath)
        if not inputShader.GetSchemaType():
            continue
        sourceSdfType = GetShaderAttrSdfType(inputShader.GetShaderId(),
                                             inputShaderPortName,
                                             isOutput=True)

        # the input type is more specific than the type it connects to
        # i.e. the connection may deliver POD but the semantics of its use
        # are in the input type
        portConnectionSdfType = shaderNode.GetInput(
            connectionName).GetTypeAsSdfType()[0]

        inputPort = shader.CreateInput(connectionName, portConnectionSdfType)

        # need to specify the output type of the source, or it inherits the input type
        inputPort.ConnectToSource(
            inputShader, str(inputShaderPortName),
            UsdShade.AttributeType.Output, sourceSdfType)

def OverWriteMaterialInterfaces(stage, parametersAttr, materialPath, material,
                                parentMaterial):
    # Not techincally the parent in USD, but it is the parent from Katana.
    parentInputs = parentMaterial.GetInputs()
    for parameterIndex in xrange(parametersAttr.getNumberOfChildren()):
        parameterName = parametersAttr.getChildName(parameterIndex)
        parameterAttr = parametersAttr.getChildByIndex(parameterIndex)
        #Because the parameter interface may have a group namespace, but
        # the parameterName does not contain this, we need to try and find
        # the related input.
        matchingParamFullName = None
        paramSdfType = None
        for input in parentInputs:
            inputName = input.GetFullName()
            inputParamName = inputName.split(":")[-1]
            if inputParamName == parameterName:
                matchingParamFullName = inputName
                paramSdfType = input.GetTypeName()
        if matchingParamFullName and paramSdfType:
            addParameterToShader(parameterName, parameterAttr, material,
                                sdfType=paramSdfType,
                                paramName=matchingParamFullName)


def WriteMaterialInterfaces(stage, parametersAttr, interfacesAttr, material):
    """
    Add the parameter interface attributes onto the material, and add
    connections from the shader to these values. Called by L{WriteMaterial}.

    @param stage: The Usd Stage to write the new prims and parameters to
    @param parametersAttr: The attribute from materials.parameters. If no
        parametersAttr is provided, this is assumed to then be the default
        value.
    @param interfacesAttr: The attribute from materials.interface
    @param material: The material prim to add the parameter interface to.
    @type stage: C{Usd.Stage}
    @type parametersAttr: C{FnAttribute.GroupAttribute}
    @type interfacesAttr: C{FnAttribute.GroupAttribute}
    @type material: C{UsdShade.Material}
    """
    for interfaceIndex in xrange(interfacesAttr.getNumberOfChildren()):
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
        #Save the parameter name so we can find it in the parameters attribute.
        parameterName = interfaceName
        if groupName:
            interfaceName = groupName + ":" + interfaceName

        materialPort = None
        if parametersAttr:
            paramAttr = parametersAttr.getChildByName(parameterName)
            if paramAttr:
                materialPort = addParameterToShader(
                    sourceParamName, paramAttr, material, shaderId,
                    paramName=interfaceName)

        if not materialPort:
            sdfType = GetShaderAttrSdfType(shaderId, sourceParamName,
                                           isOutput=False)
            materialPort = material.CreateInput(interfaceName, sdfType)

            sourceShaderPort = sourceShader.GetInput(sourceParamName)
            if sourceShaderPort:
                value = sourceShaderPort.Get()
                materialPort.Set(value)
            else:
                shaderNode = GetShaderNodeFromRegistry(shaderId)
                shaderNodeInput = shaderNode.GetInput(sourceParamName)
                materialPort.Set(shaderNodeInput.GetDefaultValue())

        sourceSdfType = materialPort.GetTypeName()
        sourceShaderPort = sourceShader.CreateInput(sourceParamName,
                                                    sourceSdfType)
        sourceShaderPort.ConnectToSource(
            material, interfaceName,
            UsdShade.AttributeType.Input, sourceSdfType)


def GetShaderAttrSdfType(shaderType, shaderAttr, isOutput=False):
    """
    Retrieves the SdfType of the attribute of a given Shader.
    This is retrieved from the Usd Shader Registry C{Sdr.Registry}. The shaders
    attempted to be written must be registered to Usd in order to write
    correctly.  If not found in the Shader Registry, try to do our best to
    find the type from the RenderInfoPlugin.

    @type shaderType: C{str}
    @type shaderAttr: C{str}
    @type isOutput: C{bool}
    @rtype: C{Sdf.ValueTypeNames}
    @param shaderType: The name of the shader.
    @param shaderAttr: The name of the attribute on the shader.
    @param isOutput: Whether the attribute is an input or output. C{False}
        by default. Set to True if the attribute is an output.
    @return: The Usd Sdf Type for the attribute of the provided shader.
    """
    shader = GetShaderNodeFromRegistry(shaderType)
    if shader:
        # From the Docs: Two scenarios can result: an exact mapping from property
        # type to Sdf type, and an inexact mapping. In the first scenario, the
        # first element in the pair will be the cleanly-mapped Sdf type, and the
        # second element, a TfToken, will be empty. In the second scenario, the
        # Sdf type will be set to Token to indicate an unclean mapping, and the
        # second element will be set to the original type returned by GetType().
        # From USD code: (So we know what an SdfTypeIndicator is in the future!)
        # typedef std::pair<SdfValueTypeName, TfToken> SdfTypeIndicator;
        if isOutput:
            output = shader.GetOutput(shaderAttr)
            if output:
                return output.GetTypeAsSdfType()[0]
        else:
            input =  shader.GetInput(shaderAttr)
            if input:
                return input.GetTypeAsSdfType()[0]
        return Sdf.ValueTypeNames.Token
    else:
        #Fallback if we cant find the shaders info in the SdrRegistry
        for renderer in RenderingAPI.RenderPlugins.GetRendererPluginNames(True):
            infoPlugin = RenderingAPI.RenderPlugins.GetInfoPlugin(renderer)
            if shaderType in infoPlugin.getRendererObjectNames("shader"):
                if isOutput:
                    if shaderAttr in infoPlugin.getShaderOutputNames(shaderType):
                        tags = infoPlugin.getShaderOutputTags(shaderType,
                                                              shaderAttr)
                    else:
                        continue
                else:
                    if shaderAttr in infoPlugin.getShaderInputNames(shaderType):
                        tags = infoPlugin.getShaderInputTags(shaderType,
                                                             shaderAttr)
                    else:
                        continue
                return convertRenderInfoShaderTagsToSdfType(tags)
    return Sdf.ValueTypeNames.Token


def WriteMaterialAssign(material, overridePrim):
    """ Expects UsdShade.Material and a UsdPrim
     Will need to use USD_KATANA_ALLOW_CUSTOM_MATERIAL_SCOPES envar from
    readPrim.cpp if Katana is not aware of the material scope.
    """
    UsdShade.MaterialBindingAPI(overridePrim.GetPrim()).Bind(material)


def GetShaderNodeFromRegistry(shaderType):
    """
    Method required to get the SdrShadingNode from the SdrRegistry in different
    ways, depending on the usdPlugin.

    @type shaderType: C{str}
    @rtype: C{Sdr.ShaderNode} or C{None}
    @param shaderType: The type of the shader we want to find from the shading
        registry.
    @return: The shader from the registry if found or None.
    """
    sdrRegistry = Sdr.Registry()
    shader = sdrRegistry.GetNodeByName(shaderType)
    if not shader:
        # try arnold, that uses identifiers instead of node names
        shader = sdrRegistry.GetShaderNodeByIdentifier(
            "arnold:{}".format(shaderType))

    return shader
