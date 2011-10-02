//
//      BSP
//  (c) Stan Melax 1998-2007
//  see file bsp.h 
//
// this module provides collision support
//
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>

#include "array.h"
#include "bsp.h"
#include "wingmesh.h"



float3   HitCheckImpactNormal;  // global variable storing the normal of the last hit 
int      HitCheckVertexHit;     // global variable storing the vertex id of the last convex obj collision
BSPNode *HitCheckNodeHitLeaf=NULL; // global variable storing the BSP node leaf hit.
BSPNode *HitCheckNodeHitOverLeaf=NULL; // global variable storing the BSP over leaf node that was just beforehit.
BSPNode *HitCheckNode=NULL; // global variable storing the BSP node plane hit.


int PointInsideFace(Face* f,const float3 &s)
{
	int inside=1;
	for(int j=0;inside && j<f->vertex.count;j++) 
    {
			float3 &pp1 = f->vertex[j] ;
			float3 &pp2 = f->vertex[(j+1)%f->vertex.count];
			float3 side = cross((pp2-pp1),(s-pp1));
			inside = (dot(f->normal(),side) >= 0.0);
	}
	return inside;
}

Face* FaceHit(BSPNode *leaf,const Plane &plane,const float3 &s)
{
	int i;
	for(i=0;i<leaf->brep.count;i++)
	{
		Face *f = leaf->brep[i];
		if(!coplanar(*f,plane)) continue;
		static int craptest=0;   // sometimes the normal is facing the other way, but that's ok.
		if(craptest)
		{ 
			craptest=0;
			assert(f->normal() == plane.normal());
			assert(f->dist()   == plane.dist());
		}
		if(PointInsideFace(f,s)) 
		{
			return f;
		}
	}
	return NULL;
}

Face* FaceHit(const float3& s)
{
	if(!HitCheckNodeHitLeaf || !HitCheckNode) return NULL;
	BSPNode *leaf=HitCheckNodeHitLeaf;
	Plane &plane = *HitCheckNode;
	return FaceHit(leaf,plane,s);
}

static int bypass_first_solid=0;  // put into hitinfo
int HitCheck(BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact) {
	assert(node);
	if(node->isleaf ){
		if(node->isleaf==UNDER ){
			HitCheckNodeHitLeaf=node;
			if(impact) *impact = v0;
		}
		else
		{
			HitCheckNodeHitOverLeaf = node;
			bypass_first_solid=0;
		}
		return (node->isleaf==UNDER  && !bypass_first_solid );
	}
	int f0 = (dot(v0,node->normal())+node->dist()>0)?1:0;  // if v0 above plane
	int f1 = (dot(v1,node->normal())+node->dist()>0)?1:0;  // if v1 above plane
	if(f0==0 && f1==0) {
		return HitCheck(node->under,1,v0,v1,impact);
	}
	if(f0==1 && f1==1) {
		return HitCheck(node->over,0,v0,v1,impact);
	}
	float3 vmid = PlaneLineIntersection(*node,v0,v1);
	if(f0==0) {
		assert(f1==1);
		// perhaps we could pass 'solid' instead of '1' here:
		if(HitCheck(node->under,1,v0,vmid,impact)) {
			return 1;
		}
		HitCheckImpactNormal=-node->normal();
		HitCheckNode = node;
		return HitCheck(node->over,0,vmid,v1,impact);
	}
	assert(f0==1 && f1==0);
	if(HitCheck(node->over,0,v0,vmid,impact)) {
		return 1;
	}
	HitCheckImpactNormal=node->normal();
	HitCheckNode = node;
	return HitCheck(node->under,1,vmid,v1,impact);
}

int HitCheckSolidReEnter(BSPNode *node,float3 v0,float3 v1,float3 *impact) 
{
	bypass_first_solid =1;
	int	h=HitCheck(node,1,v0,v1,impact);
	bypass_first_solid =0; // make sure you turn this back off in case the query never hit empty space.
	return h;
}


int SegmentUnder(const Plane &plane,const float3 &v0,const float3 &v1,const float3 &nv0,float3 *w0,float3 *w1,float3 *nw0){
	float d0,d1;
	d0 = dot(plane.normal(),v0)+plane.dist();	
	d1 = dot(plane.normal(),v1)+plane.dist();	
	if(d0>0.0f && d1>0.0f) {
		return 0;
	}
	if(d0<=0.0f && d1<=0.0f) {
		*w0 =v0;
		*w1 =v1;
		*nw0=nv0;
		return 3;
	}
	float3 vmid=PlaneLineIntersection(plane,v0,v1);
	if(d0>0.0f) {
		assert(d1<=0.0f);
		*w1=v1;
		*w0=vmid;
		*nw0=plane.normal();
		return 2;
	}
	else{
		assert(d1>0.0f);
		*w1 = vmid;
		*w0 = v0;
		*nw0=nv0;
		return 1;
	}
}
int SegmentOver(const Plane &plane,const float3 &v0,const float3 &v1,const float3 &nv0,float3 *w0,float3 *w1,float3 *nw0){
	return SegmentUnder(Plane(-plane.normal(),-plane.dist()),v0,v1,nv0,w0,w1,nw0);
}
int ConvexHitCheck(WingMesh *convex,float3 v0,float3 v1,float3 *impact) {
	int i;
	float3 nml;
	for(i=0;i<convex->faces.count;i++) {
		if(!SegmentUnder(convex->faces[i],v0,v1,nml,&v0,&v1,&nml)){
			return 0;
		}
	}
	if(impact) *impact=v0;
	return 1;
}


int HitCheckSphere(float r,BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact,const float3 &nv0) {
	assert(node);
	if(node->isleaf ){
		if(node->isleaf==UNDER ){
			if(impact) *impact = v0;
			HitCheckImpactNormal = nv0;
		}
		return (node->isleaf==UNDER);
	}
	float3 w0,w1,nw0;
	int hit=0;
	if(SegmentUnder(Plane(node->normal(),node->dist()-r),v0,v1,nv0,&w0,&w1,&nw0)) {
		hit |= HitCheckSphere(r,node->under,1,w0,w1,&v1,nw0);
	}
	if(SegmentOver( Plane(node->normal(),node->dist()+r),v0,v1,nv0,&w0,&w1,&nw0)) {
		hit |= HitCheckSphere(r,node->over,0,w0,w1,&v1,nw0);
	}
	if(hit) {
		*impact = v1;
	}
	return hit;
}

float3 TangentPointOnCylinder(const float r,const float h,const float3 &n) {
	float3 p;
	float xymag = sqrtf(n.x*n.x+n.y*n.y);
	if(xymag==0.0f) {xymag=1.0f;} // yup this works
    p.x = r * n.x /xymag;
    p.y = r * n.y /xymag;
    p.z = (n.z>0) ?h:0;  // reference point at base of cylinder 
    // p.z = (n.z>0) ?h/2:-h/2; // use if reference point at mid height of cylinder (h/2)
	return p;
}

int usebevels=1;

// hmmm this isn't working right now for some reason:
int HitCheckBevelsCylinder(float r,float h,WingMesh *convex,float3 v0,float3 v1,float3 *impact,float3 nv0) {
	int i;
	if(!convex || !convex->edges.count)
	{
		return 0; // nothing here to hit (likely an orphan plane due to bediter)
	}
	assert(convex->edges.count);
	for(i=0;i<convex->edges.count;i++) {
		WingMesh::HalfEdge &edge0 = convex->edges[i];
		WingMesh::HalfEdge &edgeA = convex->edges[edge0.adj];
		if(i>edge0.adj) {
			continue; // no need to test edge twice
		}
		if(dot(convex->faces[edge0.face].normal(),convex->faces[edgeA.face].normal())> -0.03f ) {
			continue;
		}
		Plane bev(normalize(convex->faces[edge0.face].normal() + convex->faces[edgeA.face].normal()),0);
		bev.dist() =  -dot(bev.normal(),convex->verts[edge0.v]);
		for(int j=0;j<convex->verts.count;j++) {
			extern int PlaneTest(const Plane &p,const float3 &v,float epsilon=PAPERWIDTH);
			assert(PlaneTest(bev,convex->verts[j],PAPERWIDTH*10) != OVER);
		}
	    bev.dist() += -dot(TangentPointOnCylinder(r,h,-bev.normal()),-bev.normal()); 
		if(0==SegmentUnder(bev,v0,v1,nv0,&v0,&v1,&nv0)) {
			return 0;
		}
	}
	*impact = v0;
	HitCheckImpactNormal = nv0;
	return 1;
}


int HitCheckCylinder(float r,float h,BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact,const float3 &nv0) {
	assert(node);
	if(node->isleaf ){
		if(usebevels && node->isleaf==UNDER) {
			return HitCheckBevelsCylinder(r,h,node->convex,v0,v1,impact,nv0);
		}
		if(node->isleaf==UNDER){
			if(impact) *impact = v0;
			HitCheckImpactNormal = nv0;
		}
		return (node->isleaf==UNDER);
	}

    float offset_up   = -dot(TangentPointOnCylinder(r,h,-node->normal()),-node->normal()); 
    float offset_down =  dot(TangentPointOnCylinder(r,h, node->normal()), node->normal()); 

	float3 w0,w1,nw0;
	int hit=0;
	if(SegmentUnder(Plane(node->normal(),node->dist()+offset_up  ),v0,v1,nv0,&w0,&w1,&nw0)) {
		hit |= HitCheckCylinder(r,h,node->under,1,w0,w1,&v1,nw0);
	}
	if(SegmentOver( Plane(node->normal(),node->dist()+offset_down),v0,v1,nv0,&w0,&w1,&nw0)) {
		hit |= HitCheckCylinder(r,h,node->over,0,w0,w1,&v1,nw0);
	}
	if(hit) {
		*impact = v1;
	}
	return hit;
}

class Collision
{
public:
	int hit;
	float3 impact;
	float3 normal;
	operator int(){return hit;}
	Collision(int _hit=0):hit(_hit){}
};
Collision hittest()
{
	return Collision();
}
static void testc()
{
	int x;
	if(hittest())
	{
		 x++;
	}
	Collision c = hittest();
	x++;
}


int PortionUnder(Plane plane,Array<float3> &verts,
				 const float3 &v0,const float3 &v1,const float3 &nv0,int vrtv0,
				 const Quaternion &q0,const Quaternion &q1,
				 float3 *w0,float3 *w1,float3 *nw0,int *vrtw0,
				 Quaternion *wq0,Quaternion *wq1
				){
	int i;
	Array<float3> verts0;
	Array<float3> verts1;
	int under0=0;
	int under1=0;
	int closest0=-1;
	int closest1=-1;
	if(dot(plane.normal(),v0)+plane.dist() <0)
	{
		under0=1;
	}
	else if(dot(plane.normal(),v0)+plane.dist() <5.0f)
	{
		for(i=0;i<verts.count;i++) {
			verts0.Add( rotate(q0,verts[i]));
		}
		closest0 = maxdir(verts0,-plane.normal());
		if(dot(plane.normal(),v0+verts0[closest0])+plane.dist() <0){
			under0=1;
		}
	}
	if(dot(plane.normal(),v1)+plane.dist() <0)
	{
		under1=1;
	}
	else if(dot(plane.normal(),v1)+plane.dist() <5.0f)
	{
		for(i=0;i<verts.count;i++) {
			verts1.Add(rotate(q1,verts[i]));
		}
		closest1 = maxdir(verts1,-plane.normal());
		if(dot(plane.normal(),v1+verts1[closest1])+plane.dist() <0){
			under1=1;
		}
	}
	
	if(!under0 && !under1) {
		return 0;
	}
	if(under0) {
		*nw0  = nv0;
		*wq0  = q0;
		*w0   = v0;
		*vrtw0= vrtv0;
	}
	else {
		if(closest1==-1) {
			// this code already exists above, but wasnt executed then // FIXME
			for(i=0;i<verts.count;i++) {
				verts1.Add(rotate(q1,verts[i]));
			}
			closest1 = maxdir(verts1,-plane.normal());
		}
		assert(under1);
		// could be solved analytically, but im lazy and just implementing iterative thingy
		float ta=0.0f,tb=1.0f;
		for(i=0;i<10;i++) {
			float tmid = (ta+tb)/2.0f;
			Quaternion qmid = Interpolate(q0,q1,tmid);
			float3     vmid = Interpolate(v0,v1,tmid);
			float dmid = dot(plane.normal(),vmid+rotate(qmid,verts[closest1]))+plane.dist();
			*((dmid>0)?&ta:&tb)=tmid;
		}
		*nw0 = plane.normal();
		*w0  = Interpolate(v0,v1,ta);
		*wq0 = Interpolate(q0,q1,ta);
		*vrtw0= closest1;
		// lets hope that closest1 is still the closest.
		assert(dot(plane.normal(),*w0+rotate(*wq0,verts[closest1]))+plane.dist() >0);
		for(i=0;i<verts.count;i++) {
			if(dot(plane.normal(),*w0+rotate(*wq0,verts[i]))+plane.dist() <=0){
				// call this function again with shorter
				float3 vshort     = *w0;  // shortened
				Quaternion qshort = *wq0; // shortened
				int rc= PortionUnder(plane,verts,v0,vshort,nv0,vrtv0,q0,qshort,w0,w1,nw0,vrtw0,wq0,wq1);
				assert(rc);
				return rc;
			}
		}
	}
	if(under1) {
		*wq1 = q1;
		*w1  = v1;
	}
	else {
		assert(under0);
		if(closest0==-1) {
			// yes this code already exists above but it wasn't called // FIXME
			for(i=0;i<verts.count;i++) {
				verts0.Add(rotate( q0,verts[i]));
			}
			closest0 = maxdir(verts0,-plane.normal());
		}
		float ta=0.0f,tb=1.0f;
		for(i=0;i<5;i++) {
			float tmid = (ta+tb)/2.0f;
			Quaternion qmid = Interpolate(q0,q1,tmid);
			float3     vmid = Interpolate(v0,v1,tmid);
			float dmid = dot(plane.normal(),vmid+rotate(qmid,verts[closest0]))+plane.dist();
			*((dmid<0)?&ta:&tb)=tmid;
		}
		*w1  = Interpolate(v0,v1,tb);
		*wq1 = Interpolate(q0,q1,tb);
		// lets hope that closest0 is still the closest.
		//assert(dot(plane.normal,*w1+*wq1*verts[closest0])+plane.dist <0);
	}
	return 1;
}


int HitCheckConvex(Array<float3> &verts,BSPNode *node,int solid,
				   float3 v0,float3 v1,float3 *impact,
				   const Quaternion &q0,const Quaternion &_q1,Quaternion *impactq,const float3 &nv0,const int vrtv0) {
	assert(node);
	if( node->isleaf ){
		if(node->isleaf==UNDER ){
			if(impact)  *impact = v0;
			if(impactq) *impactq= q0;
			HitCheckImpactNormal = nv0;
			HitCheckVertexHit    = vrtv0;
		}
		return (node->isleaf==UNDER);
	}

	float3 w0,w1,nw0;
	Quaternion wq0,wq1;
	Quaternion q1=_q1;
	int vrtw0;
	int hit=0;
	if(PortionUnder(Plane(node->normal(),node->dist()),verts,v0,v1,nv0,vrtv0,q0,q1,&w0,&w1,&nw0,&vrtw0,&wq0,&wq1)) {
		hit |= HitCheckConvex(verts,node->under,1, w0,w1,&v1, wq0,wq1,&q1,nw0,vrtw0);
	}
	if(PortionUnder(Plane(-node->normal(),-node->dist()),verts,v0,v1,nv0,vrtv0,q0,q1,&w0,&w1,&nw0,&vrtw0,&wq0,&wq1)) {
		hit |= HitCheckConvex(verts,node->over,0, w0,w1,&v1, wq0,wq1,&q1,nw0,vrtw0);
	}
	if(hit) {
		*impact = v1;
		*impactq= q1;
	}
	return hit;
}



