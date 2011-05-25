
#include <math.h>
#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include "wingmesh.h"
#include "bsp.h"
#include "array.h"
#include "vertex.h"



/*
class WingMesh
{
  public:
	class HalfEdge
	{
	  public:
	    short id;
		short v;
		short adj;
		short next;
		short prev;
		short face;
		short vm;
		HalfEdge(){vm=v=adj=next=prev=face=-1;}
	};
	Array<HalfEdge> edges;
	Array<float3> verts;
	Array<Plane>  faces;
	Array<short>  vback;
	Array<short>  fback;
	int unpacked; // flag indicating if any unused elements within arrays
};
*/



void PolyMeshAlloc(PolyMesh &pm,int _verts_count,int _indices_count, int _sides_count)
{
	pm.verts_count   = _verts_count;
	pm.verts         = new float3[pm.verts_count];
	pm.indices_count = _indices_count;
	pm.indices       = new int[pm.indices_count];
	pm.sides_count   = _sides_count;
	pm.sides         = new int[pm.sides_count];
}

void PolyMeshFree(PolyMesh &pm)
{
	delete pm.indices;
	delete pm.sides;
	delete pm.verts;
	memset(&pm,0,sizeof(pm));
}


static int linkmeshold(WingMesh *m)
{
	// Computes adjacency information for edges.
	// Assumes prev/next pointers are assigned for each edge.
	// Assumes edges have vertices assigned to them.
	int i;
	Array<short> *ve = new Array<short>[m->verts.count]; // for each vert a list of edges that use it
	for(i=0;i<m->edges.count;i++) ve[m->edges[i].v].Add((short)i);
	for(i=0;i<m->edges.count;i++)
	{
		WingMesh::HalfEdge &e = m->edges[i];
		short a = e.v;
		short b = m->edges[e.next].v;
		int k;
		for(k=0;k<ve[b].count;k++)
		{
			if(m->edges[m->edges[ve[b][k]].next].v == a) break;
		}
		if(k==ve[b].count)
		{
			delete ve;
			return 0; // non manifold
		}
		WingMesh::HalfEdge &ce = m->edges[ve[b][k]];
		assert(ce.v==b);
		assert( e.adj==-1 ||  e.adj==ce.id);  // could happen if non-manifold and not deteceted earlier.
		assert(ce.adj==-1 || ce.adj== e.id);
		e.adj=ce.id;
		ce.adj=e.id;
	}
	delete []ve;
	return 1;
}

static WingMesh *sortee=NULL;
static int vindexdiff(const void *_a,const void *_b) {
	short &a = *((short *) _a); 
	short &b = *((short *) _b); 
	return sortee->edges[a].v - sortee->edges[b].v; 
}
static int linkmesh(WingMesh *m)
{
	// Computes adjacency information for edges.
	// Assumes prev/next pointers are assigned for each edge.
	// Assumes edges have vertices assigned to them.
	int i;
	Array<short> edgesv(m->edges.count);  // edges indexes sorted into ascending edge's vertex index order
	for(i=0;i<m->edges.count;i++) edgesv.Add((short)i);
	assert(!sortee);
	sortee=m;
	qsort(edgesv.element,edgesv.count,sizeof(short),vindexdiff);
	sortee=NULL;
	Array<short> veback;
	veback.SetSize(m->verts.count);
	i=m->edges.count;
	while(i--)
	{
		veback[m->edges[edgesv[i]].v]=(short)i;
	}
	for(i=0;i<m->edges.count;i++)
	{
		WingMesh::HalfEdge &e = m->edges[i];
		assert(e.id==i);
		if(e.adj!=-1) continue;
		short a = e.v;
		short b = m->edges[e.next].v;
		int k=veback[b];
		while(k<m->edges.count && m->edges[edgesv[k]].v==b)
		{
			if(m->edges[m->edges[edgesv[k]].next].v == a)
			{
				e.adj = edgesv[k];
				m->edges[e.adj].adj=e.id;
				break;
			}
			k++;
		}
		assert(e.adj!=-1);
	}
	return 1;
}

void WingMeshInitBackLists(WingMesh *m)
{
	int i;
	m->vback.SetSize(m->verts.count);
	for(i=0;i<m->vback.count;i++) m->vback[i]=-1;
	m->fback.SetSize(m->faces.count);
	for(i=0;i<m->fback.count;i++) m->fback[i]=-1;
	i=m->edges.count;
	while(i--)  // going backward so back pointers will point to the first applicable edge
	{
		if(m->unpacked && m->edges[i].v==-1) continue;
		m->vback[m->edges[i].v]=i;
		m->fback[m->edges[i].face]=i;
	}
}

void checkit(WingMesh* wmesh)
{
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	int e,i;
	for(e=0;e<edges.count;e++)
	{
		if(wmesh->unpacked && edges[e].v==-1)
		{
			assert(edges[e].face==-1);
			assert(edges[e].next==-1);
			assert(edges[e].prev==-1);
			continue;
		}
		assert(edges[e].id==e);
		assert(edges[e].v>=0);
		assert(edges[e].face>=0);
		assert(edges[edges[e].next].prev==e);
		assert(edges[edges[e].prev].next==e);
		assert(edges[e].adj !=e);  // antireflexive
		assert(edges[edges[e].adj ].adj ==e);  // symmetric
		assert(edges[e].v == edges[edges[edges[e].adj].next].v);
		assert(edges[e].v != edges[edges[e].adj].v);
	}
	for(i=0;i<wmesh->vback.count;i++) assert((wmesh->unpacked && wmesh->vback[i]==-1) || edges[wmesh->vback[i]].v   ==i);
	for(i=0;i<wmesh->fback.count;i++) assert((wmesh->unpacked && wmesh->fback[i]==-1) || edges[wmesh->fback[i]].face==i);
}

Plane &ComputeNormal(WingMesh *m,int f)
{
	// Newell Normal
	float3 c(0,0,0);
	float3 n(0,0,0);
	int e0 = m->fback[f];
	int e=e0;
	int k=0;
	do
	{
		int en = m->edges[e].next;
		float3 &v0 = m->verts[m->edges[e ].v];
		float3 &v1 = m->verts[m->edges[en].v];
		n.x += (v0.y - v1.y) * (v0.z + v1.z);
		n.y += (v0.z - v1.z) * (v0.x + v1.x);
		n.z += (v0.x - v1.x) * (v0.y + v1.y);
		c += v0;
		e = en;
		k++;
	} while(e!=e0);
	Plane &p = m->faces[f];
	p.normal() = normalize(n);
	p.dist()   = -dot(c,p.normal())/(float)k;
	return p;
}

WingMesh* WingMeshCreate(float3 *verts,int3 *tris,int n)
{
	int i,j;
	if(n==0) return NULL;
	WingMesh *m = new WingMesh();
	int verts_count=-1;
	for(i=0;i<n;i++) for(j=0;j<3;j++)
	{
		verts_count = Max(verts_count,tris[i][j]);
	}
	verts_count++;
	m->verts.SetSize(verts_count);
	for(i=0;i<m->verts.count;i++)
	{
		m->verts[i]=verts[i];
	}
	for(i=0;i<n;i++)
	{
		int3 &t=tris[i];
		WingMesh::HalfEdge e0,e1,e2;
		e0.face=e1.face=e2.face=i;
		int k=m->edges.count;
		e0.id = e1.prev = e2.next = k+0;
		e1.id = e2.prev = e0.next = k+1;
		e2.id = e0.prev = e1.next = k+2;
		e0.v = (short)tris[i][0];
		e1.v = (short)tris[i][1];
		e2.v = (short)tris[i][2];
		m->edges.Add(e0);
		m->edges.Add(e1);
		m->edges.Add(e2);
	}
	m->faces.SetSize(n);
	for(i=0;i<n;i++)
	{
		float3 normal = TriNormal(verts[tris[i][0]],verts[tris[i][1]],verts[tris[i][2]]);
		float  dist   = -dot(normal,verts[tris[i][0]]);
		m->faces[i] = Plane(normal,dist);
	}
	linkmesh(m);
	WingMeshInitBackLists(m);
	return m;
}
static void SwapSlots(WingMesh *m,int a,int b)
{
	Swap(m->edges[a],m->edges[b]);
	m->edges[m->edges[a].prev].next=m->edges[m->edges[a].next].prev=m->edges[a].id=a;
	m->edges[m->edges[b].prev].next=m->edges[m->edges[b].next].prev=m->edges[b].id=b;
}


static void PackSlotEdge(WingMesh *m,int s)
{
	// used to relocate last edge into slot s
	WingMesh::HalfEdge e = m->edges.Pop();
	if(m->edges.count==s) return;
	assert(s<m->edges.count);
	assert(e.v>=0); // last edge is invalid
	if(m->vback.count && m->vback[e.v   ]==e.id) m->vback[e.v   ]=s;
	if(m->fback.count && m->fback[e.face]==e.id) m->fback[e.face]=s;
	e.id=s;
	m->edges[s] = e;
	m->edges[e.next].prev = e.id;
	m->edges[e.prev].next = e.id;
	m->edges[e.adj ].adj  = e.id;
}

static void PackSlotVert(WingMesh *m,int s)
{
	// When you no longer need verts[s], compress away unused vert
	assert(m->vback.count==m->verts.count);
	assert(m->vback[s]==-1);
	int last = m->verts.count-1;
	if(s==last) 
	{
		m->verts.count--;
		m->vback.count--;
		return;
	}
	m->vback[s] = m->vback[last];
	m->verts[s] = m->verts[last];
	int e=m->vback[s];
	assert(e!=-1);
	do{assert( m->edges[e].v==last); m->edges[e].v=s;e=m->edges[m->edges[e].adj].next;} while (e!=m->vback[s]);
	m->verts.count--;
	m->vback.count--;
}

static void PackSlotFace(WingMesh *m,int s)
{
	// When you no longer need faces[s], compress away unused face
	assert(m->fback.count==m->faces.count);
	assert(m->fback[s]==-1);
	int last = m->faces.count-1;
	if(s==last) 
	{
		m->faces.count--;
		m->fback.count--;
		return;
	}
	m->fback[s] = m->fback[last];
	m->faces[s] = m->faces[last];
	int e=m->fback[s];
	assert(e!=-1);
	do{assert( m->edges[e].face==last); m->edges[e].face=s;e=m->edges[e].next;} while (e!=m->fback[s]);
	m->faces.count--;
	m->fback.count--;
}
static void SwapFaces(WingMesh *m,int a,int b)
{
	Swap(m->faces[a],m->faces[b]);
	Swap(m->fback[a],m->fback[b]);
	if(m->fback[a] != -1)
	{
		int e=m->fback[a];
		do{assert( m->edges[e].face==b); m->edges[e].face=a;e=m->edges[e].next;} while (e!=m->fback[a]);
	}
	if(m->fback[b] != -1)
	{
		int e=m->fback[b];
		do{assert( m->edges[e].face==a); m->edges[e].face=b;e=m->edges[e].next;} while (e!=m->fback[b]);
	}
}
static void PackFaces(WingMesh *m)
{
	assert(m->fback.count==m->faces.count);
	int s=0;
	int i;
	for(i=0;i<m->faces.count;i++)
	{
		if(m->fback[i] == -1) continue;
		if(s<i) SwapFaces(m,s,i);
		s++;
	}
	m->fback.count = m->faces.count = s;
}


void WingMeshCompress(WingMesh *m)
{
	// get rid of unused faces and verts
	int i;
	assert(m->vback.count==m->verts.count);
	assert(m->fback.count==m->faces.count);
	i=m->edges.count;
	while(i--) if(m->edges[i].v==-1) PackSlotEdge(m,i);
	i=m->vback.count;
	while(i--) if(m->vback[i]==-1) PackSlotVert(m,i);
	PackFaces(m);
//	i=m->fback.count;
//	while(i--) if(m->fback[i]==-1) PackSlotFace(m,i);
	m->unpacked=0;
	checkit(m);
}



int WingMeshIsDegTwo(WingMesh *m,int v)
{
	assert(m->vback.count);
	int e = m->vback[v];
	return m->edges[e].prev == m->edges[m->edges[m->edges[e].adj].next].adj;
}

static void BackPointElsewhere(WingMesh *m,int e)
{
	// ensure vback and fback reference someone other than e
	WingMesh::HalfEdge &E = m->edges[e];
	assert(E.id==e);
	assert(E.prev!=e);
	assert(m->edges[m->edges[E.prev].adj].v==E.v);
	assert(m->edges[E.prev].face==E.face);
	if(m->vback[E.v   ]==e) m->vback[E.v   ]= m->edges[E.prev].adj;
	if(m->fback[E.face]==e) m->fback[E.face]= E.prev;
}

void WingMeshCollapseEdge(WingMesh *m,int ea,int pack=0)
{
	int eb = m->edges[ea].adj;
	WingMesh::HalfEdge &Ea  = m->edges[ea];
	WingMesh::HalfEdge &Eb  = m->edges[eb];
	WingMesh::HalfEdge &Eap = m->edges[Ea.prev];
	WingMesh::HalfEdge &Ean = m->edges[Ea.next];
	WingMesh::HalfEdge &Ebp = m->edges[Eb.prev];
	WingMesh::HalfEdge &Ebn = m->edges[Eb.next];
	assert(Ea.v>=0);
	BackPointElsewhere(m,Ea.id);
	BackPointElsewhere(m,Eb.id);
	int oldv = m->edges[ea].v;
	int newv = m->edges[eb].v;
	assert(Ean.v==newv);
	assert(Ean.face==Ea.face);
	assert(Ebn.face==Eb.face);
	if(m->vback[newv]==eb) m->vback[newv]=Ean.id;
	if(m->fback[Ea.face]==ea) m->fback[Ea.face]=Ean.id;
	if(m->fback[Eb.face]==eb) m->fback[Eb.face]=Ebn.id;
	int e=ea;
	do{assert(m->edges[e].v==oldv);m->edges[e].v=newv;e=m->edges[m->edges[e].adj].next;} while(e!=ea);
	Eap.next = Ean.id;
	Ean.prev = Eap.id;
	Ebp.next = Ebn.id;
	Ebn.prev = Ebp.id;
	m->vback[oldv]=-1;
	Ea.next=Ea.prev=Ea.face=Ea.adj=Ea.v=-1;
	Eb.next=Eb.prev=Eb.face=Eb.adj=Eb.v=-1;
	if(pack && m->unpacked==0)
	{	
		PackSlotEdge(m,Max(Ea.id,Eb.id));
		PackSlotEdge(m,Min(Ea.id,Eb.id));
		PackSlotVert(m,oldv);
	}
	else m->unpacked=1;
	checkit(m);
}

void RemoveEdge(WingMesh *m,int e,int pack=0)
{
	WingMesh::HalfEdge &Ea = m->edges[e];
	WingMesh::HalfEdge &Eb = m->edges[Ea.adj];
	BackPointElsewhere(m,Ea.id);
	BackPointElsewhere(m,Eb.id);
	int oldface = Ea.face;
	while(m->edges[e].face != Eb.face)
	{
		m->edges[e].face = Eb.face;
		e = m->edges[e].next;
	}
	assert(e==Ea.id);
	m->edges[Ea.prev].next = m->edges[Eb.next].id;
	m->edges[Eb.prev].next = m->edges[Ea.next].id;
	m->edges[Ea.next].prev = m->edges[Eb.prev].id;
	m->edges[Eb.next].prev = m->edges[Ea.prev].id;
	Ea.next=Ea.prev=Ea.face=Ea.adj=Ea.v=-1;
	Eb.next=Eb.prev=Eb.face=Eb.adj=Eb.v=-1;
	m->fback[oldface]=-1;
	if(pack)
	{
		PackSlotFace(m,oldface);
		PackSlotEdge(m,Max(Ea.id,Eb.id));
		PackSlotEdge(m,Min(Ea.id,Eb.id));
	}
	else m->unpacked=1;
	checkit(m);
}

void RemoveEdges(WingMesh *m,int *cull,int cull_count)
{
	int i;
	m->vback.count=0;
	m->fback.count=0;
	m->unpacked=1;
	for(i=0;i<cull_count;i++)
	{
		int e = cull[i];
		WingMesh::HalfEdge &Ea = m->edges[e];
		if(Ea.v==-1) continue; // already snuffed
		WingMesh::HalfEdge &Eb = m->edges[Ea.adj];
		m->edges[Ea.prev].next = m->edges[Eb.next].id;
		m->edges[Eb.prev].next = m->edges[Ea.next].id;
		m->edges[Ea.next].prev = m->edges[Eb.prev].id;
		m->edges[Eb.next].prev = m->edges[Ea.prev].id;
		Ea.next=Ea.prev=Ea.face=Ea.adj=Ea.v=-1;
		Eb.next=Eb.prev=Eb.face=Eb.adj=Eb.v=-1;
	}
	for(i=0;i<m->edges.count;i++)  // share faces now
	{
		WingMesh::HalfEdge &E = m->edges[i];
		if(E.v==-1) continue; // dead edge
		if(E.face==E.Next()->face) continue; // 
		for(int e=E.next; e!=i ; e=m->edges[e].next)
		{
			m->edges[e].face = E.face;
		}
	}
	WingMeshInitBackLists(m);
	checkit(m);
	WingMeshCompress(m);
}

int RemoveD2(WingMesh *m)
{
	int k=0;
	int i=m->edges.count;
	if(m->faces.count<3) return 0;
	while(i--)
	{
		if(i>=m->edges.count) continue;
		if(m->edges[i].prev == m->edges[i].Adj()->Next()->adj)
		{
			WingMeshCollapseEdge(m,i,1);
			k++;
		}
	}
	return k;
}


void WingMeshSort(WingMesh *m)
{
	int i;
	if(m->unpacked) WingMeshCompress(m);
	Array<WingMesh::HalfEdge> s(m->edges.count);
	int k=0;
	for(i=0;i<m->faces.count;i++)
	{
		int e=m->fback[i]; assert(e!=-1);
		do{s.Add(m->edges[e]);m->edges[e].id=s.count-1;e=m->edges[e].next;}while(e!=m->fback[i]);
	}
	for(i=0;i<s.count;i++)
	{
		s[i].id=i;
		s[i].next = m->edges[s[i].next].id;
		s[i].prev = m->edges[s[i].prev].id;
	}
	Swap(s.element,m->edges.element);
	checkit(m);
	for(i=0;i<m->edges.count;i++) {assert(m->edges[i].next<=i+1);}
}



void Twist(WingMesh *m, int e)
{
	
}


WingMesh* WingMeshCreate(float3 *verts,int3 *tris,int n,int *hidden_edges,int hidden_edges_count)
{
	WingMesh *m=WingMeshCreate(verts,tris,n);
	RemoveEdges(m,hidden_edges,hidden_edges_count);
	return m;
}
void WingMeshDelete(WingMesh *m){delete m;}

WingMesh *Dual(WingMesh *m,float r,const float3 &p)
{
	int i;
	WingMesh *d = new WingMesh();
	d->faces.SetSize(m->verts.count);
	d->fback.SetSize(m->vback.count);
	for(i=0;i<m->verts.count;i++)
	{
		d->faces[i] = Plane(normalize(m->verts[i]),r*r/magnitude(m->verts[i]));
		d->fback[i] = m->vback[i];
	}
	d->verts.SetSize(m->faces.count);
	d->vback.SetSize(m->fback.count);
	for(i=0;i<m->faces.count;i++)
	{
		d->verts[i] = m->faces[i].normal()*(r*r/m->faces[i].dist());
		d->vback[i] = m->edges[m->fback[i]].adj;
	}
	d->edges.SetSize(m->edges.count);
	for(i=0;i<m->edges.count;i++)
	{
		d->edges[i] = m->edges[i];
		d->edges[i].face = m->edges[i].v;
		d->edges[i].v = m->edges[m->edges[i].adj].face;
		d->edges[i].next = m->edges[m->edges[i].prev].adj ;
		d->edges[i].prev = m->edges[m->edges[i].adj ].next;
	}
	checkit(d);
	return d;
}

PolyMesh WingMeshToPolyData(WingMesh *m)
{
	int i;
	assert(m->unpacked==0);
	PolyMesh pm;
	PolyMeshAlloc(pm,m->verts.count,m->edges.count,m->faces.count);
	for(i=0;i<m->verts.count;i++)
	{
		pm.verts[i] = m->verts[i];
	}
	assert(m->fback.count==m->faces.count);
	int k=0;
	for(i=0;i<m->faces.count;i++)
	{
		int e0 = m->fback[i];
		int n=0;
		int e=e0;
		do{ n++; pm.indices[k++] = (int) m->edges[e].v ; e = m->edges[e].next;} while (e!=e0);
		pm.sides[i] = n;
	}
	assert(k==pm.indices_count);
	return pm;
}

WingMesh *WingMeshCube(const float3 &bmin,const float3 &bmax)
{
	WingMesh *wm = new WingMesh();
	wm->verts.SetSize(8);
	wm->verts[0] = float3(bmin.x,bmin.y,bmin.z);
	wm->verts[1] = float3(bmin.x,bmin.y,bmax.z);
	wm->verts[2] = float3(bmin.x,bmax.y,bmin.z);
	wm->verts[3] = float3(bmin.x,bmax.y,bmax.z);
	wm->verts[4] = float3(bmax.x,bmin.y,bmin.z);
	wm->verts[5] = float3(bmax.x,bmin.y,bmax.z);
	wm->verts[6] = float3(bmax.x,bmax.y,bmin.z);
	wm->verts[7] = float3(bmax.x,bmax.y,bmax.z);
	wm->faces.SetSize(6);
	wm->faces[0] = Plane(float3(-1,0,0), bmin.x);
	wm->faces[1] = Plane(float3(1,0,0), -bmax.x);
	wm->faces[2] = Plane(float3(0,-1,0), bmin.y);
	wm->faces[3] = Plane(float3(0,1,0), -bmax.y);
	wm->faces[4] = Plane(float3(0,0,-1), bmin.z);
	wm->faces[5] = Plane(float3(0,0,1), -bmax.z);
	wm->edges.SetSize(24);
	wm->edges[0 ] = WingMesh::HalfEdge( 0,0,11, 1, 3,0);
	wm->edges[1 ] = WingMesh::HalfEdge( 1,1,23, 2, 0,0);
	wm->edges[2 ] = WingMesh::HalfEdge( 2,3,15, 3, 1,0);
	wm->edges[3 ] = WingMesh::HalfEdge( 3,2,16, 0, 2,0);
										 
	wm->edges[4 ] = WingMesh::HalfEdge( 4,6,13, 5, 7,1);
	wm->edges[5 ] = WingMesh::HalfEdge( 5,7,21, 6, 4,1);
	wm->edges[6 ] = WingMesh::HalfEdge( 6,5, 9, 7, 5,1);
	wm->edges[7 ] = WingMesh::HalfEdge( 7,4,18, 4, 6,1);
										 
	wm->edges[8 ] = WingMesh::HalfEdge( 8,0,19, 9,11,2);
	wm->edges[9 ] = WingMesh::HalfEdge( 9,4, 6,10, 8,2);
	wm->edges[10] = WingMesh::HalfEdge(10,5,20,11, 9,2);
	wm->edges[11] = WingMesh::HalfEdge(11,1, 0, 8,10,2);
										 
	wm->edges[12] = WingMesh::HalfEdge(12,3,22,13,15,3);
	wm->edges[13] = WingMesh::HalfEdge(13,7, 4,14,12,3);
	wm->edges[14] = WingMesh::HalfEdge(14,6,17,15,13,3);
	wm->edges[15] = WingMesh::HalfEdge(15,2, 2,12,14,3);
										 
	wm->edges[16] = WingMesh::HalfEdge(16,0, 3,17,19,4);
	wm->edges[17] = WingMesh::HalfEdge(17,2,14,18,16,4);
	wm->edges[18] = WingMesh::HalfEdge(18,6, 7,19,17,4);
	wm->edges[19] = WingMesh::HalfEdge(19,4, 8,16,18,4);
										 
	wm->edges[20] = WingMesh::HalfEdge(20,1,10,21,23,5);
	wm->edges[21] = WingMesh::HalfEdge(21,5, 5,22,20,5);
	wm->edges[22] = WingMesh::HalfEdge(22,7,12,23,21,5);
	wm->edges[23] = WingMesh::HalfEdge(23,3, 1,20,22,5);
	WingMeshInitBackLists(wm);
	checkit(wm);
	return wm;
}
class dogan{
public:
	dogan(){WingMeshCrop(WingMeshCube(float3(0,0,0),float3(1,1,1)),Plane(float3(0,0,1),-0.5f));}
	int x;
}ddd;
void Polyize(WingMesh *m,float angle=2.0f)
{
	int i=m->edges.count;
	while(i--) 
	{
		if(i>=m->edges.count || m->edges[i].v<0) continue; // unused
		WingMesh::HalfEdge &Ea = m->edges[i];
		WingMesh::HalfEdge &Eb = m->edges[Ea.adj];
		if(Ea.next==Ea.prev)
		{
			RemoveEdge(m,i,1);
		}
		else if(Eb.next==Eb.prev)
		{
			RemoveEdge(m,Ea.adj,1);
		}
		else if(Ea.prev==Eb.Next()->adj)
		{
			WingMeshCollapseEdge(m,Ea.id,1);
		}
		else if(Eb.prev==Ea.Next()->adj)
		{
			WingMeshCollapseEdge(m,Eb.id,1);
		}
		else if(dot(m->faces[Ea.face].normal(),m->faces[Eb.face].normal())> cosf(DegToRad(angle)))
		{
			RemoveEdge(m,i,1);
		}
	}
}



int degree(WingMesh *m,int v)
{
	int e=m->vback[v];
	if(e==-1) return 0;
	int k=0;
	do{ k++ ; e = m->edges[m->edges[e].adj].next;} while (e!=m->vback[v]);
	return k;
}


void separate(WingMesh *m,Array<int> &edgeloop)
{
	int i;
	assert(edgeloop.count);
	int newedgestart=m->edges.count;
	for(i=0;i<edgeloop.count;i++)
	{
		WingMesh::HalfEdge E;
		E.id = m->edges.count;
		E.next = E.id+1  - (i==edgeloop.count-1)?edgeloop.count:0;
		E.prev = E.id-1  + (i==0)?edgeloop.count:0;
		E.face = m->faces.count;
		E.v    = m->edges[edgeloop[i]].v;
		E.adj  = m->edges[edgeloop[i]].adj;
		m->edges[E.adj].adj = E.id;
		m->vback[E.v] = E.id;
		m->edges.Add(E);
	}
	m->faces.Add(Plane());
	m->fback.Add(newedgestart);
	newedgestart=m->edges.count;
	for(i=0;i<edgeloop.count;i++)
	{
		WingMesh::HalfEdge E;
		E.id = m->edges.count;
		E.prev = E.id+1  - (i==edgeloop.count-1)?edgeloop.count:0;
		E.next = E.id-1  + (i==0)?edgeloop.count:0;
		E.face = m->faces.count;
		E.v    = m->edges[edgeloop[i]].Adj()->v;
		E.adj  = edgeloop[i];
		m->edges[E.adj].adj = E.id;
		m->edges.Add(E);
	}
	m->faces.Add(Plane());
	m->fback.Add(newedgestart);
	for(i=0;i<edgeloop.count;i++)
	{
		int v = m->verts.count;
		int e = edgeloop[i];
		int vold = m->edges[e].v;
		m->verts.Add(m->verts[m->edges[e].v]);
		m->vback.Add(e);
		do{ assert(m->edges[e].v==vold); m->edges[e].v=v; e=m->edges[e].Adj()->next;} while (e!=edgeloop[i]);
	}
}


void WingMeshSeparate(WingMesh *m,Array<int> &edgeloop)
{
	int i;
	Array<int> killstack;
	for(i=0;i<edgeloop.count;i++)
	{
		// dont want to delete vertices along the loop.
		// detach vertices along loop from to-be-deleted edges  
		int ec = edgeloop[i];
		int en = edgeloop[(i+1) %edgeloop.count];
		int e  = m->edges[ec].next;
		while(e!=en)
		{
			assert(m->edges[e].v == m->edges[en].v);
			m->edges[e].v=-1;
			e = m->edges[e].Adj()->next;
		}
		m->vback[m->edges[en].v]=en; // ensure vertex points to edge that wont be deleted.
		m->fback[m->edges[en].face] = -1;  // delete all faces 
		m->edges[en].face = m->edges[edgeloop[0]].face; // assign all loop faces to same which we ressurrect later
	}
	for(i=0;i<edgeloop.count;i++)
	{
		// detatch tobedeleted edges from loop edges.
		// and fix loop's next/prev links.
		int ec = edgeloop[i];
		int en = edgeloop[(i+1) %edgeloop.count];
		int ep = edgeloop[(i)?i-1:edgeloop.count-1];
		WingMesh::HalfEdge &E = m->edges[ec];
		if(E.next != en)
		{
			WingMesh::HalfEdge *K = E.Next();
			if(K->id>=0) killstack.Add(E.next);
			K->id=-1;
			assert(K->v==-1);
			assert(K->prev==E.id);
			K->prev=-1;
			E.next=en;
		}
		if(E.prev != ep)
		{
			WingMesh::HalfEdge *K = E.Prev();
			if(K->id>=0) killstack.Add(E.prev);
			K->id=-1;
			assert(K->next==E.id);
			K->next=-1;
			E.prev=ep;
		}
	}
	while(killstack.count)
	{
		// delete (make "-1") all unwanted edges faces and verts
		int k=killstack.Pop();
		assert(k>=0);
		WingMesh::HalfEdge &K = m->edges[k];
		assert(K.id==-1);
		if(K.next!=-1 && m->edges[K.next].id!=-1) {killstack.Add(K.next);m->edges[K.next].id=-1;}
		if(K.prev!=-1 && m->edges[K.prev].id!=-1) {killstack.Add(K.prev);m->edges[K.prev].id=-1;}
		if(K.adj !=-1 && m->edges[K.adj ].id!=-1) {killstack.Add(K.adj );m->edges[K.adj ].id=-1;}
		if(K.v   !=-1) m->vback[K.v   ]=-1;
		if(K.face!=-1) m->fback[K.face]=-1;
		K.next=K.prev=K.adj=K.v=K.face = -1;
	}
	assert(m->fback[m->edges[edgeloop[0]].face]==-1);
	m->fback[m->edges[edgeloop[0]].face]=edgeloop[0];
	ComputeNormal(m,m->edges[edgeloop[0]].face);
	SwapFaces(m,m->edges[edgeloop[0]].face,0);
	m->unpacked=1;
	checkit(m);
	WingMeshCompress(m);
}

int FaceSideCount(WingMesh *m,int f)
{
	int k=0;
	int e=m->fback[f];
	do{k++;e=m->edges[e].next;} while (e!=m->fback[f]);
	return k;
}
int TopLoop(WingMesh *m,Array<int> &edgeloop)
{
	int i;
	int f=-1;
	int s=0;
	for(i=0;i<m->fback.count;i++)
	{
		if(m->fback[i]==-1) continue;
		int e=m->fback[i];
		int k=0;
		int deg3=1;
		int less5=1;
		do{ k++ ; deg3=(3==degree(m,m->edges[e].v));less5=(FaceSideCount(m,m->edges[e].Adj()->face)<5); e = m->edges[e].next;} while (deg3 && less5&& e!=m->fback[i]);
		if(deg3&&less5&&k>s) {s=k;f=i;}
	}
	if(s<3) return 0;
	assert(f>=0);
	edgeloop.count=0;
	int cstart=m->edges[m->fback[f]].Adj()->Next()->adj;
	while(m->edges[cstart].Next()->Adj()->face==f) { cstart = m->edges[cstart].Adj()->next; }
	int c=cstart;
	do{
		edgeloop.Add(c);
		c = m->edges[c].next;
		while(m->edges[c].Next()->Adj()->face==f) { c = m->edges[c].Adj()->next; }
	} while (c!=cstart);
	if(edgeloop.count<3) return 0;
	return 1;

	
	/*	separate(m,edgeloop);
	Array<int> vbag;
	Array<int> fbag;
	for(i=0;i<edgeloop.count;i++) {vbag.Add(m->edges[i].v);}
	int e = m->fback[f];
	do{ vbag.Add(m->edges[e].v); fbag.Add(m->edges[e].Adj()->face) ; e=m->edges[e].next;} while(e!=m->fback[f]);
	fbag.Add(f);
	fbag.Add(m->faces.count-1);
	for(i=0;i<vbag.count;i++)
	{
		int e=m->vback[vbag[i]];
		do{m->edges[e].v = i; e=m->edges[e].Adj()->next;}while (e!=m->vback[vbag[i]]);
	}
	Array<int> ebag;
	for(i=0;i<fbag.count;i++)
	{
		int e=m->fback[fbag[i]];
		do{m->edges[e].face = i; ebag.Add(e);e=m->edges[e].next;}while (e!=m->fback[fbag[i]]);
	}
	for(i=0;i<ebag.count;i++)
	{
		m->edges[ebag[i]].id=i;
	}
	for(i=0;i<ebag.count;i++)
	{
		m->edges[ebag[i]].next = m->edges[m->edges[ebag[i]].next].id ;
		m->edges[ebag[i]].prev = m->edges[m->edges[ebag[i]].prev].id ;
		m->edges[ebag[i]].adj  = m->edges[m->edges[ebag[i]].adj ].id ;
	}
	WingMesh *top = new WingMesh();
	*/
	return 1;
}


/*
PolyMesh Dual(float3 *verts,int3 *tris,int n, float r)
{
	WingMesh *m = WingMeshCreate(verts,tris,n);
	Polyize(m);
	WingMesh *d = Dual(m,r);
	return WingMeshToPolyData(d);
}
*/
WingMesh *WingMeshDup(WingMesh *src)
{
	WingMesh *m=new WingMesh();
	*m=*src;
	return m;
}

/*
static int crossedge(WingMesh *m,const Plane &slice)
{
// not sure if this is the way i want to implement this
	// assumes m is convex.
	// returns edge e where m->edges[e].v is under or and next->v is over 
	// or where edges[e].v is coplanar and and next->v is over or coplanar.
	// The edge is on a face that is not entirely 'over' the plane.
	// returns -1 if it cant find such an edge.
	int v =0;
	int e;
	if(PlaneTest(slice,m->verts[v])==OVER)
	{
		e = crossedge(m,PlaneFlip(slice));
		if(e==-1) return -1;
		e = m->edges[e].adj;
		// more to do here!!
		return e;
	}
	e =m->vback[v];
	e =
}
static void calcloop(WingMesh *m,int e0,Array<int> &loop,const Plane &slice)
{
	// was to calculate sequence of edges that cucumnavigate a convex along a plane
	loop.count=0;
	loop.Add(e0);
	
}
*/


int Tess(WingMesh* wmesh,int ea,int eb)
{
	// puts an edge from ea.v to eb.v  
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	assert(edges[ea].next != eb);  // make sure they aren't too close
	assert(edges[eb].next != ea);
	assert(edges[eb].face == edges[ea].face);
	int e=ea;
	while(e!=eb)
	{
		e=edges[e].next;
		assert(e!=ea);  // make sure eb is on the same poly
	}
	int newface = wmesh->faces.count;
	WingMesh::HalfEdge sa(edges.count+0, edges[ea].v, edges.count+1, eb,edges[ea].prev, newface       );  // id,v,adj,next,prev,face
	WingMesh::HalfEdge sb(edges.count+1, edges[eb].v, edges.count+0, ea,edges[eb].prev, edges[ea].face);
	edges[sa.prev].next = edges[sa.next].prev = sa.id;
	edges[sb.prev].next = edges[sb.next].prev = sb.id;
	edges.Add(sa);
	edges.Add(sb);
	wmesh->faces.Add(wmesh->faces[edges[ea].face]); 
	if(wmesh->fback.count) {
		wmesh->fback.Add(sa.id);
		wmesh->fback[sb.face]=sb.id;
	}
	e=edges[sa.id].next;
	while(e!=sa.id)
	{
		assert(e!=ea);
		edges[e].face = newface;
		e = edges[e].next;
	}
	return sa.id;
}


void SplitEdge(WingMesh *m,int e,const float3 &vpos)
{
	// Add's new vertex vpos to mesh m
	// Adds two new halfedges to m
	Array<WingMesh::HalfEdge> &edges = m->edges;
	int ea = edges[e].adj;
	int v = m->verts.count;
	WingMesh::HalfEdge s0(edges.count+0,v          ,edges.count+1, edges[e ].next,e  , edges[e ].face);  // id,v,adj,next,prev,face
	WingMesh::HalfEdge sa(edges.count+1,edges[ea].v,edges.count+0, ea ,edges[ea].prev, edges[ea].face);
	edges[s0.prev].next = edges[s0.next].prev = s0.id;
	edges[sa.prev].next = edges[sa.next].prev = sa.id;
	edges[ea].v = v;
	edges.Add(s0);
	edges.Add(sa);
	m->verts.Add(vpos);
	if(m->vback.count) 
	{
		m->vback.Add(s0.id);
		m->vback[sa.v]=sa.id;
	}
}

void SplitEdges(WingMesh *wmesh,const Plane &split)
{
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	int e;
	for(e=0;e<edges.count;e++)
	{
		int ea = edges[e].adj;
		if((PlaneTest(split,wmesh->verts[edges[e].v])|PlaneTest(split,wmesh->verts[edges[ea].v]))==SPLIT)
		{
			float3 vpos = PlaneLineIntersection(split,wmesh->verts[edges[e].v],wmesh->verts[edges[ea].v]);
			assert(PlaneTest(split,vpos)==COPLANAR);
			SplitEdge(wmesh,e,vpos);
		}
	}
}

int findcoplanaredge(WingMesh *m,int v,const Plane &slice)
{
	// assumes the mesh straddles the plane slice.
	// tesselates the mesh if required.
	int e=m->vback[v];
	int es=e;
	while(PlaneTest(slice,m->verts[m->edges[e].Adj()->v])!=UNDER)
	{
		e = m->edges[e].Prev()->Adj()->id;
		assert(e!=es); // if(e==es) return -1; // all edges point over!
	}
	es=e;
	while(PlaneTest(slice,m->verts[m->edges[e].Adj()->v])==UNDER)
	{
		e = m->edges[e].Adj()->Next()->id;
		assert(e!=es); // if(e==es) return -1; // all edges point UNDER!
	}
	int ec=m->edges[e].next;
	while(PlaneTest(slice,m->verts[m->edges[ec].v])!=COPLANAR)
	{
		ec = m->edges[ec].next;
		assert(ec!=e);
	}
	if(ec==m->edges[e].next) {return e;}
	assert(ec!=m->edges[e].prev);
	return Tess(m,e,ec);

}

int findcoplanarvert(WingMesh *m,const Plane &slice)
{
	int i;
	for(i=0;i<m->verts.count;i++)
	{
		if(PlaneTest(slice,m->verts[i])==COPLANAR)
		{
			return i;
		}
	}
	return -1;
}

void WingMeshTess(WingMesh *m,const Plane &slice,Array<int> &loop)
{
	SplitEdges(m,slice);
	loop.count=0;
	int v0 = findcoplanarvert(m,slice);
	if(v0==-1) return; //??
	int v=v0;
	do{
		int e=findcoplanaredge(m,v,slice);
		v = m->edges[e].Adj()->v;
		loop.Add(e);
	} while (v!=v0);

}

WingMesh *WingMeshCrop(WingMesh *_m,const Plane &slice)
{
	Array<int> coplanar; // edges
	int s = WingMeshSplitTest(_m,slice);
	if(s==OVER) return NULL;
	WingMesh *m=WingMeshDup(_m);
	if(s==UNDER) return m;
	WingMeshTess(m,slice,coplanar);
	Array<int> reverse;
	for(int i=0;i<coplanar.count;i++) reverse.Add(m->edges[coplanar[coplanar.count-1-i]].adj);

	if(coplanar.count) WingMeshSeparate(m,reverse);
	checkit(m);
	assert(dot(m->faces[0].normal(),slice.normal()) > 0.99f);
	m->faces[0] = slice;
	return m;
}

int WingMeshSplitTest(const WingMesh *m,const Plane &plane) {
	int flag=0;
	for(int i=0;i<m->verts.count;i++) {
		flag |= PlaneTest(plane,m->verts[i]);
	}
	return flag;
}
void WingMeshTranslate(WingMesh *m,const float3 &offset){
	int i;
	for(i=0;i<m->verts.count;i++) {
		m->verts[i] = m->verts[i]+offset;
	}
	for(i=0;i<m->faces.count;i++) {
		m->faces[i].dist() = m->faces[i].dist() - dot(m->faces[i].normal(),offset);
	}
}

void      WingMeshRotate(WingMesh *m,const Quaternion &rot)
{
	int i;
	for(i=0;i<m->verts.count;i++) {
		m->verts[i] = rotate(rot , m->verts[i]);
	}
	for(i=0;i<m->faces.count;i++) {
		m->faces[i].normal() = rotate(rot, m->faces[i].normal());
	}
}

float WingMeshVolume(WingMesh *m)
{
	if(!m) return 0.0f;
	int i;
	float volume=0.0f;
	for(i=0;i<m->faces.count;i++)
	{
		Plane &p = m->faces[i];
		int e0 = m->fback[i];
		int ea = m->edges[e0].next;
		int eb = m->edges[ea].next;
		float3 &v0 = m->verts[m->edges[e0].v];
		while(eb!=e0)
		{
			float3 &v1 = m->verts[m->edges[ea].v];
			float3 &v2 = m->verts[m->edges[eb].v];
			volume += -p.dist() * dot(p.normal(),cross(v1-v0,v2-v1)) / 6.0f;
			ea=eb;
			eb=m->edges[eb].next;
		}
	}
	return volume;
}

int Convexercise(WingMesh *m, PolyMesh **pm_out, int *pm_count_out)
{
	Array<PolyMesh> pms;
	Array<int> edgeloop;
	Array<int> reverse;

	while(TopLoop(m,edgeloop))
	{
		WingMesh b= *m;
		for(int i=0;i<edgeloop.count;i++) reverse.Add(m->edges[edgeloop[edgeloop.count-1-i]].adj);
		WingMeshSeparate(m,edgeloop);
		RemoveD2(m);
		WingMeshSeparate(&b,reverse);
		pms.Add(WingMeshToPolyData(&b));
		edgeloop.count=reverse.count=0;
	}
	if(m->faces.count > 2 || pms.count==0)
	{
		pms.Add(WingMeshToPolyData(m));
	}
	delete m;
	*pm_out = pms.element; 
	*pm_count_out = pms.count;
	pms.element=NULL;
	return 1;
}


void Ridgeify(WingMesh* m,float crease_angle_degrees)
{
	Array<WingMesh::HalfEdge> &edges = m->edges;
	int e;
	int start_num=edges.count;
	int bevelfacestart = m->faces.count;
	for(e=0;e<start_num;e++)
	{
		int ea = edges[e].adj;
		if(ea<e) 
		{
			assert(dot(m->faces[edges[e].face].normal(),m->faces[edges[ea].face].normal())>=cosf(DegToRad(crease_angle_degrees)));
			continue;
		}
		if(edges[ea].face>=bevelfacestart) continue; // already split
		if( dot(m->faces[edges[e].face].normal(),m->faces[edges[ea].face].normal()) >= cosf(DegToRad(crease_angle_degrees))) continue;
		// Ok here we add a face between e0 and ea.  the added face has 2 verts and 2 edges after this point.
		WingMesh::HalfEdge s0(edges.count+0, edges[ea].v , e   ,edges.count+1,edges.count+1, m->faces.count );  // id,v,adj,next,prev,face
		WingMesh::HalfEdge sa(edges.count+1, edges[e ].v , ea  ,edges.count+0,edges.count+0, m->faces.count );  // id,v,adj,next,prev,face
		edges[ea].adj=sa.id;
		edges[e ].adj=s0.id;
		edges.Add(s0);
		edges.Add(sa);
		m->faces.Add(Plane(float3(0,0,0),0));
		if(m->fback.count) m->fback.Add(s0.id);
	}

	start_num=edges.count;
	for(e=0;e<start_num;e++)
	{
		if(edges[e].face<bevelfacestart) continue;  // edge[e] is a halfedge that lies on a separating splinter face
		//if( m->verts[edges[e].v] == m->verts[edges[edges[e].prev].v] ) continue; 
		int en = edges[edges[e].adj].next;
		assert(edges[en].face<bevelfacestart);
		Array<int> betweenedges;
		while(edges[en].face<bevelfacestart  || en >= start_num)
		{
			betweenedges.Add(en);
			en = edges[edges[en].adj].next;
		}
		assert(en < start_num); // this vertex has been split already along the required path   
		if(en == e) continue;  // hmmm no faceless neighbor
		int v = edges[e].v;
		int vnew = m->verts.count;
		WingMesh::HalfEdge s0(edges.count+0, v   , edges.count+1, e  , edges[e ].prev , edges[e ].face); // id,v,adj,next,prev,face
		WingMesh::HalfEdge sa(edges.count+1, vnew, edges.count+0, en , edges[en].prev , edges[en].face);
		edges.Add(s0);
		edges.Add(sa);
		edges[sa.next].prev = edges[sa.prev].next = sa.id; // edge sa
		edges[s0.next].prev = edges[s0.prev].next = s0.id; 
		edges[e].v = vnew;
		while(betweenedges.count) {edges[betweenedges.Pop()].v=vnew;}
		m->verts.Add(m->verts[v]);
		if(m->vback.count) 
		{
			m->vback.Add(sa.id);
			m->vback[v]=s0.id;
		}
		if(m->verts[edges[en].v] != m->verts[edges[edges[en].next].v]) continue;
		assert(sa.prev != edges[en].next);
		//Tess(m,sa.id,edges[en].next);
	}
}

int WingMeshTris(const WingMesh *m,Array<int3> &tris_out)
{
	int i;
	int predict=m->edges.count-m->faces.count*2;
	if(predict>0) tris_out.SetSize(predict);
	tris_out.count=0;
	for(i=0;i<m->faces.count;i++)
	{
		int e0 = m->fback[i];
		if(e0==-1) continue;
		int ea = m->edges[e0].next;
		int eb = m->edges[ea].next;
		while(eb!=e0)
		{
			tris_out.Add(int3(m->edges[e0].v,m->edges[ea].v,m->edges[eb].v));
			ea=eb;
			eb = m->edges[ea].next;
		}
	}
	return tris_out.count;
}

/*
static int find_min_indep( const Array<int> &offlimit, WingMesh *m)
{
	// returns vertex independant of others at this level with minimum degree
	int i;
	int dmin=99;
	int best=-1;
	assert(level.count==m->verts.count);
	for(i=0;i<m->verts.count;i++)
	{
		if(offlimit[i]) continue;
		int e=m->vback[i];
		if(e==-1) continue; // vertex not used in m.
		int d = degree(m,i);
		if(d>=dmin) continue;
		best = i;
		dmin = d;
	}
	return best;
}
void DK(WingMesh *m)
{
	int i;
	Array<int> level(m->verts.count);
	for(i=0;i<m->verts.count;i++)
	{
		level.Add(0);
	}
	Array<WingMesh*> hierarchy;

}
*/

/*
WingMesh *CapOf(WingMesh *m,int f)
{
	WingMesh *top = new WingMesh();
	int e=m->fback[f];
	assert(m->edges[e].face==f);

}


void Slice(WingMesh *wmesh,const Plane &split)
{
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	SplitEdges(wmesh,split);
	int e;
	for(e=0;e<edges.count;e++)
	{
		if(0!=PlaneTest(split,wmesh->verts[edges[e].v])) continue;
		if(SPLIT!=(PlaneTest(split,wmesh->verts[edges[e].v])|PlaneTest(split,wmesh->verts[edges[e].v])))continue;
		int eb = edges[e].next;
		while(PlaneTest(split,wmesh->verts[edges[eb].v]))
		{
			eb=edges[eb].next;
			assert(eb!=e);
		}
		Tess(wmesh,e,eb);
	}
	checkit(wmesh);
}
*/
void RidgeIt(WingMesh* wmesh,float crease_angle_degrees)
{
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	int e;
	int start_num=edges.count;
	for(e=0;e<start_num;e++)
	{
		assert(edges[e].face);
		int ea = edges[e].adj;
		if(ea<e) 
		{
			assert(dot(wmesh->faces[edges[e].face].normal(),wmesh->faces[edges[ea].face].normal())>=cosf(DegToRad(crease_angle_degrees)));
			continue;
		}
		if(edges[ea].face==NULL) continue; // already split
		if(dot(wmesh->faces[edges[e].face].normal(),wmesh->faces[edges[ea].face].normal())>=cosf(DegToRad(crease_angle_degrees))) continue;
		WingMesh::HalfEdge s0; // index will be edges.count+0
		WingMesh::HalfEdge sa; // index will be edges.count+1
		edges[ea].adj=s0.next=s0.prev=edges.count+1; // edge sa
		edges[e ].adj=sa.next=sa.prev=edges.count+0; // edge s0
		sa.adj = ea;
		s0.adj = e;
		s0.v = edges[ea].v;
		sa.v = edges[e ].v;
		edges.Add(s0);
		edges.Add(sa);
	}

	start_num=edges.count;
	for(e=0;e<start_num;e++)
	{
		if(edges[e].face!=NULL || wmesh->verts[edges[e].v] == wmesh->verts[edges[edges[e].prev].v] ) continue;
		int en = edges[edges[e].adj].next;
		assert(edges[en].face);
		Array<int> betweenedges;
		while(edges[en].face)
		{
			betweenedges.Add(en);
			en = edges[edges[en].adj].next;
		}
		if(en == e) continue;  // hmmm no faceless neighbor
		int v = edges[e].v;
		int vnew = wmesh->verts.count;
		wmesh->verts.Add(wmesh->verts[v]);
		WingMesh::HalfEdge s0; // index will be edges.count+0
		WingMesh::HalfEdge sa; // index will be edges.count+1
		s0.v = v;
		s0.next = e;
		s0.prev = edges[e].prev;
		edges[e].v = sa.v = vnew;
		while(betweenedges.count) {edges[betweenedges.Pop()].v=vnew;}
		sa.next = en;
		sa.prev = edges[en].prev;
		edges[en].prev = edges[edges[en].prev].next = s0.adj = edges.count+1; // edge sa
		edges[e ].prev = edges[edges[e ].prev].next = sa.adj = edges.count+0; // edge s0
		edges.Add(s0);
		edges.Add(sa);
		if(wmesh->verts[edges[en].v] != wmesh->verts[edges[edges[en].next].v]) continue;
		assert(sa.prev != edges[en].next);
		Tess(wmesh,edges.count-1,edges[en].next);
	}
}

int VertFindOrAdd(Array<float3> &array, const float3& v,float epsilon=0.001f)
{
	int i;
	for(i=0;i<array.count;i++)
	{
		if(magnitude(array[i]-v)<epsilon) 
			return i;
	}
	array.Add(v);
	return array.count-1;
}

WingMesh* WingMeshCreate(Array<Face*> faces)
{
	int i,j;
	WingMesh *wmesh = new WingMesh();
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	// make em
	wmesh->faces.SetSize(faces.count);
	for(i=0;i<faces.count;i++)
	{
		int base = edges.count;
		Face *f0 = faces[i];
		assert(f0->vertex.count);
		wmesh->faces[i] = Plane(f0->normal(),f0->dist());
		for(j=0;j<f0->vertex.count;j++)
		{
			int j1 = (j+1)%f0->vertex.count;
			int jm1 = (j+f0->vertex.count-1)%f0->vertex.count;
			WingMesh::HalfEdge e;
			assert(base+j==edges.count);
			e.id = base+j;
			e.next = base+j1;
			e.prev = base+jm1;
			e.face = i;
			e.v = VertFindOrAdd(wmesh->verts, f0->vertex[j]);
			edges.Add(e);
		}
	}
	WingMeshInitBackLists(wmesh);
	int rc=linkmesh(wmesh); 	// connect em
	assert(rc);
	checkit(wmesh);
	return wmesh;
}

//--------- mass properties ----------

float Volume(const WingMesh* mesh)
{
	Array<int3> tris;
	WingMeshTris(mesh,tris);
	const float3 *verts = mesh->verts.element;
	return Volume(verts,tris.element,tris.count);
}

float Volume(const Array<WingMesh*> &meshes)
{
	float  vol=0;
	for(int i=0;i<meshes.count;i++)
	{
		vol+=Volume(meshes[i]);
	}
	return vol;
}

float3 CenterOfMass(const Array<WingMesh*> &meshes)
{
	float3 com(0,0,0);
	float  vol=0;
	for(int i=0;i<meshes.count;i++)
	{
		Array<int3> tris;
		WingMeshTris(meshes[i],tris);
		const float3 *verts = meshes[i]->verts.element;
		float3 cg = CenterOfMass(verts,tris.element,tris.count);
		float   v = Volume(verts,tris.element,tris.count);
		vol+=v;
		com+= cg*v;
	}
	com /= vol;
	return com;
}


float3x3 Inertia(const Array<WingMesh*> &meshes, const float3& com)
{
	float  vol=0;
	float3x3 inertia(0,0,0,0,0,0,0,0,0);
	for(int i=0;i<meshes.count;i++)
	{
		Array<int3> tris;
		WingMeshTris(meshes[i],tris);
		const float3 *verts = meshes[i]->verts.element;
		float v = Volume(verts,tris.element,tris.count);
		inertia += Inertia(verts,tris.element,tris.count,com) * v;
		vol+=v;
	}
	inertia *= 1.0f/vol;
	return inertia;
}

//-----------------------


static void PackVertList(DataMesh *datamesh,Array<Vertex> &vertices)
{
	// makes a packed verticies list for 'datamesh' based on ones used by datamesh's triangles
	assert(datamesh->vertices.count==0);
	int i,j;
	Array<int> freq(vertices.count);
	Array<int> map(vertices.count);
	for(i=0;i<vertices.count;i++)
	{
		freq.Add(0);
		map.Add(-1);
	}
	assert(freq.count==vertices.count);
	for(i=0;i<datamesh->tris.count;i++) 
	 for(j=0;j<3;j++)
	{
		freq[datamesh->tris[i][j]]++;
	}
	for(i=0;i<freq.count;i++)
	{
		if(freq[i])
		{
			map[i] = datamesh->vertices.count;
			datamesh->vertices.Add(vertices[i]);
		}
	}
	for(i=0;i<datamesh->tris.count;i++) 
	 for(j=0;j<3;j++)
	{
		datamesh->tris[i][j] = map[datamesh->tris[i][j]];
		assert(datamesh->tris[i][j]!=-1);
		assert(datamesh->tris[i][j]<datamesh->vertices.count);
	}
}


/*
DataModel *MakeDataModel(WingMesh* wmesh,Array<Face*> &faces)
{
	int i,e0;
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	Array<int> mapback;
	DataModel *datamodel = new DataModel;
	DataMesh *fullmesh = new DataMesh();
	datamodel->datameshes.Add(fullmesh);
	fullmesh->matid=-1;
	Array<DataMesh*> dms;
	for(i=0;i<wmesh->verts.count;i++)
	{
		Vertex v;
		v.position = wmesh->verts[i];
		fullmesh->vertices.Add(v);
	}
	// materialseparation:
	Array<short> edgesvm(edges.count);
	for(i=0;i<edges.count;i++) 
	{
		edgesvm.Add(-1);
	}
	for(e0=0 ; e0<edges.count;e0++)
	{
		if(edgesvm[e0]>=0) continue;  // already initialized 

		edgesvm[e0] = edges[e0].v;
		//Vertex &vt = fullmesh->vertices[edges[e0].v];
		int en = e0;
		Array<float2> texcoords;
		do 
		{
			Face *f =  (faces.count>edges[en].face)?faces[edges[en].face] : NULL;
			if(f)
			{
				fullmesh->vertices[edges[e0].v].normal() += f->normal * FaceArea(f);
				float3 v = fullmesh->vertices[edges[e0].v].position;
				float2 tc(f->ot.x+dot(v,f->gu),f->ot.y+dot(v,f->gv)); 
				for(i=0;i<texcoords.count;i++)
				{
					float2 diff = tc-texcoords[i];
					float m = sqrtf(diff.x*diff.x+diff.y*diff.y);
					if(m < 0.05f) break;
				}
				if(i==texcoords.count) 
				{
					texcoords.Add(tc);
					if(i) 
					{
						fullmesh->vertices.Add(fullmesh->vertices[edges[e0].v]);
					}
				}
				int k=(i)?fullmesh->vertices.count-texcoords.count+i :  edges[en].v;
				edgesvm[en] = k;
				fullmesh->vertices[k].texcoord  = tc;
				fullmesh->vertices[k].binormal += safenormalize(f->gv);
				fullmesh->vertices[k].tangent  += safenormalize(f->gu);
			}
			else
			{
				edgesvm[en] = edges[en].v;
			}
			assert(edges[en].v==edges[e0].v);
			en = edges[edges[en].adj].next;	
			assert(en>=e0);
		}while(en!=e0);
		fullmesh->vertices[edges[e0].v].normal   = safenormalize(fullmesh->vertices[edges[e0].v].normal);
		fullmesh->vertices[edges[e0].v].binormal = safenormalize(cross(fullmesh->vertices[edges[e0].v].normal,fullmesh->vertices[edges[e0].v].tangent));
		fullmesh->vertices[edges[e0].v].tangent  = safenormalize(cross(fullmesh->vertices[edges[e0].v].binormal,fullmesh->vertices[edges[e0].v].normal ));
		for(i=1;i<texcoords.count;i++) // yes, start at 1 not 0
		{
			Vertex &vk = fullmesh->vertices[fullmesh->vertices.count-i];
			vk.normal = fullmesh->vertices[edges[e0].v].normal;
			vk.binormal = safenormalize(cross(vk.normal,vk.tangent));
			vk.tangent  = safenormalize(cross(vk.binormal,vk.normal ));
		}
	}
	for(e0=0 ; e0<edges.count;e0++)
	{
		int en = edges[e0].next;
		while(en>e0) en=edges[en].next;
		if(en<e0) continue;
		// e0 is the lowest numbered edge in the loop
		DataMesh *dm=NULL;
		if(edges[e0].face!=-1 && edges[e0].face<faces.count)
		{
			for(i=0;i<dms.count;i++)
			{
				if(dms[i]->matid == faces[edges[e0].face]->matid) break;
			}
			if(i==dms.count) dms.Add(new DataMesh);
			dm = dms[i];
			dm->matid= faces[edges[e0].face]->matid;
		}
		int en1 = edges[e0].next;
		en = edges[en1].next;
		while(en != e0)
		{
			fullmesh->tris.Add(short3(edges[e0].v,edges[en1].v,edges[en].v));
			if(dm) dm->tris.Add(short3(edgesvm[e0],edgesvm[en1],edgesvm[en]));
			en1=en;
			en=edges[en].next;
		}
	}
	for(i=0;i<dms.count;i++)
	{
		PackVertList(dms[i],fullmesh->vertices);
		datamodel->datameshes.Add(dms[i]);
	}
	return datamodel;
}
DataModel *MakeDataModel(Array<Face*> &faces,float crease_angle)
{
	WingMesh *m = WingMeshCreate(faces);
	Ridgeify(m,crease_angle);
	checkit(m);
	DataModel *model = MakeDataModel(m,faces);
	WingMeshDelete(m);
	return model;
}

DataModel *MakeDataModel(BSPNode *bsp,float crease_angle)
{
	Array<Face*> faces;
	BSPGetBrep(bsp,faces);
	return MakeDataModel(faces,crease_angle);
}
*/

DataMesh *MakeSmoothMesh(WingMesh *wmesh)
{
	int i;
	assert(wmesh);
	Array<WingMesh::HalfEdge> &edges = wmesh->edges;
	Array<int> mapback;
	DataMesh *fullmesh = new DataMesh();
	fullmesh->matid=-1;
	for(i=0;i<wmesh->verts.count;i++)
	{
		Vertex vertex;
		vertex.position = wmesh->verts[i];
		int e0 = wmesh->vback[i];
		assert(edges[e0].v==i);
		float3 &v = wmesh->verts[edges[e0].v];
		int e  = e0;
        int en = edges[edges[e].prev].adj;
		float a=0;
		do {
			float3 &va = wmesh->verts[edges[edges[e ].adj].v];
			float3 &vb = wmesh->verts[edges[edges[en].adj].v];
			//vertex.normal += cross(va-v,vb-v);   // doesn't always work since we can have a vertex between two collinear edges.
			vertex.normal += wmesh->faces[edges[e].face].normal(); // note: zero area bevel faces have 0 normal.  optional: could be weighted by angle between edges.  
			e=en;
			en = edges[edges[e].prev].adj;
		} while(e!=e0);
		vertex.normal = safenormalize(vertex.normal);
		vertex.orientation = RotationArc(float3(0,0,1),vertex.normal);
		fullmesh->vertices.Add(vertex);
	}
	for(i=0;i<wmesh->fback.count;i++)
	{
		int e0 = wmesh->fback[i]; // e0 is the first edge of this n-gon face
		int en1 = edges[e0].next;
		int en = edges[en1].next;
		while(en != e0)
		{
			fullmesh->tris.Add(short3(edges[e0].v,edges[en1].v,edges[en].v));
			en1=en;
			en=edges[en].next;
		}
	}
	return fullmesh;
}

DataMesh *MakeRigedMesh(Array<Face*> &faces,float crease_angle)
{
	WingMesh *m = WingMeshCreate(faces);
	Ridgeify(m,crease_angle);
	checkit(m);
	DataMesh *mesh = MakeSmoothMesh(m);
	WingMeshDelete(m);
	return mesh;
}

int WingMeshToFaces(WingMesh *m,Array<Face*> &faces)
{
	int i;
	assert(m->unpacked==0);
	assert(m->fback.count==m->faces.count);
	int k=0;
	for(i=0;i<m->faces.count;i++)
	{
		Face *face = new Face();
		faces.Add(face);
		face->normal() = m->faces[i].normal();
		extern void texplanar(Face *face);
		texplanar(face);
		int e0 = m->fback[i];
		int e=e0;
		do{ face->vertex.Add(m->verts[m->edges[e].v]);  e = m->edges[e].next;} while (e!=e0);
	}
	return faces.count;
}
