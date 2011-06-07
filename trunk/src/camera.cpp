//------------------------------------------
//
//   Pretty Basic Camera Implementation
//
//
// out of date code comment:
// A note about the viewport property of the camera.
// To make life convienent for the small app,
// if you dont set the viewport here then this class
// will do the best it can to render set up.
// If you just have one camera that
// you want rendered in the full window - in this case
// your WM_SIZE callback should do the equiv of 
//       glViewport(0,0,LOWORD(lParam), HIWORD(lParam));
// Otherwise you will have to make sure to SetViewPort
// for all active cameras after each resize.
// 


#include <assert.h>

#include "camera.h"
#include "console.h"

LDECLARE(Camera,position)
LDECLARE(Camera,orientation)
LDECLARE(Camera,clipnear)
LDECLARE(Camera,clipfar)
LDECLARE(Camera,camdist)
LDECLARE(Camera,viewangle)

Camera::Camera(char *_name):Entity(_name) 
{
	assert(_name);
	LEXPOSEOBJECT(Camera,"cammy");
	EXPOSEMEMBER(target);
	EXPOSEMEMBER(camdist);
	target="player";
	viewangle = 60.0f;
	clipnear  = 0.1f;
	clipfar   = 512.0f;
	camdist   = 0.0f;
	EXPOSEMEMBER(position);
	EXPOSEMEMBER(orientation);
	EXPOSEMEMBER(viewangle);
	EXPOSEMEMBER(clipnear);
	EXPOSEMEMBER(clipfar);
	viewport[0]=viewport[1]=viewport[2]=viewport[3]=0;
}
Camera::~Camera(){
}

Camera camera; // the global camera!!



//-------------------------------
// Camera::RenderSetUp() - Do the camera tranform!
// Note that we need to get the viewport dimensions in order to set
// the aspect ratio.  
// if the specified camera's viewport width and 
// height are 0 then it means that we
// are just going to use the current glviewport.
// Otherwise, we use the camera's viewport member to set
// the opengl viewport.

void Camera::RenderSetUp(){
	int i;
	float aspect;
	if(viewport[2] || viewport[3]) {
		aspect = (float)viewport[2]/viewport[3];
	}
	else {
		// We're just going to use the window 
		extern int Width,Height;
		aspect = (float)Width / (float)Height;
	}
    assert(viewangle >0.0f);
	project = MatrixPerspectiveFov(DegToRad(viewangle), aspect, clipnear , clipfar);
	viewinv = MatrixFromQuatVec(this->orientation,this->position);
	view = MatrixRigidInverse(viewinv);
	viewproject = view * project;

	viewvolume[0] = Plane(float3(0,0, 1), clipnear);
	viewvolume[1] = Plane(float3(0,0,-1),-clipfar );
	float pxl = -tanf(DegToRad(viewangle/2.0f))*aspect;
	float pxr =  tanf(DegToRad(viewangle/2.0f))*aspect;
	float pyb = -tanf(DegToRad(viewangle/2.0f));
	float pyt =  tanf(DegToRad(viewangle/2.0f));
	viewvolume[2] = Plane(TriNormal(float3(0,0,0),float3(pxl,pyt,-1),float3(pxl,pyb,-1)),0);//left
	viewvolume[3] = Plane(TriNormal(float3(0,0,0),float3(pxr,pyb,-1),float3(pxr,pyt,-1)),0);//right
	viewvolume[4] = Plane(TriNormal(float3(0,0,0),float3(pxr,pyt,-1),float3(pxl,pyt,-1)),0);//bottom
	viewvolume[5] = Plane(TriNormal(float3(0,0,0),float3(pxl,pyb,-1),float3(pxr,pyb,-1)),0);//top
	for(i=0;i<6;i++) {
		viewvolume[i].normal() = rotate(orientation,viewvolume[i].normal()); 	// transform the view volume
		viewvolume[i].dist() += -dot(viewvolume[i].normal(),position);
	}
}

int Camera::SphereVisible(float3 c,float r)
{
	for(int i=0;i<6;i++)
	{
		Plane &p =viewvolume[i];
		if(dot(p.normal(),c)+p.dist() > r) return 0;
	}
	return 1;
}

float3 Camera::MouseDir(int x,int y){
	// Converts x and y in screen coords releative to opengl window
	// To a vector pointing into the world
    assert(viewangle >0.0f);     // Note this must be Perspective
    int vp[4];
	int i;
	for(i=0;i<4;i++) {
		vp[i] = viewport[i];
	}
	if(vp[2] ==0 || vp[3] ==0) {
		extern int Width,Height;
		vp[2] = Width;
		vp[3] = Height;
	}
	assert(vp[2] > 0 && vp[3]>0);  // Hey you forgot to set your camera's viewport!
	x-=vp[0];
	y-=vp[1];
	// cull x,y with respect to the viewport
    float  spread = tanf(DegToRad(viewangle/2));  
    float3 mdir(spread * (x-vp[2]/2.0f)  /(vp[3]/2.0f),
                spread * (y-vp[3]/2.0f) /(vp[3]/2.0f),
		        -1.0f); 
    mdir=rotate(orientation, mdir);
    mdir = normalize(mdir);
    return mdir;
}


int Camera::ViewVolSegment(int x,int y,float3 *v0,float3 *v1)
{
	assert(v0);assert(v1);
    assert(viewangle >0.0f); // Perspective
	float3 mdir = MouseDir(x,y);
	*v0=position+mdir*clipnear;
	*v1=position+mdir*clipfar;
	return 1;
}


//-----------------------------------
// Camera::MapToScreen()
// Convert a point in world coordinates to where
// it would appear on the screen in pixel coordinates.
float3 Camera::MapToScreen(float3 v){
    int vp[4];
	int i;
	for(i=0;i<4;i++) {
		vp[i] = viewport[i];
	}
	if(vp[2] ==0 || vp[3] ==0) {
		extern int Width,Height;
		vp[2] = Width;
		vp[3] = Height;
	}
	assert(vp[2] > 0 && vp[3]>0);  // Hey you forgot to set your camera's viewport!
	// cull x,y with respect to the viewport
	// MouseY=Height-winy;  // I prefer opengl's standard of origin in bootom left

    assert(viewangle >0.0f); // Perspective
	v = v - position;
	v = rotate(Inverse(orientation), v);
	float3 s = PlaneLineIntersection(Plane(float3(0,0,1),1),float3(0,0,0),v);
	float  spread = tanf(DegToRad(viewangle/2.0f));   
	float  x = s.x*(vp[3]/2.0f)/spread + vp[2]/2.0f + vp[0];
	float  y = s.y*(vp[3]/2.0f)/spread + vp[3]/2.0f + vp[1];
	return float3(x,y,0);
}



float movecam;
float movecamspeed=10.0f;
int   camerahitcheck=1;
EXPORTVAR(movecam);
EXPORTVAR(movecamspeed);
EXPORTVAR(camerahitcheck);

extern float DeltaT;//fixme


extern float3 &PlayerPosition();
extern Quaternion &PlayerOrientation();
extern float& PlayerHeadtilt();
extern float& PlayerHeight();

void camnav(Camera *camera)
{

	float &headtilt = PlayerHeadtilt();
	float &targheight = PlayerHeight();
	float &camdist  = camera->camdist;
	float radius = camera->clipnear;

	camdist += movecam*movecamspeed*DeltaT;
	camdist  = Min(15.0f,camdist);
	camdist  = Max(0.0f,camdist);

	camera->orientation = YawPitchRoll(Yaw(PlayerOrientation()),90+headtilt,0);
	float3 cbase = PlayerPosition()+float3(0,0,targheight-radius);
	float3 ctarg = cbase + camera->orientation.zdir()*camdist;
	if(camerahitcheck) {
//		HitCheckSphere(radius,area_bsp,0,cbase,ctarg,&ctarg,float3(0,0,0));
	}
	camera->position = ctarg;

	movecam=0;
}
