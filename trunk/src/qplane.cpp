
#include <stdlib.h> // for rand()
#include "vecmath.h"
#include "qplane.h"
#include "console.h"


float qsnap = 0.25f; // turns out to be 1/4th this value in practice for axial aligned planes
EXPORTVAR(qsnap);
int qcuberadius=4;
EXPORTVAR(qcuberadius);

unsigned int CubeSide(const float3 &v)
{
	unsigned char side=0;
	float3 a = vabs(v);	
	for(int i=1;i<3;i++)
	{
		if(a[i]>a[side]) side=i;
	}
	if(v[side]<0.0f)
	{
		side+=3;
	}
	return side;
}


float3 CubeProjected(const float3 &v)
{
	int side = CubeSide(v);
	return v* ((float)qcuberadius/fabsf(v[side%3]));
	
}

Plane CubeProjected(const Plane &p)
{
	int side = CubeSide(p.normal());
	float mag = ((float)qcuberadius/fabsf(p.normal()[side%3]));
	return Plane(p.normal()*mag,p.dist()*mag);
}

float3 CubeRounded(const float3 &v)
{
	return Round(CubeProjected(v),1.0f);
}

float3 Quantized(const float3 &v)
{
	float3 r = CubeRounded(v);
	return normalize(r);
}


float QuantumDist(const float3 &n)
{
	float3 r = CubeRounded(n);
	float mag = magnitude(r);
	return qsnap/mag;
}
Plane Quantized(const Plane &p)
{
	float3 r = CubeRounded(p.normal());
	float mag = magnitude(r);
	return Plane(r/mag,Round(p.dist(),qsnap/mag));
}
Quaternion Quantized(const Quaternion &q)
{
	float3 zdir = Quantized(q.zdir());
	Quaternion qb= RotationArc(q.zdir(),zdir) * q;
	float3 xdir = Quantized(qb.xdir());
	return RotationArc(qb.xdir(),xdir) *qb;
}



//----- test theory 


int largest(const float3 vectors[],int vectors_count)
{
	int a = -1;
	float mag2=-1.0f;
	for(int i=0;i<vectors_count;i++)
	{
		float m = dot(vectors[i],vectors[i]);
		if(m>mag2)
		{
			mag2=m;
			a=i;
		}
	}
	return a;
}


class int3x3
{
  public:
	int3 x,y,z;  // the 3 rows of the Matrix
	int3x3(){}
	int3x3(int xx,int xy,int xz,int yx,int yy,int yz,int zx,int zy,int zz):x(xx,xy,xz),y(yx,yy,yz),z(zx,zy,zz){}
	int3x3(const int3 &_x, const int3 &_y, const int3 &_z):x(_x),y(_y),z(_z){}
	int3&       operator[](int i)       {assert(i>=0&&i<3);return (&x)[i];}
	const int3& operator[](int i) const {assert(i>=0&&i<3);return (&x)[i];}
};

inline int determinant(const int3x3 m)
{
	return  m.x.x*m.y.y*m.z.z + m.y.x*m.z.y*m.x.z + m.z.x*m.x.y*m.y.z 
		   -m.x.x*m.z.y*m.y.z - m.y.x*m.x.y*m.z.z - m.z.x*m.y.y*m.x.z ;
}


int3x3 InverseTimesDet(const int3x3 &a)
{
	int3x3 b;
	for(int i=0;i<3;i++) 
    {
		int i1=(i+1)%3;
		int i2=(i+2)%3;
		for(int j=0;j<3;j++) 
        {
			int j1=(j+1)%3;
			int j2=(j+2)%3;
			b[j][i] = (a[i1][j1]*a[i2][j2]-a[i1][j2]*a[i2][j1]);  // cant divide by determinant since this is integer
		}
	}
	// Matrix check=a*b; // Matrix 'check' should be the identity (or perhaps 0 if determinant is 0)
	return b;
}

int3 cubenorm(int i)  // 0<=i< 3*64+1 (==193)
{
	// enumerates 1/2 of the 386 discrete points on the cubemap lattice of halfextent 4
	if(i<64 ) return int3( 4           , -4+(i&7)    , -3+(i>>3) );
	if(i<128) return int3( -3+(i>>3&7) , 4           , -4+(i&7)  );
	if(i<192) return int3( -4+(i&7)    , -3+(i>>3&7) , 4         );
	return int3(4,4,4);
}


int findbigdet()
{
	// test to find the largest determinant of a 3x3 matrix with integer values no larger than 4.
	// The application of the resulting knowledge is to determine a minimum distance between points 
	// where each point is the intersection of 3 planes and where each plane can be described with integer coefficients 
	// of the plane equation Ax+By+Cz+D==0 such that A,B,C are within [-4,+4]
	int det=0;
	int3x3 mat(4,0,0,0,0,0,0,0,0);
	for(int i1=0 ;i1<=4;i1++)
	for(int i2=0 ;i2<=4;i2++)
	for(int i3=-4;i3<=4;i3++)
	for(int i4= 0;i4<=4;i4++)
	for(int i5=-4;i5<=4;i5++)
	for(int i6=-4;i6<=4;i6++)
	for(int i7=-4;i7<=4;i7++)
	for(int i8=-4;i8<=4;i8++)
	{
		int d = determinant(int3x3(4,i1,i2,i3,i4,i5,i6,i7,i8)); 
		if(d>det)
		{
			det=d;
			mat=int3x3(4,i1,i2,i3,i4,i5,i6,i7,i8);;
		}
	}
	return det;
}
// biggest determinant found is 256
// example matrix (not the only one):
//
//  | 4 -4  0 |
//  | 4  4 -4 |
//  | 4  4  4 |
//

String testmaximumdeterminant(String)
{
	return String(findbigdet());
}
//EXPORTFUNC(testmaximumdeterminant);
int dot(const int3 &a, const int3&b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z ;
}
int3 operator+(const int3 &a, const int3&b)
{
	return int3(a.x+b.x , a.y+b.y , a.z+b.z );
}
float testminsep()
{
	// generate all possible points that are the intersection of 3 quantized planes
	// then test to see how close each point can be to another plane that's not coplanar
	// the minimum of these is returned.    
	// Then we know at runtime we will never generate two non-coincident points 
	// that will be closer that this distance (about 1/256 or 0.0039).
	float minsep=1.0f;
	int3x3 m;
	for(int i0=0;i0<193;i0++)
	{
		m[0] = 	cubenorm(i0);
		for(int i1=0;i1<193;i1++)
		{
			m[1] = 	cubenorm(i1);
			for(int i2=0;i2<193;i2++)
			{
				m[2] = 	cubenorm(i2);
				int d = determinant(m);
				if(d==0) continue;
				int3x3 inv = InverseTimesDet(m);
				int3 v = inv[0]+inv[1]+inv[2]; // b==1,1,1
				for(int j=0;j<193;j++)
				{
					int3 p = cubenorm(j);
					int dp = dot(p,v);
					int pdr = dp%d;
					if(pdr==0) continue;
					int pdi = dp/d;
					float s = fabsf( (float)pdr/(float)d)  ;
					if(s<minsep)
					{
						minsep=s;
					}
				}
			}
		}
	}
	return minsep;
}
String testminimumseparation(String)
{
	return String(testminsep());
}

//EXPORTFUNC(testminimumseparation);

class testtesttest
{
public: 
	testtesttest()
	{
		Plane planes[] = 
		{
			Plane(float3(1,0,0),0),
			Plane(normalize(float3(1,1,0)),10),
			Plane(float3(0,1,0),0),
			Plane(float3(0,0,1),5),
		};
		float minsep = testminsep();
		for(int i=0;i<100;i++)
		{
			float3 d((float)rand()/(float)rand(),(float)rand()/(float)rand(),(float)rand()/(float)rand());
			float3 s((float)rand()/(float)rand(),(float)rand()/(float)rand(),(float)rand()/(float)rand());
			float3x3 m(d.x,s.z,s.y, s.z,d.y,s.x, s.y,s.x,d.z);
			Quaternion q = Diagonalizer(m);
			float3x3 Q = q.getmatrix();
			float3x3 D = Q*m*Transpose(Q);
			float3 dd=Diagonal(D);
		}
		float3 v = PlanesIntersection(planes,sizeof(planes)/sizeof(Plane),float3(1000,1000,1000));
		v;
	}
};
