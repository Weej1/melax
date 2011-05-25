

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

