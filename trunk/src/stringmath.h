//
//      String
//
// string routines that convert between with basic math types
//

#ifndef SM_STRINGMATH_H
#define SM_STRINGMATH_H

#include <stdio.h>
#include <string.h>
#include "smstring.h"
#include "vecmath.h"
#include "array.h"

String AsString(const int    &a);
String AsString(const float  &a);
String AsString(const float3 &a);
String AsString(const float4 &a);
String AsString(const float4x4 &a);

void   WriteTo(const String &s,int &a);
void   WriteTo(const String &s,float &a);
void   WriteTo(const String &s,float3 &a);
void   WriteTo(const String &s,float4 &a);
void   WriteTo(const String &s,float4x4 &a);
void   WriteTo(const String &s,String &dst);

int      AsInt     (const String &s);
float    AsFloat   (const String &s);
float3   AsFloat3  (const String &s);
float4   AsFloat4  (const String &s);
float3x3 AsFloat3x3(const String &s);



inline String &operator+=(String &s,const float3 &v)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%g %g %g",v[0],v[1],v[2]);
	return s+=buf;
}
inline String &operator+=(String &s,const float4 &v)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%g %g %g %g",v[0],v[1],v[2],v[3]);
	return s+=buf;
}
inline String &operator+=(String &s,const float2 &v)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%g %g",v[0],v[1]);
	return s+=buf;
}

inline String &operator+=(String &s,const int3 &v)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%d %d %d",v[0],v[1],v[2]);
	return s+=buf;
}

inline String &operator+=(String &s,const short3 &t)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%d %d %d",(int)t[0],(int)t[1],(int)t[2]);
	return s+=buf;
}
inline String &operator+=(String &s,const byte4 &v)
{
	char buf[256];
	sprintf_s(buf,sizeof(buf),"%d %d %d %d",(int)v[0],(int)v[1],(int)v[2],(int)v[3]);
	return s+=buf;
}


inline String &operator+=(String &s,const float3x3 &m)
{
	char buf[512];
	sprintf_s(buf,sizeof(buf),"%g %g %g  %g %g %g  %g %g %g",m[0][0],m[0][1],m[0][2],m[1][0],m[1][1],m[1][2],m[2][0],m[2][1],m[2][2]);
	return s+=buf;
}


class StringIter
{
public:
	const char *p;
	explicit StringIter(const char *_p):p(_p){}
	StringIter(const String &s):p((const char*)s){}
};

inline int rip(char *buf,const char *p)
{
	int n=0;
	while(*p && !IsOneOf(*p," \t\n\r;,"))
	{
		buf[n++] = *p++;
	}
	buf[n]='\0';
	return n;
}
inline StringIter &operator >>(StringIter &s,int &a)
{
	s.p=SkipChars(s.p," \t\n\r;,");
	char buf[256];
	s.p+=rip(buf,s.p);
	sscanf_s(buf,"%d",&a);
	return s;
}
inline StringIter &operator >>(StringIter &s,unsigned char &a)
{
	s.p=SkipChars(s.p," \t\n\r;,");
	char buf[256];
	s.p+=rip(buf,s.p);
	int tmp;
	sscanf_s(buf,"%d",&tmp);
	a = (unsigned char) tmp;
	return s;
}
inline StringIter &operator >>(StringIter &s,short int &a)
{
	s.p=SkipChars(s.p," \t\n\r;,");
	char buf[256];
	s.p+=rip(buf,s.p);
	sscanf_s(buf,"%hd",&a);
	return s;
}
inline StringIter &operator >>(StringIter &s,float &a)
{
	s.p=SkipChars(s.p," \t\n\r;,");
	char buf[256];
	s.p+=rip(buf,s.p);
	sscanf_s(buf,"%f",&a);
	return s;
}

struct vecdlm
{
	StringIter &s;
	char d;
	vecdlm(StringIter &_s):s(_s),d('\0') {s.p=SkipChars(s.p," \t\n\r;,"); if(s.p[0]=='(') {s.p++;d=')';} }
	~vecdlm(){ if(!d)return; s.p=SkipChars(s.p," \t\n\r;,"); if(s.p[0]==d){s.p++;} }
};

inline StringIter &operator >>(StringIter &s,int3 &v)
{
	return s>>v.x>>v.y>>v.z;
}
inline StringIter &operator >>(StringIter &s,short3 &v)
{
	return s>>v.x>>v.y>>v.z;
}
inline StringIter &operator >>(StringIter &s,byte4 &v)
{
	return s>>v.x>>v.y>>v.z>>v.w;
}
inline StringIter &operator >>(StringIter &s,float3 &v)
{
	vecdlm bitbucket(s);
	return s>>v.x>>v.y>>v.z;
}
inline StringIter &operator >>(StringIter &s,float4 &v)
{
	return s>>v.x>>v.y>>v.z>>v.w;
}
inline StringIter &operator >>(StringIter &s,float2 &v)
{
	return s>>v.x>>v.y;
}
inline StringIter &operator >>(StringIter &s,float3x3 &m)
{
	return s>>m.x>>m.y>>m.z;
}

template<class T>
void ArrayImport(T *a,const char *string,int count)
{
	StringIter s(string);
	for(int i=0;i<count;i++)
	{
		assert(*s.p);
		s >> a[i];
	}
}

template<class T>
void ArrayImport(Array<T> &a,const char *string,int count)
{
	a.SetSize(count);
	ArrayImport(a.element,string,count);
}


#endif
