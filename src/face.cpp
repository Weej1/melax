//------------
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "vecmath.h"
#include "bsp.h"
#include "wingmesh.h"
#include "console.h"

#define FUZZYWIDTH (PAPERWIDTH*100)
extern float qsnap;
#define QUANTIZEDCHECK (qsnap*(1.0f/256.0f * 0.5f))



void texplanar(Face *face)
{
	float3 n(face->normal());
	if(fabsf(n.x)>fabsf(n.y) && fabsf(n.x)>fabsf(n.z)) {
		face->gu = float3(0,(n.x>0.0f)?1.0f:-1.0f,0);
		face->gv = float3(0,0,1);
	}
	else if(fabsf(n.y)>fabsf(n.z)) {
		face->gu = float3((n.y>0.0f)?-1.0f:1.0f,0,0);
		face->gv = float3(0,0,1);
	}
	else {
		face->gu = float3(1,0,0);
		face->gv = float3(0,(n.z>0.0f)?1.0f:-1.0f,0);
	}

}
void AssignTex(Face* face)
{
	texplanar(face);
}

void AssignTex(BSPNode *node,int matid){
	if(!node) {
		return;
	}
	int i;
	for(i=0;i<node->brep.count;i++) {
		Face *face = node->brep[i];
		face->matid = matid;
		AssignTex(face);
	}
	AssignTex(node->under,matid);
	AssignTex(node->over,matid);
}



float3 gradient(const float3 &v0,const float3 &v1,const float3 &v2,
				const float t0  ,const float t1  ,const float t2   ) {
	float3 e0 = v1-v0;
	float3 e1 = v2-v0;
	float  d0 = t1-t0;
	float  d1 = t2-t0;
	float3 pd = e1*d0 - e0*d1 ;
	if(pd == float3(0,0,0)){
		return float3 (0,0,1);
	}
	pd = normalize(pd);
	if(fabsf(d0)>fabsf(d1)) {
		e0 = e0 + pd * -dot(pd,e0);
		e0 = e0 * (1.0f/d0);
		return e0 * (1.0f/dot(e0,e0));;
	}
	// else
	assert(fabsf(d0)<=fabsf(d1));
	e1 = e1 + pd * -dot(pd,e1);
	e1 = e1 * (1.0f/d1);
	return e1 * (1.0f/dot(e1,e1));
}


void FaceExtractMatVals(Face *face,
	                    const float3  &v0,const float3  &v1,const float3  &v2,
	                    const float2 &t0,const float2 &t1,const float2 &t2) 
{
	face->gu = gradient(v0,v1,v2,t0.x,t1.x,t2.x);
	face->gv = gradient(v0,v1,v2,t0.y,t1.y,t2.y);
	face->ot.x =  t0.x - dot(v0,face->gu);
	face->ot.y =  t0.y - dot(v0,face->gv);
}

void texcylinder(Face *face)
{
	face->gv = float3(0,0,1);
	face->gu = safenormalize(cross(face->gv,face->normal()));
	float3 sa=face->vertex[maxdir(face->vertex,-face->gu)];
	float3 sb=face->vertex[maxdir(face->vertex, face->gu)];
	sa.z=0;
	sb.z=0;
	float ta = atan2f(sa.x,sa.y)/(2.0f*M_PIf);
	float tb = atan2f(sb.x,sb.y)/(2.0f*M_PIf);
	if(ta < tb-0.5f) ta+= 1.0f;
	if(tb < ta-0.5f) tb+= 1.0f;
	FaceExtractMatVals(face,sa,sb,sb+float3(0,0,1.0f),float2(ta,0),float2(tb,0),float2(tb,1));
//	float3 so=LineProject(sa,sb,face->normal * -face->dist);
	//float to = atan2f(face->normal.x,face->normal.y)/(2.0f*PI);
//	float to = ta + (tb-ta) * magnitude(so-sa)/magnitude(sb-sa);
//	face->gu *= (tb-ta)/magnitude(sa-sb);
//	face->gv *= magnitude(face->gu);
//	face->ot = float3(to,0,0);
}

float2 FaceTexCoord(Face *f,int i)
{
	return float2(f->ot.x+dot(float3(f->vertex[i]),f->gu),f->ot.y+dot(float3(f->vertex[i]),f->gv));
}

float2 FaceTexCoord(Face *f,const float3 &v)
{
	return float2(f->ot.x+dot(v,f->gu),f->ot.y+dot(v,f->gv));
}

Face *FaceNewQuad(const float3 &v0,const float3 &v1,const float3 &v2,const float3 &v3)
{
	Face *f = new Face();
	f->vertex.Add(v0);
	f->vertex.Add(v1);
	f->vertex.Add(v2);
	f->vertex.Add(v3);
	f->normal() = safenormalize( cross(v1-v0,v2-v1) + cross(v3-v2,v0-v3));
	f->dist() = -dot(f->normal(),(v0+v1+v2+v3)/4.0f);
	assert(!PlaneTest(f->normal(),f->dist(),v0,PAPERWIDTH)); // must be coplanar
	assert(!PlaneTest(f->normal(),f->dist(),v1,PAPERWIDTH));
	assert(!PlaneTest(f->normal(),f->dist(),v2,PAPERWIDTH));
	assert(!PlaneTest(f->normal(),f->dist(),v3,PAPERWIDTH));
	FaceExtractMatVals(f,v0,v1,v3,float2(0,0),float2(1,0),float2(0,1));
	f->gu = safenormalize(f->gu);
	f->gv = safenormalize(f->gv);
	f->ot = float3(0,0,0);
	return f;
}

Face *FaceNewTri(const float3 &v0,const float3 &v1,const float3 &v2)
{
	Face *f = new Face();
	f->vertex.Add(v0);
	f->vertex.Add(v1);
	f->vertex.Add(v2);
	f->normal() = safenormalize( cross(v1-v0,v2-v1));
	f->dist() = -dot(f->normal(),(v0+v1+v2)/3.0f);
	f->gu = safenormalize(v1-v0);
	f->gv = safenormalize(cross(float3(f->normal()),f->gu));
	return f;
}

Face *FaceNewTriTex(const float3 &v0,const float3 &v1,const float3 &v2,const float2 &t0,const float2 &t1,const float2 &t2)
{
	Face *f = new Face();
	f->vertex.Add(v0);
	f->vertex.Add(v1);
	f->vertex.Add(v2);
	f->normal() = safenormalize( cross(v1-v0,v2-v1));
	f->dist() = -dot(f->normal(),(v0+v1+v2)/3.0f);
	FaceExtractMatVals(f,v0,v1,v2,t0,t1,t2);
	return f;
}

Face *FaceDup(Face *face){
	int i;
	Face *newface = new Face();
	for(i=0;i<face->vertex.count;i++) {
		newface->vertex.Add(face->vertex[i]);
	}
	newface->normal() = face->normal();
	newface->dist()   = face->dist();
	newface->matid  = face->matid;
	newface->gu     = face->gu;
	newface->gv     = face->gv;
	newface->ot     = face->ot;
	return newface;	
}

float FaceArea(Face *face){
	float area=0;
	int i;
	for(i=0;i<face->vertex.count;i++) {
		int i1=(i+1)%face->vertex.count;
		int i2=(i+2)%face->vertex.count;
		float3 &vb=face->vertex[0];
		float3 &v1=face->vertex[i1];
		float3 &v2=face->vertex[i2];
		area += dot(face->normal(),cross(v1-vb,v2-v1)) / 2.0f;
	}
	return area;

}

float3 FaceCenter(Face *face)
{
	float3 centroid; // ok not really, but ill make a better routine later!
	int i;
	for(i=0;i<face->vertex.count;i++) {
		centroid = centroid + face->vertex[i];
	}
	return centroid/(float)face->vertex.count;
}

void FaceTranslate(Face *face,const float3 &offset){
	int i;
	for(i=0;i<face->vertex.count;i++) {
		face->vertex[i] += float3(offset);
	}
	face->dist() -= dot(face->normal(),float3(offset));
	face->ot.x   -= dot(offset,face->gu);
	face->ot.y   -= dot(offset,face->gv);
}
 
void FaceRotate(Face *face,const Quaternion &r){
	int i;
	for(i=0;i<face->vertex.count;i++) {
		face->vertex[i] = rotate(r,face->vertex[i]);
	}
	face->normal() = rotate(r,face->normal());
	face->gu = rotate(r,face->gu);
	face->gv = rotate(r,face->gv);
	face->ot = rotate(r,face->ot);
}

void FaceTranslate(Array<Face *> &faces,const float3 &offset) {
	int i;
	for(i=0;i<faces.count;i++) {
		FaceTranslate(faces[i],offset);
	}
}

int FaceClosestEdge(Face *face,const float3 &sample_point)
{
	// sample_point assumed to be an interior point
	int i;
	assert(face->vertex.count>=3);
	int closest=-1;
	float mindr;
	for(i=0;i<face->vertex.count;i++)
	{
		int i1 = (i+1)%face->vertex.count;
		float3 v0(face->vertex[i ]);
		float3 v1(face->vertex[i1]);
		float d = magnitude(LineProject(v0,v1,sample_point)-sample_point);
		if(closest==-1 || d<mindr)
		{
			mindr=d;
			closest = i;
		}
	}
	assert(closest>=0);
	return closest;
}


void FaceSlice(Face *face,const Plane &clip) {
	int count=0;
	for(int i=0;i<face->vertex.count;i++) {
		int i2=(i+1)%face->vertex.count;
		int   vf0 = PlaneTest(clip.normal(),clip.dist(),face->vertex[i]);
		int   vf2 = PlaneTest(clip.normal(),clip.dist(),face->vertex[i2]);
		if((vf0==OVER  && vf2==UNDER)||
		   (vf0==UNDER && vf2==OVER )  )
		{
			float3 vmid;
			vmid = PlaneLineIntersection(clip,face->vertex[i],face->vertex[i2]);
			assert(!PlaneTest(clip.normal(),clip.dist(),vmid));
			face->vertex.Insert(vmid,i2);
			i=0;	
			assert(count<2);
			count++;
		}
	}
}
Face *FaceClip(Face *face,const Plane &clip) {
	int flag = FaceSplitTest(face,clip.normal(),clip.dist());
	if(flag == UNDER || flag==COPLANAR) {
		return face;
	}
	if(flag == OVER) {
		delete face;
		return NULL;
	}
	assert(flag==SPLIT);
	FaceSlice(face,clip);
	Array<float3> tmp;
	int i;
	for(i=0;i<face->vertex.count;i++){
		if(PlaneTest(clip.normal(),clip.dist(),face->vertex[i])!=OVER) {
			tmp.Add(face->vertex[i]);
		}
	}
	face->vertex.count=0;
	for(i=0;i<tmp.count;i++){
		face->vertex.Add(tmp[i]);
	}
	return face;
}

int FaceSplitTest(Face *face,float3 splitnormal,float splitdist,float epsilon){
	int i;
	int flag=COPLANAR;
	for(i=0;i<face->vertex.count;i++) {
		flag |= PlaneTest(splitnormal,splitdist,face->vertex[i],epsilon);
	}
	return flag;
}

void  FaceSliceEdge(Face *face,int e0,BSPNode *n) {
	if(n->isleaf) {
		return;
	}
	int e1 = (e0+1) % face->vertex.count;
	assert(0);
	// fill this in!!!
}

int edgesplitcount=0;
static void FaceEdgeSplicer(Face *face,int vi0,BSPNode *n)
{
	// the face's edge starting from vertex vi0 is sliced by any incident hypeplane in the bsp
	if(n->isleaf) return;
	int vi1 = (vi0+1)%face->vertex.count;
	float3 v0 = face->vertex[vi0];
	float3 v1 = face->vertex[vi1];
	assert(magnitude(v0-v1) > QUANTIZEDCHECK );
	int f0 = PlaneTest(n->normal(),n->dist(),v0);
	int f1 = PlaneTest(n->normal(),n->dist(),v1);
	if(f0==COPLANAR && f1== COPLANAR)
	{
		// have to pass down both sides, but we have to make sure we do all subsegments generated by the first side
		int count = face->vertex.count;
		FaceEdgeSplicer(face,vi0,n->under);
		int k=vi0 + (face->vertex.count-count);
		while(k>=vi0)
		{
			FaceEdgeSplicer(face,k,n->over);
			k--;
		}
	}
	else if((f0|f1) == UNDER)
	{
		FaceEdgeSplicer(face,vi0,n->under);
	}
	else if((f0|f1) == OVER)
	{
		FaceEdgeSplicer(face,vi0,n->over);
	}
	else
	{
		edgesplitcount++;
		assert(magnitude(v0-v1) > QUANTIZEDCHECK);
		assert((f0|f1) == SPLIT);
		float3 vmid = PlaneLineIntersection(*n,v0,v1);
		assert(magnitude(vmid-v1) > QUANTIZEDCHECK);
		assert(magnitude(v0-vmid) > QUANTIZEDCHECK);
		int fmid = PlaneTest(n->normal(),n->dist(),vmid);
		assert(fmid==0);
		face->vertex.Insert(vmid,vi0+1);
		if(f0==UNDER)
		{
			FaceEdgeSplicer(face,vi0+1,n->over);
			FaceEdgeSplicer(face,vi0  ,n->under);
		}
		else
		{
			assert(f0==OVER);
			FaceEdgeSplicer(face,vi0+1,n->under);
			FaceEdgeSplicer(face,vi0  ,n->over);
		}
	}
	
}

int FaceSplitifyEdges(BSPNode *root)
{
	edgesplitcount=0;
	Array<BSPNode*>stack;
	stack.Add(root);
	while(stack.count)
	{
		BSPNode *n=stack.Pop();
		if(!n) continue;
		stack.Add(n->over);
		stack.Add(n->under);
		for(int i=0;i<n->brep.count;i++)
		{
			Face *face = n->brep[i];
			int j=face->vertex.count;
			while(j--)
			{
				FaceEdgeSplicer(face,j,root);
			}
		}
	}
	return edgesplitcount;
}

void FaceSanityCheck(Face *face)
{
	int i;
	for(i=0;i<face->vertex.count;i++)
	{
		int i1 = (i+1)%face->vertex.count;
		assert(magnitude(face->vertex[i1]-face->vertex[i])>QUANTIZEDCHECK);
	}
}
void FaceEmbed(BSPNode *node,Face *face) {
	assert(node);
	if(node->isleaf==OVER) {
		delete face;
		return;
	}
	if(node->isleaf==UNDER) {
		node->brep.Add(face);
		return;
	}
	int flag = FaceSplitTest(face,node->normal(),node->dist());
	if(flag==UNDER) {
		FaceEmbed(node->under,face);
		return;
	}
	if(flag==OVER) {
		FaceEmbed(node->over,face);
		return;
	}
	if(flag==COPLANAR) {
		FaceEmbed((dot(node->normal(),face->normal())>0)?node->under:node->over,face);
		return;
	}
	assert(flag==SPLIT);
	//FaceSanityCheck(face);
	Face *ovr = FaceDup(face);
	FaceClip(face,(Plane)(*node));
	FaceClip(ovr ,Plane(-node->normal(),-node->dist()));
	//FaceSanityCheck(face);
	//FaceSanityCheck(ovr);
	// FIXME:  add FaceSliceEdge calls here!
	FaceEmbed(node->under,face);
	FaceEmbed(node->over ,ovr );
}



static BSPNode *root=NULL;
/*static void GenerateFaces(BSPNode *n) {
	assert(root);
	assert(n);
	if(n->isleaf==UNDER) {
		return;
	}
	if(n->isleaf==OVER) {
		int i;
		assert(n->cell);
		for(i=0;i<n->cell->face.count;i++) {
			Face *face = FaceMakeBack(n->cell->face[i]);
			FaceEmbed(root,face);
		}
		return;
	}
	GenerateFaces(n->under);
	GenerateFaces(n->over);

}
*/


void GenerateFacesReverse(WingMesh *m,Array<Face*> &flist) 
{
	int i;
	for(i=0;i<m->faces.count;i++)
	{
		Face *f = new Face();
		f->normal() = -m->faces[i].normal();
		f->dist()   = -m->faces[i].dist();
		extern int currentmaterial;
		f->matid=currentmaterial;
		int e0 = m->fback[i];
		int e=e0;
		do {
			f->vertex.Add(m->verts[m->edges[e].v]);
			e = m->edges[e].prev;
		} while (e!=e0);
		AssignTex(f);
		flist.Add(f);
	}
}

void GenerateFaces(WingMesh *m,Array<Face*> &flist) 
{
	int i;
	for(i=0;i<m->faces.count;i++)
	{
		Face *f = new Face();
		f->normal() = m->faces[i].normal();
		f->dist()   = m->faces[i].dist();
		extern int currentmaterial;
		f->matid=currentmaterial;
		int e0 = m->fback[i];
		int e=e0;
		do {
			f->vertex.Add(m->verts[m->edges[e].v]);
			e = m->edges[e].next;
		} while (e!=e0);
		AssignTex(f);
		flist.Add(f);
	}
}

static void GenerateFaces(BSPNode *n) {
	int i;
	assert(root);
	assert(n);
	if(n->isleaf==UNDER) {
		return;
	}
	if(n->isleaf==OVER) {
		if(!n->convex) { return; }
		assert(n->convex);
		Array<Face *>flist;
		GenerateFacesReverse(n->convex,flist);
		for(i=0;i<flist.count;i++) {
			FaceEmbed(root,flist[i]);
		}
		return;
	}
	GenerateFaces(n->under);
	GenerateFaces(n->over);

}



static void ExtractMat(Face *face,Face *src) {
	int i;
	if(dot(face->normal(),src->normal())<0.95f) {
		return;
	}
	if(FaceSplitTest(face,src->normal(),src->dist(),PAPERWIDTH)!=COPLANAR) {
		return;
	}
	float3 interior;
	for(i=0;i<face->vertex.count;i++) {
		interior += face->vertex[i];
	}
	interior = interior * (1.0f/face->vertex.count);
	if(!HitCheckPoly(src->vertex.element,src->vertex.count,interior+face->normal(),interior-face->normal(),NULL,NULL)){
		return;
	}
	// src and face coincident
	face->matid = src->matid;
	face->gu    = src->gu;
	face->gv    = src->gv;
	face->ot    = src->ot;
}

static void ExtractMat(BSPNode *n,Face *poly) {
	int i;
	for(i=0;i<n->brep.count;i++) {
		ExtractMat(n->brep[i],poly);		
	}
	if(n->isleaf) {
		return;
	}
	int flag = FaceSplitTest(poly,n->normal(),n->dist());
	if(flag==COPLANAR) {
		ExtractMat((dot(n->normal(),poly->normal())>0)?n->under:n->over,poly);
		return;
	}
	if(flag & UNDER) {
		ExtractMat(n->under,poly);
	}
	if(flag & OVER) {
		ExtractMat(n->over ,poly);
	}
}

void BSPFreeBrep(BSPNode *r)
{
	if(!r) return;
	while(r->brep.count){delete r->brep.Pop();}
	BSPFreeBrep(r->over);
	BSPFreeBrep(r->under);
}

void BSPRipBrep(BSPNode *r,Array<Face*> &faces)
{
	if(!r) return;
	while(r->brep.count){faces.Add(r->brep.Pop());}
	BSPRipBrep(r->over,faces);
	BSPRipBrep(r->under,faces);
}
void BSPGetBrep(BSPNode *r,Array<Face*> &faces)
{
	if(!r) return;
	for(int i=0;i<r->brep.count;i++){faces.Add(r->brep[i]);}
	BSPGetBrep(r->over,faces);
	BSPGetBrep(r->under,faces);
}

void BSPMakeBrep(BSPNode *r)
{
	assert(!root);
	root=r;
	GenerateFaces(r);
	root=NULL;
}


Face* NeighboringEdgeFace;
int   NeighboringEdgeId;
BSPNode *NeighboringEdgeNode;
Face *NeighboringEdge(BSPNode *root,Face *face,int eid)
{
	float3 &v0 = face->vertex[eid];
	float3 &v1 = face->vertex[(eid+1)%face->vertex.count];
	assert(v0 != v1);
	float3 s = (v0+v1)/2.0f;
	float3 fnxe = cross((v1-v0),face->normal());  // points away from face's interior
	Plane p0 = *face;
	int f;
	while((f=PlaneTest(root->normal(),root->dist(),s)))
	{
		root=(f==OVER)?root->over:root->under;
		assert(!root->isleaf);
	}
	Array<BSPNode *> stack;
	stack.Add(root);
	while(stack.count)
	{
		BSPNode *n=stack.Pop();
		if(n->isleaf==OVER) continue;
		if(n->isleaf==UNDER)
		{
			int i,j;
			for(i=0;i<n->brep.count;i++)
			{
				Face *f = n->brep[i];
				if(f==face) continue;
				for(j=0;j<f->vertex.count;j++)
				{
					int j1 = (j+1)%f->vertex.count;
					if(magnitude(f->vertex[j]-v1)<0.001f && magnitude(f->vertex[j1]-v0)<0.001f)
					{
						NeighboringEdgeNode=n;
						NeighboringEdgeId=j;
						NeighboringEdgeFace=f;
						return f;
					}
				}
			}
			continue;
		}
		assert(!n->isleaf);
		f=PlaneTest(n->normal(),n->dist(),s);
		if(!(f&OVER))  stack.Add(n->under);
		if(!(f&UNDER)) stack.Add(n->over);
	}
	assert(0);
	return NULL;
}

void BSPMakeBrep(BSPNode *r,Array<Face*> &faces)
{
	int i;
	assert(!root);
	root=r;
	GenerateFaces(r);
	FaceSplitifyEdges(r);
	root=NULL;
	for(i=0;i<faces.count;i++) {
		ExtractMat(r,faces[i]);
	}
}

void BSPClipFace(BSPNode *n,Face* face,const float3 &position,Array<Face*> &under,Array<Face*> &over,Array<Face*> &created)
{
	if(n->isleaf==UNDER)
	{
		under.Add(face);
		return;
	}
	if(n->isleaf==OVER)
	{
		over.Add(face);
		return;
	}
	Plane plane(n->normal(),n->dist() + dot(position,n->normal()));
	int flag = FaceSplitTest(face,plane.normal(),plane.dist());
	if(flag == UNDER) {
		return BSPClipFace(n->under,face,position,under,over,created);
	}
	if(flag == OVER) {
		return BSPClipFace(n->over,face,position,under,over,created);
	}
	if(flag==COPLANAR)
	{
		return BSPClipFace((dot(n->normal(),face->normal())>0)?n->under:n->over,face,position,under,over,created);
	}
	assert(flag==SPLIT);
	
	Face *funder= new Face();
	Face *fover = new Face();
	created.Add(funder);
	created.Add(fover);
	fover->normal()=funder->normal()=face->normal();
	fover->dist()  = funder->dist() = face->dist();
	fover->gu   = funder->gu   = face->gu;
	fover->gv   = funder->gv   = face->gv;
	fover->ot   = funder->ot   = face->ot;
	fover->matid= funder->matid= face->matid;
	int i;
	for(i=0;i<face->vertex.count;i++){
		float3& vi = face->vertex[i];
		float3& vi1= face->vertex[(i+1)%face->vertex.count];
		int vf = PlaneTest(plane.normal(),plane.dist(),vi);
		int vf1= PlaneTest(plane.normal(),plane.dist(),vi1);
		if(vf==COPLANAR) 
		{
			funder->vertex.Add(vi);
			fover->vertex.Add(vi);
			continue;   // possible loop optimization
		}
		else if(vf==UNDER)
		{
			funder->vertex.Add(vi);
		}
		else 
		{
			assert(vf==OVER);
			fover->vertex.Add(vi);
		}
		if(vf != vf1 && vf !=COPLANAR && vf1 != COPLANAR)
		{
			float3 vmid = PlaneLineIntersection(plane.normal(),plane.dist(),vi,vi1);
			funder->vertex.Add(vmid);
			fover->vertex.Add(vmid);
		}
	}
	BSPClipFace(n->under,funder,position,under,over,created);
	BSPClipFace(n->over ,fover ,position,under,over,created);
}

void BSPClipFaces(BSPNode *bsp,Array<Face*> &faces,const float3 &position,Array<Face*> &under,Array<Face*> &over,Array<Face*> &created)
{
	for(int i=0;i<faces.count;i++)
		BSPClipFace(bsp,faces[i],position,under,over,created);
}