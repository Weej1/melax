//  
//             Mesh 
//  
//    

#ifndef SIMPLE_MESH_H
#define SIMPLE_MESH_H

#include "vecmath.h"

class BSPNode;
class Model;
class xmlNode;

Model *ModelLoad(const char *filename);
Model *ModelLoad(xmlNode *mnode);
void ModelRender(Model* model);
void ModelDelete(Model* model);
void Line(const float3 &_v0,const float3 &_v1,const float3 &color_rgb);
void DrawBox(const float3 &bmin,const float3 &bmax,const float3 &color);
Model *CreateModel(BSPNode *bsp,int shadow);
void ModelAnimate(Model *model,float t);  // will take modulo of animation interval for t


#endif
