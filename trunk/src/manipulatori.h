

#ifndef SM_MANIPULATOR_INTERNAL_H
#define SM_MANIPULATOR_INTERNAL_H

#include "vecmath.h"
#include "bsp.h"
#include "manipulator.h"
#include "array.h"

class Manipulator
{
  private:
	float3 positiona;
  public:
	virtual float3& Position(){return positiona;}
	Manipulator();
	virtual ~Manipulator();
	virtual float3 Bmin(){return Position()-float3(0.01f,0.01f,0.01f);}
	virtual float3 Bmax(){return Position()+float3(0.01f,0.01f,0.01f);}
	virtual int Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int DragStart(const float3 &v0, const float3 &v1);
	virtual int DragEnd(const float3 &v0, const float3 &v1);
	virtual int HitCheck(const float3 &v0, float3 v1,float3 *impact);
	virtual int UserInput(int) {return 0;}
	int drawit;
	virtual void Render();
	virtual int  KeyPress(int k){return 0;}
	virtual int  Wheel(int w);
	virtual void PositionUpdated(){}
	int          InDrag();
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

