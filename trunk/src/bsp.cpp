
//
//              BSP 
// 
//
//


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>


#define FUZZYWIDTH (PAPERWIDTH*100)

#include "vecmath.h"
#include "bsp.h"
#include "wingmesh.h"
#include "console.h"

int facetestlimit=50;
int allowaxial=1;
int solidbias =0;
int usevolcalc=1;
int allowhull =0;
BSPNode *currentbsp=NULL;


BBox VertexExtremes(const Array<Face*> &faces)
{
	BBox box;
	box.bmin=float3( FLT_MAX, FLT_MAX, FLT_MAX);
	box.bmax=float3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	int i,j;
	for(i=0;i<faces.count;i++)
	 for(j=0;j<faces[i]->vertex.count;j++)
	{
		box.bmin = VectorMin(box.bmin,float3(faces[i]->vertex[j]));
		box.bmax = VectorMax(box.bmax,float3(faces[i]->vertex[j]));
	}
	return box;
}

int bspnodecount=0;
EXPORTVAR(bspnodecount);
BSPNode::BSPNode(const float3 &n,float d):Plane(n,d){
	under = NULL;
	over  = NULL;
	isleaf= 0;
	convex= NULL;
	flag = 0;
	bspnodecount++;
}
BSPNode::BSPNode(const Plane &p): Plane(p)
{
	under=over=NULL;
	isleaf=0;
	flag=0;
	convex=NULL;
	bspnodecount--;
}

BSPNode::~BSPNode() {
	delete under;
	delete over;
	delete convex;
	while(brep.count) {
		delete brep.Pop();
	}
	bspnodecount--;
}

int BSPCount(BSPNode *n)
{
	if(!n) return 0;
	return 1+BSPCount(n->under)+BSPCount(n->over);
}

class SortPerm{
  public:
	int   i;
	float  v;
	Face *f;
	SortPerm(){}
	SortPerm(int _i,float _v,Face *_f):i(_i),v(_v),f(_f){}
};
static int FaceAreaCompare(const void *_a,const void *_b) {
	SortPerm *a = ((SortPerm *) _a); 
	SortPerm *b = ((SortPerm *) _b); 
	return ((a->v < b->v)?-1:1); 
}
void ReorderFaceArray(Array<Face *> &face){
	int i;
	Array<SortPerm> sp;
	for(i=0;i<face.count;i++) {
		sp.Add(SortPerm(i,FaceArea(face[i]),face[i]));
	}
	assert(sp.count==face.count);
	qsort(&sp.element[0],(unsigned long)sp.count,sizeof(SortPerm),FaceAreaCompare);
	for(i=0;i<face.count;i++) {
		face[i] = sp[i].f;
		if(i) {
			assert(FaceArea(face[i])>=FaceArea(face[i-1]));
		}
	}
}

static int count[4];

int PlaneTest(float3 normal,float dist,float3 v,float epsilon) {
	float a  = dot(v,normal)+dist;
	int   flag = (a>epsilon)?OVER:((a<-epsilon)?UNDER:COPLANAR);
	return flag;
}

int PlaneTest(const Plane &p, const float3 &v,float epsilon) {
	float a  = dot(v,p.normal())+p.dist();
	int   flag = (a>epsilon)?OVER:((a<-epsilon)?UNDER:COPLANAR);
	return flag;
}

float sumbboxdim(WingMesh *convex)
{
	float3 bmin,bmax;
	BoxLimits(convex->verts.element,convex->verts.count,bmin,bmax);
	return dot(float3(1,1,1),bmax-bmin);
}

float PlaneCost(Array<Face *> &inputfaces,const Plane &split,WingMesh *space,int onbrep)
{
	int i;
	count[COPLANAR] = 0;
	count[UNDER]    = 0;
	count[OVER]     = 0;
	count[SPLIT]    = 0;
	for(i=0;i<inputfaces.count;i++) {
		count[FaceSplitTest(inputfaces[i],split.normal(),split.dist(),FUZZYWIDTH)]++;
	}
	if(space->verts.count==0) {
		// The following formula isn't that great.
		// Better to use volume as well eh.
		return (float)(abs(count[OVER]-count[UNDER]) + count[SPLIT] - count[COPLANAR]);
	}
	float volumeover =(float)1.0;
	float volumeunder=(float)1.0;
	float volumetotal=WingMeshVolume(space);
	WingMesh *spaceunder= WingMeshCrop(space,Plane( split.normal(), split.dist()));
	WingMesh *spaceover = WingMeshCrop(space,Plane(-split.normal(),-split.dist()));
	if(usevolcalc==1)
	{
		volumeunder = WingMeshVolume(spaceunder);
		volumeover  = WingMeshVolume(spaceover );
	}
	else if (usevolcalc==2)
	{
		volumeunder = sumbboxdim(spaceunder);
		volumeover  = sumbboxdim(spaceover );
	}
	delete spaceover;
	delete spaceunder;
	assert(volumeover/volumetotal>=-0.01);
	assert(volumeunder/volumetotal>=-0.01);
	if(fabs((volumeover+volumeunder-volumetotal)/volumetotal)>0.01)	{
		// ok our volume equasions are starting to break down here
		// lets hope that we dont have too many polys to deal with at this point.
		volumetotal=volumeover+volumeunder;
	}
	if(solidbias && onbrep && count[OVER]==0 && count[SPLIT]==0)
	{
		return volumeunder;
	}
	return volumeover *powf(count[OVER] +1.5f*count[SPLIT],0.9f) + 
	       volumeunder*powf(count[UNDER]+1.5f*count[SPLIT],0.9f);
}



void DividePolys(const Plane &splitplane,Array<Face *> &inputfaces,
				 Array<Face *> &under,Array<Face *> &over,Array<Face *> &coplanar){
	int i=inputfaces.count;
	while(i--) {
		int flag = FaceSplitTest(inputfaces[i],splitplane.normal(),splitplane.dist(),FUZZYWIDTH);

		if(flag == OVER) {
			over.Add(inputfaces[i]);
		}
		else if(flag == UNDER) {
			under.Add(inputfaces[i]);
		}
		else if(flag == COPLANAR) {
			coplanar.Add(inputfaces[i]);
		}
		else {
			assert(flag == SPLIT);
			Face *und=FaceClip(FaceDup(inputfaces[i]),splitplane);
			Face *ovr=FaceClip(FaceDup(inputfaces[i]),Plane(-splitplane.normal(),-splitplane.dist()));
			assert(ovr);
			assert(und);
			over.Add(ovr);
			under.Add(und);
		}
	}
}



BSPNode * BSPCompile(Array<Face *> &inputfaces,WingMesh *space,int side) {
	int i;
	if(inputfaces.count==0) {
		BSPNode *node = new BSPNode();
		node->convex = space;
		node->isleaf=side; 
		return node;
	}
	Array<Face *> over;
	Array<Face *> under;
	Array<Face *> coplanar;
	ReorderFaceArray(inputfaces);
	// select partitioning plane
	float minval=FLT_MAX;
	Plane split(float3(0,0,0),0);

	if(inputfaces.count>1 && inputfaces.count <= allowhull)
	{
		int j;
		Array<int3> tris;
		Array<float3> verts;
		for(i=0;i<inputfaces.count;i++) 
			for(j=0;j<inputfaces[i]->vertex.count;j++)
				verts.AddUnique(inputfaces[i]->vertex[j]);
		calchull(verts.element,verts.count,tris.element,tris.count,50);
		for(i=0;i<tris.count;i++)
		{
			Plane p(TriNormal(verts[tris[i][0]],verts[tris[i][1]],verts[tris[i][2]]),0);
			if(p.normal()==float3(0,0,0))continue;
			p.dist() = -dot(verts[tris[i][0]],p.normal());
			float3 c = (verts[tris[i][0]]+verts[tris[i][1]]+verts[tris[i][2]]) /3.0f + p.normal() * 0.001f;
			if(solidbias && !currentbsp) continue; 
			if(solidbias && HitCheck(currentbsp,1,c,c,&c)) continue;
			if(WingMeshSplitTest(space,p)!= SPLIT) continue;
			float val = PlaneCost(inputfaces,p,space,1)*1.01f;
			if(val<minval) 
			{
				minval=val;
				split = p;
			}
		}
	}

	if(!solidbias || split.normal()==float3(0,0,0))
	{
		for(i=0;i<inputfaces.count && i<facetestlimit;i++) {
			float val=PlaneCost(inputfaces,*inputfaces[i],space,1);
			if(val<minval) {
				minval=val;
				split.normal() = inputfaces[i]->normal();
				split.dist()   = inputfaces[i]->dist();
			}
		}
		assert(split.normal() != float3(0,0,0));
		PlaneCost(inputfaces,split,space,1);
		if(allowaxial && inputfaces.count > 8) {
			// consider some other planes:
			for(i=0;i<inputfaces.count && i<facetestlimit;i++) {
				int j;
				for(j=0;j<inputfaces[i]->vertex.count;j++ ) {
					float val;
					if(allowaxial & (1<<0))
					{
						val = PlaneCost(inputfaces,Plane(float3(1,0,0),-inputfaces[i]->vertex[j].x),space,0);
						if(val<minval && (count[OVER]*count[UNDER]>0 || count[SPLIT]>0)) { 
							minval=val;
							split.normal() = float3(1,0,0);
							split.dist()   = -inputfaces[i]->vertex[j].x;
						}
					}
					if(allowaxial & (1<<1))
					{
						val = PlaneCost(inputfaces,Plane(float3(0,1,0),-inputfaces[i]->vertex[j].y),space,0);
						if(val<minval && (count[OVER]*count[UNDER]>0 || count[SPLIT]>0)) { 
							minval=val;
							split.normal() = float3(0,1,0);
							split.dist()   = -inputfaces[i]->vertex[j].y;
						}
					}
					if(allowaxial & (1<<2))
					{
						val = PlaneCost(inputfaces,Plane(float3(0,0,1),-inputfaces[i]->vertex[j].z),space,0);
						if(val<minval && (count[OVER]*count[UNDER]>0 || count[SPLIT]>0)) { 
							minval=val;
							split.normal() = float3(0,0,1);
							split.dist()   = -inputfaces[i]->vertex[j].z;
						}
					}
				}
			}
		}
	}
	// Divide the faces
	BSPNode *node=new BSPNode();
	node->normal() = split.normal();
	node->dist()   = split.dist();
	node->convex   = space;

	DividePolys(Plane(split.normal(),split.dist()),inputfaces,under,over,coplanar);

	for(i=0;i<over.count;i++) {
		for(int j=0;j<over[i]->vertex.count;j++) {
			assert(dot(node->normal(),over[i]->vertex[j])+node->dist() >= -FUZZYWIDTH);
		}
	}
	for(i=0;i<under.count;i++) {
		for(int j=0;j<under[i]->vertex.count;j++) {
			assert(dot(node->normal(),under[i]->vertex[j])+node->dist() <= FUZZYWIDTH);
		}
	}

	WingMesh* space_under=WingMeshCrop(space,Plane( split.normal(), split.dist()));
	WingMesh* space_over =WingMeshCrop(space,Plane(-split.normal(),-split.dist()));

	node->under = BSPCompile(under,space_under,UNDER);
	node->over  = BSPCompile(over ,space_over ,OVER );
	return node;
}

/*
void DeriveCells(BSPNode *node,Polyhedron *cell) {
	assert(node);
	node->cell = cell;
	cell->volume = Volume(cell);
	if(node->isleaf) {
		return;
	}
	// Under SubTree
	Polyhedron *under = PolyhedronDup(cell);
	Poly *sf = under->Crop(Plane(node->normal,node->dist));
	assert(node->under); 
	DeriveCells(node->under,under);
	// Over  SubTree
	Polyhedron *over  = PolyhedronDup(cell);
	over->Crop(Plane(-node->normal,-node->dist));
	assert(node->over );
	DeriveCells(node->over ,over );

}
*/

void BSPDeriveConvex(BSPNode *node,WingMesh *convex) {
	assert(node);
	if(convex && convex->edges.count && convex->verts.count)
	{
		assert(convex->verts.count);
		assert(convex->edges.count);
		assert(convex->faces.count);
	}
	else 
	{
		convex=NULL;
	}
	if(node->convex)  // clean up previous convex if there is one.  
	{
		delete node->convex;
		node->convex=NULL;
	}

	node->convex = convex;
	if(node->isleaf) {
		return;
	}
	// if we are "editing" a bsp then the current plane may become coplanar to one of its parents (boundary of convex) or outside of the current volume (outside the convex)
	WingMesh *cu=NULL;
	WingMesh *co=NULL;
	if(convex)
	{
		int f=WingMeshSplitTest(convex,*node);
		if(f==SPLIT)
		{
			cu = WingMeshCrop(convex,*node);
			co = WingMeshCrop(convex,Plane(-node->normal(),-node->dist()));
		}
		else if(f==OVER)
		{
			co = WingMeshDup(convex);
		}
		else if(f==UNDER)
		{
			cu = WingMeshDup(convex);
		}
		else
		{
			assert(0); // hunh? the 3d convex has 0 volume
		}
	}

	// Under SubTree
	assert(node->under);
	BSPDeriveConvex(node->under,cu); 
	// Over  SubTree
	assert(node->over );
	BSPDeriveConvex(node->over ,co);
}


void BSPGetSolidsx(BSPNode *bsp,Array<WingMesh*> &meshes)
{
	Array<BSPNode *> stack;
	stack.Add(bsp);
	while(stack.count)
	{
		BSPNode *n = stack.Pop();
		if(!n) continue;
		stack.Add(n->under);
		stack.Add(n->over);
		if(n->isleaf == UNDER)
		{
			meshes.Add(n->convex);
		}
	}
}

 
void BSPGetSolids(BSPNode *n,Array<WingMesh*> &meshes)
{
		if(!n) return;
		if(n->isleaf == UNDER)
		{
			meshes.Add(n->convex);
		}
		BSPGetSolids(n->under,meshes);
		BSPGetSolids(n->over,meshes);
	
}



void BSPTranslate(BSPNode *n,const float3 &_offset){
	int i;
	float3 offset(_offset);
	if(!n ) {
		return;
	}
	n->dist() = n->dist() - dot(n->normal(),offset);
	if(n->convex) {
		WingMeshTranslate(n->convex,_offset);
	}
	for(i=0;i<n->brep.count;i++) {
		FaceTranslate(n->brep[i],_offset);
	}
	BSPTranslate(n->under,_offset);
	BSPTranslate(n->over,_offset);
}


void BSPRotate(BSPNode *n,const Quaternion &r)
{
	int i;
	if(!n ) {
		return;
	}
	n->normal() = rotate(r,n->normal());
	if(n->convex) 
	{
		WingMeshRotate(n->convex,r);
	}
	for(i=0;i<n->brep.count;i++) {
		FaceRotate(n->brep[i],r);
	}
	BSPRotate(n->under,r);
	BSPRotate(n->over,r);
}


void BSPScale(BSPNode *n,float s){
	int i;
	if(!n) return;
	n->dist() = n->dist() * s;
	if(n->convex) {
		for(i=0;i<n->convex->verts.count;i++){
			n->convex->verts[i] *= s;
		}
		for(i=0;i<n->convex->faces.count;i++) {
			n->convex->faces[i].dist() *=s;
		}
	}
	for(i=0;i<n->brep.count;i++) {
		Face *f = n->brep[i];
		f->dist() *= s;
		// Scale(f->vertex,s);
		for(int j=0;j<f->vertex.count;j++){
			f->vertex[j] *= s;
		}
			
	}
	BSPScale(n->under,s);
	BSPScale(n->over,s);
}

void NegateFace(Face *f)
{
	int i;
	f->dist() *=-1.0f;
	f->normal()*=-1.0f;
	Array<float3> tmp;
	for(i=0;i<f->vertex.count;i++)
	{
		tmp.Add(f->vertex[f->vertex.count-i-1]);
	}
	for(i=0;i<f->vertex.count;i++)
	{
		f->vertex[i] = tmp[i];
	}
}

void NegateTreePlanes(BSPNode *n) 
{
	int i;
	if(!n) {
		return;
	}
	for(i=0;i<n->brep.count;i++)
	{
		NegateFace(n->brep[i]);
	}
	if(n->isleaf) {
		n->isleaf = (n->isleaf==UNDER)?OVER:UNDER;
		return;
	}
	n->normal() = -n->normal();
	n->dist()   = -n->dist();
	NegateTreePlanes(n->under);
	NegateTreePlanes(n->over);
	BSPNode *tmp = n->over;
	n->over  = n->under;
	n->under = tmp;
}


void NegateTree(BSPNode *root) 
{
	int i;
	Array<Face*> faces;
	NegateTreePlanes(root);
	BSPRipBrep(root,faces);
	for(i=0;i<faces.count;i++) {
		extern void FaceEmbed(BSPNode *node,Face *face);
		FaceEmbed(root,faces[i]);
	}
}

BSPNode *BSPDup(BSPNode *n) {
	int i;
	if(!n) {
		return NULL;
	}
	BSPNode* a = new BSPNode();
	a->normal() = n->normal();
	a->dist()   = n->dist();
	a->isleaf = n->isleaf;
	if(n->convex) {
		a->convex = WingMeshDup(n->convex);
	}
	for(i=0;i<n->brep.count;i++) {
		a->brep.Add(FaceDup(n->brep[i]));
	}
	a->under= BSPDup(n->under);
	a->over = BSPDup(n->over);
	return a;
}

int BSPFinite(BSPNode *bsp)
{
	return !HitCheck(bsp,1,float3(999999,9999999,999999),float3(999999,9999999,999999),NULL);
}

