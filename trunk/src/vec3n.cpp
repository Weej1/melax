//
// Big Vector and Sparse Matrix Classes
// 

#include <float.h>

#include "vec3n.h"
#include "console.h"


float conjgrad_lasterror;
float conjgrad_epsilon = 0.02f;
int   conjgrad_loopcount;
int   conjgrad_looplimit = 100;
EXPORTVAR(conjgrad_lasterror);
EXPORTVAR(conjgrad_epsilon  );
EXPORTVAR(conjgrad_loopcount);
EXPORTVAR(conjgrad_looplimit);

int  ConjGradient(float3N &X, float3Nx3N &A, float3N &B)
{
	// Solves for unknown X in equation AX=B
	conjgrad_loopcount=0;
	int n=B.count;
	float3N q(n),d(n),tmp(n),r(n); 
	r = B - Mul(tmp,A,X);    // just use B if X known to be zero
	d = r;
	float s = dot(r,r);
	float starget = s * squared(conjgrad_epsilon);
	while( s>starget && conjgrad_loopcount++ < conjgrad_looplimit)
	{
		Mul(q,A,d); // q = A*d;
		float a = s/dot(d,q);
		X = X + d*a;
		if(conjgrad_loopcount%50==0) 
		{
			r = B - Mul(tmp,A,X);  
		}
		else 
		{
			r = r - q*a;
		}
		float s_prev = s;
		s = dot(r,r);
		d = r+d*(s/s_prev);
	}
	conjgrad_lasterror = s;
	return conjgrad_loopcount<conjgrad_looplimit;  // true means we reached desired accuracy in given time - ie stable
}




static inline void filter(float3N &V,const float3Nx3N &S)
{
	for(int i=0;i<S.blocks.count;i++)
	{
		V[S.blocks[i].r] = V[S.blocks[i].r]*S.blocks[i].m;
	}
}

static inline void filterH(float3N &V,Array<HalfConstraint> &H)
{
	for(int i=0;i<H.count;i++)
	{
		float3& v = V[H[i].vi];
		float d=dot(v,H[i].n)-H[i].t;
		if(d<0.0f)
			v += H[i].n * -d;
	}
}




int  ConjGradientFiltered(float3N &X, const float3Nx3N &A, const float3N &B,const float3Nx3N &S,Array<HalfConstraint> &H)
{

	// Solves for unknown X in equation AX=B
	conjgrad_loopcount=0;
	int n=B.count;
	float3N q(n),d(n),tmp(n),r(n); 
	//r = B - A*X;   // just set r to B if X known to be zero
	tmp = A*X;
	r   = B-tmp;
	filter(r,S);
	d = r;
	float s = dot(r,r);
	float starget = s * squared(conjgrad_epsilon);
	while( s>starget && conjgrad_loopcount++ < conjgrad_looplimit)
	{
		q = A*d;
		filter(q,S);
		float a = s/dot(d,q);
		X = X + d*a;
		filterH(X,H);
		if(H.count || conjgrad_loopcount%50==0) 
		{
			tmp = A*X;  // r = B - A*X   // Mul(tmp,A,X); 
			r = B-tmp;
			filter(r,S);
		}
		else 
		{
			r = r - q*a;
		}
		float s_prev = s;
		s = dot(r,r);
		d = r+d*(s/s_prev);
		filter(d,S);
	}
	conjgrad_lasterror = s;
	return conjgrad_loopcount<conjgrad_looplimit;  // true means we reached desired accuracy in given time - ie stable
}


