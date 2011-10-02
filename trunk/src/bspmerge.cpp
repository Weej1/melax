//
//      BSP
//  (c) Stan Melax 1998-2007
//  see file bsp.h 
//
// this module provides csg boolean operation (geomod) support
//
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>

#include "vecmath.h"
#include "bsp.h"
#include "wingmesh.h"

static int fusenodes=0;

BSPNode *BSPClean(BSPNode *n)
{
	// removes empty cells.
	if(n->convex == NULL) 
	{
		delete n;
		return NULL;
	}
	if(n->isleaf)
	{
		n->normal() = float3(0,0,0); 
		n->dist()=0;
		assert(n->over==NULL);
		assert(n->under==NULL);
		return n;
	}
	n->under = BSPClean(n->under);
	n->over  = BSPClean(n->over );
	if(!n->over) 
	{
		BSPNode *r= n->under; 
		n->under=NULL;
		delete n;
		return r;
	}
	if(!n->under)
	{
		BSPNode *r= n->over; 
		n->over=NULL;
		delete n;
		return r;
	}
	if(n->over->isleaf && n->over->isleaf==n->under->isleaf)  
	{
		// prune when both children are leaf cells of the same type
		n->isleaf = n->over->isleaf;
		delete n->over;
		delete n->under;
		n->over=n->under=NULL;
		n->normal()=float3(0,0,0);
		n->dist()  =0;
	}
	assert(n->convex);
	return n;
}

void BSPPartition(BSPNode *n,const Plane &p,BSPNode * &nodeunder, BSPNode * &nodeover) {
	nodeunder=NULL;
	nodeover =NULL;
	if(!n) {
		return;
	}
//	assert(n->cell);
	int flag;
	//flag = SplitTest(n->cell,p);
	//assert(flag==SplitTest(*n->convex,p));
	flag=WingMeshSplitTest(n->convex,p);
	if(flag == UNDER) {
		nodeunder = n;
		return;
	}
	if(flag==OVER) {
		nodeover = n;
		return;
	}
	assert(flag==SPLIT);
//	Polyhedron *cellover  = PolyhedronDup(n->cell);
//	Polyhedron *cellunder = PolyhedronDup(n->cell);
//	cellunder->Crop(p);
//	cellover->Crop(Plane(-p.normal,-p.dist));
	nodeunder = new BSPNode(n->normal(),n->dist());
	nodeover  = new BSPNode(n->normal(),n->dist());
	nodeunder->isleaf = n->isleaf;
	nodeover->isleaf = n->isleaf;
//	nodeunder->cell= cellunder;
//	nodeover->cell = cellover;
	nodeunder->convex = WingMeshCrop(n->convex,p);
	nodeover->convex  = WingMeshCrop(n->convex,Plane(-p.normal(),-p.dist()));
	if(n->isleaf==UNDER) { 
		int i;
		BSPNode fake(p.normal(),p.dist());
		fake.under=nodeunder;
		fake.over=nodeover;
		i=n->brep.count;
		while(i--){
			Face *face = n->brep[i];
			FaceEmbed(&fake,face);
		}
		n->brep.count=0;
		fake.under=fake.over=NULL;

	}
	BSPPartition(n->under,p,nodeunder->under,nodeover->under);
	BSPPartition(n->over ,p,nodeunder->over ,nodeover->over );
	if(n->isleaf) { 
		assert(nodeunder->isleaf);
		assert(nodeover->isleaf);
		n->over=n->under=NULL;
		delete n;
		return;
	} 
	assert(nodeunder->over || nodeunder->under);
	assert(nodeover->over  || nodeover->under);
	n->over=n->under=NULL;
	delete n;
	n=NULL;
	if(!nodeunder->under) {
//		assert(SplitTest(nodeunder->cell,*nodeunder)==OVER);
		BSPNode *r = nodeunder;
		nodeunder = nodeunder->over; 
		r->over=NULL;
		delete r;
	}
	else if(!nodeunder->over) {
//		assert(SplitTest(nodeunder->cell,*nodeunder)==UNDER);
		BSPNode *r = nodeunder;
		nodeunder = nodeunder->under;
		r->under=NULL; 
		delete r;
	}
	assert(nodeunder);
	assert(nodeunder->isleaf || (nodeunder->under && nodeunder->over));
	if(!nodeover->under) {
//		assert(SplitTest(nodeover->cell,*nodeover)==OVER);
		BSPNode *r = nodeover;
		nodeover = nodeover->over; 
		r->over=NULL;
		delete r;
	}
	else if(!nodeover->over) {
//		assert(SplitTest(nodeover->cell,*nodeover)==UNDER);
		BSPNode *r = nodeover;
		nodeover = nodeover->under;
		r->under = NULL; 
		delete r;
	}
	assert(nodeover);
	assert(nodeover->isleaf || (nodeover->under && nodeover->over));
	if(!nodeunder->isleaf && nodeunder->over->isleaf && nodeunder->over->isleaf==nodeunder->under->isleaf) {
		nodeunder->isleaf = nodeunder->over->isleaf; // pick one of the children
		int i;
		i=nodeunder->under->brep.count;
		while(i--){
			nodeunder->brep.Add(nodeunder->under->brep[i]);
		}
		nodeunder->under->brep.count=0;
		i=nodeunder->over->brep.count;
		while(i--){
			nodeunder->brep.Add(nodeunder->over->brep[i]);
		}
		nodeunder->over->brep.count=0;
		delete nodeunder->under;
		delete nodeunder->over;
		nodeunder->over = nodeunder->under = NULL;
	}
	// wtf:	if(!nodeover->isleaf && nodeover->over->isleaf==nodeover->under->isleaf) {
	if(!nodeover->isleaf && nodeover->over->isleaf && nodeover->over->isleaf==nodeover->under->isleaf) {
		nodeover->isleaf = nodeover->over->isleaf; // pick one of the children
		int i;
		i=nodeover->under->brep.count;
		while(i--){
			nodeover->brep.Add(nodeover->under->brep[i]);
		}
		nodeover->under->brep.count=0;
		i=nodeover->over->brep.count;
		while(i--){
			nodeover->brep.Add(nodeover->over->brep[i]);
		}
		nodeover->over->brep.count=0;
		delete nodeover->under;
		delete nodeover->over;
		nodeover->over = nodeover->under = NULL;
	}
/*	if(fusenodes) {
		if(0==nodeunder->isleaf) {
			if(nodeunder->over->isleaf==UNDER) {
				DeriveCells(nodeunder->under,nodeunder->cell);
				nodeunder = nodeunder->under; // memleak
			}
			else if(nodeunder->under->isleaf==OVER) {
				DeriveCells(nodeunder->over,nodeunder->cell);
				nodeunder = nodeunder->over; // memleak
			}
		}
		assert(nodeunder);
		if(0==nodeover->isleaf) {
			if(nodeover->over->isleaf==UNDER) {
				DeriveCells(nodeover->under,nodeover->cell);
				nodeover = nodeover->under; // memleak
			}
			else if(nodeover->under->isleaf==OVER) {
				DeriveCells(nodeover->over,nodeover->cell);
				nodeover = nodeover->over;  // memleak
			}
		}
		assert(nodeover);
	}
*/
}

void FaceCutting(BSPNode *n,Array<Face*> &faces)
{
	int i;
	if(n->isleaf==OVER)
	{
		return;
	}
	if(n->isleaf==UNDER)
	{
		for(i=0;i<faces.count;i++)
		{
			delete faces[i];
		}
		faces.count=0;
		return;
	}
	Array<Face*> faces_over;
	Array<Face*> faces_under;
	Array<Face*> faces_coplanar;
	while(faces.count)
	{
		Face *f;
		f= faces.Pop();
		int s = FaceSplitTest(f,n->normal(),n->dist());
		if(s==COPLANAR)
			faces_coplanar.Add(f);
		else if(s==UNDER)
			faces_under.Add(f);
		else if(s==OVER)
			faces_over.Add(f);
		else
		{
			assert(s==SPLIT);
			Face *ovr = FaceDup(f);
			FaceClip(f,(Plane)(*n));
			FaceClip(ovr ,Plane(-n->normal(),-n->dist()));
			faces_under.Add(f);
			faces_over.Add(ovr);
		}
	}
	FaceCutting(n->under,faces_under);
	FaceCutting(n->over,faces_over);
	for(i=0;i<faces_under.count;i++)
		faces.Add(faces_under[i]);
	for(i=0;i<faces_over.count;i++)
		faces.Add(faces_over[i]);
	for(i=0;i<faces_coplanar.count;i++)
		faces.Add(faces_coplanar[i]);
}

BSPNode *BSPUnion(BSPNode *a,BSPNode *b) {
	if(!a || b->isleaf == UNDER || a->isleaf==OVER) {
		if(a && b->isleaf==UNDER)
		{
			FaceCutting(a,b->brep);
		}
		return b;
	}
	if(a->isleaf == UNDER || b->isleaf==OVER) {
		return a;
	}
	BSPNode *aover;
	BSPNode *aunder;
	// its like "b" is the master, so a should be the little object and b is the area's shell
	assert(!a->isleaf);
	BSPPartition(a,Plane(b->normal(),b->dist()),aunder,aover);
	assert(aunder || aover);
	b->under = BSPUnion(aunder,b->under);
	b->over  = BSPUnion(aover ,b->over );
/*	if(fusenodes) {
		if(b->over->isleaf == UNDER) {
			DeriveCells(b->under,b->cell);
			return b->under;
		}
		if(b->under->isleaf == OVER) {
			DeriveCells(b->over,b->cell);
			return b->over;
		}
	}
*/
	return b;
}


int bspmergeallowswap=0;
BSPNode *BSPIntersect(BSPNode *a,BSPNode *b) {
	int swapflag;
	if(!a||a->isleaf == UNDER || b->isleaf==OVER) {
		if(a&&a->isleaf==UNDER ) {
			while(a->brep.count) {
				FaceEmbed(b,a->brep.Pop());
			}
		}
		delete a;
		return b;
	}
	assert(b);
	if(b->isleaf == UNDER || a->isleaf==OVER) {
		if(b->isleaf==UNDER ) {
			while(b->brep.count) {
				FaceEmbed(a,b->brep.Pop());
			}
		}
		delete b;
		return a;
	}
	// I'm not sure about the following bit - it only works if booleaning bsp's cells cover entire area volume too
	if(bspmergeallowswap)if( SPLIT != (swapflag = WingMeshSplitTest(b->convex,*a))) {
		if(swapflag == OVER) {
			a->over = BSPIntersect(a->over,b);
			return a;
		}
		if(swapflag == UNDER) {
			a->under= BSPIntersect(a->under,b);
			return a;
		}
	}
	BSPNode *aover;
	BSPNode *aunder;
	// its like "b" is the master, so a should be the little object and b is the area's shell
	BSPPartition(a,Plane(b->normal(),b->dist()),aunder,aover);
	b->under = BSPIntersect(aunder,b->under);
	b->over  = BSPIntersect(aover ,b->over );
	if(b->over->isleaf && b->over->isleaf==b->under->isleaf) {  // both children are leaves of same type so merge them into parent
		while(b->over->brep.count) {
			b->brep.Add(b->over->brep.Pop());
		}
		assert(b->over->brep.count==0);
		while(b->under->brep.count) {
			b->brep.Add(b->under->brep.Pop());
		}
		b->isleaf = b->over->isleaf;
		delete b->over;
		delete b->under;
		b->over=b->under=NULL;
	}
/*	if(fusenodes) {
		if(b->over->isleaf == UNDER) {
			DeriveCells(b->under,b->cell);
			return b->under;
		}
		if(b->under->isleaf == OVER) {
			DeriveCells(b->over,b->cell);
			return b->over;
		}
	}
*/
	return b;
}

int HitCheckConvexGJK(const Collidable *dude, BSPNode *n)
{
	Contact hitinfo;
	if(n->isleaf==OVER)  return 0;
	if(n->isleaf==UNDER) return (!Separated(dude,n->convex,hitinfo,1));
	int f=0;
	int h=0;

	int u = dude->Support(-n->normal());
	int o = dude->Support( n->normal());
	f |= PlaneTest(n->normal(),n->dist(),dude->GetVert(u),0);
	f |= PlaneTest(n->normal(),n->dist(),dude->GetVert(o),0);
	if(f&UNDER)
	{
		h=HitCheckConvexGJK(dude,n->under);
		if(h) return 1;
	}
	if(f&OVER)
	{
		h=HitCheckConvexGJK(dude,n->over);
		if(h) return 1;
	}
	return 0;
}

int ProximityCells(const Collidable *dude, BSPNode *n,Array<WingMesh*> &cells,float padding)
{
	Contact hitinfo;
	if(n->isleaf==OVER)  return 0;
	if(n->isleaf==UNDER) 
	{ 
		cells.Add(n->convex); 
		return 1;
	}
	int f=0;
	int u = dude->Support(-n->normal());
	int o = dude->Support( n->normal());
	f |= PlaneTest(n->normal(),n->dist()-padding,dude->GetVert(u),0);  // verify sign for padding, seems right
	f |= PlaneTest(n->normal(),n->dist()+padding,dude->GetVert(o),0);
	if(f&UNDER)
	{
		ProximityCells(dude,n->under,cells,padding);
	}
	if(f&OVER)
	{
		ProximityCells(dude,n->over,cells,padding);
	}
	return cells.count;
}
