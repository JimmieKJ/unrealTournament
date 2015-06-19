/************************************************************************************

Filename    :   SwipeHintComponent.h
Content     :
Created     :   Feb 12, 2015
Authors     :   Madhu Kalva, Jim Dose

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_SwipeHintComponent_h )
#define OVR_SwipeHintComponent_h

#include "VRMenuComponent.h"

namespace OVR {

	class VRMenu;

	class OvrSwipeHintComponent : public VRMenuComponent
	{
		class Lerp
		{
		public:
			void	Set( double startDomain_, double startValue_, double endDomain_, double endValue_ )
			{
				startDomain = startDomain_;
				endDomain = endDomain_;
				startValue = startValue_;
				endValue = endValue_;
			}

			double	Value( double domain ) const
			{
				const double f = OVR::Alg::Clamp( ( domain - startDomain ) / ( endDomain - startDomain ), 0.0, 1.0 );
				return startValue * ( 1.0 - f ) + endValue * f;
			}

			double	startDomain;
			double	endDomain;
			double	startValue;
			double	endValue;
		};

	public:
		OvrSwipeHintComponent( const bool isRightSwipe, const float totalTime, const float timeOffset, const float delay );
		static menuHandle_t CreateSwipeSuggestionIndicator( App * appPtr, VRMenu * rootMenu, const menuHandle_t rootHandle, const int menuId,
															const char * img, const Posef pose, const Vector3f direction );

		static const char *			TYPE_NAME;
		static bool					ShowSwipeHints;

		virtual const char *		GetTypeName() const { return TYPE_NAME; }
		void						Reset( VRMenuObject * self );

	private:
		bool 						IsRightSwipe;
		float 						TotalTime;
		float						TimeOffset;
		float 						Delay;
		double 						StartTime;
		bool						ShouldShow;
		bool						IgnoreDelay;
		Lerp						TotalAlpha;

	public:
		void 						Show( const double now );
		void 						Hide( const double now );
		virtual eMsgStatus      	OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
													VRMenuObject * self, VRMenuEvent const & event );
		eMsgStatus              	Opening( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
													VRMenuObject * self, VRMenuEvent const & event );
		eMsgStatus              	Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
													VRMenuObject * self, VRMenuEvent const & event );
	};


}

#endif // OVR_SwipeHintComponent_h
