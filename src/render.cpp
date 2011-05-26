//  
//  Graphics/Rendering Engine
// 
// This module is graphics engine internal.  That means that
// these routines have no knowledge of any game stuff, bsp, or brushes.
//
// Basically the game has to tell which things it wants to render
// then the graphics engine determines things like
// what order and how many times to draw each mesh
// 
// One advantage over using a completely separate graphics engine is that this code
// uses the same basic libraries for math, i/o, console and object management.
// 
// Ideally we try to keep game code and graphics engine code as separate as possible,
// and strive to minimize the api and interactions between the two.
// However, when there is a variable that are needed by both, then 
// ownership should be given to the graphics engine.  
// Data duplication is avoided.
//

#include <Windows.h>
#include <d3dx9.h>
#include <assert.h>

#include "winsetup.h"
#include "mesh.h"
#include "d3dfont.h"
#include "console.h"
#include "camera.h"
#include "manipulator.h"
#include "light.h"
#include "vertex.h"
#include "material.h"

extern void DoShadows();
extern void RenderLines();  // use these mostly for debugging stuff 
extern void DrawHud();  // GUI stuff  
extern Camera camera;
extern int Width,Height;

extern float4x4& View;
extern float4x4& Proj;
extern float4x4& ViewProj;
extern float4x4& ViewInv;

int rendershadows=1;
EXPORTVAR(rendershadows);
int shadowsperlight=1;
EXPORTVAR(shadowsperlight);
int updatecube=0;
EXPORTVAR(updatecube);
float3 cubecop(0,0,2.0f);
EXPORTVAR(cubecop);
float alpha=0.3f;
EXPORTVAR(alpha);

IDirect3DCubeTexture9* CubeMap;
ID3DXRenderToEnvMap*   RenderToEnvMap;




static Array<Model *>    models;      //     models submitted for rendering this frame

Array<MeshBase*> regmeshes;
Array<MeshBase*> alphameshes;
Array<MeshBase*> shadowmeshes;

int meshintersectsphere(MeshBase *mesh,const float3& sphere_worldpos, float r)
{
	// solve in local space of the mesh.
	return BoxInside((float4(sphere_worldpos,1.0f)*MatrixRigidInverse(mesh->GetWorld())).xyz(), mesh->Bmin()-float3(r,r,r), mesh->Bmax()+float3(r,r,r));
}
float3 meshp;
EXPORTVAR(meshp);
Quaternion meshq;
EXPORTVAR(meshq);
void drawmesh(MeshBase* mesh)
{
	extern float4x4 World;
	World = mesh->GetWorld();
	meshq = quatfrommat<float>(float3x3(World.x.xyz(),World.y.xyz(),World.z.xyz()));
	meshp = World.w.xyz();
	extern void SetupMatrices();
	SetupMatrices();
	DataMesh *datamesh = dynamic_cast<DataMesh*>(mesh);
	if(datamesh)
	{
		extern void DrawDataMesh(DataMesh *datamesh);
		DrawDataMesh(datamesh);
	}
}

void drawmeshes(Array<MeshBase*> &meshes)
{
	for(int i=0;i<meshes.count;i++)
	{
		MeshBase *mesh=meshes[i];
		drawmesh(mesh);		
	}
}

int spherecull=1;
EXPORTVAR(spherecull);

int culledmeshcount=0;
EXPORTVAR(culledmeshcount);

void drawmeshes_spherecull(Array<MeshBase*> &meshes,const float3 &sphere_worldpos,float sphere_radius)
{
	for(int i=0;i<meshes.count;i++)
	{
		MeshBase *mesh=meshes[i];
		if(spherecull && !meshintersectsphere(mesh,sphere_worldpos,sphere_radius))
		{
			culledmeshcount++;
			continue;
		}
		drawmesh(mesh);		
	}
}

static int shadowmatid;
void categorizemesh(MeshBase *m)
{
	extern int hack_usealpha;
	int matid = m->GetMat();
	if(matid==shadowmatid)
	{
		shadowmeshes.Add(m);
	}
	else if( MaterialSpecial(matid)|| hack_usealpha)
	{
		alphameshes.Add(m);
	}
	else
	{
		regmeshes.Add(m);
	}
}

void groupmeshes()
{
	int i,j;

	for(i=0;i<models.count;i++)
	{
		for(j=0;j<models[i]->datameshes.count;j++)
		{
			DataMesh *m = models[i]->datameshes[j];
			m->model = models[i];
			categorizemesh((MeshBase*)m);
		}
	}
}






void ModelSetMatrix(Model* model, const float4x4 &matrix)
{
	model->modelmatrix = matrix;
}


void ModelRender(Model* model)
{
	models.Add(model);
	for(int j=0;j<model->datameshes.count;j++)
	{
		DataMesh *m = model->datameshes[j];
		assert(m->model==model);
		categorizemesh((MeshBase*)m);
	}
}

void DoneMeshRendering()
{
	// no longer carry pointers to temporary datameshes.
	models.count =0;
	regmeshes.count =0;
	alphameshes.count =0;
	shadowmeshes.count =0;
}






//--------------------------------------------------------------------------------------

void initcube()
{
	D3DXCreateCubeTexture( g_pd3dDevice, 256, 1,D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8  , D3DPOOL_DEFAULT, &CubeMap ) && VERIFY_RESULT;
	D3DXCreateRenderToEnvMap( g_pd3dDevice, 256, 1, D3DFMT_X8R8G8B8, TRUE, D3DFMT_D24S8, &RenderToEnvMap )  && VERIFY_RESULT;

}



float4x4 D3DUtil_GetCubeMapViewMatrix( int dwFace )
{
    float3 eye = cubecop;
    float3 LookDir;
    float3 UpDir;

    switch( dwFace )
    {
	case 0:// D3DCUBEMAP_FACE_POSITIVE_X:
            LookDir = float3( 1.0f, 0.0f, 0.0f );
            UpDir   = float3( 0.0f, -1.0f, 0.0f );
            break;
	case 1:// D3DCUBEMAP_FACE_NEGATIVE_X:
            LookDir = float3(-1.0f, 0.0f, 0.0f );
            UpDir   = float3( 0.0f, -1.0f, 0.0f );
            break;
	case 2://D3DCUBEMAP_FACE_POSITIVE_Y:
            LookDir = float3( 0.0f, 1.0f, 0.0f );
            UpDir   = float3( 0.0f, 0.0f, 1.0f );
            break;
	case 3://D3DCUBEMAP_FACE_NEGATIVE_Y:
            LookDir = float3( 0.0f,-1.0f, 0.0f );
            UpDir   = float3( 0.0f, 0.0f,-1.0f );
            break;
	case 4://D3DCUBEMAP_FACE_POSITIVE_Z:
            LookDir = float3( 0.0f, 0.0f, 1.0f );
            UpDir   = float3( 0.0f, -1.0f, 0.0f );
            break;
	case 5://D3DCUBEMAP_FACE_NEGATIVE_Z:
            LookDir = float3( 0.0f, 0.0f,-1.0f );
            UpDir   = float3( 0.0f, -1.0f, 0.0f );
            break;
    }
	LookDir *=-1.0f;  // this is so  i can use a right handed system.
    float4x4 m= MatrixLookAt(eye,eye+LookDir,UpDir);
	return m;
}


float3 clearcolor(0,0,0);
EXPORTVAR(clearcolor);

void UpdateCube()
{
	int i;
	Proj = MatrixPerspectiveFov(DegToRad(90.0f), 1, 0.5f , 200.0f);
	RenderToEnvMap->BeginCube( CubeMap ) &&VERIFY_RESULT;
	for(i=0;i<6;i++)
	{
		RenderToEnvMap->Face( (D3DCUBEMAP_FACES) i, 0 )&&VERIFY_RESULT;
		View = D3DUtil_GetCubeMapViewMatrix( (D3DCUBEMAP_FACES) i );
		ViewInv = Inverse(View);
		ViewProj = View*Proj;
		g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
							 D3DCOLOR_COLORVALUE(clearcolor.x,clearcolor.y,clearcolor.z,1.0f), 1.0f, 0L ); // color macro maps floating point channels (0.f to 1.f range) to D3DCOLOR
		//RenderAreaTest();

//		BrushRenderTest();



	}
	RenderToEnvMap->End(0)&&VERIFY_RESULT;
}

float3 ambient(0.2f,0.2f,0.3f);
EXPORTVAR(ambient);
float3 lightposn(0,0,5.0f);
EXPORTVAR(lightposn);
float lightradius=12.0f;
EXPORTVAR(lightradius);
float lightradiusinv=1.0f/lightradius;
EXPORTVAR(lightradiusinv);
float3 lightcolor(0.5f,0.5f,0.5f);
EXPORTVAR(lightcolor);




int scissortest=1;
EXPORTVAR(scissortest);
int stencilclearimmediate=1;  // flag to clear this immediately after used which could be best since scissors are set to dirty area
EXPORTVAR(stencilclearimmediate); 
int culledlightcount=0;
EXPORTVAR(culledlightcount);
int clippedcount=0;
EXPORTVAR(clippedcount);

void ScissorsRestore()
{
	// restore scissors
	RECT orig;  orig.top=orig.left=0; orig.bottom=Height; orig.right = Width;
	g_pd3dDevice->SetScissorRect(&orig);
	g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE ,FALSE);
}
int scissor_left;
int scissor_right;
int scissor_top;
int scissor_bottom; 
EXPORTVAR(scissor_left);
EXPORTVAR(scissor_right);
EXPORTVAR(scissor_top);
EXPORTVAR(scissor_bottom);
int scissordebug=0;
EXPORTVAR(scissordebug);
void ScissorsSetUp()
{
	// Set up scissors
	float3 tolight = lightposn-camera.position;
	float len = magnitude(tolight);
	//if(len < lightradius * 2.0f) 
	if(len < lightradius * 1.1f) 
	{
		ScissorsRestore();
		return;
	}
	float3 clocal = rotate(Inverse(camera.orientation), tolight) * (len-lightradius*lightradius/len);  // in camera space center of 2d circle where lightsphere is tangent to rays from cameraposition
	float3 c = camera.position + tolight * (1-lightradius*lightradius/(len*len));  // in world space center of 2d circle where lightsphere is tangent to rays from cameraposition
	float  r = lightradius/len*sqrtf(len*len-lightradius*lightradius); // radius of 2d circle with normal 'tolight', centered at c and tangent to lightsphere 
	float3 xvec = safenormalize(cross(tolight,camera.orientation.ydir())); // if working in camera space would have used float3(0,1.0f,0) 
	float3 yvec = safenormalize(cross(camera.orientation.xdir(),tolight)); // float3(1.0f,0,0) in cam
	if(scissordebug) r*=0.5; // this should chop area extents by 1/2
	float3 cr = c+xvec*r;
	float3 cl = c-xvec*r;
	float3 ct = c+yvec*r;
	float3 cb = c-yvec*r;

	clippedcount++;
	RECT rect;
	float3 cz = camera.orientation.zdir();
	scissor_left   = rect.left  = (dot(cl-camera.position,cz)>=0)? 0     : clamp((int) camera.MapToScreen(cl).x  ,0,Width);
	scissor_right  = rect.right = (dot(cr-camera.position,cz)>=0)? Width : clamp((int) camera.MapToScreen(cr).x  ,rect.left,Width);
	scissor_top    = rect.top   = (dot(ct-camera.position,cz)>=0)? 0     : clamp(Height-(int) camera.MapToScreen(ct).y  ,0,Height);
	scissor_bottom = rect.bottom= (dot(cb-camera.position,cz)>=0)? Height: clamp(Height-(int) camera.MapToScreen(cb).y  ,rect.top,Height);
	if(scissortest)g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE ,TRUE);
	g_pd3dDevice->SetScissorRect(&rect);
}


int renderclipit=1;
EXPORTVAR(renderclipit);
int renderclipitbox=1;
EXPORTVAR(renderclipitbox);
void ClipPlanesSetUp()
{
	float4x4 projinvtrans = MatrixTranspose(Inverse(Proj));
	float df = dot(-camera.orientation.zdir(),lightposn-camera.position) + lightradius ;  // debug tip: use fraction of radius to see hard boundary
	float4 pf= float4(0,0,1,df) * projinvtrans;
	g_pd3dDevice->SetClipPlane(0,(float*)&pf);
	float dn = dot(-camera.orientation.zdir(),lightposn-camera.position) - lightradius ;
	float4 pn= float4(0,0,-1,-dn) * projinvtrans;
	g_pd3dDevice->SetClipPlane(1,(float*)&pn);
	float4x4 viewprojinvtrans = MatrixTranspose(Inverse(ViewProj));
	if(renderclipitbox)
	  for(int i=0;i<6;i++)
	{
		float4 p;
		p.w = -lightradius;
		p.xyz()[i/2]=(i%2)?1.0f:-1.0f;
		p.w = -dot(p.xyz(),lightposn) + lightradius; // debug visualize with reduced: lightradius/2.0f;
		float4 pclip = p * viewprojinvtrans;
		g_pd3dDevice->SetClipPlane(i,(float*)&pclip);
	}
	if(renderclipit)
	{
		g_pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE ,renderclipitbox?63:3);
	}
}
void ClipPlanesRestore()
{
	g_pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE ,FALSE);
}


int render_floor_reflection=0;
EXPORTVAR(render_floor_reflection);
int render_window_reflection=0;
EXPORTVAR(render_window_reflection);


void RenderStuff()
{
	extern String techniqueoverride;
	culledlightcount=clippedcount=culledmeshcount=0;

    // Clear the backbuffer and the zbuffer
	g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
						 D3DCOLOR_COLORVALUE(clearcolor.x,clearcolor.y,clearcolor.z,1.0f), 1.0f, 0L ); // color macro maps floating point channels (0.f to 1.f range) to D3DCOLOR
	int stencildirty=0;

	g_pd3dDevice->BeginScene();// Begin the scene

	techniqueoverride="";

		techniqueoverride= "unlit";

		drawmeshes(regmeshes);

		techniqueoverride="lightpass";
		extern int lightingpasses;
		lightingpasses=1;
		for(int i=0;i<lights.count;i++) // for each light
		{
			if(!lights[i]->enable || lights[i]->radius<=0.0f) continue;
			if(stencildirty) g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_STENCIL,D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );
			stencildirty=0;
			lightposn   = lights[i]->position;
			lightradius = lights[i]->radius;
			lightradiusinv = 1.0f/lightradius;
			lightcolor  = lights[i]->color;
			if(!camera.SphereVisible(lightposn,lightradius)) {culledlightcount++;continue;}

			ScissorsSetUp();
			if(lights[i]->shadowcast)
			{
				drawmeshes_spherecull(shadowmeshes,lightposn,lightradius);// create shadow volumes for this light 
				stencildirty=1;
			}
			ClipPlanesSetUp();
			//g_pd3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,4) && VERIFY_RESULT;
			//g_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS,4) && VERIFY_RESULT;
			drawmeshes(regmeshes);

			//DWORD junk; g_pd3dDevice->GetRenderState(D3DRS_DEPTHBIAS,&junk);		
			//g_pd3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0) && VERIFY_RESULT;
			//g_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS,0);
			if(stencildirty && stencilclearimmediate)
			{
				g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_STENCIL,D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );
                stencildirty=0;
			}
			ClipPlanesRestore();
			ScissorsRestore();

		}
		techniqueoverride="";
		lightingpasses=0;


	extern int hack_usealpha;
	hack_usealpha++;
	drawmeshes(alphameshes);
	hack_usealpha--;

	RenderLines();

	RenderStrings();  // todo: rename to DrawStrings()


    g_pd3dDevice->EndScene(); 	// End the scene

}

void Render()
{
	PROFILE(render);	
	::shadowmatid = MaterialFind("extendvolume");
	if(updatecube)
	{
		if(!CubeMap) initcube();
		UpdateCube();
	}
	camera.RenderSetUp(); // in case updatecube() screwed stuff up.
	RenderStuff();

	extern void DoneMeshRendering();
	DoneMeshRendering(); // tells mesh module it can remove pointers to temp meshes.

	HRESULT hr = g_pd3dDevice->Present( NULL, NULL, NULL, NULL );     // Present the backbuffer contents to the display
	// if(hr==D3DERR_DEVICELOST)
}
