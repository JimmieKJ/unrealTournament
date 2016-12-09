/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CoreMinimal.h"
#include "IGoogleVRControllerPlugin.h"

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS

THIRD_PARTY_INCLUDES_START

#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
#include "gvr_controller.h"
#endif

#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
#include "gvr_controller_emulator.h"
#endif

THIRD_PARTY_INCLUDES_END

#endif // GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS