# These files began life as part of the main USD distribution
# https://github.com/PixarAnimationStudios/USD.
# In 2019, Foundry and Pixar agreed Foundry should maintain and curate
# these plug-ins, and they moved to
# https://github.com/TheFoundryVisionmongers/katana-USD
# under the same Modified Apache 2.0 license, as shown below.
#
# Copyright 2017 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
import unittest
import os

from Katana import NodegraphAPI, CacheManager
from PyUtilModule import AttrDump

class TestPxrOpUsdInInternalPointInstancer(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        '''Procedurally create nodes for testing.'''

        # Add a PxrUsdIn node and set its motion sample times
        pxrUsdInNode = NodegraphAPI.CreateNode(
            'PxrUsdIn', NodegraphAPI.GetRootNode())
        pxrUsdInNode.getParameter('motionSampleTimes').setValue('0 -1', 0)

        # Add a RenderSettings node and turn on motion blur
        renderSettingsNode = NodegraphAPI.CreateNode(
            'RenderSettings', NodegraphAPI.GetRootNode())
        mts = renderSettingsNode.getParameter(
            'args.renderSettings.maxTimeSamples')
        mts.getChild('value').setValue(2, 0)
        mts.getChild('enable').setValue(1, 0)
        sc = renderSettingsNode.getParameter(
            'args.renderSettings.shutterClose')
        sc.getChild('value').setValue(0.5, 0)
        sc.getChild('enable').setValue(1, 0)

        # Connect the two nodes together and view from the RS node
        pxrUsdInNode.getOutputPortByIndex(0).connect(
            renderSettingsNode.getInputPortByIndex(0))
        NodegraphAPI.SetNodeViewed(renderSettingsNode, True, True)

        # Set the current frame
        NodegraphAPI.SetCurrentTime(50)

    def cleanUpTestFile(self, testfile):
        '''Prep the given test file for a baseline compare.'''
        cwd = os.getcwd() + '/'
        with open(testfile, 'r') as f:
            contents = f.read()

        # Remove the current working directory
        contents = contents.replace(cwd, '')

        # Remove irrelevant attrs by ignoring all dump results prior to those
        # for /root/world/geo/Rocks
        locKey = '<location path='
        splitContents = contents.split(locKey)
        newContents = ""
        preserve = False
        for piece in splitContents:
            if piece.startswith('"/root/world/geo/Rocks">'):
                preserve = True
            if preserve:
                newContents += (locKey + piece)

        with open(testfile, 'w') as f:
            f.write(newContents)

    def compareAgainstBaseline(self, testfile):
        '''Compare the given test file to its associated baseline file.'''
        baselinefile = testfile.replace('test.', 'baseline.')
        print 'Comparing %s against baseline %s' % \
                (os.path.abspath(testfile), os.path.abspath(baselinefile))

        # Load files.
        text1 = open(testfile).read()
        text2 = open(baselinefile).read()

        # Split the text into words, so we compare word by word.
        words1 = text1.replace(" ", "\n").split("\n")
        words2 = text2.replace(" ", "\n").split("\n")

        # If different number of words, assume mismatch.
        if len(words1) != len(words2):
            print "Number of lines mismatch ({} != {})".format(
                len(words1), len(words2))
            return False

        # Compare word by word. If word is parseable as a real number,
        # evaluate as numbers with a small epsilon.
        for n in xrange(len(words1)):
            word1 = words1[n]
            word2 = words2[n]

            try:
                if abs(float(word1) - float(word2)) > 0.0001:
                    print "{} and {} are too different".format(
                        word1, word2)
                    return False

            except ValueError:
                if word1 != word2:
                    print "{} != {}".format(word1, word2)
                    return False

        return True

    def test_motion(self):
        '''Change the PxrUsdIn's file and verify the dumped result.'''

        # test.motion.usda is a UsdGeomPointInstancer with positions,
        # orientations, scales, velocities, and angular velocities
        NodegraphAPI.GetNode('PxrUsdIn').getParameter('fileName').setValue(
            'test.motion.usda', 0)
        CacheManager.flush()

        testfile = 'test.motion.xml'
        AttrDump.AttrDump(testfile)
        self.assertTrue(os.path.exists(testfile))

        self.cleanUpTestFile(testfile)
        self.assertTrue(self.compareAgainstBaseline(testfile))

    def test_translateOnly(self):
        '''Change the PxrUsdIn's file and verify the dumped result.'''

        # test.translateOnly.usda is a UsdGeomPointInstancer with positions
        NodegraphAPI.GetNode('PxrUsdIn').getParameter('fileName').setValue(
            'test.translateOnly.usda', 0)
        CacheManager.flush()

        testfile = 'test.translateOnly.xml'
        AttrDump.AttrDump(testfile)
        self.assertTrue(os.path.exists(testfile))

        self.cleanUpTestFile(testfile)
        self.assertTrue(self.compareAgainstBaseline(testfile))

if __name__ == '__main__':
    import sys

    test_suite = unittest.TestSuite()
    test_suite.addTest(unittest.makeSuite(TestPxrOpUsdInInternalPointInstancer))

    runner = unittest.TextTestRunner()
    result = runner.run(test_suite)

    sys.exit(not result.wasSuccessful())
