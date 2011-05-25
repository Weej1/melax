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


#define EXPOSEMEMBER(m) 

class Entity;


extern Array<Entity*> Objects;
extern Entity *ObjectFind(const char *name);
class Tracker
{
  public:
	virtual void notify(Entity *)=0;  // invoked when object deleted
};

class Entity {
  public:
	String id;
	//Hash<String,Reference>	hash;
	Entity(const char *_name,int hashinit=0);
	virtual ~Entity();
	Array<Tracker*> trackers;
};

class xmlNode;
xmlNode *ObjectExport(Entity *object,xmlNode *n=NULL);
int ObjectImportMember(Entity *object,xmlNode *n);
void ObjectImport(Entity *object,xmlNode *n);


#endif
