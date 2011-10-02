//  
// Manipulators
// by: s melax (c) 2003-2007
// 
// This is another include that has more stuff in it for modules that want to derive 
// manipulators reusing some common base classes
//  

#ifndef SM_MANIPULATOR_INTERNAL_H
#define SM_MANIPULATOR_INTERNAL_H

#include "vecmath.h"
#include "bsp.h"
#include "manipulator.h"
#include "array.h"
#include "smstring.h"

class Manipulator
{
  private:
	float3     positiona;
	Quaternion orientationa;
  public:
	virtual float3& Position(){return positiona;}
	virtual Quaternion& Orientation(){return orientationa;}
	virtual float3 ScaleIt(float3 &s) {return float3(0,0,0);}
	virtual float3 Center() { return Position() ;}
	virtual float3 Bmin(){return Position()-float3(0.01f,0.01f,0.01f);}
	virtual float3 Bmax(){return Position()+float3(0.01f,0.01f,0.01f);}
	Manipulator();
	virtual ~Manipulator();
	virtual int Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int DragStart(const float3 &v0, const float3 &v1);
	virtual int DragEnd(const float3 &v0, const float3 &v1);
	virtual int HitCheck(const float3 &v0, float3 v1,float3 *impact);
	virtual int UserInput(int) {return 0;}
	int drawit;
	virtual void Render();
	virtual int  KeyPress(int k){return 0;}
	virtual int  KeysGrab(){return 0;}
	virtual int  Wheel(int w);
	virtual void PositionUpdated(){}
	int          InDrag();
	int 		mode;
	virtual String tooltip(){return String("");}
};

class PlanarManipulator : public Manipulator
{
  public:
	Plane plane;  // use pointer?
	PlanarManipulator(Plane &_plane):Manipulator(),plane(_plane){}
	virtual int Drag(const float3 &v0, float3 v1,float3 *impact);
	
};

extern Array<Manipulator*> Manipulators;

extern float3 manipv0,manipv1;
extern float3 manipv0_start,manipv1_start;
extern Manipulator *currentmanipulator;


#endif

