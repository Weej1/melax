//
//      StringMath
//
// string routines that work with basic math types
//

#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "array.h"
#include "smstring.h"
#include "vecmath.h"

inline char *ctype(const float2&){return "ff";}
inline char *ctype(const float3&){return "fff";}
inline char *ctype(const float4&){return "ffff";}
inline char *ctype(const float3x3&){return "fffffffff";}
inline char *ctype(const int3&)  {return "ddd";}


String AsString(const int    &a)
{
	String s;
	s.sprintf("%d",a);
	return s;
}

String AsString(const float  &a)
{
	String s;
	s.sprintf("%g",a);
	return s;
}

String AsString(const float3 &a)
{
	String s;
	s.sprintf("%g %g %g",a.x,a.y,a.z);
	return s;
}

String AsString(const float4 &a)
{
	String s;
	s.sprintf("%g %g %g %g",a.x,a.y,a.z,a.w);
	return s;
}

String AsString(const float4x4 &m)
{
	String s;
	for(int i=0;i<4;i++) s+= AsString(m[i])+ "  ";
	return s;
}


void   WriteTo(const String &s,String &dst) { dst = s;}
void   WriteTo(const String &s,int &a)      {sscanf_s(s,"%d",&a);}
void   WriteTo(const String &s,float &a)    {sscanf_s(s,"%f",&a);}
void   WriteTo(const String &s,float3 &a)   {sscanf_s(s,"%f %f %f",&a.x,&a.y,&a.z);}
void   WriteTo(const String &s,float4 &a)   {sscanf_s(s,"%f %f %f %f",&a.x,&a.y,&a.z,&a.w);}
void   WriteTo(const String &s,float4x4 &a) 
{
	const char *p=s;
	if(!p) return;
	for(int i=0;i<4;i++)
	{
		int advance=0;
		sscanf_s(p,"%f %f %f %f",&a[i].x,&a[i].y,&a[i].z,&a[i].w,&advance);
		p+= advance;
	}
}


int      AsInt     (const String &s) { int      a ; WriteTo(s,a); return a; }
float    AsFloat   (const String &s) { float    a ; WriteTo(s,a); return a; }
float3   AsFloat3  (const String &s) { float3   a ; WriteTo(s,a); return a; }
float4   AsFloat4  (const String &s) { float4   a ; WriteTo(s,a); return a; }
float4x4 AsFloat4x4(const String &s) { float4x4 a ; WriteTo(s,a); return a; }

