//
//    Standard Camera Functionality
//
// The camera is assumed to use a regular
// perspective projection.  
//

#ifndef SMCAMERA_H
#define SMCAMERA_H

#include "vecmath.h"
#include "object.h"
#include "bsp.h"

class Camera : public Object
{
  public:
						Camera(char *_name="camera");
						~Camera();
	void 				RenderSetUp(); // set the opengl projection and modelview matrices
	float3				MapToScreen(float3 v);
	float3				MouseDir(int x,int y); // x and y in screen coords releative to opengl window
	int					ViewVolSegment(int x,int y,float3 *v0,float3 *v1); 
	int					SphereVisible(float3 sphere_position,float sphere_radius);
	float3				position;
	Quaternion			orientation;
	float				viewangle; // for perspective projection
	float				clipnear;
	float				clipfar;
	int					viewport[4]; // left,bottom,width,height;
	Plane				viewvolume[6];
	float4x4			viewinv,view,project,viewproject;
	String				target;
	float				camdist;
};


#endif
