
//
// hull and convex utility routines 
//
//
#include <assert.h>
#include "vecmath.h"
#include "array.h"




int3 roll3(int3 a) 
{
	int tmp=a[0];
	a[0]=a[1];
	a[1]=a[2];
	a[2]=tmp;
	return a;
}
int isa(const int3 &a,const int3 &b) 
{
	return ( a==b || roll3(a)==b || a==roll3(b) );
}
int b2b(const int3 &a,const int3 &b) 
{
	return isa(a,int3(b[2],b[1],b[0]));
}
int above(float3* vertices,const int3& t, const float3 &p, float epsilon) 
{
	float3 n=TriNormal(vertices[t[0]],vertices[t[1]],vertices[t[2]]);
	return (dot(n,p-vertices[t[0]]) > epsilon); // EPSILON???
}
int hasedge(const int3 &t, int a,int b)
{
	for(int i=0;i<3;i++)
	{
		int i1= (i+1)%3;
		if(t[i]==a && t[i1]==b) return 1;
	}
	return 0;
}
int hasvert(const int3 &t, int v)
{
	return (t[0]==v || t[1]==v || t[2]==v) ;
}
int shareedge(const int3 &a,const int3 &b)
{
	int i;
	for(i=0;i<3;i++)
	{
		int i1= (i+1)%3;
		if(hasedge(a,b[i1],b[i])) return 1;
	}
	return 0;
}

class Tri;

Array<Tri*> tris;

class Tri : public int3
{
public:
	int3 n;
	int id;
	int vmax;
	float rise;
	Tri(int a,int b,int c):int3(a,b,c),n(-1,-1,-1)
	{
		id = tris.count;
		tris.Add(this);
		vmax=-1;
		rise = 0.0f;
	}
	~Tri()
	{
		assert(tris[id]==this);
		tris[id]=NULL;
	}
	int &neib(int a,int b);
};


int &Tri::neib(int a,int b)
{
	static int er=-1;
	int i;
	for(i=0;i<3;i++) 
	{
		int i1=(i+1)%3;
		int i2=(i+2)%3;
		if((*this)[i]==a && (*this)[i1]==b) return n[i2];
		if((*this)[i]==b && (*this)[i1]==a) return n[i2];
	}
	assert(0);
	return er;
}
void b2bfix(Tri* s,Tri*t)
{
	int i;
	for(i=0;i<3;i++) 
	{
		int i1=(i+1)%3;
		int i2=(i+2)%3;
		int a = (*s)[i1];
		int b = (*s)[i2];
		assert(tris[s->neib(a,b)]->neib(b,a) == s->id);
		assert(tris[t->neib(a,b)]->neib(b,a) == t->id);
		tris[s->neib(a,b)]->neib(b,a) = t->neib(b,a);
		tris[t->neib(b,a)]->neib(a,b) = s->neib(a,b);
	}
}

void removeb2b(Tri* s,Tri*t)
{
	b2bfix(s,t);
	delete s;
	delete t;
}

void checkit(Tri *t)
{
	int i;
	assert(tris[t->id]==t);
	for(i=0;i<3;i++)
	{
		int i1=(i+1)%3;
		int i2=(i+2)%3;
		int a = (*t)[i1];
		int b = (*t)[i2];
		assert(a!=b);
		assert( tris[t->n[i]]->neib(b,a) == t->id);
	}
}
void extrude(Tri *t0,int v)
{
	int3 t= *t0;
	int n = tris.count;
	Tri* ta = new Tri(v,t[1],t[2]);
	ta->n = int3(t0->n[0],n+1,n+2);
	tris[t0->n[0]]->neib(t[1],t[2]) = n+0;
	Tri* tb = new Tri(v,t[2],t[0]);
	tb->n = int3(t0->n[1],n+2,n+0);
	tris[t0->n[1]]->neib(t[2],t[0]) = n+1;
	Tri* tc = new Tri(v,t[0],t[1]);
	tc->n = int3(t0->n[2],n+0,n+1);
	tris[t0->n[2]]->neib(t[0],t[1]) = n+2;
	checkit(ta);
	checkit(tb);
	checkit(tc);
	if(hasvert(*tris[ta->n[0]],v)) removeb2b(ta,tris[ta->n[0]]);
	if(hasvert(*tris[tb->n[0]],v)) removeb2b(tb,tris[tb->n[0]]);
	if(hasvert(*tris[tc->n[0]],v)) removeb2b(tc,tris[tc->n[0]]);
	delete t0;

}

Tri *extrudable(float epsilon)
{
	int i;
	Tri *t=NULL;
	for(i=0;i<tris.count;i++)
	{
		if(!t || (tris[i] && t->rise<tris[i]->rise))
		{
			t = tris[i];
		}
	}
	return (t->rise >epsilon)?t:NULL ;
}



int4 FindSimplex(float3 *verts,int verts_count)
{
	float3 basis[3];
	basis[0] = float3( 0.01f, 0.02f, 1.0f );      
	int p0 = maxdir(verts,verts_count, basis[0]);   
	int	p1 = maxdir(verts,verts_count,-basis[0]);
	basis[0] = verts[p0]-verts[p1];
	if(p0==p1 || basis[0]==float3(0,0,0)) 
		return int4(-1,-1,-1,-1);
	basis[1] = cross(float3(1,0,0),basis[0]);
	basis[2] = cross(float3(0,1,0),basis[0]);
	basis[1] = normalize( (magnitude(basis[1])>magnitude(basis[2])) ? basis[1]:basis[2]);
	int p2 = maxdir(verts,verts_count,basis[1]);
	if(p2 == p0 || p2 == p1)
	{
		p2 = maxdir(verts,verts_count,-basis[1]);
	}
	if(p2 == p0 || p2 == p1) 
		return int4(-1,-1,-1,-1);
	basis[1] = verts[p2] - verts[p0];
	basis[2] = cross(basis[1],basis[0]);
	int p3 = maxdir(verts,verts_count,basis[2]);
	if(p3==p0||p3==p1||p3==p2) p3 = maxdir(verts,verts_count,-basis[2]);
	if(p3==p0||p3==p1||p3==p2) 
		return int4(-1,-1,-1,-1);
	assert(!(p0==p1||p0==p2||p0==p3||p1==p2||p1==p3||p2==p3));
	if(dot(verts[p3]-verts[p0],cross(verts[p1]-verts[p0],verts[p2]-verts[p0])) <0) {Swap(p2,p3);}
	return int4(p0,p1,p2,p3);
}


int calchull(float3 *verts,int verts_count, int3 *&tris_out, int &tris_count,int vlimit) 
{
	int i;
	if(verts_count <4) return 0;
	if(vlimit==0) vlimit=1000000000;
	int j;
	float3 bmin(*verts),bmax(*verts);
	Array<int> isextreme(verts_count);
	for(j=0;j<verts_count;j++) 
	{
		isextreme.Add(0);
		bmin = VectorMin(bmin,verts[j]);
		bmax = VectorMax(bmax,verts[j]);
	}
	float epsilon = magnitude(bmax-bmin) * 0.001f;


	int4 p = FindSimplex(verts,verts_count);
	if(p.x==-1) return 0; // simplex failed

	float3 center = (verts[p[0]]+verts[p[1]]+verts[p[2]]+verts[p[3]]) /4.0f;  // a valid interior point
	Tri *t0 = new Tri(p[2],p[3],p[1]); t0->n=int3(2,3,1);
	Tri *t1 = new Tri(p[3],p[2],p[0]); t1->n=int3(3,2,0);
	Tri *t2 = new Tri(p[0],p[1],p[3]); t2->n=int3(0,1,3);
	Tri *t3 = new Tri(p[1],p[0],p[2]); t3->n=int3(1,0,2);
	isextreme[p[0]]=isextreme[p[1]]=isextreme[p[2]]=isextreme[p[3]]=1;
	checkit(t0);checkit(t1);checkit(t2);checkit(t3);

	for(j=0;j<tris.count;j++)
	{
		Tri *t=tris[j];
		assert(t);
		assert(t->vmax<0);
		float3 n=TriNormal(verts[(*t)[0]],verts[(*t)[1]],verts[(*t)[2]]);
		t->vmax = maxdir(verts,verts_count,n);
		t->rise = dot(n,verts[t->vmax]-verts[(*t)[0]]);
	}
	Tri *te;
	vlimit-=4;
	while(vlimit >0 && (te=extrudable(epsilon)))
	{
		int3 ti=*te;
		int v=te->vmax;
		assert(!isextreme[v]);  // wtf we've already done this vertex
		isextreme[v]=1;
		//if(v==p0 || v==p1 || v==p2 || v==p3) continue; // done these already
		j=tris.count;
		int newstart=j;
		while(j--) {
			if(!tris[j]) continue;
			int3 t=*tris[j];
			if(above(verts,t,verts[v],0.01f*epsilon)) 
			{
				extrude(tris[j],v);
			}
		}
		// now check for those degenerate cases where we have a flipped triangle or a really skinny triangle
		j=tris.count;
		while(j--)
		{
			if(!tris[j]) continue;
			if(!hasvert(*tris[j],v)) break;
			int3 nt=*tris[j];
			if(above(verts,nt,center,0.01f*epsilon)  || magnitude(cross(verts[nt[1]]-verts[nt[0]],verts[nt[2]]-verts[nt[1]]))< epsilon*epsilon*0.1f )
			{
				Tri *nb = tris[tris[j]->n[0]];
				assert(nb);assert(!hasvert(*nb,v));assert(nb->id<j);
				extrude(nb,v);
				j=tris.count; 
			}
		} 
		j=tris.count;
		while(j--)
		{
			Tri *t=tris[j];
			if(!t) continue;
			if(t->vmax>=0) break;
			float3 n=TriNormal(verts[(*t)[0]],verts[(*t)[1]],verts[(*t)[2]]);
			t->vmax = maxdir(verts,verts_count,n);
			if(isextreme[t->vmax]) 
			{
				t->vmax=-1; // already done that vertex - algorithm needs to be able to terminate.
			}
			else
			{
				t->rise = dot(n,verts[t->vmax]-verts[(*t)[0]]);
			}
		}
		vlimit --;
	}
	Array<int3> ts;
	for(int i=0;i<tris.count;i++)if(tris[i])
	{
		ts.Add((*tris[i]));
		delete tris[i];
	}
	Array<int> used;
	Array<int> map;
	int n=0;
	for(i=0;i<verts_count;i++){ used.Add(0);map.Add(0);}
	for(i=0;i<ts.count;i++  )for(j=0;j<3;j++){used[(ts[i])[j]]++;}
	for(i=0;i<used.count;i++){if(used[i]) verts[map[i]=n++]=verts[i];else map[i]=-1;}
	for(i=0;i<ts.count;i++  )for(j=0;j<3;j++){(ts[i])[j] = map[(ts[i])[j]];}
	tris_count = ts.count;
	tris_out   = ts.element;
	ts.element=NULL; ts.count=ts.array_size=0;
	tris.count=0;
	return 1;
}

