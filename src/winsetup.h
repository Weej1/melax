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

#include <Windows.h>
#include <d3dx9.h>

#ifdef NDEBUG
#define VERIFY_RESULT  (0)
#else
#define VERIFY_RESULT  (_assert("call returned failure", __FILE__, __LINE__),0)
#endif

extern LPDIRECT3DDEVICE9       g_pd3dDevice; 


LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
int  WinD3DInit( HINSTANCE hInst,WNDPROC MsgProc);
void ShutDown();
int  WindowUp();
extern float DeltaT;
extern float GlobalTime;


#endif
