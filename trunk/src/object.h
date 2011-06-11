//
//
//
//
//

#ifndef SM_OBJECT_H
#define SM_OBJECT_H

#include <assert.h>
#include "smstring.h"
#include "vecmath.h"
#include "array.h"



class Entity;




class Tracker
{
  public:
	virtual void notify(Entity *)=0;  // invoked when object deleted
};

class Entity {
  public:
	String id;
	Entity(const char *_name,int hashinit=0);
	virtual ~Entity();
	Array<Tracker*> trackers;
};



#endif
