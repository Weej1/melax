//
//   Windows Setup Stuff
//
// Code here was written by: stan melax (Copyright 2003)
// with some minor updates since then (up to 2011)
// Currently windows and direct3d only - no opengl codepaths right now.
//
// The minimalistic objective is to enable the easiest possible code for building 3D applications. 
// i.e. without a lot of API baggage or programmer style/religion being shoved down your throat,
// and without depending on some sort of samples infrasturcture with dependancies you may not want.
// No inheriting from some class and filling in a bunch of virtual functions.
// You write a main() with a while(){} loop.
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

// was used for instrumetation, now obsolete:
#define PROFILE(X)


#endif
