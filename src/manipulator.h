//  
// Manipulators
// by: s melax (c) 2003-2007
// 
// if you like to think of that model-view-controller then these things are the "controller" 
// i.e. the UI shim that lets me pull, push, rotate, edit, or activate some sort of object in the scene
//  

#ifndef SM_MANIPULATOR_H
#define SM_MANIPULATOR_H

#include "vecmath.h"

class Manipulator;
void ManipulatorInit();
Manipulator* ManipulatorNew(const float3 &position);
Manipulator* ManipulatorNew(float3 *position);
Manipulator* ManipulatorHit(const float3 &v0, float3 v1, float3 *impact);
Manipulator* ManipulatorDrag(const float3 &v0, float3 v1, float3 *impact);
int          ManipulatorUserInput(int);
int          ManipulatorKeyPress(int k);
int          ManipulatorKeysGrab();
int          ManipulatorWheel(int w);
void         ManipulatorRender();



#endif

