
#ifndef SM_QPLANE_H
#define SM_QPLANE_H

#include "vecmath.h"
#include "bsp.h"


unsigned int CubeSide(const float3 &v); // in [0..5] i.e. {x,y,z,-x,-y,-z}
float3 CubeProjected(const float3 &v);
Plane CubeProjected(const Plane &p);
float3 CubeRounded(const float3 &v);
float3 Quantized(const float3 &v);
extern float qsnap; //  eg 1.0f turns out to be 0.25 in practice for axial aligned planes
float QuantumDist(const float3 &n);
Plane Quantized(const Plane &p);
Quaternion Quantized(const Quaternion &q);

#endif

