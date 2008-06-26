//
//      Convex 
//
// minimal data size
// maintains adjacency info
// single pass through edge list
// during crop/split
// Ok this implementation is kinda ugly, 
// but i think it should be pretty fast.
// the data representation is about a small as can be
// and still maintain neighbor info and have everything
// packed into 1 list of "edges".
//
																													  
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstring>

#include "vecmath.h"
#include "bsp.h"

typedef Convex::HalfEdge HalfEdge;


Convex::Convex(int vertices_size,int edges_size,int facets_size)
	:vertices(vertices_size)
	,edges(edges_size)
	,facets(facets_size)
{
	vertices.count=vertices_size;
	edges.count   = edges_size;
	facets.count  = facets_size;
}

Convex *ConvexDup(Convex *src) {
	Convex *dst = new Convex(src->vertices.count,src->edges.count,src->facets.count);
	memcpy(dst->vertices.element,src->vertices.element,sizeof(float3)*src->vertices.count);
	memcpy(dst->edges.element,src->edges.element,sizeof(HalfEdge)*src->edges.count);
	memcpy(dst->facets.element,src->facets.element,sizeof(Plane)*src->facets.count);
	return dst;
}


int SplitTest(Convex &convex,const Plane &plane) {
	int flag=0;
	for(int i=0;i<convex.vertices.count;i++) {
		flag |= PlaneTest(plane,convex.vertices[i]);
	}
	return flag;
}

class VertFlag 
{
public:
	unsigned char planetest;
	unsigned char junk;
	unsigned char undermap;
	unsigned char overmap;
};
class EdgeFlag 
{
public:
	unsigned char planetest;
	unsigned char fixes;
	short undermap;
	short overmap;
};
class PlaneFlag 
{
public:
	unsigned char undermap;
	unsigned char overmap;
};
class Coplanar{
public:
	unsigned short ea;
	unsigned char v0;
	unsigned char v1;
};

void AssertIntact(Convex &convex) {
	int i;
	int estart=0;
	for(i=0;i<convex.edges.count;i++) {
		if(convex.edges[estart].p!= convex.edges[i].p) {
			estart=i;
		}
		int inext = i+1;
		if(inext>= convex.edges.count || convex.edges[inext].p != convex.edges[i].p) {
			inext = estart;
		}
		assert(convex.edges[inext].p == convex.edges[i].p);
		HalfEdge &edge = convex.edges[i];
		int nb = convex.edges[i].ea;
		assert(nb!=255);
		assert(nb!=-1);
		assert(i== convex.edges[nb].ea);
	}
	for(i=0;i<convex.edges.count;i++) {
		assert(COPLANAR==PlaneTest(convex.facets[convex.edges[i].p],convex.vertices[convex.edges[i].v]));
		if(convex.edges[estart].p!= convex.edges[i].p) {
			estart=i;
		}
		int i1 = i+1;
		if(i1>= convex.edges.count || convex.edges[i1].p != convex.edges[i].p) {
			i1 = estart;
		}
		int i2 = i1+1;
		if(i2>= convex.edges.count || convex.edges[i2].p != convex.edges[i].p) {
			i2 = estart;
		}
		if(i==i2) continue; // i sliced tangent to an edge and created 2 meaningless edges
		float3 localnormal = TriNormal(convex.vertices[convex.edges[i ].v],
			                           convex.vertices[convex.edges[i1].v],
			                           convex.vertices[convex.edges[i2].v]);
		assert(dot(localnormal,convex.facets[convex.edges[i].p].normal)>0);
	}
}

// back to back quads
Convex *test_btbq() {
	Convex *convex = new Convex(4,8,2);
	convex->vertices[0] = float3(0,0,0);
	convex->vertices[1] = float3(1,0,0);
	convex->vertices[2] = float3(1,1,0);
	convex->vertices[3] = float3(0,1,0);
	convex->facets[0] = Plane(float3(0,0,1),0);
	convex->facets[1] = Plane(float3(0,0,-1),0);
	convex->edges[0]  = HalfEdge(7,0,0);
	convex->edges[1]  = HalfEdge(6,1,0);
	convex->edges[2]  = HalfEdge(5,2,0);
	convex->edges[3]  = HalfEdge(4,3,0);

	convex->edges[4]  = HalfEdge(3,0,1);
	convex->edges[5]  = HalfEdge(2,3,1);
	convex->edges[6]  = HalfEdge(1,2,1);
	convex->edges[7]  = HalfEdge(0,1,1);
	AssertIntact(*convex);
	return convex;
}

Convex *test_cube() {
	Convex *convex = new Convex(8,24,6);
	convex->vertices[0] = float3(0,0,0);
	convex->vertices[1] = float3(0,0,1);
	convex->vertices[2] = float3(0,1,0);
	convex->vertices[3] = float3(0,1,1);
	convex->vertices[4] = float3(1,0,0);
	convex->vertices[5] = float3(1,0,1);
	convex->vertices[6] = float3(1,1,0);
	convex->vertices[7] = float3(1,1,1);

	convex->facets[0] = Plane(float3(-1,0,0),0);
	convex->facets[1] = Plane(float3(1,0,0),-1);
	convex->facets[2] = Plane(float3(0,-1,0),0);
	convex->facets[3] = Plane(float3(0,1,0),-1);
	convex->facets[4] = Plane(float3(0,0,-1),0);
	convex->facets[5] = Plane(float3(0,0,1),-1);

	convex->edges[0 ] = HalfEdge(11,0,0);
	convex->edges[1 ] = HalfEdge(23,1,0);
	convex->edges[2 ] = HalfEdge(15,3,0);
	convex->edges[3 ] = HalfEdge(16,2,0);

	convex->edges[4 ] = HalfEdge(13,6,1);
	convex->edges[5 ] = HalfEdge(21,7,1);
	convex->edges[6 ] = HalfEdge( 9,5,1);
	convex->edges[7 ] = HalfEdge(18,4,1);

	convex->edges[8 ] = HalfEdge(19,0,2);
	convex->edges[9 ] = HalfEdge( 6,4,2);
	convex->edges[10] = HalfEdge(20,5,2);
	convex->edges[11] = HalfEdge( 0,1,2);

	convex->edges[12] = HalfEdge(22,3,3);
	convex->edges[13] = HalfEdge( 4,7,3);
	convex->edges[14] = HalfEdge(17,6,3);
	convex->edges[15] = HalfEdge( 2,2,3);

	convex->edges[16] = HalfEdge( 3,0,4);
	convex->edges[17] = HalfEdge(14,2,4);
	convex->edges[18] = HalfEdge( 7,6,4);
	convex->edges[19] = HalfEdge( 8,4,4);
	
	convex->edges[20] = HalfEdge(10,1,5);
	convex->edges[21] = HalfEdge( 5,5,5);
	convex->edges[22] = HalfEdge(12,7,5);
	convex->edges[23] = HalfEdge( 1,3,5);

	
	return convex;
}
/*
Convex *ConvexMakeCube(float smin,float smax) {
	Convex *convex = test_cube();
	convex->vertices[0] = float3(smin,smin,smin);
	convex->vertices[1] = float3(smin,smin,smax);
	convex->vertices[2] = float3(smin,smax,smin);
	convex->vertices[3] = float3(smin,smax,smax);
	convex->vertices[4] = float3(smax,smin,smin);
	convex->vertices[5] = float3(smax,smin,smax);
	convex->vertices[6] = float3(smax,smax,smin);
	convex->vertices[7] = float3(smax,smax,smax);

	convex->facets[0] = Plane(float3(-1,0,0), smin);
	convex->facets[1] = Plane(float3(1,0,0), -smax);
	convex->facets[2] = Plane(float3(0,-1,0), smin);
	convex->facets[3] = Plane(float3(0,1,0), -smax);
	convex->facets[4] = Plane(float3(0,0,-1), smin);
	convex->facets[5] = Plane(float3(0,0,1), -smax);
	return convex;
}
*/
Convex *ConvexMakeCube(const float3 &bmin, const float3 &bmax) {
	Convex *convex = test_cube();
	convex->vertices[0] = float3(bmin.x,bmin.y,bmin.z);
	convex->vertices[1] = float3(bmin.x,bmin.y,bmax.z);
	convex->vertices[2] = float3(bmin.x,bmax.y,bmin.z);
	convex->vertices[3] = float3(bmin.x,bmax.y,bmax.z);
	convex->vertices[4] = float3(bmax.x,bmin.y,bmin.z);
	convex->vertices[5] = float3(bmax.x,bmin.y,bmax.z);
	convex->vertices[6] = float3(bmax.x,bmax.y,bmin.z);
	convex->vertices[7] = float3(bmax.x,bmax.y,bmax.z);

	convex->facets[0] = Plane(float3(-1,0,0), bmin.x);
	convex->facets[1] = Plane(float3(1,0,0), -bmax.x);
	convex->facets[2] = Plane(float3(0,-1,0), bmin.y);
	convex->facets[3] = Plane(float3(0,1,0), -bmax.y);
	convex->facets[4] = Plane(float3(0,0,-1), bmin.z);
	convex->facets[5] = Plane(float3(0,0,1), -bmax.z);
	return convex;
}


Convex *ConvexCrop(Convex &convex,const Plane &slice)
{
	int i;
	int vertcountunder=0;
	int vertcountover =0;
	int edgecountunder=0;
	int edgecountover =0;
	int planecountunder=0;
	int planecountover =0;
	static Array<int> vertscoplanar;  // existing vertex members of convex that are coplanar
	vertscoplanar.count=0;
	static Array<int> edgesplit;  // existing edges that members of convex that cross the splitplane
	edgesplit.count=0;

	assert(convex.edges.count<480);

	EdgeFlag  edgeflag[512];
	VertFlag  vertflag[256];
	PlaneFlag planeflag[128];
	HalfEdge  tmpunderedges[512];
	Plane	  tmpunderplanes[128];
	Coplanar coplanaredges[512];
	int coplanaredges_num=0;

	Array<float3> createdverts;
	// do the side-of-plane tests
	for(i=0;i<convex.vertices.count;i++) {
		vertflag[i].planetest = PlaneTest(slice,convex.vertices[i]);
		if(vertflag[i].planetest == COPLANAR) {
			// ? vertscoplanar.Add(i);
			vertflag[i].undermap = vertcountunder++;
			vertflag[i].overmap  = vertcountover++;
		}
		else if(vertflag[i].planetest == UNDER)	{
			vertflag[i].undermap = vertcountunder++;
		}
		else {
			assert(vertflag[i].planetest == OVER);
			vertflag[i].overmap  = vertcountover++;
			vertflag[i].undermap = -1; // for debugging purposes
		}
	}
	int vertcountunderold = vertcountunder; // for debugging only

	int under_edge_count =0;
	int underplanescount=0;
	int e0=0;

	for(int currentplane=0; currentplane<convex.facets.count; currentplane++) {
		int estart =e0;
		int enextface;
		int planeside = 0;
		int e1 = e0+1;
		int eus=-1;
		int ecop=-1;
		int vout=-1;
		int vin =-1;
		int coplanaredge = -1;
		do{

			if(e1 >= convex.edges.count || convex.edges[e1].p!=currentplane) {
				enextface = e1;
				e1=estart;
			}
			HalfEdge &edge0 = convex.edges[e0];
			HalfEdge &edge1 = convex.edges[e1];
			HalfEdge &edgea = convex.edges[edge0.ea];


			planeside |= vertflag[edge0.v].planetest;
			//if((vertflag[edge0.v].planetest & vertflag[edge1.v].planetest)  == COPLANAR) {
			//	assert(ecop==-1);
			//	ecop=e;
			//}


			if(vertflag[edge0.v].planetest == OVER && vertflag[edge1.v].planetest == OVER){
				// both endpoints over plane
				edgeflag[e0].undermap  = -1;
			}
			else if((vertflag[edge0.v].planetest | vertflag[edge1.v].planetest)  == UNDER) {
				// at least one endpoint under, the other coplanar or under
				
				edgeflag[e0].undermap = under_edge_count;
				tmpunderedges[under_edge_count].v = vertflag[edge0.v].undermap;
				tmpunderedges[under_edge_count].p = underplanescount;
				if(edge0.ea < e0) {
					// connect the neighbors
					assert(edgeflag[edge0.ea].undermap !=-1);
					tmpunderedges[under_edge_count].ea = edgeflag[edge0.ea].undermap;
					tmpunderedges[edgeflag[edge0.ea].undermap].ea = under_edge_count;
				}
				under_edge_count++;
			}
			else if((vertflag[edge0.v].planetest | vertflag[edge1.v].planetest)  == COPLANAR) {
				// both endpoints coplanar 
				// must check a 3rd point to see if UNDER
				int e2 = e1+1; 
				if(e2>=convex.edges.count || convex.edges[e2].p!=currentplane) {
					e2 = estart;
				}
				assert(convex.edges[e2].p==currentplane);
				HalfEdge &edge2 = convex.edges[e2];
				if(vertflag[edge2.v].planetest==UNDER) {
					
					edgeflag[e0].undermap = under_edge_count;
					tmpunderedges[under_edge_count].v = vertflag[edge0.v].undermap;
					tmpunderedges[under_edge_count].p = underplanescount;
					tmpunderedges[under_edge_count].ea = -1;
					// make sure this edge is added to the "coplanar" list
					coplanaredge = under_edge_count;
					vout = vertflag[edge0.v].undermap;
					vin  = vertflag[edge1.v].undermap;
					under_edge_count++;
				}
				else {
					edgeflag[e0].undermap = -1;
				}
			}
			else if(vertflag[edge0.v].planetest == UNDER && vertflag[edge1.v].planetest == OVER) {
				// first is under 2nd is over 
				
				edgeflag[e0].undermap = under_edge_count;
				tmpunderedges[under_edge_count].v = vertflag[edge0.v].undermap;
				tmpunderedges[under_edge_count].p = underplanescount;
				if(edge0.ea < e0) {
					assert(edgeflag[edge0.ea].undermap !=-1);
					// connect the neighbors
					tmpunderedges[under_edge_count].ea = edgeflag[edge0.ea].undermap;
					tmpunderedges[edgeflag[edge0.ea].undermap].ea = under_edge_count;
					vout = tmpunderedges[edgeflag[edge0.ea].undermap].v;
				}
				else {
					Plane &p0 = convex.facets[edge0.p];
					Plane &pa = convex.facets[edgea.p];
					createdverts.Add(ThreePlaneIntersection(p0,pa,slice));
					//createdverts.Add(PlaneProject(slice,PlaneLineIntersection(slice,convex.vertices[edge0.v],convex.vertices[edgea.v])));
					//createdverts.Add(PlaneLineIntersection(slice,convex.vertices[edge0.v],convex.vertices[edgea.v]));
					vout = vertcountunder++;
				}
				under_edge_count++;
				/// hmmm something to think about: i might be able to output this edge regarless of 
				// wheter or not we know v-in yet.  ok i;ll try this now:
				tmpunderedges[under_edge_count].v = vout;
				tmpunderedges[under_edge_count].p = underplanescount;
				tmpunderedges[under_edge_count].ea = -1;
				coplanaredge = under_edge_count;
				under_edge_count++;

				if(vin!=-1) {
					// we previously processed an edge  where we came under
					// now we know about vout as well

					// ADD THIS EDGE TO THE LIST OF EDGES THAT NEED NEIGHBOR ON PARTITION PLANE!!
				}

			}
			else if(vertflag[edge0.v].planetest == COPLANAR && vertflag[edge1.v].planetest == OVER) {
				// first is coplanar 2nd is over 
				
				edgeflag[e0].undermap = -1;
				vout = vertflag[edge0.v].undermap;
				// I hate this but i have to make sure part of this face is UNDER before ouputting this vert
				int k=estart;
				assert(edge0.p == currentplane);
				while(!(planeside&UNDER) && k<convex.edges.count && convex.edges[k].p==edge0.p) {
					planeside |= vertflag[convex.edges[k].v].planetest;
					k++;
				}
				if(planeside&UNDER){
					tmpunderedges[under_edge_count].v = vout;
					tmpunderedges[under_edge_count].p = underplanescount;
					tmpunderedges[under_edge_count].ea = -1;
					coplanaredge = under_edge_count; // hmmm should make a note of the edge # for later on
					under_edge_count++;
					
				}
			}
			else if(vertflag[edge0.v].planetest == OVER && vertflag[edge1.v].planetest == UNDER) {
				// first is over next is under 
				// new vertex!!!
				assert(vin==-1);
				if(e0<edge0.ea) {
					Plane &p0 = convex.facets[edge0.p];
					Plane &pa = convex.facets[edgea.p];
					createdverts.Add(ThreePlaneIntersection(p0,pa,slice));
					//createdverts.Add(PlaneLineIntersection(slice,convex.vertices[edge0.v],convex.vertices[edgea.v]));
					//createdverts.Add(PlaneProject(slice,PlaneLineIntersection(slice,convex.vertices[edge0.v],convex.vertices[edgea.v])));
					vin = vertcountunder++;
				}
				else {
					// find the new vertex that was created by edge[edge0.ea]
					int nea = edgeflag[edge0.ea].undermap;
					assert(tmpunderedges[nea].p==tmpunderedges[nea+1].p);
					vin = tmpunderedges[nea+1].v;
					assert(vin < vertcountunder);
					assert(vin >= vertcountunderold);   // for debugging only
				}
				if(vout!=-1) {
					// we previously processed an edge  where we went over
					// now we know vin too
					// ADD THIS EDGE TO THE LIST OF EDGES THAT NEED NEIGHBOR ON PARTITION PLANE!!
				}
				// output edge
				tmpunderedges[under_edge_count].v = vin;
				tmpunderedges[under_edge_count].p = underplanescount;
				edgeflag[e0].undermap = under_edge_count;
				if(e0>edge0.ea) {
					assert(edgeflag[edge0.ea].undermap !=-1);
					// connect the neighbors
					tmpunderedges[under_edge_count].ea = edgeflag[edge0.ea].undermap;
					tmpunderedges[edgeflag[edge0.ea].undermap].ea = under_edge_count;
				}
				assert(edgeflag[e0].undermap == under_edge_count);
				under_edge_count++;
			}
			else if(vertflag[edge0.v].planetest == OVER && vertflag[edge1.v].planetest == COPLANAR) {
				// first is over next is coplanar 
				
				edgeflag[e0].undermap = -1;
				vin = vertflag[edge1.v].undermap;
				assert(vin!=-1);
				if(vout!=-1) {
					// we previously processed an edge  where we came under
					// now we know both endpoints
					// ADD THIS EDGE TO THE LIST OF EDGES THAT NEED NEIGHBOR ON PARTITION PLANE!!
				}

			}
			else {
				assert(0);
			}
			

			e0=e1;
			e1++; // do the modulo at the beginning of the loop

		} while(e0!=estart) ;
		e0 = enextface;
		if(planeside&UNDER) {
			planeflag[currentplane].undermap = underplanescount;
			tmpunderplanes[underplanescount] = convex.facets[currentplane];
			underplanescount++;
		}
		else {
			planeflag[currentplane].undermap = 0;
		}
		if(vout>=0 && (planeside&UNDER)) {
			assert(vin>=0);
			assert(coplanaredge>=0);
			assert(coplanaredge!=511);
			coplanaredges[coplanaredges_num].ea = coplanaredge;
			coplanaredges[coplanaredges_num].v0 = vin;
			coplanaredges[coplanaredges_num].v1 = vout;
			coplanaredges_num++;
		}
	}

	// add the new plane to the mix:
	if(coplanaredges_num>0) {
		tmpunderplanes[underplanescount++]=slice;
	}
	for(i=0;i<coplanaredges_num-1;i++) {
		if(coplanaredges[i].v1 != coplanaredges[i+1].v0) {
			for(int j=i+2;j<coplanaredges_num;j++) {
				if(coplanaredges[i].v1 == coplanaredges[j].v0) {
					Coplanar tmp = coplanaredges[i+1];
					coplanaredges[i+1] = coplanaredges[j];
					coplanaredges[j] = tmp;
					break;
				}
			}
			if(j>=coplanaredges_num)
			{
				assert(j<coplanaredges_num);
			}
		}
	}
	Convex *punder = new Convex(vertcountunder,under_edge_count+coplanaredges_num,underplanescount);
	Convex &under = *punder;
	int k=0;
	for(i=0;i<convex.vertices.count;i++) {
		if(vertflag[i].planetest != OVER){
			under.vertices[k++] = convex.vertices[i];
		}
	}
	i=0;
	while(k<vertcountunder) {
		under.vertices[k++] = createdverts[i++];
	}
	assert(i==createdverts.count);

	for(i=0;i<coplanaredges_num;i++) {
		under.edges[under_edge_count+i].p  = underplanescount-1;
		under.edges[under_edge_count+i].ea = coplanaredges[i].ea;
		tmpunderedges[coplanaredges[i].ea].ea = under_edge_count+i;
		under.edges[under_edge_count+i].v  = coplanaredges[i].v0;
	}
	
	memcpy(under.edges.element,tmpunderedges,sizeof(HalfEdge)*under_edge_count);
	memcpy(under.facets.element,tmpunderplanes,sizeof(Plane)*underplanescount);
	return punder;
}



float ConvexVolume(Convex *convex){
	int i;
	float volume=0;
	int currentplane=-1;
	int currentedge;
	for(i=0;i<convex->edges.count;i++) {
		if(convex->edges[i].p!= currentplane) {
			currentplane = convex->edges[i].p;
			currentedge  = i;
			i+=1;  // skip one vertex
			continue;
		}
		float3 &vb=convex->vertices[convex->edges[currentedge].v];
		float3 &v1=convex->vertices[convex->edges[i-1].v];
		float3 &v2=convex->vertices[convex->edges[i].v];
		volume += -convex->facets[currentplane].dist * dot(convex->facets[currentplane].normal,cross(v1-vb,v2-v1)) / 6.0f;
	}
	return volume;
}


void ConvexTranslate(Convex *convex,const float3 &_offset){
	float3 offset(_offset);
	int i;
	for(i=0;i<convex->vertices.count;i++) {
		convex->vertices[i] = convex->vertices[i]+offset;
	}
	for(i=0;i<convex->facets.count;i++) {
		convex->facets[i].dist = convex->facets[i].dist - dot(convex->facets[i].normal,offset);
	}

}

void ConvexRotate(Convex *convex,const Quaternion &r){
	int i;
	for(i=0;i<convex->vertices.count;i++) {
		convex->vertices[i] = float3(r*float3(convex->vertices[i]));
	}
	for(i=0;i<convex->facets.count;i++) {
		convex->facets[i].normal  = float3(r*float3(convex->facets[i].normal)) ;
	}

}




void testvol() {
	//Convex *convex = ConvexMakeCube(-2,3);
	//float volume = ConvexVolume(convex);
}





float testplanes[][4]={
	{1,0,0,-0.5f},
	{-1,0,0,0.5f},
	{1,0,0,-1.0f}, // fully but 1 edge should be neighborless till i get that fixed
	{1,0,0,-2.0f}, // fully intact
	{1,1,0,-SQRT_OF_2 *0.25f},
	{1,1,0,-SQRT_OF_2 *0.50f},
	{1,1,0,-SQRT_OF_2 *0.75f},
	{-1,-1,0,SQRT_OF_2 *0.25f},
	{-1,-1,0,SQRT_OF_2 *0.50f},
	{-1,-1,0,SQRT_OF_2 *0.75f},
};

void printconvex(Convex *convex) {
	int i;
	printf("Vertices\n");
	for(i=0;i<convex->vertices.count;i++){
		float3 &v = float3(convex->vertices[i]);
		printf("  %2d  %5.2f %5.2f %5.2f \n",i,v.x,v.y,v.z);
	}
	printf("Planes\n");
	for(i=0;i<convex->facets.count;i++){
		float3 &v = float3(convex->facets[i].normal);
		printf("  %2d  %5.2f %5.2f %5.2f %5.2f\n",i,v.x,v.y,v.z,convex->facets[i].dist);
	}
	printf("Edges\n");
	printf("  id   p  v  ea\n");
	for(i=0;i<convex->edges.count;i++){
		printf("  %2d  %2d %2d %2d\n",i,(int)convex->edges[i].p,(int)convex->edges[i].v,(int)convex->edges[i].ea);
	}
	printf("\n");

}

int testmain(int,char**){
	int i;
	//assert(sizeof(testplanes)/4/sizeof(float) ==9);
	Convex *convex=test_btbq();
	convex = test_cube();
	AssertIntact(*convex);
	for(i=0;i<sizeof(testplanes)/4/sizeof(float);i++) {
		Convex *under;
		Plane crop(normalize(float3(testplanes[i][0],testplanes[i][1],testplanes[i][2])),testplanes[i][3]);
		printf("Slice:\n");
		printf("  %5.2f %5.2f %5.2f %5.2f\n",crop.normal.x,crop.normal.y,crop.normal.z,crop.dist);
		under = ConvexCrop(*convex,crop);
		AssertIntact(*under);
		printconvex(under);
	}
	return 0;
}

