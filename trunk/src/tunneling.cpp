//
//  (c) Stan Melax 2007
// used for an example of CSG boolean ops (geomod)
//

#include <assert.h>
#include "camera.h"
#include "bsp.h"
#include "wingmesh.h"
#include "console.h"
#include "sound.h"
#include "material.h" // for texture name lookup
#include "brush.h"
#include "mesh.h"

int boolnumrandplanes=8;
EXPORTVAR(boolnumrandplanes);


static BSPNode *masherlist[10];
extern Brush *GetOperand();

void initbools() {
	int m;
	for(m=0;m<10;m++) {
		//Convex *mashbox = ConvexMakeCube(float3(1,1,1) * -0.5f,float3(1,1,1) *0.5f);
		WingMesh *mashbox = WingMeshCube(float3(1,1,1) * -0.5f,float3(1,1,1) *0.5f);
		int i;
		for(i=0;i<boolnumrandplanes;i++) {
			float3 rn((rand()%9-4)/4.0f,(rand()%9-4)/4.0f,(rand()%9-4)/4.0f);
			//float3 rn((rand()%5-2)/2.0f,(rand()%5-2)/2.0f,(rand()%5-2)/2.0f);
			rn = safenormalize(rn);
			float rd = -1.00f* (((rand()%10000)/10000.0f) * 0.75f + 0.25f );
			extern Plane Quantized(const Plane &p);
			WingMesh *tmp = WingMeshCrop(mashbox,Quantized(Plane(rn,rd)));
			delete mashbox;
			mashbox=tmp;
		}
		for(i=0;i<mashbox->verts.count;i++) {
			mashbox->verts[i]=mashbox->verts[i]*-1.0f;
		}
		for(i=0;i<mashbox->faces.count;i++) {
			mashbox->faces[i].dist() *= -1.0f;
		}
		extern void GenerateFaces(WingMesh *convex,Array<Face*> &flist);
		Array<Face*> flist;
		GenerateFaces(mashbox,flist);
		BSPNode *masher = BSPCompile(flist,WingMeshCube(30.51f*-float3(1,1,1),30.51f*float3(1,1,1)),UNDER);
		BSPDeriveConvex(masher,WingMeshCube(-30.51f*float3(1,1,1),30.51f*float3(1,1,1)));
		BSPMakeBrep(masher,flist);
		AssignTex(masher,MaterialFind("rockwallrm"));
		masherlist[m] = masher;
	}
}

int boolop_usehittexture=1;
EXPORTVAR(boolop_usehittexture);
int boolop_facesplitifyedges=1;
EXPORTVAR(boolop_facesplitifyedges);

Sound *shotsound;
char *boolop(const char *param) 
{
	Brush *operand = GetOperand(); 
	if(!operand) return "nothing to bool against";

	extern Camera camera;	
	float boxsize = 0.25;
	sscanf_s(param,"%f",&boxsize);
	if(boxsize<=0.0f) {
		return "bad size for bool-op";
	}
	if(!masherlist[0]) {
		initbools();
		shotsound = SoundCreate("shot.wav",5);
	}
	static int current=0;
	current++;
	current%=10;
	float3 seepoint;
	int h=HitCheck(operand->bsp,0,camera.position-operand->position,camera.position+camera.orientation.zdir()*-100.0f-operand->position,&seepoint);
	if(!h) return "didn't hit the operand";
	extern float qsnap;
	seepoint=Round(seepoint,qsnap);
	BSPNode *masher = BSPDup(masherlist[current]);
	BSPScale(masher,boxsize);
	BSPTranslate(masher,seepoint);
	extern Face* FaceHit(const float3& s);  // uses hitinfo from last hitcheck call
	Face *facehit;
	if(boolop_usehittexture && (facehit=FaceHit(seepoint)) )
	{
		Array<Face*> faces;
		BSPGetBrep(masher,faces);
		for(int i=0;i<faces.count;i++)
			faces[i]->matid = facehit->matid;
	}
	extern BSPNode *BSPIntersect(BSPNode *a,BSPNode *b);
	BSPNode *n=BSPIntersect(masher,operand->bsp);
	if(boolop_facesplitifyedges) FaceSplitifyEdges(operand->bsp);
	operand->Modified();  // geometry modified
	operand->model = CreateModel(operand->bsp,operand->shadowcast);
	SoundPlay(shotsound);
	//assert(n==area_bsp);
	return "OK";
}
EXPORTFUNC(boolop);

