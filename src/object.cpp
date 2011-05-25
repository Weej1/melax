

#include "object.h"
#include "stringmath.h"
#include "xmlparse.h"

Array<Entity*> Objects(-1);



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
Entity::Entity(const char *_name,int hashinit):id(Uniqueify(_name))
{
	EXPOSEMEMBER(id);
	Objects.Add(this);
}

Entity::~Entity()
{
	while(trackers.count)
	{
		trackers.Pop()->notify(this);  // note that the trackers may delete themselves and try to remove themselves from trackers array.
	}
	if(Objects.element)  // quick check to ensure global list hasn't been destructed already
		Objects.Remove(this);
}
Entity *ObjectFind(const char *name)
{
	for(int i=0;i<Objects.count;i++)
	 if(!_stricmp(Objects[i]->id,name)) 
		 return Objects[i];
	return NULL;
}

xmlNode *ObjectExport(Entity *object,xmlNode *n)
{
	if (!n) n=new xmlNode(object->id);
	else n->attribute("name") = object->id;
/*	for(int i=0;i<object->hash.slots_count;i++) 
	 if(object->hash.slots[i].used)
	{
		if(object->hash.slots[i].key=="id") continue;
		if(object->hash.slots[i].key=="name") continue;
		xmlNode *c=new xmlNode(object->hash.slots[i].key);
		c->body = object->hash.slots[i].value.Get();
		n->children.Add(c);
	}
	*/
	return n;
}
int ObjectImportMember(Entity *object,xmlNode *n)
{
//	if(!object->hash.Exists(n->tag)) return 0;
//	object->hash[n->tag].Set(n->body);
	return 1;
}

void ObjectImport(Entity *object,xmlNode *n)
{
	for(int i=0;i<n->children.count;i++)
	{
		int rc=ObjectImportMember(object,n->children[i]);
		if(rc) continue;
	}
}


