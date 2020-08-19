# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

from Katana import RenderingAPI, FnAttribute
import PyFnAttribute
import logging

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from fnpxr import Usd, UsdShade, Sdf, Gf, Ndr, Sdr
    # These includes also require fnpxr
    from .typeConversionMaps import (myTypeMap, valueTypeCastMethods,
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
        oldest parent, to youngest. We use this to read the attribute types
        from the oldest parent, but specialise/intherit from the youngest
        parent.
    @type stage: C{Usd.Stage}
    @type materialSdfPath: C{Sdf.Path}
    @type materialAttribute: C{FnAttribute.GroupAttribute}
    @type parentMaterialSdfPaths: C{list}
    """
    if not materialAttribute:
        return
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
    oldestParentSdfPath = parentMaterialSdfPaths[0]
    youngestParentSdfPath = parentMaterialSdfPaths[-1]
    oldestParentMaterial = UsdShade.Material(
            stage.GetPrimAtPath(oldestParentSdfPath))
    youngestParentMaterial = UsdShade.Material(
        stage.GetPrimAtPath(youngestParentSdfPath))
    if oldestParentMaterial and youngestParentMaterial:
        material.SetBaseMaterial(youngestParentMaterial)
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

    parameters = materialAttribute.getChildByName("parameters")
    interfaces = materialAttribute.getChildByName("interface")
    if parameters and interfaces:
        WriteMaterialInterfaces(stage, parameters, interfaces, materialPath,
                                material)

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
        else:
            terminalName = terminalName[3:].lower()
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
    if not shaderId:
        log.warning("Unable to find shaderId, and sdfType is not provided for"
        " shaderParamName:{}".format(
            shaderParamName))
    if sdfType is None:
        sdfType = GetShaderAttrSdfType(shaderId, shaderParamName,
                                       isOutput=False)
    if shaderParamName == "varname":
        # used in properties such as the texcoordreader inputs,
        # but the sdftype of 'string' does not work, so force to a token
        sdfType = Sdf.ValueTypeNames.Token
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
    reg = Sdr.Registry()
    shaderNode = reg.GetNodeByName(shader.GetShaderId())
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
        if parameterIndex in foundParameterIndices:
            continue
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


def WriteMaterialInterfaces(stage, parametersAttr, interfacesAttr,
                            materialPath, material):
    """
        Search through the parameter interface attributes and add these as
        parameters onto the material, and add connections from the shader to
        these values.

        @param stage: The Usd Stage to write the new prims and parameters to
        @param parametersAttr: The attribute from materials.parameters
        @param interfacesAttr: The attribute from materials.interface
        @param materialPath: The path to the material Prim in the UsdStage
        @param material: The material prim to add the parameter interface to.
        @type stage: C{Usd.Stage}
        @type parametersAttr: C{FnAttribute.GroupAttribute}
        @type interfacesAttr: C{FnAttribute.GroupAttribute}
        @type materialPath: C{SdfPath}
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
        sourceValueSplit = str(sourceAttr.getValue()).split(".")
        sourceShaderName = sourceValueSplit[0]
        sourceParam = sourceValueSplit[1]

        paramAttr = parametersAttr.getChildByName(interfaceName)
        sourceShaderPath = materialPath.AppendChild(sourceShaderName)
        sourceShader = UsdShade.Shader.Get(stage, sourceShaderPath)
        shaderId = sourceShader.GetShaderId()
        if groupName:
            interfaceName = groupName + ":" + interfaceName

        materialPort = addParameterToShader(
            sourceParam, paramAttr, material, shaderId,
            paramName=interfaceName)

        sourceSdfType = materialPort.GetTypeName()
        sourceShaderPort = sourceShader.CreateInput(sourceParam, sourceSdfType)
        sourceShaderPort.ConnectToSource(
            material, interfaceName,
            UsdShade.AttributeType.Input, sourceSdfType)


def WriteMaterialOverride(stage, sdfLocationPath, overridePrim,
                          sharedOverridesKey, attribute):
    """ Write out the material overrides.  If the material hasnt been created
    (It is not part of the selectedMaterialTreeRootLocations on the
    LookFileBake Node), then we need to create the material.  This is only for
    resolved materialAssigns, where the material data gets written to the node
    itself.
    """
    # TODO: Investigate whether this is fully required, we may not support
    # resolved materials from the first release.  If we do, we may want to look
    # into further de-duplication, check the materialOverrides attribute, and the
    # originally assigned material, and write that out instead, and apply overrides
    # where necessary, rather than writing out the entire material multiple times
    # for each different set of overrides.
    materialOverridePath = sdfLocationPath.AppendChild("material")
    materialOverride = \
        UsdShade.Material.Define(stage, materialOverridePath)
    stage.OverridePrim("/resolveMaterials")
    materialSdfPath = "/resolveMaterials/material"+sharedOverridesKey
    material = UsdShade.Material.Get(stage, materialSdfPath)
    if not material:
        material = WriteMaterial(stage, materialSdfPath,
                                 attribute)
    references = materialOverride.GetPrim().GetReferences()
    references.AddInternalReference(materialSdfPath)
    return materialOverride


def GetShaderAttrSdfType(shaderType, shaderAttr, isOutput=False):
    reg = Sdr.Registry()
    shader = reg.GetNodeByName(shaderType)
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
            return shader.GetOutput(shaderAttr).GetTypeAsSdfType()[0]
        else:
            return shader.GetInput(shaderAttr).GetTypeAsSdfType()[0]
    else:
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


def addMaterialAssignment(sharedOverrides, materialOverridePrim):
    # If info is in the sharedOverrides, we know its already been resolved.
    if "info" in sharedOverrides.keys():
        pass
    else:
        pass