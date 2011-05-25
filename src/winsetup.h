//
//   Windows Setup Stuff
//
// Code here was written by: stan melax (Copyright 2003)
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

#ifndef SM_WIN_SETUP_H
#define SM_WIN_SETUP_H


#include <assert.h>

#ifdef NDEBUG
#define VERIFY_RESULT  (0)
#else
#define VERIFY_RESULT  (assert(0),0)
#endif

extern struct IDirect3DDevice9 *       g_pd3dDevice; 



int  WinD3DInit( );
void ShutDown();
int  WindowUp();
extern float DeltaT;
extern float GlobalTime;



class Profile
{
public:
	//static Profile* current;
	//Profile *parent;
	__int64 start;
	const char *name;
	inline Profile(const char *_name):name(_name)
	{
		//parent=current;
		//current=this;
		__int64 _start=0;
	/*	__asm
		{
			cpuid;                // Force completion of out-of-order processing
			rdtsc;                // Read timestamp counter
			mov DWORD PTR [_start    ], eax;  // Copy counter value into variable
			mov DWORD PTR [_start + 4], edx 
		}
	*/
		start=_start;
	}
	inline ~Profile()
	{
		__int64 finish=1;
/*
		__asm
		{
			cpuid;                // Force completion of out-of-order processing
			rdtsc;                // Read timestamp counter
			mov DWORD PTR [finish    ], eax;  // Copy counter value into variable
			mov DWORD PTR [finish + 4], edx 
		}
*/
		addrecord(name,(int) (finish-start));
		//current=parent;
	}
	void addrecord(const char *name,int cycles);
};
void ProfileReset();
#define PROFILE(n) Profile _pf ## n (#n);

#endif
