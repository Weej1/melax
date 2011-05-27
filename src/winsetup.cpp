//
//   Windows Setup Stuff
//
// If your video card gives you grief, then try
// changing:  D3DDEVTYPE_HAL to D3DDEVTYPE_REF
//


#include "winsetup.h"
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>
#include "d3dfont.h"
#include "vecmath.h"
#include "console.h"
#include "manipulator.h"

#ifndef WM_MOUSEWHEEL 
#define WM_MOUSEWHEEL  (0x020A)
#define WHEEL_DELTA    (120)
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif 

extern void effectresetdevice();
extern void effectlostdevice();

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
HWND hWnd;
RECT window_inset;  // the extra pixels windows needs for its borders on windowed windows.
LPDIRECT3D9             g_pD3D       = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
D3DPRESENT_PARAMETERS   d3dpp;
float   DeltaT = 0.016f;
float   DeltaTprev = 0.016f;
float	GlobalTime=0.0f;
float   fps;
int     MouseX = 0;
int     MouseY = 0;
int		MouseXOld = 0;
int		MouseYOld = 0;
float3  MouseVector(0,0,-1);      // 3D direction mouse points
static float3  MouseVectorOld(0,0,-1);
static float3	MouseDiff;
float 	MouseDX;
float	MouseDY;
int     MouseLeft=0;      // true iff left button down
int     MouseRight=0;     // true iff right button down
extern float  &viewangle;
int		centermouse=1;
int     focus;
int     quitrequest;
int     windowwidth=640,windowheight=480;
int     windowx=50,windowy=50;
float   winresreduction=1.0f;
int 	Width  = windowwidth;
int 	Height = windowheight;
int     shiftdown=0;
int     ctrldown =0;
EXPORTVAR(shiftdown);
EXPORTVAR(ctrldown);
EXPORTVAR(windowwidth);
EXPORTVAR(windowheight);
EXPORTVAR(windowx);
EXPORTVAR(windowy);
EXPORTVAR(winresreduction);
EXPORTVAR(focus);
EXPORTVAR(fps);
EXPORTVAR(Width);
EXPORTVAR(Height);
EXPORTVAR(viewangle);
EXPORTVAR(GlobalTime);
EXPORTVAR(centermouse);
EXPORTVAR(MouseDX);
EXPORTVAR(MouseDY);
EXPORTVAR(MouseVectorOld);
EXPORTVAR(MouseVector);
EXPORTVAR(MouseLeft);
EXPORTVAR(MouseRight);
float oneoverwidth=1.0f/640.0f;
float oneoverheight=1.0f/480.0f;
EXPORTVAR(oneoverwidth);
EXPORTVAR(oneoverheight);

String windowtitle("Insert Game Title Here");
EXPORTVAR(windowtitle);

int entertext=0;
char comstring[512];

float FixDeltaT=0.0f;
EXPORTVAR(FixDeltaT);
EXPORTVAR(DeltaT);
int mousewheel;
EXPORTVAR(mousewheel);


void CalcFPSDeltaT(){
	// occasionally I get some sort of weird looping thing with timegettime()
	// its like timeGetTime() loops past maxint or something.
	// I clamp deltat - this seems to have solved the problems.
	DeltaTprev = DeltaT;
	static int timeinit=0;
	static int start,start2,current,last;
	static int frame=0, frame2=0;
	if(!timeinit){
		frame=0;
		start=timeGetTime();
		timeinit=1;
	}
	frame++;
	frame2++;
	current=timeGetTime(); // found in winmm.lib
	double dif=(double)(current-start)/CLOCKS_PER_SEC;
	double rv = (dif)? (double)frame/(double)dif:-1.0;
	if(dif<0 || (dif>2.0 && frame >10)) {
		start  = start2;
		frame  = frame2;
		start2 = timeGetTime();
		frame2 = 0;
	}		   
	DeltaT = (float)(current-last)/CLOCKS_PER_SEC;
	if(current==last) { 
		DeltaT = 1.0f/1000.0f;  // assume no more than 1000fps
	}
	DeltaT=Max(DeltaT,0.001f);  // clamp DeltaT to avoid unstability errors
	DeltaT=Min(DeltaT,0.100f);  
	fps = (float)rv;
	last = current;

	if(FixDeltaT>0){DeltaT=FixDeltaT;}
	GlobalTime += DeltaT;
}


void ComputeMouseVector(int winx,int winy){
		/* Win32 is a bit strange about the x, y position that
		   it returns when the mouse is off the left or top edge
		   of the window (due to them being unsigned). therefore,
		   roll the Win32's 0..2^16 pointer co-ord range to the
		   more amenable (and useful) 0..+/-2^15. */
	if(winx & 1 << 15) winx -= (1 << 16);
	if(winy & 1 << 15) winy -= (1 << 16);
	MouseX=winx;
	MouseY=Height-winy;  // I prefer opengl's standard of origin in bootom left
	float spread = tanf(DegToRad(viewangle/2));  
	float y = spread * ((MouseY)-Height/2) /(Height/2.0f);
	float x = spread * (MouseX-Width/2)  /(Height/2.0f);
	float3 v(x ,y,-1.0f); 
	// v=UserOrientation *v;
	v=normalize(v);
	MouseVector = v;
}


void PrintStats(){
	String s;
	if(entertext) {
		s << "> " << comstring;
		PostString(s,0,0,5);
	}
}

static char* quit(const char*)
{
	quitrequest=1;
	return "quitting";
}
EXPORTFUNC(quit);
static char* exit(const char*)
{
	quitrequest=1;
	return "quitting";
}
EXPORTFUNC(exit);

static float lastexectimer;
static String lastexec;
static const char *lastkey(const char*)
{
	if(lastexectimer<=0) {return "";}
	lastexectimer -= DeltaT;
	return lastexec;
}
EXPORTFUNC(lastkey);

void DoBindings(){
	if(!focus) return;
	for(int i=0;i<128;i++)
	{
		GetAsyncKeyState(i);// hack since getasynckeystate detects later
		if(GetAsyncKeyState(i))
		{
			extern void keyvent(const char *name,int k);
			keyvent("keyheld",i);
		}
	}
} 

static char *keysdown(const char *s) {
	int i;
	static char buf[256];
	if(*s && 1==sscanf(s,"%d",&i) && i>=0)
	{
		return (GetAsyncKeyState(i))?"1":"0";
	}
	buf[0]='\0';
	for(i=0;i<256;i++) {
		if(GetAsyncKeyState(i)) {
			sprintf(buf+strlen(buf),"%d(%c) ",i,(char)i);
		}
	}
	return buf;
}
EXPORTFUNC(keysdown);


char *CommandCompletion(const char *name,Array<String> &match) 
{
	static char returnvalue[11000];
	returnvalue[0]='\0';
	match.count=0;
	if(!name) return returnvalue;
	strcpy(returnvalue,name);
	
	int i;
    char *returnarg = returnvalue;
    const char *lastargument = name;

    // Move to the last argument...
    int len = strlen( name );
    for (i=0; i<len; i++)
    {
        if (name[i] == ' ')
        {
            lastargument = name + i+1;
            returnarg = returnvalue + i+1;
        }
    }
	LVarMatches(lastargument,match);
	if(IsOneOf('.',lastargument))
	{
		String n("");
		const char *s=lastargument;
		while(*s!='.')  n << *s++;
		s++;
		ClassDesc *cd=NULL;
		if(LVarLookup(n,cd))
		 for(int i=0;i<cd->members.count;i++)
		{
			if(!strncmp(cd->members[i].mname,s,strlen(s)))
			{
				match.Add(n + "." + cd->members[i].mname);
			}
		}
	}

	if(match.count==0) return returnvalue;
	if(match.count==1) {
		sprintf(returnarg,"%s",(const char*)match[0]);  //,(foundobject)?".":" ");
		return returnvalue;
	}
	int j=0;
	while(lastargument[j]) {
		returnarg[j]=lastargument[j];
		j++;
	}
	char c=' ';
	while(c) {
		c=' ';
		for(i=0;c!='\0' && i<match.count;i++) {
			if(c==' ') c=match[i][j];
			else if(c != match[i][j]) c='\0';
		}
		returnarg[j]=c;
		j++;
	}
	return returnvalue;
}



void doconsole(char c) {
	if(c==27){  // ESC escape key
		entertext=0;
		comstring[0]='\0';
		PostString("------",0,0,0.25);
		return;
	}
	if(c=='\n' || c== '\r') {
		entertext=0;
		char buf[1024];
		sprintf(buf,"* %s",comstring);
		PostString(buf,0,0,5);
		String rv=FuncInterp(comstring);
		PostString(rv,0,1,5);
		return;
	}
	if(c=='\b') {
		if(strlen(comstring)) {
			comstring[strlen(comstring)-1]='\0';
		}
		return;
	}
	if(c=='\t') {
		Array<String> match;
		char *rv=CommandCompletion(comstring,match);
		strcpy(comstring,rv);
		String pre=FuncPreInterp(comstring);
		PostString(pre,0,1,1);
		if(match.count>1) {
			static int offset=0;
			float hold=(match.count>15) ?4.0f: 1.0f;
			if(match.count < 15) {
				offset=0;
			}
			for(int i=0;i<match.count && i<15;i++){
				PostString(match[(i+offset)%match.count],4,6+i,hold);
			}
			offset+=5;
		}
		return;
	}
	int len=min(500,strlen(comstring));
	comstring[len]=c;
	comstring[len+1]='\0';
}

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
int backbuffercount = 1;
int vsync =1;
static int vsync_current=vsync; // so we can track when this is changed and reset if necessary

EXPORTVAR(backbuffercount);
EXPORTVAR(vsync);

int d3dcreate_software_vertexprocessing=0;
EXPORTVAR(d3dcreate_software_vertexprocessing);


HRESULT InitD3D( HWND hWnd )
{
    // Create the D3D object.
    if( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed = TRUE;
	d3dpp.BackBufferFormat = (d3dpp.Windowed)?D3DFMT_UNKNOWN:D3DFMT_X8R8G8B8 ;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.BackBufferCount = backbuffercount;
	d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	vsync_current = vsync;
    // Create the D3DDevice
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      ((d3dcreate_software_vertexprocessing)? D3DCREATE_SOFTWARE_VERTEXPROCESSING :D3DCREATE_HARDWARE_VERTEXPROCESSING),
                                      &d3dpp, &g_pd3dDevice ) ) )
    {
		if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
										  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
										  &d3dpp, &g_pd3dDevice ) ) )
			if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
											  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
											  &d3dpp, &g_pd3dDevice ) ) )
		        return E_FAIL;
    }

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ); // Turn off culling
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );  // Turn off D3D lighting
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );     // Turn on the zbuffer
    return S_OK;
}

void cuberelease()
{
	unsigned long rc;
	extern ID3DXRenderToEnvMap*   RenderToEnvMap;
	if(RenderToEnvMap)
	{
		rc = RenderToEnvMap->Release();
		assert(rc==0);
	}
	RenderToEnvMap=NULL;

	extern IDirect3DCubeTexture9* CubeMap;
	if(CubeMap)
	{
		rc=CubeMap->Release();
		assert(rc==0);
	}
	CubeMap=NULL;
}

int windowed=1;
EXPORTVAR(windowed);
int screenwidth=0,screenheight=0;
int resetdevice(const char *)
{
	if(windowed==0)
	{
		if(screenwidth==0 || screenheight==0)
		{
			D3DDISPLAYMODE displaymode;
			g_pd3dDevice->GetDisplayMode(0,&displaymode);
			screenwidth  = displaymode.Width;
			screenheight = displaymode.Height;
		}
		Width = screenwidth;
		Height= screenheight;
	}
	else // windowed
	{
		Width = (int)(windowwidth * winresreduction);
		Height= (int)(windowheight* winresreduction);
	}
	oneoverwidth=1.0f/Width;
	oneoverheight=1.0f/Height;
	d3dpp.BackBufferWidth = Width;
	d3dpp.BackBufferHeight = Height;
    d3dpp.Windowed = (windowed)?1:0;
	d3dpp.BackBufferFormat = (d3dpp.Windowed)?D3DFMT_UNKNOWN:D3DFMT_X8R8G8B8 ;
	d3dpp.BackBufferCount = backbuffercount;
	d3dpp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	vsync_current = vsync;
	effectlostdevice();
	cuberelease();
	extern void rendertargetsrelease();
	rendertargetsrelease();
	HRESULT hr = g_pd3dDevice->Reset(&d3dpp);
	assert(hr==0);
	effectresetdevice();
	if(windowed)
            SetWindowPos( hWnd, HWND_NOTOPMOST,windowx+window_inset.left,windowy+window_inset.top,windowwidth+window_inset.right-window_inset.left, windowheight+window_inset.bottom-window_inset.top,SWP_SHOWWINDOW );
	return hr;
}
EXPORTFUNC(resetdevice);

//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
static void Cleanup()
{
    if( g_pd3dDevice != NULL )
        g_pd3dDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();
}




int wm_size_width; 
int wm_size_height;
EXPORTVAR(wm_size_width);
EXPORTVAR(wm_size_height);


int msglast;
EXPORTVAR(msglast);
//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
Array<String> dropfiles;

LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
		case WM_DROPFILES:
			{
				/*RECT rect;
				POINT pt;
				GetWindowRect(hWnd,&rect);
				GetCursorPos(&pt);
				if(windowed)
				{	
					rect.left -= window_inset.left; // todo: look into the function: ScreenToClient(hwnd,point)
					rect.top  -= window_inset.top;
				}
				ComputeMouseVector(pt.x-rect.left,pt.y-rect.top);
				*/
				//MouseVector = float3(0,0,-1);
				HDROP hDrop = (HDROP)wParam;
				unsigned int n = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
				for(unsigned int i=0; i<n; i++)
				{
					char fname[1024];
					if(DragQueryFileA(hDrop, i, fname, sizeof(fname)))
					{
						char *s = fname+strlen(fname);
						while (s>fname && *s != '.' ) s--;
						if(*s=='.') s++;
						dropfiles.Add(String(s) + " \"" + fname + "\"");
					}
				}
				DragFinish(hDrop);
				break;
			}
		case WM_SETFOCUS:
			focus=1;
			break;
		case WM_KILLFOCUS:
			focus=0;
			break;
        case WM_DESTROY:
            PostQuitMessage( 0 );
			quitrequest=1;
            return 0;
		case WM_CLOSE:
			PostQuitMessage(0);
			quitrequest=1;
			return 0;
		case WM_CHAR:
			if(entertext) {
				doconsole(wParam);
				return 0;
			}
			if(ManipulatorKeyPress((int) wParam))  // manipulator is taking the key input!!!
				return 0;
			switch (wParam) {
				case 27: /* ESC key */
					PostQuitMessage(0);
					quitrequest=1;
					break;
				case '`': 
				case '\n': 
				case '\r': 
					entertext=1;
					comstring[0]='\0';
					break;
				default:
					{
						extern void keyvent(const char *name,int k);
						keyvent("keydown",toupper((int)wParam));
						break;
					}
			}
			return 0;
		case WM_KEYUP:
			{
				if(entertext || ManipulatorKeysGrab())
					return 0;
				extern void keyvent(const char *name,int k);
				keyvent("keyup",toupper((int)wParam));
				return 0;
			}
		case WM_LBUTTONDOWN:
			/* if we don't set the capture we won't get mouse move
			   messages when the mouse moves outside the window. */
			SetCapture(hWnd);
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			MouseLeft = 1;
			return 0;

		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			MouseRight = 1; 
			FuncInterp("rbuttondown");
			return 0;

		case WM_LBUTTONUP:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			MouseLeft=0;
			/* remember to release the capture when we are finished. */
			if(!MouseRight) ReleaseCapture();
			return 0;

		case WM_RBUTTONUP:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			MouseRight=0;
			/* remember to release the capture when we are finished. */
			if(!MouseLeft) ReleaseCapture();
			return 0;

		case WM_MOUSEMOVE:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			return 0;
		case  WM_MOUSEWHEEL:
			shiftdown = (wParam&MK_SHIFT)?1:0;
			ctrldown  = (wParam&MK_CONTROL)?1:0;
			mousewheel += GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA;
			return 0;
        case WM_SETCURSOR:
            // Turn off Windows cursor
             SetCursor( NULL );
             g_pd3dDevice->ShowCursor( 0 );
			return 1;
		case WM_SYSCHAR:
			switch(wParam)
			{
				case 27: /* ESC key */
					PostQuitMessage(0);
					quitrequest=1;
					break;
				case '\n':
				case '\r':
					windowed = !windowed;
					resetdevice(NULL);
					break;
			}
			return 0;
		case WM_MOVE:
			if(windowed)
			{
				windowx = LOWORD(lParam);
				windowy = HIWORD(lParam);
			}
			return 1;
		case WM_SIZE:
			wm_size_width  = LOWORD(lParam);
			wm_size_height = HIWORD(lParam);
			windowwidth = LOWORD(lParam);
			windowheight= HIWORD(lParam);
			return 1;
		case WM_EXITSIZEMOVE:
			if(windowed && (Width != (int)(windowwidth * winresreduction) || Height!= (int)(windowheight* winresreduction)))
			{
				resetdevice(NULL);
			}
			return 1;
    }
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

 
//Attepmt to find a particular window on the desktop 

static int wrow =0;
FILE *swfp=NULL;
BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam)
{
	WINDOWINFO windowinfo;
	GetWindowInfo(hwnd,&windowinfo);
	char buf[256];
	buf[0]=buf[255]='\0';
	GetClassName(hwnd,buf,255);
	fprintf(swfp,"'%s' %x    %x\n",buf,hwnd,GetParent(hwnd));
	return true;
}


String scanwindows(String s)
{
	swfp=fopen("windowclasses.txt","w");
	EnumChildWindows(GetDesktopWindow(),EnumWindowsProc,0);
	fclose(swfp);
	swfp=NULL;
	return "ok";
}
EXPORTFUNC(scanwindows);



static HINSTANCE hInst;
int WinD3DInit(  )
{
    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "PhysView", NULL };
    RegisterClassEx( &wc );
	hInst = wc.hInstance;

    SetRect( &window_inset, 0, 0, 0, 0 );
    AdjustWindowRect( &window_inset, WS_OVERLAPPEDWINDOW, 0 );
    //RECT rc;
    //SetRect( &rc, 0, 0, Width, Height );
    //AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, 0 );


    // Create the application's window
	Width = windowwidth;
	Height= windowheight;
    hWnd = CreateWindow( "PhysView", windowtitle,
                              WS_OVERLAPPEDWINDOW, windowx+window_inset.left, windowy+window_inset.top, Width+window_inset.right-window_inset.left, Height+window_inset.bottom-window_inset.top,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL );
/* 	HWND parentwindow=GetDesktopWindow();
	parentwindow=(HWND)0xa0618;
   hWnd = CreateWindow( "PhysView", "PhysView",
                              WS_CHILD, 0, 0, 200,200,
                              parentwindow, NULL, wc.hInstance, NULL );
*/

    // Initialize Direct3D
    HRESULT hr;
	hr =  InitD3D( hWnd );

    // Show the window
    ShowWindow( hWnd, SW_SHOWDEFAULT );
    UpdateWindow( hWnd );
	DragAcceptFiles(hWnd,true);
	InitFont(g_pd3dDevice);

	return 1;
}
void ShutDown()
{
	DeleteFont();
    Cleanup();
    UnregisterClass( "PhysView", hInst );
}

static int centered_last_frame=0;

int WindowUp()
{
	mousewheel=0;
	MouseVectorOld=MouseVector;
	MouseXOld = MouseX;
	MouseYOld = MouseY;
	while(dropfiles.count)  // I had to delay this a frame since mousevector and manipulator needs updating.
		FuncInterp(dropfiles.Pop());
	MSG   msg;				/* message */
	while(PeekMessage(&msg, hWnd, 0, 0, PM_NOREMOVE)) {
		if(GetMessage(&msg, hWnd, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			return 0;
		}
	}
	HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
	while(hr==D3DERR_DEVICELOST) 
	{
		Sleep(1);
		if(GetMessage(&msg, hWnd, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			return 0;
		}
		hr = g_pd3dDevice->TestCooperativeLevel();
	} 
	if (hr==D3DERR_DEVICENOTRESET) 
	{
		resetdevice(NULL);
	}

	if(vsync_current != vsync)
	{
		resetdevice(NULL);
	}

	MouseDiff = MouseVector-MouseVectorOld;
	MouseDX=(MouseX-MouseXOld)/(Width/2.0f);
	MouseDY=(MouseY-MouseYOld)/(Height/2.0f);
	if(centermouse && focus) {
		RECT rect;
		RECT crect;
		POINT pt;
		GetWindowRect(hWnd,&rect);
		GetClientRect(hWnd,&crect);
		GetCursorPos(&pt);
		if(windowed)
		{	
			rect.left -= window_inset.left; // todo: look into the function: ScreenToClient(hwnd,point)
			rect.top  -= window_inset.top;
		}
		if(centered_last_frame)
		{
			MouseDX = (float)(pt.x-rect.left-(Width/2))/(Width/2.0f);
			MouseDY = -(float)(pt.y-rect.top-(Height/2))/(Height/2.0f);
		}
		SetCursorPos(Width/2+rect.left,Height/2+rect.top);
		MouseVector = float3(0,0,-1);
		centered_last_frame =1;
	}
	else
	{
		centered_last_frame =0;
	}

	CalcFPSDeltaT();
	if(!entertext && !ManipulatorKeysGrab()) {
		DoBindings();
	}
	else {
		PrintStats();
	}
	return (msg.message!=WM_QUIT && !quitrequest);
}

static String screengrabprefix("screenshot");
EXPORTVAR(screengrabprefix);
static int screengrabcount=0;
EXPORTVAR(screengrabcount);

String screengrab(String filename)
{
	if(filename=="" || filename == " () ")
	{
		filename.sprintf("%s_%5.5d_.bmp",(const char*)screengrabprefix,screengrabcount);		
		screengrabcount++;
	}
	HRESULT hr;
	// get display dimensions
	// this will be the dimensions of the front buffer
	D3DDISPLAYMODE mode;
	if (FAILED(hr=g_pd3dDevice->GetDisplayMode(0,&mode)))
	{
		return "fail getdisplaymode";
	}
	// create the image surface to store the front buffer image
	// note that call to GetFrontBuffer will always convert format to A8R8G8B8
	static LPDIRECT3DSURFACE9 surf=NULL;
	if (!surf && FAILED(hr=g_pd3dDevice->CreateOffscreenPlainSurface(mode.Width,mode.Height,
		D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&surf,NULL)))
	{
		return "fail createimagesurface";
	}
 	// Next, this surface is passed to the GetFrontBuffer() method of the device, which will copy the entire screen into our image buffer:
	// read the front buffer into the image surface
	if (FAILED(hr=g_pd3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&surf))) 
	{
		surf->Release();
		return "fail getfrontbuffer";
	}
	//Finally, we call D3DXSaveSurfaceToFile() to create the BMP file, and release the temporary image surface:
	// write the entire surface to the requested file
	hr=D3DXSaveSurfaceToFile(filename,D3DXIFF_BMP,surf,NULL,NULL);
	// release the image surface
	// surf->Release();
	// return status of save operation to caller
	return (hr==D3D_OK)? filename + " exported!" : "something failed";
}
EXPORTFUNC(screengrab);



