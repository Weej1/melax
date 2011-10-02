//
//  (c) Stan Melax 2004
//
// simple local light objects in the scene
//

#ifndef SM_LIGHT_H
#define SM_LIGHT_H

#include "array.h"
#include "object.h"
#include "xmlparse.h"

class Light :public Entity
{
public:
	float3 position;
	float  radius;
	float3 color;
	int    shadowcast;
	int    enable;
	Light();
	~Light();
};

extern Array<Light*> lights;
int LightsLoad(xmlNode *scene);
int LightExportAll(xmlNode *scene_output);



#endif