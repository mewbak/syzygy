# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'detectors_lib',
      'type': 'static_library',
      'dependencies': [
        '<(src)/syzygy/refinery/types/types.gyp:types_lib',
      ],
      'sources': [
        'lfh_entry_detector.cc',
        'lfh_entry_detector.h',
      ],
    },
    {
      'target_name': 'detectors_unittest_utils',
      'type': 'static_library',
      'dependencies': [
        '<(src)/testing/gtest.gyp:gtest',
        '<(src)/syzygy/refinery/types/types.gyp:types_lib',
      ],
      'sources': [
        'unittest_util.cc',
        'unittest_util.h',
      ],
    },
  ],
}
