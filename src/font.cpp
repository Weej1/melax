//-----------------------------------------------------------------------------
// this file was adapted from the d3d demo common library,
// with some minor modificaitons here and there.
// File: D3DFont.cpp
//
// Desc: Texture-based font class
//
// Copyright (c) 1999-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include <D3DX9.h>
#include "D3DFont.h"
#include "array.h"
#include "console.h"
#include "winsetup.h"

//-----------------------------------------------------------------------------
// Name: class CD3DFont
// Desc: Texture-based font class for doing text in a 3D scene.
//-----------------------------------------------------------------------------
class CD3DFont
{
public:
    LPDIRECT3DDEVICE9       m_pd3dDevice; // A D3DDevice used for rendering
    LPDIRECT3DTEXTURE9      m_pTexture;   // The d3d texture for this font
    DWORD   m_dwTexWidth;                 // Texture dimensions
    DWORD   m_dwTexHeight;
    FLOAT   m_fTexCoords[256][4];
    DWORD   m_dwSpacing;                  // Character pixel spacing per side

	LPD3DXEFFECT effect;
	D3DXHANDLE diffusemap;


    // 2D and 3D text drawing functions
    HRESULT DrawText( FLOAT x, FLOAT y, DWORD dwColor, 
                      const TCHAR* strText );
	float *gettexcoords(){return &m_fTexCoords[0][0];}
    // Initializing and destroying device-dependent objects
    HRESULT InitDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT DeleteDeviceObjects();

    // Constructor / destructor
    CD3DFont();
    ~CD3DFont();
};



#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }



//-----------------------------------------------------------------------------
// Custom vertex types for rendering text
//-----------------------------------------------------------------------------

struct FONT2DVERTEX { D3DXVECTOR4 p;   DWORD color;     FLOAT tu, tv; };

#define D3DFVF_FONT2DVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

inline FONT2DVERTEX InitFont2DVertex( const D3DXVECTOR4& p, D3DCOLOR color,
                                      FLOAT tu, FLOAT tv )
{
    FONT2DVERTEX v;   v.p = p;   v.color = color;   v.tu = tu;   v.tv = tv;
    return v;
}


//-----------------------------------------------------------------------------
// Name: CD3DFont()
// Desc: Font class constructor
//-----------------------------------------------------------------------------
CD3DFont::CD3DFont()
{
    m_dwSpacing            = 0;

    m_pd3dDevice           = NULL;
    m_pTexture             = NULL;

}




//-----------------------------------------------------------------------------
// Name: ~CD3DFont()
// Desc: Font class destructor
//-----------------------------------------------------------------------------
CD3DFont::~CD3DFont()
{
    DeleteDeviceObjects();
}


#include "xmlparse.h"
#include "stringmath.h"
//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Initializes device-dependent objects, including the vertex buffer used
//       for rendering text and the texture map which stores the font image.
//-----------------------------------------------------------------------------


String genfxml(String)
{
	Array<int> w;
	xmlNode *n = XMLParseFile("textures/newfont/f.xml");
	ArrayImport(w,n->body,512);
	for(int i=0;i<256;i++)
	{
		w[i]=w[i*2+1];
	}
	w.count=256;
	n->body="";
	for(int i=0;i<128;i++)
	{
		float2 t(1/32.0f + (i %16)/16.0f , 1/32.0f + (i /16)/16.0f);
		float2 r(1/32.0f,1/32.0f);
		n->body << i+32 << " " << t-r  << " " << t+r << ",\n";
	}
	XMLSaveFile(n,"textures/newfont/ff.xml");
	return "wrotethestuffout";
}
EXPORTFUNC(genfxml);

HRESULT CD3DFont::InitDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice )
{
    HRESULT hr;

    // Keep a local copy of the device
    m_pd3dDevice = pd3dDevice;


    // Large fonts need larger textures
	//m_dwTexWidth  = m_dwTexHeight = 256;

	xmlNode *node = XMLParseFile("textures/font.xml");
	assert(node);
	int count = node->attribute("count").Asint();
	m_dwSpacing  = node->attribute("spacing").Asint() ;
	m_dwTexWidth = node->attribute("width").Asint();
	m_dwTexHeight= node->attribute("height").Asint();
	String imagefile = "textures/font.png";
	StringIter p(node->body);
    for( int i=0;i<count;i++)
    {
		int c;
        p >> c;
		p >> m_fTexCoords[c-32][0] >> m_fTexCoords[c-32][1] >> m_fTexCoords[c-32][2] >> m_fTexCoords[c-32][3];
    }
	hr=D3DXCreateTextureFromFile(m_pd3dDevice,(const char*)imagefile,&m_pTexture);
	LPD3DXBUFFER error;
	extern String effectpath;
	D3DXCreateEffectFromFile(m_pd3dDevice,effectpath + "/font.fx",NULL,NULL,0,NULL,&effect,&error)  && VERIFY_RESULT;
	assert(effect);
	diffusemap = effect->GetParameterByName(NULL,"diffusemap");
	assert(diffusemap);
	effect->SetTexture(diffusemap,m_pTexture) && VERIFY_RESULT;
    return S_OK;
}









//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DeleteDeviceObjects()
{
    SAFE_RELEASE( m_pTexture );
    m_pd3dDevice = NULL;

    return S_OK;
}






//-----------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draws 2D text. Note that sx and sy are in pixels
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DrawText( FLOAT sx, FLOAT sy, DWORD dwColor,
                            const TCHAR* strText )
{
    if( m_pd3dDevice == NULL )
        return E_FAIL;


	effect->SetTechnique("screenspace") &&VERIFY_RESULT;
	Array<FONT2DVERTEX> vertices;
    



    // Adjust for character spacing
    sx -= m_dwSpacing;
    FLOAT fStartX = sx;

    while( *strText )
    {
        TCHAR c = *strText++;

        if( c == _T('\n') )
        {
            sx = fStartX;
            sy += (m_fTexCoords[0][3]-m_fTexCoords[0][1])*m_dwTexHeight;
        }

        if( (c-32) < 0 || (c-32) >= 128-32 )
            continue;

        FLOAT tx1 = m_fTexCoords[c-32][0];
        FLOAT ty1 = m_fTexCoords[c-32][1];
        FLOAT tx2 = m_fTexCoords[c-32][2];
        FLOAT ty2 = m_fTexCoords[c-32][3];

        FLOAT w = (tx2-tx1) *  m_dwTexWidth ;
        FLOAT h = (ty2-ty1) * m_dwTexHeight;

        if( c != _T(' ') )
        {
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+0-0.5f,sy+h-0.5f,0.01f,1.0f), dwColor, tx1, ty2 ));
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+0-0.5f,sy+0-0.5f,0.01f,1.0f), dwColor, tx1, ty1 ));
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+w-0.5f,sy+h-0.5f,0.01f,1.0f), dwColor, tx2, ty2 ));
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+w-0.5f,sy+0-0.5f,0.01f,1.0f), dwColor, tx2, ty1 ));
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+w-0.5f,sy+h-0.5f,0.01f,1.0f), dwColor, tx2, ty2 ));
            vertices.Add(InitFont2DVertex( D3DXVECTOR4(sx+0-0.5f,sy+0-0.5f,0.01f,1.0f), dwColor, tx1, ty1 ));
        }

        sx += w - (2 * m_dwSpacing);
    }

	if( vertices.count == 0 ) return S_OK;
	unsigned n;
	int i;
	effect->Begin(&n,1)  &&VERIFY_RESULT;
	for(i=0;i<(int)n;i++)
	{
		effect->BeginPass(i)      &&VERIFY_RESULT; 
		m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, vertices.count/3, &vertices[0],sizeof(FONT2DVERTEX));
		effect->EndPass();

	}
	effect->End()        &&VERIFY_RESULT;
    return S_OK;
}





static CD3DFont *font;

float4 getfontlookup(int c){ return ((float4*)font->gettexcoords())[c-32];}
int   getfontspacing(){return font->m_dwSpacing;}

void InitFont(LPDIRECT3DDEVICE9 device)
{
	assert(!font);
	font = new CD3DFont();
	font->InitDeviceObjects(device);
	assert(font);
}

void DeleteFont()
{
	assert(font);
	delete font;  // does the invalidate and release stuff
	font = NULL;
}

void DrawText(float x,float y,const char *s)
{
	font->DrawText(x,y,0xFFFFFF00,s);
}


//-----------------------------------------------------------

void PrintString(char *s,int x,int y){
	extern int Width;
	extern int Height;
	int rows = (Height-40)/14;
	int cols = Width/10;
	if(y>=rows){ y=rows-1;}
	if(y<0) { y+= rows;} // caller gives a negative y
	if(y<0) { y = 0;} // caller gives a too much negative y
	if(x<0) { x = cols+x-strlen(s)+1;}
	if(x+(int)strlen(s)>cols) { x=cols-strlen(s);}
	if(x<0) {x=0;}
 
	DrawText((float)x/(float)cols*Width,(float)y/(float)rows*(Height-40),s);
}
class PostedString {
public:
	char s[1024];
	float life;
	float lifestart;
	int x,y;
	PostedString(const char *_s,int _x,int _y,float _life=5.0);
	~PostedString();
	void Post();
};
Array<PostedString *> posts;
PostedString::PostedString(const char *_s,int _x,int _y,float _life) {
	strncpy_s(s,_s,1023);
	s[1023]='\0';
	lifestart = _life;
	life=_life;
	x=_x;
	y=_y;
	int i;
	i=posts.count;
	while(i--) {
		if(posts[i]->x==x && posts[i]->y==y) {
			delete(posts[i]);
		}
	}
	posts.Add(this);
}
PostedString::~PostedString() {
	posts.Remove(this);
}
void PostedString::Post() {
	PrintString(s,x,y);
	extern float DeltaT;
	life -= DeltaT;
	if(life<0) {delete this;}
}


void PostString(const char *_s,int _x,int _y,float _life){
	new PostedString(_s,_x,_y,_life);
}


//------------------------------------
// A simple Trace utility
//
class TraceString{
  public:
	String command;
	TraceString(const char *c);
	~TraceString();
};
Array<TraceString *>TraceStrings;
TraceString::TraceString(const char *c) {
	command=c;
	TraceStrings.Add(this);
}
TraceString::~TraceString() {
	TraceStrings.Remove(this);
}


void PostTraceStrings(){
	int k=0;
	int i;
	i=TraceStrings.count;
	while(i--) {
		char s[2048];
		sprintf_s<2048>(s,"%s: ",(const char *)TraceStrings[i]->command);
		String r = FuncInterp(TraceStrings[i]->command);
		if(!r[0] || r==" () " )
		{
			if(i)
			{
				Swap(TraceStrings[i],TraceStrings[i-1]);
			}
			continue;
		}
		strcat_s<2048>(s,r);
		assert(strlen(s)<2000);
		k++;
		PostString(s,0,-k,0);
	}
}
String trace(String s) {
	int i;
	for(i=0;i<TraceStrings.count;i++) {
		if(TraceStrings[i]->command == s) {
			return "already being traced";
		}
	}
	TraceString *ts=new TraceString(s);
	return ts->command;
}
EXPORTFUNC(trace);

char *untrace(const char *s) {
	int i=TraceStrings.count;
	int n=0;
	while(i--) {
		TraceString *ts=TraceStrings[i];
		if(!s || !*s || !strncmp(ts->command,s,strlen(s))) {
			delete ts;
			n++;
		}
	}
	static char buf[256];
	sprintf_s<256>(buf,"deleted %d trace commands",n);
	return buf;
}
EXPORTFUNC(untrace);


void RenderStrings() {
	int i;
	i=posts.count;
	while(i--) {
		posts[i]->Post();
	}
	PostTraceStrings();
}


