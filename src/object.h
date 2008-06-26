//
//
//
//
//

#ifndef SM_OBJECT_H
#define SM_OBJECT_H

#include <assert.h>
#include "smstring.h"
#include "hash.h"
#include "vecmath.h"
#include "array.h"

#define EXPOSEMEMBER(m) hash[#m]=Reference(m);

class Object;

class Reference
{
public:
	void  *data;
	short int type;
	short int flag;
	Reference():type(0),data(NULL){}
	Reference(int   &_data)      :type(1),data(&_data),flag(0){}
	Reference(float &_data)      :type(2),data(&_data),flag(0){}
	Reference(float3 &_data)     :type(3),data(&_data),flag(0){}
	Reference(Quaternion &_data) :type(4),data(&_data),flag(0){}
	Reference(String &_data)     :type(5),data(&_data),flag(0){}
	Reference(Object *_data)     :type(6),data( _data),flag(0){}
	Reference(const Reference &r):type(r.type),data(r.data),flag(r.flag){}
	Reference(float4x4  &_data)   :type(7),data(&_data),flag(0){}
	operator int&()       { if(0&&!type) { String *s=((String*)data); *this = Reference(*(new int(0)));    flag=1;if(s)Set(*s);delete s;}assert(type==1);return *(int*)data;       }
	operator float&()     { if(0&&!type) { String *s=((String*)data); *this = Reference(*(new float(0.f)));flag=1;if(s)Set(*s);delete s;}assert(type==2);return *(float*)data;     }
	operator float3&()    { if(0&&!type) { String *s=((String*)data); *this = Reference(*(new float3));    flag=1;if(s)Set(*s);delete s;}assert(type==3);return *(float3*)data;    }
	operator Quaternion&(){ if(0&&!type) { String *s=((String*)data); *this = Reference(*(new Quaternion));flag=1;if(s)Set(*s);delete s;}assert(type==4);return *(Quaternion*)data;}
	operator String&()    { if(0&&!type) { String *s=((String*)data); *this = Reference(*(new String));    flag=1;if(s)Set(*s);delete s;}assert(type==5);return *(String*)data;    }
	operator Object*&()   { assert(type==6);return *((Object**)&data); }
	operator float4x4&()   { assert(type==7);return *((float4x4*)data); }
	void operator=(const int   x)   { if(type==2) {*this=(float)x;return;} assert(type==1); *(int*) data =x;}
	void operator=(const float x)      {assert(type==2);     *(float*)data =x;}
	void operator=(const float3 &x)    {assert(type==3);    *(float3*)data =x;}
	void operator=(const Quaternion &x){assert(type==4);*(Quaternion*)data =x;}
	void operator=(const String &x)    {assert(type==5);    *(String*)data =x;}
	void operator=(Object *x)          {assert(type==6);              data =x;}
	void operator=(const Reference &r) {data=r.data;type=r.type;flag=r.flag;}
	String Get() const;
	void Set(String s);
};


extern Array<Object*> Objects;
extern Object *ObjectFind(const char *name);
class Tracker
{
  public:
	virtual void notify(Object *)=0;  // invoked when object deleted
};

class Object {
  public:
	String id;
	Hash<String,Reference>	hash;
	Object(const char *_name,int hashinit=0);
	virtual ~Object();
	Array<Tracker*> trackers;
};

class xmlNode;
xmlNode *ObjectExport(Object *object,xmlNode *n=NULL);
int ObjectImportMember(Object *object,xmlNode *n);
void ObjectImport(Object *object,xmlNode *n);


#endif
