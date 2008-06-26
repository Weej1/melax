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
int		centermouse;
int     focus;
int     quitrequest;
int     windowwidth=640,windowheight=480;
int     windowx=50,windowy=50;
float   winresreduction=1.0f;
int 	Width  = windowwidth;
int 	Height = windowheight;
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

String windowtitle("The Boston Tea Party!  (mini game demo - www.melax.com)");
//EXPORTVAR(windowtitle);

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
	char buf[1024];buf[0]='\0';
	if(entertext) {
		sprintf(buf,"> %s",comstring);
		PostString(buf,0,0,5);
	}
}

static char* quit(char*)
{
	quitrequest=1;
	return "quitting";
}
EXPORTFUNC(quit);
static char* exit(char*)
{
	quitrequest=1;
	return "quitting";
}
EXPORTFUNC(exit);

//----------------------------------------------------
// Key Bindings
// 
class Binding {
  public:
	char key;
	char command[256];
	char command_negative[256];
	Binding();
	~Binding();
	int flag; // 1==continuous/repeat  0==discrete/once  2==tied_to_keystate
	int active;  // true if key was down last frame
	int *varbinding;
	int IsDown(); // if the button (or whatever device), is down, true, or whatever.
};
Array<Binding *> bindings;
int BindingActivated;

Binding::Binding(){
	key='\0';
	command[0]='\0';
	command_negative[0]='\0';
	bindings.Add(this);
	flag=0;
	active=0;
	varbinding=NULL;
}

Binding::~Binding(){
	bindings.Remove(this);
}

int Binding::IsDown(){
	if(varbinding) {
		return *varbinding;
	}
	GetAsyncKeyState((int)key);// hack since getasynckeystate detects later
	return(GetAsyncKeyState((int)key));
}

Binding *FindBinding(char key) {
	for(int i=0;i<bindings.count;i++) {
		if(bindings[i]->key==key){return bindings[i];}
	}
	return NULL;
}
Binding *FindBinding(int *varbinding) {
	if(!varbinding) {
		return NULL;
	}
	for(int i=0;i<bindings.count;i++) {
		if(bindings[i]->varbinding==varbinding){return bindings[i];}
	}
	return NULL;
}

static float lastexectimer;
static String lastexec;
static const char *lastkey(char*)
{
	if(lastexectimer<=0) {return "";}
	lastexectimer -= DeltaT;
	return lastexec;
}
EXPORTFUNC(lastkey);

void DoBindings(){
	if(!focus) return;
	for(int i=0;i<bindings.count;i++) {
		if(bindings[i]->flag==2)
		{
			char buf[256];
			sprintf(buf,"%s %s",bindings[i]->command,(bindings[i]->IsDown())?"1":"0");
			FuncInterp(buf);
		}
		else if(bindings[i]->IsDown()) {
			if(bindings[i]->flag || !bindings[i]->active) {
				BindingActivated = (bindings[i]->active==0);
				String s = FuncInterp(bindings[i]->command);
				if(bindings[i]->flag==0)
				{
					lastexec = String(bindings[i]->command) + ": "+ s;
					lastexectimer = 1.0f;
				}
				BindingActivated=0;
			}
			bindings[i]->active=1;
		}
		else {
			bindings[i]->active=0;
			if(*bindings[i]->command_negative) {
				FuncInterp(bindings[i]->command_negative);
			}
		}
	}
} 

static char *keysdown(char *s) {
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

static char *bind(char *param) {
	static char buf[512];
	int n,i;
	int neg=0;
	int flag=0;
	char keyname[64];
	char key='\0';
	int *varbinding=NULL;
	keyname[0]='\0';
	buf[0]='\0';
	while(*param==' '){param++;}
	sscanf(param,"%s%n",keyname,&n);
	if(! keyname[0]) {
		sprintf(buf,"%d bindings: ",bindings.count);
		for(i=0;i<bindings.count;i++) {
			sprintf(buf+strlen(buf)," %c(%d)",bindings[i]->key,(int)bindings[i]->key);
		}
		return buf;
	}
	param += n;
	while(*param==' '||*param=='\t') {param++;}
	if(*param == '+') {
		// using the quake standard!
		flag|=1;
		param++;
	}
	if(*param == '-') {
		// execute when user lifts this key
		flag|=1;
		neg=1;
		param++;
	}
	if(*param == '=') {
		// execute every frame using keystate
		flag=2;
		param++;
	}

	if(!stricmp(keyname,"MouseLeft")) {
		varbinding = &MouseLeft;
	}
	else if(!stricmp(keyname,"MouseRight")) {
		varbinding = &MouseRight;
	}
	else if(!stricmp(keyname,"SPACE")) {
		key = 32;
	}
	else if(!stricmp(keyname,"LEFT")) {
		key = 37;
	}
	else if(!stricmp(keyname,"RIGHT")) {
		key = 39;
	}
	else if(!stricmp(keyname,"UP")) {
		key = 38;
	}
	else if(!stricmp(keyname,"DOWN")) {
		key = 40;
	}
	else if(keyname[0]=='\\') {
		int tmp=0;
		sscanf(keyname+1,"%d",&tmp);
		key = tmp;
	}
	else if(strlen(keyname)!=1) {
		return "sorry, only basic alpha chars at this time";
	}
	else if(keyname[0]>='a' && keyname[0] <='z') {
		key = keyname[0] += 'A'-'a';
	}
	else {
		key = keyname[0];
	}
	Binding *b=FindBinding(key);
	if(!b && varbinding) {
		b=FindBinding(varbinding);
	}
	if(!param[0]){
		if(!b) {
			sprintf(buf,"%s has no binding",keyname);
		}
		else {
			sprintf(buf,"%s bound to %s (%s)",keyname,b->command,(flag==2)?"tied":(b->flag)?"repeat":"once");
		}
		return buf;
	}
	char firstword[64];
	firstword[0]='\0';
	sscanf(param,"%s",firstword);
	if(!stricmp(firstword,"0") ||!stricmp(firstword,"NULL")) {
		if(!b) {
			sprintf(buf,"%s didn't have a binding anyway",keyname);
		}
		else {
			sprintf(buf,"%s no longer bound to %s",keyname,b->command);
			delete b;
		}
		return buf;
	}
	sprintf(buf,"%s binding for %s \"%s\" %s",(b)?"changing":"adding",keyname,param,(flag==2)?"tied":(flag)?"repeat":"once");
	if(!b) {
		b=new Binding();
		b->key = key;
		b->varbinding = varbinding;
	}
	if(!neg) {
		b->flag = flag;
		strcpy(b->command,param);
		b->command_negative[0]='\0';
	}
	else{
		strcpy(b->command_negative,param);
	}
	return buf;
}
EXPORTFUNC(bind);



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
		char *rv=FuncInterp(comstring);
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
		Array<const char *> match;
		char *rv=CommandCompletion(comstring,match);
		strcpy(comstring,rv);
		char *pre=FuncPreInterp(comstring);
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
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
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
int resetdevice(char *)
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
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
		case WM_DROPFILES:
			{
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
						FuncInterp(String(s) + " " + fname);
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
					extern int ManipulatorKeyPress(int k);
					ManipulatorKeyPress((int) wParam);
					break;
			}
			return 0;

		case WM_LBUTTONDOWN:
			/* if we don't set the capture we won't get mouse move
			   messages when the mouse moves outside the window. */
			SetCapture(hWnd);
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			MouseLeft = 1;
			return 0;

		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			MouseRight = 1;
			return 0;

		case WM_LBUTTONUP:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			MouseLeft=0;
			/* remember to release the capture when we are finished. */
			if(!MouseRight) ReleaseCapture();
			return 0;

		case WM_RBUTTONUP:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			MouseRight=0;
			/* remember to release the capture when we are finished. */
			if(!MouseLeft) ReleaseCapture();
			return 0;

		case WM_MOUSEMOVE:
			ComputeMouseVector(LOWORD(lParam),HIWORD(lParam));
			return 0;
		case  WM_MOUSEWHEEL:
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
int WinD3DInit( HINSTANCE ,WNDPROC MsgProc)
{
	PROFILE(wind3dinit);
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
	if(!entertext) {
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
	if(filename=="")
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