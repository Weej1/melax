//
// If you are building a new project and are getting link errors...
// Dont forget to add the libraries:  d3d9.lib d3dx9.lib winmm.lib
//
// instructions for Visual Studio version 6:
//   goto project menu, settings (or alt-F7), select the link tab,
//   select category "Input" in the dropdown Category box,
//   and add those guys under object/library modules.
//   You may have to add the path to the directx libs
//   In the "additional library path" textbox, add c:\dxsdk\lib\
//   depending on where you installed the directx sdk.
//


#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <shlwapi.h>  // also add shlwapi.lib
#include <direct.h>  // for chdir
#include <assert.h>
#include <process.h>



#include "winsetup.h"
#include "mesh.h"
#include "hash.h"
#include "d3dfont.h"
#include "console.h"
#include "object.h"
#include "camera.h"
#include "manipulator.h"
#include "sound.h"



extern void Line(const float3 &_v0,const float3 &_v1,const float3 &_c);
extern int MouseLeft;
extern int mousewheel;


String instructions("");
EXPORTVAR(instructions);


EXPORTFUNC(chdir);


extern Camera camera;
float &viewangle = camera.viewangle;


int spinnav;
int flynav;
EXPORTVAR(spinnav);
EXPORTVAR(flynav);
char *flymode(const char*)
{
	return (flynav)?"ON":"";
}
EXPORTFUNC(flymode);
char *spinmode(const char*)
{
	return (spinnav)?"ON":"";
}
EXPORTFUNC(spinmode);



void crosshair()
{
	extern float3 MouseVector;
	float3 cp = camera.position+rotate(camera.orientation,PlaneLineIntersection(Plane(float3(0,0,1),camera.clipnear*1.1f),float3(0,0,0),MouseVector));
	Line(cp - rotate(camera.orientation,float3( 0.01f,0.01f,0))*camera.clipnear, cp + rotate(camera.orientation,float3( 0.01f,0.01f,0))*camera.clipnear,float3(1,1,0));
	Line(cp - rotate(camera.orientation,float3(-0.01f,0.01f,0))*camera.clipnear, cp + rotate(camera.orientation,float3(-0.01f,0.01f,0))*camera.clipnear,float3(1,1,0));
	Line(camera.position+rotate(camera.orientation,float3(-0.05f,0,-2))*camera.clipnear , camera.position+rotate(camera.orientation,float3(0.05f,0,-2))*camera.clipnear , float3(1,0,1)) ;
	Line(camera.position+rotate(camera.orientation,float3(0,-0.05f,-2))*camera.clipnear , camera.position+rotate(camera.orientation,float3(0,0.05f,-2))*camera.clipnear , float3(1,0,1)) ;
}

BOOL CALLBACK EnumResNameProc(HMODULE hModule,LPCTSTR lpszType,LPTSTR lpszName,LONG_PTR lParam)
{
	String &s = * (String*) lParam;
	s += (IS_INTRESOURCE(lpszName)) ? String((int) lpszName) : lpszName ;
	s += "  ";
	s += (IS_INTRESOURCE(lpszType)) ? String((int) lpszType) : lpszType ;
	s += "   ";
	return 1;
}

String enumrestest(const char *s)
{
	String res = "resources:  ";
	// EnumResourceNames(NULL,NULL,EnumResNameProc,(LONG_PTR)&res);
	EnumResourceNames(NULL,RT_RCDATA,EnumResNameProc,(LONG_PTR)&res);
	res+= "Materials: ";
	EnumResourceNames(NULL,"material",EnumResNameProc,(LONG_PTR)&res);
	return res;
}
 
String resourcetest(const char *s)
{
	if(!s|| !*s) return enumrestest(s);
    HRSRC hresinfo = FindResource(NULL,s,RT_RCDATA);
	if(!hresinfo) return "resource not found";
	int size = SizeofResource(NULL,hresinfo);
	HGLOBAL hg=LoadResource(NULL,hresinfo);
	if(!hg) return "found but unable to load resource";
	void *ptr = LockResource(hg);
	return "ok";
}
EXPORTFUNC(resourcetest);

String gamefile("");
EXPORTVAR(gamefile);

int core =1;
EXPORTVAR(core)

static __declspec(align(32)) float testp[24];

String loadmats(String)
{
	extern void LoadMaterials(void *);
	_beginthread( LoadMaterials, 0, NULL );
	return "loading mats now";
}
EXPORTFUNC(loadmats);

float3 focuspoint;
EXPORTVAR(focuspoint);
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR arg, int )
{


	//testhdw();return 0;

	char progpath[1024] = "";
	GetModuleFileNameA(0, progpath, 1023);
	PathRemoveFileSpecA(progpath);
	if(*progpath)
	{
		int rc = _chdir(progpath);
		assert(rc==0);
	}

	cfg("melax3de.cfg");  // engine setup


	camera.position = float3(1.0f, -5.0f,2.0f );
	camera.orientation = YawPitchRoll(10,90,0);

	WinD3DInit();
	extern void LoadMaterial(char *);
	extern void LoadMaterials(void *);

	LoadMaterial("default.mat");
	LoadMaterial("font.mat");
	LoadMaterial("extendvolume.mat");
	LoadMaterial("highlight.mat");

	LoadMaterials(NULL);

	
	//SoundInit();

	
	
	

	ManipulatorInit();
	//ManipulatorNew(&lightposn);


	extern void AreaLoadDefault();
	AreaLoadDefault();  // if nothing loaded, it loads a basic room.

	float3 spinpoint;

	Entity *player = ObjectFind("player");
	assert(player);

	FuncInterp(arg);
	cfg(gamefile);

	

	int _core=-1;
	while(WindowUp())
	{
		if(core!=_core)
			SetThreadAffinityMask(GetCurrentThread(),core);
		_core=core;

		PROFILE(mainloop);
		extern void DoNetworkStuff();
		DoNetworkStuff();

		extern void AreaMotionController();
		extern void AreaPositionUpdate();
		AreaMotionController();  // sets where brushes are moving this frame.  this was just for some simple testing.  obsolete!


		// console updates
		extern void UpdateTerms();
		UpdateTerms();

		void AnimationUpdate();
		AnimationUpdate();

		extern void spinmove(Entity *object,const float3 &cr);
		extern void wasd_mlook(Entity *object);
		extern void playermove(Entity *object);
		extern void wasd_fly(Entity *object);

		if(spinnav)
		{
			spinmove(player,spinpoint);
		}
		else if(flynav)
		{
			wasd_fly(player);
		}
		else
		{
			PROFILE(playernav);
			wasd_mlook(player);
			playermove(player);
		}


		AreaPositionUpdate();  // needs to be before rendering, but not sure if this should go here or later on after manip and physics updates


		extern void camnav(Camera *camera);
		camnav(&camera);

		camera.RenderSetUp();
		extern float3 MouseVector;
		float3 v1 = camera.position+ rotate(camera.orientation, MouseVector) * 100.0f;
		crosshair();
		((MouseLeft)?ManipulatorDrag:ManipulatorHit)(camera.position,v1,&v1);
		focuspoint=v1;
		if(mousewheel) ManipulatorWheel(mousewheel);

		if(!spinnav && camera.position!=v1) { spinpoint = v1;}


		extern void DoCharacters();
		DoCharacters();

		extern void JointUpdate();
		JointUpdate();

		extern void PhysicsUpdate() ;
		PhysicsUpdate();

		// do attachement updates here!

		extern void AreaAggregate();
		AreaAggregate();

		extern void SceneRender();
		SceneRender();
		//ProfileReset();

	}
	SoundClose();
	ShutDown();
	return 0;
}



