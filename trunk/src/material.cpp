//
// The surface of a piece of geometry is typically assigned a material
// that specifies how it is shaded.  This is encapsulated in the Material class.
// A Material is associated with an effect, zero one or more textures, 
// some material specific properties and connections to other parameters.
//
//
// todo:
// To keep the design clean its nice to be able to work with 
// generated textures (rendertargets) similar to
// how we work with textures from files.
// On device reset I believe rendertargets will have to be freed and recreated
// consequently the d3dtexture pointer may change.  
// Unfortunately this calls for a layer of indirection.
// therefore, we have a texture class that contains this potentially changing pointer
// and materials point to these Texture objects.
//
//

#include <assert.h>
#include <d3dx9.h>
#include "material.h"
#include "winsetup.h"
#include "console.h"

float gloss=0.5f;
EXPORTVAR(gloss); // used by deferred renderer right now.
float fresnelpower=4.0f; // used by reflection shader.  need to pick which standard to use:  valve's artist friendly system or the techy scale,power,bias
EXPORTVAR(fresnelpower);
LPD3DXEFFECTPOOL g_effectpool=NULL;
//------------------ textures ----------------------

String texturepath("./ textures/ textures/");
String texturesuffix("jpg dds tga png");
EXPORTVAR(texturepath);

class Texture
{
public:
	String name;
	LPDIRECT3DTEXTURE9 d3dtexture;
	Texture(String _name,LPDIRECT3DTEXTURE9 _d3dtexture=NULL);
	~Texture();
	void Release();
	void CreateRenderTarget(D3DFORMAT format);
	int  isrendertarget;
};

static Hash<String,Texture*> Textures;

Texture::Texture(String _name,LPDIRECT3DTEXTURE9 _d3dtexture):name(_name),d3dtexture(_d3dtexture)
{
	Textures[name] = this;
}
void Texture::Release()
{
	if(d3dtexture) d3dtexture->Release()&&VERIFY_RESULT;
	d3dtexture=NULL;
}
Texture::~Texture()
{
	Release();
}
static Hash<String,LPDIRECT3DTEXTURE9> g_textures;
void Texture::CreateRenderTarget(D3DFORMAT format)
{
	Release();
	extern int Width,Height;
	g_pd3dDevice->CreateTexture(Width,Height,1,D3DUSAGE_RENDERTARGET,format,D3DPOOL_DEFAULT,&d3dtexture ,NULL)&& VERIFY_RESULT;
	g_textures[name]=d3dtexture;
	isrendertarget=1;
}

static Texture* GetTexture(String &texturename)
{
	HRESULT hr;
	Texture *t = Textures[texturename];
	if(t) return t;  // already loaded
	String f = filefind(texturename,texturepath,texturesuffix);
	assert(f!="");
	LPDIRECT3DTEXTURE9 d3dtexture;
	hr = D3DXCreateTextureFromFile(g_pd3dDevice,f,&d3dtexture);
	assert(!hr);
	assert(d3dtexture);
	t=new Texture(texturename,d3dtexture);
	return t;
}
LPDIRECT3DTEXTURE9 TextureFind(String name) // since mesh.cpp cant see textures directly, this routine lets it get textures directly (not just materials)
{
	Texture *t = Textures.Exists(name) ? GetTexture(name) : NULL;  // dont try to create it if it isn't there
	return (t) ? t->d3dtexture : NULL;
}

static Hash<String,LPDIRECT3DCUBETEXTURE9> cubetextures;
static LPDIRECT3DCUBETEXTURE9 GetCubeTexture(String &texturename)
{
	HRESULT hr;
	LPDIRECT3DCUBETEXTURE9 &texture = cubetextures[texturename];
	if(texture) return texture;
	String f = filefind(texturename,texturepath,".dds");
	assert(f!="");
	hr = D3DXCreateCubeTextureFromFile(g_pd3dDevice,f,&texture);
	assert(texture);
	return texture;
}

static LPDIRECT3DTEXTURE9 GetTextureHACK(String &texturename)
{
	LPDIRECT3DTEXTURE9 texture;
		D3DXCreateTextureFromFile(g_pd3dDevice,texturename,&texture) &&
		  D3DXCreateTextureFromFile(g_pd3dDevice,texturename+".jpg",&texture) && 
		  D3DXCreateTextureFromFile(g_pd3dDevice,texturename+".tga",&texture) && 
		  D3DXCreateTextureFromFile(g_pd3dDevice,texturename+".png",&texture) && VERIFY_RESULT;
		assert(texture);
	return texture;
}

int TexWidth(LPDIRECT3DTEXTURE9 t)
{
	IDirect3DSurface9 *surf;
	t->GetSurfaceLevel(0,&surf) && VERIFY_RESULT;
	D3DSURFACE_DESC surfacedesc;
	surf->GetDesc(&surfacedesc) && VERIFY_RESULT;
	surf->Release();
	return surfacedesc.Width;
}
int TexHeight(LPDIRECT3DTEXTURE9 t)
{
	IDirect3DSurface9 *surf;
	t->GetSurfaceLevel(0,&surf) && VERIFY_RESULT;
	D3DSURFACE_DESC surfacedesc;
	surf->GetDesc(&surfacedesc) && VERIFY_RESULT;
	surf->Release();
	return surfacedesc.Height;
}

unsigned char filtermap(unsigned char *image,int x,int y,int w,int h,int src_pixelwidth)
{
	// simple low pass filter to smooth out height changes.  This was suggested by papers on relief mapping
	int sum=0;
	for(int i=-1;i<=1;i++)
		for(int j=-1;j<=1;j++)
			sum += image[src_pixelwidth*  ( (x+w+i)%w + w*((y+j+h)%h) )];
	return (unsigned char)(sum/9);
}
String texmakenormalmap(String filename)
{
	LPDIRECT3DTEXTURE9 texture = GetTextureHACK(filename);
	if(!texture) return filename + " - no such texture ";
	IDirect3DSurface9 *src;
	texture->GetSurfaceLevel(0,&src) && VERIFY_RESULT;  // get level 0 surface so we know width and height
	D3DSURFACE_DESC srcdesc;
	src->GetDesc(&srcdesc) && VERIFY_RESULT;

	// create destination texture maps 
	LPDIRECT3DTEXTURE9 normalmap; // output of d3dx's routine for normal map calculation
	g_pd3dDevice->CreateTexture(srcdesc.Width,srcdesc.Height,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&normalmap ,NULL)&& VERIFY_RESULT;
	LPDIRECT3DTEXTURE9 extramap;  // final map we write out that includes normal and height information
	g_pd3dDevice->CreateTexture(srcdesc.Width,srcdesc.Height,0,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&extramap ,NULL)&& VERIFY_RESULT;
	D3DXComputeNormalMap(normalmap,texture,NULL,NULL,D3DX_CHANNEL_LUMINANCE,2.0f)&& VERIFY_RESULT;

	src->Release();  // release level 0 for now, 

	int levelcount = texture->GetLevelCount();

	for(int i=0;i<levelcount;i++)
	{
		texture->GetSurfaceLevel(i,&src) && VERIFY_RESULT;  // get level 0 surface so we know width and height
		D3DSURFACE_DESC srcdesc;
		src->GetDesc(&srcdesc) && VERIFY_RESULT;

		IDirect3DSurface9 *dst;
		normalmap->GetSurfaceLevel(i,&dst) && VERIFY_RESULT;
		D3DSURFACE_DESC dstdesc;
		dst->GetDesc(&dstdesc) && VERIFY_RESULT;

		IDirect3DSurface9 *ext;
		extramap->GetSurfaceLevel(i,&ext) && VERIFY_RESULT;
		D3DSURFACE_DESC extdesc;
		ext->GetDesc(&extdesc) && VERIFY_RESULT;

		D3DLOCKED_RECT srcdata,dstdata,extdata;
		src->LockRect(&srcdata , NULL , D3DLOCK_READONLY) && VERIFY_RESULT;
		dst->LockRect(&dstdata , NULL , D3DLOCK_READONLY ) && VERIFY_RESULT;
		ext->LockRect(&extdata , NULL , 0 ) && VERIFY_RESULT;
		int srcchannel = 0; // srcdesc.For
		int src_pixelwidth = (srcdesc.Format==D3DFMT_L8)? 1 : 4;
		for(unsigned int i=0;i< srcdesc.Width*srcdesc.Height ; i++ )
		{
			((unsigned char*)extdata.pBits)[4* i + 0] = ((unsigned char *)dstdata.pBits)[4 * i + 0];
			((unsigned char*)extdata.pBits)[4* i + 1] = 255-((unsigned char *)dstdata.pBits)[4 * i + 1];
			((unsigned char*)extdata.pBits)[4* i + 2] = ((unsigned char *)dstdata.pBits)[4 * i + 2];
			((unsigned char*)extdata.pBits)[4* i + 3] = filtermap((unsigned char *)srcdata.pBits,i%srcdesc.Width,i/srcdesc.Width,srcdesc.Width,srcdesc.Height,src_pixelwidth);
		}
		src->UnlockRect();
		dst->UnlockRect();
		ext->UnlockRect();
		src->Release();
		dst->Release();
		ext->Release();
	}
	filename = splitpathdir(filename) + splitpathfname(filename);  // remove extention if its there.
	if(filename.Length()>2&& filename[filename.Length()-2]=='_')
	{
		filename = String(filename,filename.Length()-2) ;
	}
	D3DXSaveTextureToFile(filename+"_n.dds",D3DXIFF_DDS ,extramap,NULL) &&VERIFY_RESULT;
	normalmap->Release();
	texture->Release();
	extramap->Release();
	return filename+"_n.dds created. ";
}
EXPORTFUNC(texmakenormalmap);


int texgrabcount=0;
int texgrab=0;
EXPORTVAR(texgrab);

String jacktexture(String name)
{
	if(!Textures.Exists(name))
	{
		return name + " no such texture";
	}
	LPDIRECT3DTEXTURE9 texture = Textures[name]->d3dtexture;
	IDirect3DSurface9 *src;
	texture->GetSurfaceLevel(0,&src) && VERIFY_RESULT;  // get level 0 surface so we know width and height
	D3DSURFACE_DESC srcdesc;
	src->GetDesc(&srcdesc) && VERIFY_RESULT;
	D3DLOCKED_RECT srcdata;
	src->LockRect(&srcdata , NULL , 0 );
	static int hack;
	hack = srcdesc.Format;
	if(srcdesc.Format != D3DFMT_X8R8G8B8  && srcdesc.Format != D3DFMT_A8R8G8B8  )
		return "not a texture format we understand";
	for(unsigned int i=0;i< srcdesc.Width*srcdesc.Height ; i++ )
	{
		((unsigned char*)srcdata.pBits)[4* i + 0] = 0;    // b
		((unsigned char*)srcdata.pBits)[4* i + 1] = 255;  // g
		((unsigned char*)srcdata.pBits)[4* i + 2] = 0;    // r
		((unsigned char*)srcdata.pBits)[4* i + 3] = 255;  // x
	}
	src->UnlockRect();
	src->Release();
	D3DXFilterTexture(texture,NULL,0,D3DX_DEFAULT);
	return name + " jacked";
}
EXPORTFUNC(jacktexture);


String jacktexture(String name,unsigned char *image,int w,int h)
{
	if(!Textures.Exists(name))
	{
		return name + " no such texture";
	}
	LPDIRECT3DTEXTURE9 texture = Textures[name]->d3dtexture;
	IDirect3DSurface9 *src;
	texture->GetSurfaceLevel(0,&src) && VERIFY_RESULT;  // get level 0 surface so we know width and height
	D3DSURFACE_DESC srcdesc;
	src->GetDesc(&srcdesc) && VERIFY_RESULT;
	D3DLOCKED_RECT srcdata;
	src->LockRect(&srcdata , NULL , 0 );
	static int hack;
	hack = srcdesc.Format;
	if(srcdesc.Format != D3DFMT_X8R8G8B8  && srcdesc.Format != D3DFMT_A8R8G8B8  )
		return "not a texture format we understand";
	for(unsigned int i=0;i< srcdesc.Height && i<h ; i++ )
	 for(unsigned int j=0;j< srcdesc.Width  && j<w ; j++ )
	  for(unsigned int k=0;k<4;k++) 
	{
		((unsigned char*)srcdata.pBits)[(srcdesc.Width*4*i) + 4* j + k] = image[w*i*4+4*j+k];    // b
	}
	src->UnlockRect();
	if(texgrabcount<texgrab)
	{
			D3DXSaveSurfaceToFile(String("texgrab_") + String(texgrabcount) + ".bmp" ,D3DXIFF_BMP,src,NULL,NULL);
			texgrabcount++;
	}
	src->Release();
	D3DXFilterTexture(texture,NULL,0,D3DX_DEFAULT);
	return name + " jacked";
}



String texcompileheightmaps(String s)
{
	String rv;
	int count=0;
	Array<String> files;
	filesearch("./textures/*_h.tga",files);
	filesearch("./textures/*_h.jpg",files);
	filesearch("./textures/*_h.png",files);
	for(int i=0;i<files.count;i++)
	{
		String f = files[i];
		if(f.Length()<7) continue; // need to have _h suffix and .xxx extention
		String n = String(f,f.Length()-6) + "_n.dds";
		if(fileexists(String("./textures/")+n)) continue ; // already have a normalmap for that heightmap
		rv << texmakenormalmap(String("./textures/") + f);
		count ++;
	}
	return String(count) + " of " + String(files.count) + " normalmaps created. " + rv;
}
EXPORTFUNC(texcompileheightmaps);


Texture *diffusetex;
Texture *normaltex;
Texture *positiontex;
Texture *consoletex;
IDirect3DSurface9 *defaultrendertarget; 

void rendertargetsrelease()
{
	diffusetex->Release();
	normaltex->Release();
	positiontex->Release();
	if(consoletex) consoletex->Release();
}
void createrendertargets()
{
	rendertargetsrelease();
	diffusetex->CreateRenderTarget(D3DFMT_A8R8G8B8);
	normaltex->CreateRenderTarget(D3DFMT_A8R8G8B8);
	positiontex->CreateRenderTarget(D3DFMT_R32F);
}
void inittargets()
{
	extern int Width,Height;
	if(!consoletex)
		consoletex  = new Texture("consoletex" );
	if(!diffusetex)
	{
		diffusetex  = new Texture("diffusetex" );
		normaltex   = new Texture("normaltex"  );
		positiontex = new Texture("positiontex");
	}
	//createrendertargets();
}

void SetRenderTarget(int slot,Texture *t)
{
	IDirect3DSurface9 *surf;
	t->d3dtexture->GetSurfaceLevel( 0, &surf) && VERIFY_RESULT;
	g_pd3dDevice->SetRenderTarget( slot,surf);
	surf->Release();
}


void deferred_settargets()
{
	assert(diffusetex);
	if(!diffusetex->d3dtexture) createrendertargets();
	g_pd3dDevice->GetRenderTarget(0,&defaultrendertarget); // so we can restore it later 
	SetRenderTarget(0,diffusetex);
	SetRenderTarget(1,normaltex);
	SetRenderTarget(2,positiontex);
}

void deferred_restoretargets()
{
	 assert(defaultrendertarget);
	g_pd3dDevice->SetRenderTarget(0,defaultrendertarget);
	g_pd3dDevice->SetRenderTarget(1,NULL);
	g_pd3dDevice->SetRenderTarget(2,NULL);
	defaultrendertarget->Release();
	defaultrendertarget=NULL;
}

float3 cmatcolor(0,0,0.25f);
EXPORTVAR(cmatcolor);
String consolemat(String s)
{
	if(!consoletex) consoletex  = new Texture("consoletex" );
	if(!consoletex->d3dtexture) consoletex->CreateRenderTarget(D3DFMT_A8R8G8B8);
	assert(consoletex->isrendertarget);

	g_pd3dDevice->GetRenderTarget(0,&defaultrendertarget); // so we can restore it later 
	SetRenderTarget(0,consoletex);
	g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
						 D3DCOLOR_COLORVALUE(cmatcolor.x,cmatcolor.y,cmatcolor.z,1.0f), 1.0f, 0L );

	g_pd3dDevice->BeginScene();// Begin the scene
	// ok render stuff here!!!
	extern void DrawText(float x,float y,const char *s);
	DrawText(0,0,s);
    g_pd3dDevice->EndScene(); 	// End the scene
	assert(defaultrendertarget);
	g_pd3dDevice->SetRenderTarget(0,defaultrendertarget);  // restore regular rendertarget
	defaultrendertarget->Release();
	defaultrendertarget=NULL;
	return "ok";
}
EXPORTFUNC(consolemat);


//
// The EffectParameter objects are the linkages between effect variables and variables/objects in the running application.
// For example if an effect file used the float3 'gravity', we use this system to automatically link that to the gravity variable in the game
// instead of writing code to upload each and every parameter.
//
struct BaseEffectParameter
{
	BaseEffectParameter(LPD3DXEFFECT _effect,D3DXHANDLE _handle,String _name){effect=_effect;handle=_handle;name=_name;}
	BaseEffectParameter(){}
	virtual void Load()=0;
	LPD3DXEFFECT effect;
	D3DXHANDLE handle;
	String     name;
};

template<class T>
struct EffectParameter : public BaseEffectParameter
{
	T var;
	EffectParameter(LPD3DXEFFECT _effect,D3DXHANDLE _handle,T _var,String _name):BaseEffectParameter(_effect,_handle,_name){var=_var;}
	EffectParameter(){}
	void Load();
};

template<>
void EffectParameter<Texture*>::Load()
{
	effect->SetTexture(handle,var->d3dtexture) &&VERIFY_RESULT;
}

int usecubedude=1;
EXPORTVAR(usecubedude);
template<>
void EffectParameter<LPDIRECT3DCUBETEXTURE9>::Load()
{
	extern IDirect3DCubeTexture9* CubeMap;
	if(usecubedude)
		effect->SetTexture(handle,CubeMap) &&VERIFY_RESULT;
	else
		effect->SetTexture(handle,var) &&VERIFY_RESULT;
}

template<>
void EffectParameter<float4x4*>::Load()
{
	effect->SetMatrix(handle,*var) &&VERIFY_RESULT;
}
template<class T>
void EffectParameter<T>::Load()
{
	effect->SetValue(handle,var,sizeof(*var)) &&VERIFY_RESULT;
}

class Material : public Entity
{
  public:
	String name;
	String technique;  // active technique for this materials effect.
	int    nid;  // numeric ID, which should match spot in Materials array
	//LPDIRECT3DTEXTURE9	diffusemap;
	//LPDIRECT3DTEXTURE9	normalmap;
	LPD3DXEFFECT		effect;
	Array<EffectParameter<Texture*> > textures;
	Array<EffectParameter<LPDIRECT3DCUBETEXTURE9> > cubetextures;
	Array<EffectParameter<float4x4 *> > matrices;
	Array<EffectParameter<float3 *> > vectors;
	Array<EffectParameter<float4 *> > vec4s;
	Array<EffectParameter<float  *> > floats;
	int special;
	Material(String _name,int _special);
	~Material();
	float3 emissive;
	float  gloss;
	float reflectivity;
	float3 fresnel; // scale bias power 
	float diffusecoef;
	int   draw; // flag to indicate if this material should be rendered.
	int lightpasses; // flag to allow multiple passes for multiple lights default is '1' or on. 
};



Array<Material*> Materials;
Hash<String,LPD3DXEFFECT> Effects;

Texture *foo;
String createfoo(String)
{
	if(foo) return "already created";
	foo=new Texture("foo");
	foo->CreateRenderTarget(D3DFMT_A16B16G16R16F);
	SetRenderTarget(3,foo);
	return "created foo";
}
EXPORTFUNC(createfoo);
String releasefoo(String)
{
	if(!foo) return "no foo you fool";
	g_pd3dDevice->SetRenderTarget(3,NULL);
	Effects["effects/deferredlighting.fx"]->SetTexture("diffusetex",foo->d3dtexture);
	Effects["effects/deferredlighting.fx"]->SetTexture("diffusetex",NULL);
	foo->Release();
	delete foo;
	foo=NULL;
	return "ok";
}
EXPORTFUNC(releasefoo);


void effectlostdevice()
{
	int i;
	for(i=0;i<Effects.slots_count;i++)
	{
		if(Effects.slots[i].used)
		{
			Effects.slots[i].value->OnLostDevice() && VERIFY_RESULT;
		}
	}
}
void effectresetdevice()
{
	int i;
	for(i=0;i<Effects.slots_count;i++)
	{
		if(Effects.slots[i].used)
		{
			Effects.slots[i].value->OnResetDevice();
		}
	}
}

const char *MaterialGetName(int matid)
{
	return(Materials[matid]->name);
}

int MaterialFind(const char *_matname)
{
	String matname = ToLower(_matname);
	int i= Materials.count;
	while(i--)
	{
		if(Materials[i]->name == matname)
		{
			return i;
		}
	}
	return 0;
}
String materialfind(String matname)
{
	return String(MaterialFind(matname));
}
EXPORTFUNC(materialfind);



LDECLARE(Material,technique);
LDECLARE(Material,emissive);
LDECLARE(Material,gloss);
LDECLARE(Material,reflectivity);
LDECLARE(Material,fresnel);
LDECLARE(Material,diffusecoef);
LDECLARE(Material,draw);
LDECLARE(Material,lightpasses);

Material::Material(String _name,int _special):Entity(String("material_")+_name),name(ToLower(_name)),special(_special)
{
	gloss = 0.5f;  // default value.
	reflectivity = 0.1f; 
	fresnel = float3(0.1f,0.0f,4.0f); // scale bias power
	diffusecoef = 1.0f;
	LEXPOSEOBJECT(Material,this->id);
	//EXPOSEMEMBER(technique);
	//EXPOSEMEMBER(emissive);
	//EXPOSEMEMBER(gloss);
	//EXPOSEMEMBER(reflectivity);
	//EXPOSEMEMBER(fresnel);
	//EXPOSEMEMBER(diffusecoef);
	//EXPOSEMEMBER(draw);
	//EXPOSEMEMBER(lightpasses);
	draw= 1;	
	lightpasses =1;
	technique = "t0";
	nid = Materials.count;
	Materials.Add(this);
}
Material::~Material()
{
	Materials.Remove(this);
}

static String GetMaterialName(xmlNode *elem)
{
	assert(elem->tag == "materialdef");
	String	mn("");
	if(elem->hasAttribute("name"))
	{
		mn=elem->attribute("name");
	}
	else if(elem->Child("name"))
	{
		mn = elem->Child("name")->body;
	}
	else if(elem->Child("diffusemap"))
	{
		mn = elem->Child("diffusemap")->body;
	}
	return mn;
}

int MaterialNextRegular(int i)
{
	do i=(i+1)%Materials.count; while(Materials[i]->special);
	return i;
}

//String effectpath("effects");
String effectpath("effects"); 
EXPORTVAR(effectpath);

Material *MakeMaterial(xmlNode *elem)
{
	int i;
	assert(elem->tag == "materialdef");
	
	String	mn = GetMaterialName(elem);
	if(mn=="") mn = String(Materials.count);
	
	Material *m = new Material(mn,(elem->Child("special"))?1:0);
	String en = effectpath + "/" + "default.fx";
	assert(!elem->hasAttribute("effect"));  // old way: en = String("effects/") + elem->attributes["effect"];
	if(elem->Child("effect"))
	{
		en = effectpath + "/" + elem->Child("effect")->body;
	}
	LPD3DXEFFECT &effect = Effects[en];
	if(!effect)
	{
		LPD3DXBUFFER error;
		assert(effect==NULL);
		//HRESULT hr = ; // && VERIFY_RESULT
		if(D3DXCreateEffectFromFile(g_pd3dDevice,en+"o",NULL,NULL,0,g_effectpool,&effect,&error) && 
		   D3DXCreateEffectFromFile(g_pd3dDevice,en    ,NULL,NULL,0,g_effectpool,&effect,&error)) 
		{
			char * buf= (char*)error->GetBufferPointer();
			int i=3;
			i++;
		}
		assert(effect);
	}
	m->effect = effect;
	assert(m->effect);
	assert(m->effect == Effects[en]);
	if(elem->hasAttribute("lightingpasses"))
		m->lightpasses = elem->attribute("lightingpasses").Asint() ; 

	D3DXEFFECT_DESC effect_desc;
	effect->GetDesc(&effect_desc) && VERIFY_RESULT;
	for(i=0;i<(int)effect_desc.Parameters;i++)
	{
		D3DXHANDLE ph;
		D3DXPARAMETER_DESC pdesc;
		ph=effect->GetParameter(NULL,i);  // useful to put breakpoint here
		effect->GetParameterDesc(ph,&pdesc) && VERIFY_RESULT;
		int hasbody = (elem->Child(pdesc.Name))?1:0;
		String body = (elem->Child(pdesc.Name))?elem->Child(pdesc.Name)->body : "";  
		if(pdesc.Type == D3DXPT_TEXTURE)
		{
			Texture *t;
			// use parameter name itself if the material file doesn't have a texture name for this parameter.  eg diffusemap.jpg
			String texname = (elem->Child(pdesc.Name))?elem->Child(pdesc.Name)->body : String(pdesc.Name);  
			
			String texstring = texname;
			if(PopFirstWord(texstring)=="computenormalmap")
			{
				LPDIRECT3DTEXTURE9 d;
				String tn = PopFirstWord(texstring);
				t=GetTexture(tn);
				d=GetTextureHACK(tn);
				D3DXComputeNormalMap(d,t->d3dtexture,NULL,NULL,D3DX_CHANNEL_LUMINANCE,2.0f)&& VERIFY_RESULT;
				D3DXSaveTextureToFile(tn+".dds",D3DXIFF_DDS ,d,NULL) &&VERIFY_RESULT;
			}
			else
				t = GetTexture(texname);
			m->textures.Add(EffectParameter<Texture*>(effect,ph,t,texname));
		}
		if(pdesc.Type== D3DXPT_TEXTURECUBE)
		{
			String &texname = elem->Child(pdesc.Name)->body;
			IDirect3DCubeTexture9* t=NULL;
			t = GetCubeTexture(texname);
			m->cubetextures.Add(EffectParameter<LPDIRECT3DCUBETEXTURE9>(effect,ph,t,texname));
		}
		if(pdesc.Class==D3DXPC_MATRIX_ROWS && pdesc.Type==D3DXPT_FLOAT)
		{
			String cs;
			void *data = LVarLookup(pdesc.Name,cs);
			assert(data);
			//Reference r = GetVar(pdesc.Name);
			//assert(r.data==data);
			assert(cs=="float4x4");
			if(data) m->matrices.Add(EffectParameter<float4x4*>(effect,ph, (float4x4*)data,pdesc.Name));
		}
		if(pdesc.Class==D3DXPC_VECTOR && pdesc.Type==D3DXPT_FLOAT)
		{
			String cs;
			void *data = deref(m,"Material",pdesc.Name,cs);
			if(!data) data = LVarLookup(pdesc.Name,cs);
			//Reference r = m->hash.Exists(pdesc.Name) ? m->hash[pdesc.Name] : GetVar(pdesc.Name);
			//assert(data==r.data);
			if(data && cs=="float3" )
			{
				m->vectors.Add(EffectParameter<float3*>(effect,ph, (float3*)data,pdesc.Name));
				if(hasbody)
					StringIter(body) >> *((float3*)data) ;
			}
			if(data && cs=="float4" )
			{
				m->vec4s.Add(EffectParameter<float4*>(effect,ph, (float4*)data,pdesc.Name));
			}
		}
		if(pdesc.Class==D3DXPC_SCALAR && pdesc.Type==D3DXPT_FLOAT)
		{
			String cs;
			void *data = deref(m,"Material",pdesc.Name,cs);
			if(!data) data = LVarLookup(pdesc.Name,cs);
			//Reference r = m->hash.Exists(pdesc.Name) ? m->hash[pdesc.Name] : GetVar(pdesc.Name);
			//assert(data==r.data);
			assert(!data || cs=="float");
			if(data) 
			{
				m->floats.Add(EffectParameter<float*>(effect,ph, (float*)data,pdesc.Name));
				if(hasbody)
					StringIter(body) >> *((float*)data) ;
			}
		}
	}
	for(i=0;i<elem->children.count;i++)
	{
		xmlNode *child = elem->children[i];
//		if(ObjectImportMember(m,child)) continue;
//		D3DXHANDLE h= effect->GetParameterByName(NULL,child->attribute("param"));
//		if(!h) continue;
	}
	return m;
}


Material *MaterialGet(xmlNode *matelem)
{
	assert(matelem->tag == "materialdef");
	int matid=MaterialFind(GetMaterialName(matelem));
	Material *m= (matid>0)?Materials[matid]:NULL;
	if(!m) m = MakeMaterial(matelem);
	return m;
}

void MakeMaterialList(xmlNode *elem,Array<Material*> &mlist)
{
	int i;
	assert(elem->tag == "materialdeflist");
	mlist.count=0;
	for(i=0;i<elem->children.count;i++)
	{
		xmlNode *matelem = elem->children[i];
		assert(matelem->tag == "materialdef");
		Material *m= MaterialGet(matelem);
		mlist.Add(m);
	}
}

Material *LoadMatFile(String fname)
{
	xmlNode *m = XMLParseFile(String("textures/") + fname);
	assert(m);
	assert(m->tag=="materialdef");
	assert(!m->hasAttribute("name")||!strncmp(m->attribute("name"),fname,strlen(m->attribute("name"))));
	m->attribute("name")=PopFirstWord(fname,".");
	return MakeMaterial(m);
}

void LoadMaterial(char *fname)
{
	if(!g_effectpool)
	{
		D3DXCreateEffectPool(&g_effectpool) && VERIFY_RESULT;
		inittargets(); // initial  rendertargets for deferred lighting, this lets materials link up with these textures
	}
	assert(g_effectpool);
	LoadMatFile(fname);
}

void LoadMaterials(void*)
{
	// Automatically generate materials based on the contents of the ./textures/ directory
	// First create any material based on a .mat/xml material file.
	// Next, just make a typical diffuse (possibly bumped) material from each remaining image file. 
	// ignore images with a name that already corresponds to a material name and ignore 
	// normal maps and cube maps according to filename '_b' and '_c' suffixes (such as "_b.jpg" and "_c.dds")
	// prefer to put this into startup cfg:  
	texcompileheightmaps("");
	if(!g_effectpool)
	{
		D3DXCreateEffectPool(&g_effectpool) && VERIFY_RESULT;
		inittargets(); // initial  rendertargets for deferred lighting, this lets materials link up with these textures

	}
	assert(g_effectpool);
	int i;
	//assert(Materials.count==0);
	//LoadMatFile("default.mat");
	//assert(Materials.count);
	Array<String> files;
	files.Add("default.mat");  // must exist
	filesearch("./textures/*.mat",files);
	//filesearch("./textures/*.xml",files);
	for(i=0;i<files.count;i++)
	{
		if(files[i]=="default.mat") continue;
		LoadMatFile(files[i]);
	}
	files.count =0;
	filesearch("./textures/*.dds",files);
	filesearch("./textures/*.jpg",files);
	filesearch("./textures/*.tga",files);
	filesearch("./textures/*.png",files);
	for(i=0;i<files.count;i++)
	{
		String ext = files[i];
		String fname = PopFirstWord(ext,".");
		if(fname=="" || ext =="") continue;
		if(fname=="default") continue;
		if(MaterialFind(fname)) continue;
		if(fname.Length()>2 && fname[fname.Length()-2]=='_' && IsOneOf(fname[fname.Length()-1],"bBcCnNhH")) continue;
		xmlNode e("materialdef");
		xmlNode *d = new xmlNode("diffusemap",&e);
		d->body = fname;
		int suffixlen = (fname.Length()>2 && fname[fname.Length()-2]=='_'  && fname[fname.Length()-1]=='d') ? 2 : 0 ;
		String bmap = String(fname,fname.Length()-suffixlen) + "_n";
		if(filefind(bmap,texturepath,texturesuffix)!="")
		{
			xmlNode *b = new xmlNode("normalmap",&e);
			b->body = bmap;
			xmlNode *fx = new xmlNode("effect",&e);
			fx->body = "parallax.fx";
		}
		MakeMaterial(&e);
	}
}

int hack_usealpha=0;
EXPORTVAR(hack_usealpha);


void MaterialUnload(int k)
{
	Material *mat=Materials[k];
	for(int i=0;i<mat->textures.count;i++) {
		if(mat->textures[i].var->isrendertarget)
			mat->textures[i].effect->SetTexture(mat->textures[i].handle,NULL);
	}
}

LPD3DXEFFECT MaterialUpload(Material *mat)
{
	int i;
	for(i=0;i<mat->textures.count;i++) {
		mat->textures[i].Load();
	}
	for(i=0;i<mat->cubetextures.count;i++) {
		mat->cubetextures[i].Load();
	}
	for(i=0;i<mat->matrices.count;i++) {
		mat->matrices[i].Load();
	}
	for(i=0;i<mat->vectors.count;i++) {
		mat->vectors[i].Load();
	}
	for(i=0;i<mat->vec4s.count;i++) {
		mat->vec4s[i].Load();
	}
	for(i=0;i<mat->floats.count;i++) {
		mat->floats[i].Load();
	}

	return mat->effect;
}

String techniqueoverride;
int lightingpasses=0;
LPD3DXEFFECT MaterialSetup(Material *mat)
{
	if(!mat->draw) return NULL;  // this material set to not be drawn
	if(lightingpasses && !mat->lightpasses) return NULL;  // this material set to not be drawn
	MaterialUpload(mat);
	if(hack_usealpha && D3D_OK==mat->effect->SetTechnique("alphatechnique")) 
	{
		return mat->effect;
	}
	if(techniqueoverride!="" && D3D_OK==mat->effect->SetTechnique(techniqueoverride) )
	{
		return mat->effect;
	}
	HRESULT hr;
	hr = mat->effect->SetTechnique((const char *)mat->technique);
	if(hr != D3D_OK)
	{
		// invalid technique, find a valid one
		D3DXHANDLE htechnique=NULL;
		D3DXTECHNIQUE_DESC tdesc;
		tdesc.Name=NULL;
		hr = mat->effect->FindNextValidTechnique(NULL,&htechnique);
		assert(hr==D3D_OK);
		hr = mat->effect->GetTechniqueDesc(htechnique,&tdesc);
		assert(hr==D3D_OK);
		assert(tdesc.Name);
		mat->technique = tdesc.Name; 
		hr = mat->effect->SetTechnique((const char *)mat->technique);
		assert(hr==D3D_OK);
	}
	return mat->effect;
}

LPD3DXEFFECT MaterialUpload(int k)
{
	return MaterialUpload(Materials[k%Materials.count]);
}


LPD3DXEFFECT MaterialSetup(int k)
{
	return MaterialSetup(Materials[k%Materials.count]);
}

int MaterialSpecial(int k)
{
	return Materials[k]->special;
}
