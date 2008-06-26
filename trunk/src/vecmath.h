// 
//
//

#ifndef SM_VEC_MATH_H
#define SM_VEC_MATH_H

#include <stdio.h>
#include <math.h>
#include <assert.h>

#define M_PIf (3.1415926535897932384626433832795f)

inline float DegToRad(float angle_degrees) { return angle_degrees * M_PIf / 180.0f; } // returns Radians.
inline float RadToDeg(float angle_radians) { return angle_radians * 180.0f / M_PIf; } // returns Degrees.

#define OFFSET(Class,Member)  (((char*) (&(((Class*)NULL)-> Member )))- ((char*)NULL))




int    argmin(const float a[],int n);
int    argmax(const float a[],int n);
float  squared(float a); 
float  clamp(float a,const float minval=0.0f, const float maxval=1.0f);
int    clamp(int   a,const int minval,const int maxval) ;
float  Round(float a,float precision);
float  Interpolate(const float &f0,const float &f1,float alpha) ;

template <class T>
void Swap(T &a,T &b) 
{
	T tmp = a;
	a=b;
	b=tmp;
}



template <class T>
T Max(const T &a,const T &b) 
{
	return (a>b)?a:b;
}

template <class T>
T Min(const T &a,const T &b) 
{
	return (a<b)?a:b;
}

//----------------------------------

class int3  
{
public:
	int x,y,z;
	int3(){};
	int3(int _x,int _y, int _z){x=_x;y=_y;z=_z;}
	const int& operator[](int i) const {return (&x)[i];}
	int& operator[](int i) {return (&x)[i];}
};

inline int operator ==(const int3 &a,const int3 &b) {return (a.x==b.x && a.y==b.y && a.z==b.z);}

class int4
{
public:
	int x,y,z,w;
	int4(){};
	int4(int _x,int _y, int _z,int _w){x=_x;y=_y;z=_z;w=_w;}
	const int& operator[](int i) const {return (&x)[i];}
	int& operator[](int i) {return (&x)[i];}
};

class short3  
{
public:
	short x,y,z;
	short3(){};
	short3(short _x,short _y, short _z){x=_x;y=_y;z=_z;}
	const short& operator[](int i) const {return (&x)[i];}
	short& operator[](int i) {return (&x)[i];}
};

inline int operator ==(const short3 &a,const short3 &b) {return (a.x==b.x && a.y==b.y && a.z==b.z);}


class byte4
{
public:
	unsigned char x,y,z,w;
	unsigned char& operator[](int i)             {return ((unsigned char*)this)[i];}
	const unsigned char& operator[](int i) const {return ((unsigned char*)this)[i];}
};

//-------- 2D --------

class float2
{
public:
	float x,y;
	float2(){x=0;y=0;};
	float2(float _x,float _y){x=_x;y=_y;}
	float& operator[](int i) {assert(i>=0&&i<2);return ((float*)this)[i];}
	const float& operator[](int i) const {assert(i>=0&&i<2);return ((float*)this)[i];}
};
inline float2 operator-( const float2& a, const float2& b ){return float2(a.x-b.x,a.y-b.y);}
inline float2 operator+( const float2& a, const float2& b ){return float2(a.x+b.x,a.y+b.y);}

//--------- 3D ---------

class double3;
class float3 // 3D
{
  public:
	float x,y,z;
	float3(){x=0;y=0;z=0;};
	float3(float _x,float _y,float _z){x=_x;y=_y;z=_z;};
	//operator float *() { return &x;};
	float& operator[](int i) {assert(i>=0&&i<3);return ((float*)this)[i];}
	const float& operator[](int i) const {assert(i>=0&&i<3);return ((float*)this)[i];}
#	ifdef PLUGIN_3DSMAX
	float3(const Point3 &p):x(p.x),y(p.y),z(p.z){}
	operator Point3(){return *((Point3*)this);}
#	endif
	explicit float3(const double3 &p);
};


float3& operator+=( float3 &a, const float3& b );
float3& operator-=( float3 &a ,const float3& b );
float3& operator*=( float3 &v ,const float s );
float3& operator/=( float3 &v, const float s );

float  magnitude( const float3& v );
float3 normalize( const float3& v );
float3 safenormalize(const float3 &v);
float3 vabs(const float3 &v);
float3 operator+( const float3& a, const float3& b );
float3 operator-( const float3& a, const float3& b );
float3 operator-( const float3& v );
float3 operator*( const float3& v, const float s );
float3 operator*( const float s, const float3& v );
float3 operator/( const float3& v, const float s );
inline int operator==( const float3 &a, const float3 &b ) { return (a.x==b.x && a.y==b.y && a.z==b.z); }
inline int operator!=( const float3 &a, const float3 &b ) { return (a.x!=b.x || a.y!=b.y || a.z!=b.z); }
// due to ambiguity and inconsistent standards ther are no overloaded operators for mult such as va*vb.
float  dot( const float3& a, const float3& b );
float3 cmul( const float3 &a, const float3 &b);
float3 cross( const float3& a, const float3& b );
float3 Interpolate(const float3 &v0,const float3 &v1,float alpha);
float3 Round(const float3& a,float precision);
void   BoxLimits(const float3 *verts,int verts_count, float3 &bmin_out,float3 &bmax_out);
inline float3 VectorMin(const float3 &a,const float3 &b) {return float3(Min(a.x,b.x),Min(a.y,b.y),Min(a.z,b.z));}
inline float3 VectorMax(const float3 &a,const float3 &b) {return float3(Max(a.x,b.x),Max(a.y,b.y),Max(a.z,b.z));}
int overlap(const float3 &bmina,const float3 &bmaxa,const float3 &bminb,const float3 &bmaxb);


class float3x3
{
  public:
	float3 x,y,z;  // the 3 rows of the Matrix
	float3x3(){}
	float3x3(float xx,float xy,float xz,float yx,float yy,float yz,float zx,float zy,float zz):x(xx,xy,xz),y(yx,yy,yz),z(zx,zy,zz){}
	float3x3(float3 _x,float3 _y,float3 _z):x(_x),y(_y),z(_z){}
	float3&       operator[](int i)       {assert(i>=0&&i<3);return (&x)[i];}
	const float3& operator[](int i) const {assert(i>=0&&i<3);return (&x)[i];}
	float&        operator()(int r, int c)       {assert(r>=0&&r<3&&c>=0&&c<3);return ((&x)[r])[c];}
	const float&  operator()(int r, int c) const {assert(r>=0&&r<3&&c>=0&&c<3);return ((&x)[r])[c];}
}; 
float3x3 Transpose( const float3x3& m );
float3   operator*( const float3& v  , const float3x3& m  );
float3   operator*( const float3x3& m , const float3& v   );
float3x3 operator*( const float3x3& m , const float& s   );
float3x3 operator*( const float3x3& ma, const float3x3& mb );
float3x3 operator/( const float3x3& a, const float& s ) ;
float3x3 operator+( const float3x3& a, const float3x3& b );
float3x3 operator-( const float3x3& a, const float3x3& b );
float3x3 &operator+=( float3x3& a, const float3x3& b );
float3x3 &operator-=( float3x3& a, const float3x3& b );
float3x3 &operator*=( float3x3& a, const float& s );
float    Determinant(const float3x3& m );
float3x3 Inverse(const float3x3& a);  // its just 3x3 so we simply do that cofactor method
float3x3 outerprod(const float3& a,const float3& b);

//-------- 4D Math --------

class float4
{
public:
	float x,y,z,w;
	float4(){x=0;y=0;z=0;w=0;};
	float4(float _x,float _y,float _z,float _w){x=_x;y=_y;z=_z;w=_w;}
	float4(const float3 &v,float _w){x=v.x;y=v.y;z=v.z;w=_w;}
	//operator float *() { return &x;};
	float& operator[](int i) {assert(i>=0&&i<4);return ((float*)this)[i];}
	const float& operator[](int i) const {assert(i>=0&&i<4);return ((float*)this)[i];}
	const float3& xyz() const { return *((float3*)this);}
	float3&       xyz()       { return *((float3*)this);}
};


struct D3DXMATRIX; 

class float4x4
{
  public:
	float4 x,y,z,w;  // the 4 rows
	float4x4(){}
	float4x4(const float4 &_x, const float4 &_y, const float4 &_z, const float4 &_w):x(_x),y(_y),z(_z),w(_w){}
	float4x4(float m00, float m01, float m02, float m03, 
	          float m10, float m11, float m12, float m13, 
			  float m20, float m21, float m22, float m23, 
			  float m30, float m31, float m32, float m33 )
			:x(m00,m01,m02,m03),y(m10,m11,m12,m13),z(m20,m21,m22,m23),w(m30,m31,m32,m33){}
	float4&       operator[](int i)       {assert(i>=0&&i<4);return (&x)[i];}
	const float4& operator[](int i) const {assert(i>=0&&i<4);return (&x)[i];}
	float&       operator()(int r, int c)       {assert(r>=0&&r<4&&c>=0&&c<4);return ((&x)[r])[c];}
	const float& operator()(int r, int c) const {assert(r>=0&&r<4&&c>=0&&c<4);return ((&x)[r])[c];}
    operator       float* ()       {return &x.x;}
    operator const float* () const {return &x.x;}
	operator       struct D3DXMATRIX* ()       { return (struct D3DXMATRIX*) this;}
	operator const struct D3DXMATRIX* () const { return (struct D3DXMATRIX*) this;}
};


int     operator==( const float4 &a, const float4 &b );
float4 Homogenize(const float3 &v3,const float &w=1.0f); // Turns a 3D float3 4D vector4 by appending w
float4 cmul( const float4 &a, const float4 &b);
float4 operator*( const float4 &v, float s);
float4 operator*( float s, const float4 &v);
float4 operator+( const float4 &a, const float4 &b);
float4 operator-( const float4 &a, const float4 &b);
float4x4 operator*( const float4x4& a, const float4x4& b );
float4 operator*( const float4& v, const float4x4& m );
float4x4 Inverse(const float4x4 &m);
float4x4 MatrixRigidInverse(const float4x4 &m);
float4x4 MatrixTranspose(const float4x4 &m);
float4x4 MatrixPerspectiveFov(float fovy, float Aspect, float zn, float zf );
float4x4 MatrixTranslation(const float3 &t);
float4x4 MatrixRotationZ(const float angle_radians);
float4x4 MatrixLookAt(const float3& eye, const float3& at, const float3& up);
int     operator==( const float4x4 &a, const float4x4 &b );


//-------- Quaternion ------------

class Quaternion :public float4
{
 public:
	Quaternion() { x = y = z = 0.0f; w = 1.0f; }
	Quaternion(float _x, float _y, float _z, float _w){x=_x;y=_y;z=_z;w=_w;}
	float angle() const { return acosf(w)*2.0f; }
	float3 axis() const { float3 a(x,y,z); if(fabsf(angle())<0.0000001f) return float3(1,0,0); return a*(1/sinf(angle()/2.0f)); }
	float3 xdir() const { return float3( 1-2*(y*y+z*z),  2*(x*y+w*z),  2*(x*z-w*y) ); }
	float3 ydir() const { return float3(   2*(x*y-w*z),1-2*(x*x+z*z),  2*(y*z+w*x) ); }
	float3 zdir() const { return float3(   2*(x*z+w*y),  2*(y*z-w*x),1-2*(x*x+y*y) ); }
	float3x3 getmatrix() const { return float3x3( xdir(), ydir(), zdir() ); }
	//operator float3x3() { return getmatrix(); }
	void Normalize();
};

inline Quaternion QuatFromAxisAngle(const float3 &_v, float angle_radians ) 
{ 
	float3 v = normalize(_v)*sinf(angle_radians/2.0f); 
	return Quaternion(v.x,v.y,v.z,cosf(angle_radians/2.0f));
}

Quaternion& operator*=(Quaternion& a, float s );
Quaternion	operator*( const Quaternion& a, float s );
Quaternion	operator*( const Quaternion& a, const Quaternion& b);
Quaternion	operator+( const Quaternion& a, const Quaternion& b );
Quaternion	normalize( const Quaternion& a );
float		dot( const Quaternion &a, const Quaternion &b );
float3		rotate( const Quaternion& q, const float3& v );
//float3		operator*( const Quaternion& q, const float3& v );
//float3		operator*( const float3& v, const Quaternion& q );
Quaternion	slerp(const Quaternion &a, const Quaternion& b, float t );
Quaternion  Interpolate(const Quaternion &q0,const Quaternion &q1,float t); 
Quaternion  RotationArc(float3 v0, float3 v1 );  // returns quat q where q*v0*q^-1=v1
Quaternion  Inverse(const Quaternion &q);
float4x4    MatrixFromQuatVec(const Quaternion &q, const float3 &v);


//---------------- Pose ------------------

class Pose
{
public:
	float3 position;
	Quaternion orientation;
	Pose(){}
	Pose(const float3 &p,const Quaternion &q):position(p),orientation(q){}
	Pose &pose(){return *this;}
	const Pose &pose() const {return *this;}
};

inline float3 operator*(const Pose &a,const float3 &v)
{
	return a.position + rotate(a.orientation,v);
}

inline Pose operator*(const Pose &a,const Pose &b)
{
	return Pose(a.position + rotate(a.orientation,b.position),a.orientation*b.orientation);
}

inline Pose Inverse(const Pose &a)
{
	Quaternion q = Inverse(a.orientation);
	return Pose(rotate(q,-a.position),q);
}

inline Pose slerp(const Pose &p0,const Pose &p1,float t)
{
	return Pose(p0.position * (1.0f-t) + p1.position * t,slerp(p0.orientation,p1.orientation,t));
}

inline float4x4 MatrixFromPose(const Pose &pose)
{
	return MatrixFromQuatVec(pose.orientation,pose.position);
}

//------ Euler Angle -----

Quaternion YawPitchRoll( float yaw, float pitch, float roll );
float Yaw( const Quaternion& q );
float Pitch( const Quaternion& q );
float Roll( Quaternion q );
float Yaw( const float3& v );
float Pitch( const float3& v );

//------- Plane ----------
class Plane : public float4
{
  public:
	float3&	normal(){ return xyz(); } 
	const float3&	normal() const { return xyz(); } 
	float&	dist(){return w;}   // distance below origin - the D from plane equasion Ax+By+Cz+D=0
	const float&	dist() const{return w;}   // distance below origin - the D from plane equasion Ax+By+Cz+D=0
	Plane(const float3 &n,float d):float4(n,d){}
	Plane(){dist()=0;}
	explicit Plane(const float4 &v):float4(v){}
};

Plane   Transform(const Plane &p, const float3 &translation, const Quaternion &rotation); 

inline  Plane PlaneFlip(const Plane &p){return Plane(-p.normal(),-p.dist());}
inline  int   operator==( const Plane &a, const Plane &b ) { return (a.normal()==b.normal() && a.dist()==b.dist()); }
inline  int   coplanar( const Plane &a, const Plane &b ) { return (a==b || a==PlaneFlip(b)); }

float3  PlaneLineIntersection(const Plane &plane, const float3 &p0, const float3 &p1);
float3  PlaneProject(const Plane &plane, const float3 &point);
float3  PlanesIntersection(const Plane &p0,const Plane &p1, const Plane &p2);
float3  PlanesIntersection(const Plane *planes,int planes_count,const float3 &seed=float3(0,0,0));

int     Clip(const Plane &p,const float3 *verts_in,int count,float* verts_out); // verts_out must be preallocated with sufficient size >= count+1 or more if concave
int     ClipPolyPoly(const float3 &normal,const float3 *clipper,int clipper_count,const float3 *verts_in, int in_count,float3 *scratch);  //scratch must be preallocated


//--------- Utility Functions ------

float3  PlaneLineIntersection(const float3 &normal,const float dist, const float3 &p0, const float3 &p1);
float3  LineProject(const float3 &p0, const float3 &p1, const float3 &a);  // projects a onto infinite line p0p1
float   LineProjectTime(const float3 &p0, const float3 &p1, const float3 &a);  
int     BoxInside(const float3 &p,const float3 &bmin, const float3 &bmax) ;
int     BoxIntersect(const float3 &v0, const float3 &v1, const float3 &bmin, const float3 &bmax, float3 *impact);
float   DistanceBetweenLines(const float3 &ustart, const float3 &udir, const float3 &vstart, const float3 &vdir, float3 *upoint=NULL, float3 *vpoint=NULL);
float3  TriNormal(const float3 &v0, const float3 &v1, const float3 &v2);
float3  NormalOf(const float3 *vert, const int n);
Quaternion VirtualTrackBall(const float3 &cop, const float3 &cor, const float3 &dir0, const float3 &dir1);
int     Clip(const float3 &plane_normal,float plane_dist,const float3 *verts_in,int count,float* verts_out); // verts_out must be preallocated with sufficient size >= count+1 or more if concave
int     ClipPolyPoly(const float3 &normal,const float3 *clipper,int clipper_count,const float3 *verts_in, int in_count,float3 *scratch);  //scratch must be preallocated
float3  Diagonal(const float3x3 &M);
Quaternion Diagonalizer(const float3x3 &A);
float3  Orth(const float3& v);
int     SolveQuadratic(float a,float b,float c,float *ta,float *tb);  // if true returns roots ta,tb where ta<=tb
int     HitCheckPoly(const float3 *vert,const int n,const float3 &v0, const float3 &v1, float3 *impact=NULL, float3 *normal=NULL);
int     HitCheckRaySphere(const float3& sphereposition,float radius, const float3& _v0, const float3& _v1, float3 *impact,float3 *normal);
int     HitCheckRayCylinder(const float3 &p0,const float3 &p1,float radius,const float3& _v0,const float3& _v1, float3 *impact,float3 *normal);
int     HitCheckSweptSphereTri(const float3 &p0,const float3 &p1,const float3 &p2,float radius, const float3& v0,const float3& _v1, float3 *impact,float3 *normal);


//--------------------- double --------------------------


class double3
{
public:
	double x,y,z;
	double3(){}
	double3(const double _x,const double _y,const double _z){x=_x;y=_y;z=_z;};
	double3(const double3 &p){x=p.x;y=p.y;z=p.z;};
	explicit double3(const float3 &p){x=p.x;y=p.y;z=p.z;};
	double& operator[](int i) {return ((double*)this)[i];}
	const double& operator[](int i) const {return ((double*)this)[i];}
	//operator float3() { return float3((float)x,(float)y,(float)z);} 
};

inline float3::float3(const double3 &p){x=(float)p.x;y=(float)p.y;z=(float)p.z;}


class double3x3
{
public:
	double3 x,y,z;
	double3x3(){}
	double3x3(const double3 &_x, const double3 &_y,const double3 &_z){x=_x;y=_y;z=_z;}
	double3& operator[](int i) {return ((double3*)this)[i];}
	const double3& operator[](int i) const {return ((double3*)this)[i];}
};

double3  operator+ (const double3& a, const double3& b);
double3  operator* (const double   s, const double3& b);
double3& operator+=(      double3& a, const double3& b);
double3& operator/=(      double3& v, const double s);
double3& operator-=( double3 &a ,const double3& b );
double3& operator*=( double3 &v ,const double s );
double3  operator-( const double3& a, const double3& b );
double3  operator-( const double3& v );
double3  operator*( const double3& v, const double s );
double3  operator*( const double s, const double3& v );
double3  operator/( const double3& v, const double s );
inline int operator==( const double3 &a, const double3 &b ) { return (a.x==b.x && a.y==b.y && a.z==b.z); }
inline int operator!=( const double3 &a, const double3 &b ) { return (a.x!=b.x || a.y!=b.y || a.z!=b.z); }

double  magnitude( const double3& v );
double3 normalize( const double3& v );
double3 safenormalize(const double3 &v);
double3 vabs(const double3 &v);
// due to ambiguity and inconsistent standards ther are no overloaded operators for mult such as va*vb.
double  dot( const double3& a, const double3& b );
double3 cmul( const double3 &a, const double3 &b);
double3 cross( const double3& a, const double3& b );
double3 Interpolate(const double3 &v0,const double3 &v1,double alpha);
double3 Round(const double3& a,double precision);
double3	VectorMax(const double3 &a, const double3 &b);
double3	VectorMin(const double3 &a, const double3 &b);
double3 TriNormal(const double3 &v0, const double3 &v1, const double3 &v2);

double3x3 Transpose( const double3x3& m );
double3   operator*( const double3& v  , const double3x3& m  );
double3   operator*( const double3x3& m , const double3& v   );
double3x3 operator*( const double3x3& m , const double& s   );
double3x3 operator*( const double3x3& ma, const double3x3& mb );
double3x3 operator/( const double3x3& a, const double& s ) ;
double3x3 operator+( const double3x3& a, const double3x3& b );
double3x3 operator-( const double3x3& a, const double3x3& b );
double3x3 &operator+=( double3x3& a, const double3x3& b );
double3x3 &operator-=( double3x3& a, const double3x3& b );
double3x3 &operator*=( double3x3& a, const double& s );
double    Determinant(const double3x3& m );
double3x3 Inverse(const double3x3& a);  // its just 3x3 so we simply do that cofactor method

template<class T>
inline int maxdir(const T *p,int count,const T &dir)
{
	assert(count);
	int m=0;
	for(int i=1;i<count;i++)
	{
		if(dot(p[i],dir)>dot(p[m],dir)) m=i;
	}
	return m;
}

float3 CenterOfMass(const float3 *vertices, const int3 *tris, const int count) ;
float3x3 Inertia(const float3 *vertices, const int3 *tris, const int count, const float3& com=float3(0,0,0)) ;
float Volume(const float3 *vertices, const int3 *tris, const int count) ;

#endif // VEC_MATH_H
