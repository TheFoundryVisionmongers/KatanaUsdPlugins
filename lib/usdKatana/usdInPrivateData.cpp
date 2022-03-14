// These files began life as part of the main USD distribution
// https://github.com/PixarAnimationStudios/USD.
// In 2019, Foundry and Pixar agreed Foundry should maintain and curate
// these plug-ins, and they moved to
// https://github.com/TheFoundryVisionmongers/KatanaUsdPlugins
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
#include "pxr/pxr.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

#include "pxr/base/gf/interval.h"
#include "pxr/usd/usdGeom/xform.h"

#include <pystring/pystring.h>
#include <FnGeolib/util/Path.h>

namespace
{
// Returns the time sample above and below the provided time. If the time
// equals one of the time samples, it will return the time and the higher value.
bool GetUpperBoundedClosestTimes(const std::vector<double>& timeSamples,
                                 double time,
                                 double* tLower,
                                 double* tUpper)
{
    // A time sample time will exist at each discrete integer frame for the
    // duration of the generated animation and will already be cached.
    if (timeSamples.empty())
    {
        return false;
    }

    auto const it = std::upper_bound(timeSamples.begin(), timeSamples.end(), time);
    if (it == timeSamples.end())
    {
        *tLower = *tUpper = timeSamples.back();
        return true;
    }
    else if (it == timeSamples.begin())
    {
        *tLower = *tUpper = timeSamples.front();
        return true;
    }
    else
    {
        size_t endFrameIdx = it - timeSamples.begin();
        *tUpper = *it;
        *tLower = timeSamples[endFrameIdx - 1];
    }
    return true;
}
}  // namespace

PXR_NAMESPACE_OPEN_SCOPE


PxrUsdKatanaUsdInPrivateData::PxrUsdKatanaUsdInPrivateData(
        const UsdPrim& prim,
        PxrUsdKatanaUsdInArgsRefPtr usdInArgs,
        const PxrUsdKatanaUsdInPrivateData* parentData)
    : _prim(prim), _usdInArgs(usdInArgs), _extGb(0)
{
    // None of the below is safe or relevant if the prim is not valid
    // This is most commonly due to an invalid isolatePath -- which is 
    // already reported as a katana error from pxrUsdIn.cpp
    if (!prim)
    {
        return;
    }
    
    _outputTargets = _usdInArgs->GetOutputTargets();

    // XXX: manually track instance and master path for possible
    //      relationship re-retargeting. This approach does not yet
    //      support nested instances -- which is expected to be handled
    //      via the forthcoming GetMasterWithContext.
    //
    if (prim.IsInstance())
    {
        if (prim.IsInMaster() && parentData &&
            parentData->GetInstancePath() != SdfPath::EmptyPath())
        {
            SdfPath descendentPrimPath =
                prim.GetPath().ReplacePrefix(prim.GetPath().GetPrefixes()[0],
                                             SdfPath::ReflexiveRelativePath());

            _instancePath =
                parentData->GetInstancePath().AppendPath(descendentPrimPath);
        }
        else
        {
            _instancePath = prim.GetPath();
        }

        const UsdPrim& masterPrim = prim.GetMaster();            
        if (masterPrim)
        {
            _masterPath = masterPrim.GetPath();
        }
    }
    else if (parentData)
    {
        // Pass along instance and master paths to children.
        //
        if (!parentData->GetInstancePath().IsEmpty())
        {
            _instancePath = parentData->GetInstancePath();
        }

        if (!parentData->GetMasterPath().IsEmpty())
        {
            _masterPath = parentData->GetMasterPath();
        }
    }

    //
    // Apply session overrides for motion.
    //

    const std::string primPath = prim.GetPrimPath().GetString();
    const std::string isolatePath = usdInArgs->GetIsolatePath();
    const std::string sessionPath = usdInArgs->GetSessionLocationPath();
    FnKat::GroupAttribute sessionAttr = usdInArgs->GetSessionAttr();

    // XXX: If an isolatePath has been specified, it means the PxrUsdIn is
    // probably loading USD contents below the USD root. This can prevent
    // overrides from trickling down the hierarchy, e.g. the overrides for /A/B
    // won't get applied to children if the isolatePath is /A/B/C/D.
    //
    // So, if the usdInArgs suggest that an isolatePath has been specified and
    // we don't have any parentData, we'll need to check if there are overrides
    // for the prim and any of its parents.
    //
    std::vector<std::string> pathsToCheck;
    if (!parentData and !isolatePath.empty() and
        pystring::startswith(primPath, isolatePath+"/"))
    {
        std::vector<std::string> parentLocs;
        Foundry::Katana::Util::Path::GetLocationStack(parentLocs, primPath);
        std::reverse(std::begin(parentLocs), std::end(parentLocs));
        for (size_t i = 0; i < parentLocs.size(); ++i)
        {
            pathsToCheck.push_back(FnKat::DelimiterEncode(
                    sessionPath + parentLocs[i]));
        }
    }
    else
    {
        pathsToCheck.push_back(FnKat::DelimiterEncode(sessionPath + primPath));
    }

    //
    // If a session override is specified, use its value. If no override exists,
    // try asking the parent data for its value. Otherwise, fall back on the
    // usdInArgs value.
    //

    bool overrideFound;

    const double startTime = usdInArgs->GetStage()->GetStartTimeCode();
    const double tcps = usdInArgs->GetStage()->GetTimeCodesPerSecond();
    const double fps = usdInArgs->GetStage()->GetFramesPerSecond();
    const double timeScaleRatio = tcps / fps;

    // Current time.
    //
    overrideFound = false;
    for (size_t i = 0; i < pathsToCheck.size(); ++i)
    {
        FnKat::FloatAttribute currentTimeAttr =
            sessionAttr.getChildByName(
                "overrides."+pathsToCheck[i]+".currentTime");
        if (currentTimeAttr.isValid())
        {
            _currentTime = currentTimeAttr.getValue();
            overrideFound = true;
            break;
        }
    }
    if (!overrideFound)
    {
        if (parentData)
        {
            _currentTime = parentData->GetCurrentTime();
        }
        else
        {
            _currentTime = usdInArgs->GetCurrentTime();

            // Apply time scaling.
            //
            _currentTime =
                startTime + ((_currentTime - startTime) * timeScaleRatio);
        }
    }

    // Shutter open.
    //
    overrideFound = false;
    for (size_t i = 0; i < pathsToCheck.size(); ++i)
    {
        FnKat::FloatAttribute shutterOpenAttr =
                sessionAttr.getChildByName(
                    "overrides."+pathsToCheck[i]+".shutterOpen");
        if (shutterOpenAttr.isValid())
        {
            _shutterOpen = shutterOpenAttr.getValue();
            overrideFound = true;
            break;
        }
    }
    if (!overrideFound)
    {
        if (parentData)
        {
            _shutterOpen = parentData->GetShutterOpen();
        }
        else
        {
            _shutterOpen = usdInArgs->GetShutterOpen();
        }
    }

    // Shutter close.
    //
    overrideFound = false;
    for (size_t i = 0; i < pathsToCheck.size(); ++i)
    {
        FnKat::FloatAttribute shutterCloseAttr =
                sessionAttr.getChildByName(
                    "overrides."+pathsToCheck[i]+".shutterClose");
        if (shutterCloseAttr.isValid())
        {
            _shutterClose = shutterCloseAttr.getValue();
            overrideFound = true;
            break;
        }
    }
    if (!overrideFound)
    {
        if (parentData)
        {
            _shutterClose = parentData->GetShutterClose();
        }
        else
        {
            _shutterClose = usdInArgs->GetShutterClose();

            // Apply time scaling.
            //
            _shutterClose =
                _shutterOpen + ((_shutterClose - _shutterOpen) * timeScaleRatio);
        }
    }

    // Motion sample times.
    //
    // Fallback logic is a little more complicated for motion sample times, as
    // they can vary per attribute, so store both the overridden and the
    // fallback motion sample times for use inside GetMotionSampleTimes.
    //
    bool useDefaultMotionSamples = false;
    if (!prim.IsPseudoRoot())
    {
        TfToken useDefaultMotionSamplesToken("katana:useDefaultMotionSamples");
        UsdAttribute useDefaultMotionSamplesUsdAttr = 
            prim.GetAttribute(useDefaultMotionSamplesToken);
        if (useDefaultMotionSamplesUsdAttr)
        {
            // If there is no katana op override and there is a usd 
            // attribute "katana:useDefaultMotionSamples" set to true,
            // interpret this as "use usdInArgs defaults".
            //
            useDefaultMotionSamplesUsdAttr.Get(&useDefaultMotionSamples);
            if (useDefaultMotionSamples)
            {
                _motionSampleTimesOverride = usdInArgs->GetMotionSampleTimes();
            }
        }
    }

    for (size_t i = 0; i < pathsToCheck.size(); ++i)
    {
        FnKat::Attribute motionSampleTimesAttr =
                sessionAttr.getChildByName(
                    "overrides."+pathsToCheck[i]+".motionSampleTimes");
        if (motionSampleTimesAttr.isValid())
        {
            // Interpret an IntAttribute as "use usdInArgs defaults"
            //
            if (motionSampleTimesAttr.getType() == kFnKatAttributeTypeInt)
            {
                _motionSampleTimesOverride = usdInArgs->GetMotionSampleTimes();
                break;
            }
            // Interpret a FloatAttribute as an explicit value override
            //
            if (motionSampleTimesAttr.getType() == kFnKatAttributeTypeFloat)
            {
                FnAttribute::FloatAttribute attr(motionSampleTimesAttr);
                const auto& sampleTimes = attr.getNearestSample(0.0f);;
                if (!sampleTimes.empty())
                {
                    if (useDefaultMotionSamples)
                    {
                        // Clear out default samples before adding overrides
                        _motionSampleTimesOverride.clear();
                    }
                    for (float sampleTime : sampleTimes)
                        _motionSampleTimesOverride.push_back(
                            (double)sampleTime);
                    break;
                }
            }
        }
        else if (parentData && !useDefaultMotionSamples)
        {
            _motionSampleTimesOverride =
                    parentData->_motionSampleTimesOverride;
        }
    }
    if (parentData)
    {
        _motionSampleTimesFallback = parentData->GetMotionSampleTimes();
    }
    else
    {
        _motionSampleTimesFallback = usdInArgs->GetMotionSampleTimes();

        // Apply time scaling.
        //
        if (_motionSampleTimesFallback.size() > 0)
        {
            const double firstSample = _motionSampleTimesFallback[0];
            for (size_t i = 0; i < _motionSampleTimesFallback.size(); ++i)
            {
                _motionSampleTimesFallback[i] =
                    firstSample + ((_motionSampleTimesFallback[i] - firstSample) *
                                   timeScaleRatio);
            }
        }
    }


    if (parentData)
    {
        _collectionQueryCache = parentData->_collectionQueryCache;
        _bindingsCache = parentData->_bindingsCache;
        
    }

    if (!_collectionQueryCache)
    {
        _collectionQueryCache.reset(
                new UsdShadeMaterialBindingAPI::CollectionQueryCache);
    }

    if (!_bindingsCache)
    {
        _bindingsCache.reset(
            new UsdShadeMaterialBindingAPI::BindingsCache);
    }

    _evaluateUsdSkelBindings = _usdInArgs->GetEvaluateUsdSkelBindings();
}

const bool
PxrUsdKatanaUsdInPrivateData::IsMotionBackward() const
{
    if (_motionSampleTimesOverride.size() > 0)
    {
        return (_motionSampleTimesOverride.size() > 1 &&
            _motionSampleTimesOverride.front() >
            _motionSampleTimesOverride.back());
    }
    else
    {
        return (_motionSampleTimesFallback.size() > 1 &&
            _motionSampleTimesFallback.front() >
            _motionSampleTimesFallback.back());
    }
}

std::vector<PxrUsdKatanaUsdInPrivateData::UsdKatanaTimePair>
PxrUsdKatanaUsdInPrivateData::GetUsdAndKatanaTimes(
        const UsdAttribute& attr) const
{
    const std::vector<double> motionSampleTimes = GetMotionSampleTimes(attr);
    std::vector<UsdKatanaTimePair> result(motionSampleTimes.size());
    const bool isMotionBackward = IsMotionBackward();
    for (size_t i = 0; i < motionSampleTimes.size(); ++i) {
        double t = motionSampleTimes[i];

        UsdKatanaTimePair& pair = result[i];
        pair.usdTime = _currentTime + t;
        pair.katanaTime = isMotionBackward ?
            PxrUsdKatanaUtils::ReverseTimeSample(t) : t;
    } 
    return result;
}

const std::vector<double> PxrUsdKatanaUsdInPrivateData::GetSkelMotionSampleTimes(
    const UsdSkelAnimQuery& skelAnimQuery,
    std::vector<double>& blendShapeMotionSampleTimes,
    std::vector<double>& jointTransformMotionSampleTimes) const
{
    static std::vector<double> noMotion = {0.0};
    // If the UsdIn node does not explicitly set a fallback motion sample setting,
    // return no motion, since it is not requested.
    if (_motionSampleTimesFallback.size() < 2)
    {
        return noMotion;
    }
    // If an override was explicitly specified for this prim, return it.
    if (_motionSampleTimesOverride.size() > 0)
    {
        return _motionSampleTimesOverride;
    }
    // Early exit if we don't have a valid UsdSkel Animation Query.
    if (!skelAnimQuery)
    {
        return _motionSampleTimesFallback;
    }
    // Store whether the joint of blend samples are actually animated.
    bool hasJointTransformSamples = skelAnimQuery.JointTransformsMightBeTimeVarying();
    bool hasBlendShapeSamples = skelAnimQuery.BlendShapeWeightsMightBeTimeVarying();
    if (!hasJointTransformSamples && !hasBlendShapeSamples)
    {
        return noMotion;
    }

    // Allowable error in sample time comparison.
    static const double epsilon = 0.0001;

    double shutterStartTime, shutterCloseTime;
    // Calculate shutter start and close times based on
    // the direction of motion blur.
    if (IsMotionBackward())
    {
        shutterStartTime = _currentTime - _shutterClose;
        shutterCloseTime = _currentTime - _shutterOpen;
    }
    else
    {
        shutterStartTime = _currentTime + _shutterOpen;
        shutterCloseTime = _currentTime + _shutterClose;
    }

    // get the time samples for our frame interval
    if (!skelAnimQuery.GetBlendShapeWeightTimeSamplesInInterval(
            GfInterval(shutterStartTime, shutterCloseTime), &blendShapeMotionSampleTimes))
    {
        blendShapeMotionSampleTimes.insert(blendShapeMotionSampleTimes.begin(),
                                           _motionSampleTimesFallback.begin(),
                                           _motionSampleTimesFallback.end());
    }
    if (!skelAnimQuery.GetJointTransformTimeSamplesInInterval(
            GfInterval(shutterStartTime, shutterCloseTime), &jointTransformMotionSampleTimes))
    {
        jointTransformMotionSampleTimes.insert(jointTransformMotionSampleTimes.begin(),
                                               _motionSampleTimesFallback.begin(),
                                               _motionSampleTimesFallback.end());
    }
    std::vector<double> blendShapeTimes, jointTransformTimes;
    skelAnimQuery.GetBlendShapeWeightTimeSamples(&blendShapeTimes);
    skelAnimQuery.GetJointTransformTimeSamples(&jointTransformTimes);

    // We have a single mid sample, but we know there are more samples.
    // We should find the next and previous samples to add.
    if (blendShapeMotionSampleTimes.size() == 1 && blendShapeTimes.size() > 1)
    {
        double lower, upper;
        if (GetUpperBoundedClosestTimes(blendShapeTimes, shutterStartTime, &lower, &upper))
        {
            if (fabs(lower - blendShapeMotionSampleTimes.front()) > epsilon)
            {
                blendShapeMotionSampleTimes.push_back(lower);
            }
        }
        if (GetUpperBoundedClosestTimes(blendShapeTimes, shutterCloseTime, &lower, &upper))
        {
            if (fabs(upper - blendShapeMotionSampleTimes.back()) > epsilon)
            {
                blendShapeMotionSampleTimes.push_back(upper);
            }
        }
    }
    if (jointTransformMotionSampleTimes.size() == 1 && jointTransformTimes.size() > 1)
    {
        double lower, upper;
        if (GetUpperBoundedClosestTimes(jointTransformTimes, shutterStartTime, &lower, &upper))
        {
            if (fabs(lower - jointTransformMotionSampleTimes.front()) > epsilon)
            {
                jointTransformMotionSampleTimes.push_back(lower);
            }
        }
        if (GetUpperBoundedClosestTimes(jointTransformTimes, shutterCloseTime, &lower, &upper))
        {
            if (fabs(upper - jointTransformMotionSampleTimes.back()) > epsilon)
            {
                jointTransformMotionSampleTimes.push_back(upper);
            }
        }
    }

    // There may be differing motion samples for blendshapes and joint transforms.
    // Although unlikely this will cause problems when generating the points time samples,
    // since one sample may have joint transforms applied, the next would have only blend
    // shapes applied, making for some bad motion blur.
    // We therefore only pick up values which match in both, or if one has time samples
    // and not the other, then use the samples from the animated values.
    std::vector<double> result;
    const auto& blendShapeBeginIt = blendShapeMotionSampleTimes.begin();
    const auto& blendShapeEndIt = blendShapeMotionSampleTimes.end();
    const auto& jointBeginIt = jointTransformMotionSampleTimes.begin();
    const auto& jointEndIt = jointTransformMotionSampleTimes.end();
    if (!hasJointTransformSamples && hasBlendShapeSamples)
    {
        result.insert(result.begin(), blendShapeBeginIt, blendShapeEndIt);
    }
    else if (!hasBlendShapeSamples && hasJointTransformSamples)
    {
        result.insert(result.begin(), jointBeginIt, jointEndIt);
    }
    // If they both have samples, only add samples in both sample sets.
    else if (hasBlendShapeSamples && hasJointTransformSamples)
    {
        for (auto t : jointTransformMotionSampleTimes)
        {
            if (std::count(blendShapeBeginIt, blendShapeEndIt, t))
            {
                result.push_back(t);
            }
        }
    }
    // Always take the currentTime sample if none are provided.
    if (result.empty())
    {
        return noMotion;
    }
    // convert from absolute to frame-relative time samples
    for (std::vector<double>::iterator I = result.begin(); I != result.end(); ++I)
    {
        (*I) -= _currentTime;
    }

    return result;
}

const std::vector<double>
PxrUsdKatanaUsdInPrivateData::GetMotionSampleTimes(
    const UsdAttribute& attr,
    bool fallBackToShutterBoundary) const
{
    static std::vector<double> noMotion = {0.0};

    if ((attr && !PxrUsdKatanaUtils::IsAttributeVarying(attr, _currentTime)) ||
            _motionSampleTimesFallback.size() < 2)
    {
        return noMotion;
    }

    // If an override was explicitly specified for this prim, return it.
    //
    if (_motionSampleTimesOverride.size() > 0)
    {
        return _motionSampleTimesOverride;
    }

    //
    // Otherwise, try computing motion sample times. If they can't be computed,
    // fall back on the parent data's times.
    //

    // Early exit if we don't have a valid attribute.
    //
    if (!attr)
    {
        return _motionSampleTimesFallback;
    }

    // Allowable error in sample time comparison.
    static const double epsilon = 0.0001;

    double shutterStartTime, shutterCloseTime;

    // Calculate shutter start and close times based on
    // the direction of motion blur.
    if (IsMotionBackward())
    {
        shutterStartTime = _currentTime - _shutterClose;
        shutterCloseTime = _currentTime - _shutterOpen;
    }
    else
    {
        shutterStartTime = _currentTime + _shutterOpen;
        shutterCloseTime = _currentTime + _shutterClose;
    }

    // get the time samples for our frame interval
    std::vector<double> result;
    if (!attr.GetTimeSamplesInInterval(
            GfInterval(shutterStartTime, shutterCloseTime), &result))
    {
        return _motionSampleTimesFallback;
    }

    bool foundSamplesInInterval = !result.empty();

    double firstSample, lastSample;

    if (foundSamplesInInterval)
    {
        firstSample = result.front();
        lastSample = result.back();
    }
    else
    {
        firstSample = shutterStartTime;
        lastSample = shutterCloseTime;
    }

    // If no samples were found or the first sample is later than the 
    // shutter start time then attempt to get the previous sample in time.
    if (!foundSamplesInInterval || (firstSample-shutterStartTime) > epsilon)
    {
        double lower, upper;
        bool hasTimeSamples;

        if (attr.GetBracketingTimeSamples(
                shutterStartTime, &lower, &upper, &hasTimeSamples))
        {
            if (lower > shutterStartTime)
            {
                // Did not find a sample ealier than the shutter start. 
                if (fallBackToShutterBoundary)
                {
                    lower = shutterStartTime;
                }
                else
                {
                    // Return no motion.
                    return noMotion;
                }
            }

            // Insert the first sample as long as it is different
            // than what we already have.
            if (!foundSamplesInInterval || fabs(lower-firstSample) > epsilon)
            {
                result.insert(result.begin(), lower);
            }
        }
    }

    // If no samples were found or the last sample is earlier than the
    // shutter close time then attempt to get the next sample in time.
    if (!foundSamplesInInterval || (shutterCloseTime-lastSample) > epsilon)
    {
        double lower, upper;
        bool hasTimeSamples;

        if (attr.GetBracketingTimeSamples(
                shutterCloseTime, &lower, &upper, &hasTimeSamples))
        {
            if (upper < shutterCloseTime)
            {
                // Did not find a sample later than the shutter close. 
                if (fallBackToShutterBoundary)
                {
                    upper = shutterCloseTime;
                }
                else
                {
                    // Return no motion.
                    return noMotion;
                }
            }

            // Append the last sample as long as it is different
            // than what we already have.
            if (!foundSamplesInInterval || fabs(upper-lastSample) > epsilon)
            {
                result.push_back(upper);
            }
        }
    }

    // convert from absolute to frame-relative time samples
    for (std::vector<double>::iterator I = result.begin();
            I != result.end(); ++I)
    {
        (*I) -= _currentTime;
    }

    return result;
}

void PxrUsdKatanaUsdInPrivateData::setExtensionOpArg(
        const std::string & name, FnAttribute::Attribute attr) const
{
    if (!_extGb)
    {
        _extGb = new FnAttribute::GroupBuilder;
    }

    _extGb->set("ext." + name, attr);
}


FnAttribute::Attribute PxrUsdKatanaUsdInPrivateData::getExtensionOpArg(
        const std::string & name, FnAttribute::GroupAttribute opArgs) const
{
    if (name.empty())
    {
        return opArgs.getChildByName("ext");
    }
    
    return opArgs.getChildByName("ext." + name);
}


FnAttribute::GroupAttribute
PxrUsdKatanaUsdInPrivateData::updateExtensionOpArgs(
        FnAttribute::GroupAttribute opArgs) const
{
    if (!_extGb)
    {
        return opArgs;
    }
    
    return FnAttribute::GroupBuilder()
        .update(opArgs)
        .deepUpdate(_extGb->build())
        .build();
}


UsdShadeMaterialBindingAPI::CollectionQueryCache *
PxrUsdKatanaUsdInPrivateData::GetCollectionQueryCache() const
{
    return _collectionQueryCache.get();
   
}

UsdShadeMaterialBindingAPI::BindingsCache *
PxrUsdKatanaUsdInPrivateData::GetBindingsCache() const
{
    return _bindingsCache.get();
}


PxrUsdKatanaUsdInPrivateData *
PxrUsdKatanaUsdInPrivateData::GetPrivateData(
        const FnKat::GeolibCookInterface& interface)
{
    return static_cast<PxrUsdKatanaUsdInPrivateData*>(
            interface.getPrivateData());
}

PXR_NAMESPACE_CLOSE_SCOPE

