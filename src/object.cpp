

#include "object.h"
#include "stringmath.h"
#include "xmlparse.h"

Array<Object*> Objects(-1);


String Reference::Get() const
{
	switch(type){
		case 0:return (data)?*(String*)data:String("");
		case 1:return AsString(*(int*)data);
		case 2:return AsString(*(float*)data);
		case 3:return AsString(*(float3*)data);
		case 4:return AsString(*(Quaternion*)data);
		case 5:return *(String*)data;
		case 6:return ((Object*)data)->hash["name"];
		case 7:return AsString(*(float4x4*)data);
		default:assert(0);
	}
	return String("fucked");
}
void Reference::Set(String s)
{
	switch(type){
	case 0:if(!data) {data = new String();flag=1;} ;(*(String*)data)=s; break;
		case 1:WriteTo(s,*(int*)data); break;
		case 2:WriteTo(s,*(float*)data); break;
		case 3:WriteTo(s,*(float3*)data); break;
		case 4:WriteTo(s,*(Quaternion*)data); break;
		case 5:WriteTo(s,*(String*)data); break;
		case 6:assert(0);  break;
		default:assert(0); break;
	}
}

static String Uniqueify(const char *a)
{
	String s=a;
	int k=0;
	while(ObjectFind(s))
	{
		k++;
		s=String(a)+String(k);
	}
	return s;
}
Object::Object(const char *_name,int hashinit):id(Uniqueify(_name)),hash(hashinit)
{
	EXPOSEMEMBER(id);
	Objects.Add(this);
}

Object::~Object()
{
	while(trackers.count)
	{
		trackers.Pop()->notify(this);  // note that the trackers may delete themselves and try to remove themselves from trackers array.
	}
	if(Objects.element)  // quick check to ensure global list hasn't been destructed already
		Objects.Remove(this);
}
Object *ObjectFind(const char *name)
{
	for(int i=0;i<Objects.count;i++)
	 if(!stricmp(Objects[i]->id,name)) 
		 return Objects[i];
	return NULL;
}

xmlNode *ObjectExport(Object *object,xmlNode *n)
{
	if (!n) n=new xmlNode(object->id);
	else n->attribute("name") = object->id;
	for(int i=0;i<object->hash.slots_count;i++) 
	 if(object->hash.slots[i].used)
	{
		if(object->hash.slots[i].key=="id") continue;
		if(object->hash.slots[i].key=="name") continue;
		xmlNode *c=new xmlNode(object->hash.slots[i].key);
		c->body = object->hash.slots[i].value.Get();
		n->children.Add(c);
	}
	return n;
}
int ObjectImportMember(Object *object,xmlNode *n)
{
	if(!object->hash.Exists(n->tag)) return 0;
	object->hash[n->tag].Set(n->body);
	return 1;
}

void ObjectImport(Object *object,xmlNode *n)
{
	for(int i=0;i<n->children.count;i++)
	{
		int rc=ObjectImportMember(object,n->children[i]);
		if(rc) continue;
	}
}


