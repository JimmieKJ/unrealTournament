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

#pragma once

#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

namespace GoogleCardboardViewerPreviews
{

// Note: These meshes are generated against 40x40-point grids for each eye.

namespace GoogleCardboard1
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace GoogleCardboard2
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace ViewMaster
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace SnailVR
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace RiTech2
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

namespace Go4DC1Glass
{
	extern const float InterpupillaryDistance;

	extern const FMatrix LeftStereoProjectionMatrix;
	extern const FMatrix RightStereoProjectionMatrix;

	extern const unsigned int NumLeftVertices;
	extern const FDistortionVertex* LeftVertices;

	extern const unsigned int NumRightVertices;
	extern const FDistortionVertex* RightVertices;
}

}

#endif // !GOOGLEVRHMD_SUPPORTED_PLATFORMS
