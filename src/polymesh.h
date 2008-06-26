//
//  polymesh
//
// A basic polymesh class that matches the semantic used by maya, xsi, and collada. 
// Whereas vrml uses -1 to delimit faces within the index list, the preferred standard
// in the DCC community is to use another list with number-of-face elements that indicate
// the number of sides of each face.
//
// The Polymesh data structure isn't very useful for mesh computation and is
// only meant to provide a simple standard means to exchange polymesh information.
// Passing PolyMesh or returning it from a function only does a shallow copy.
// That is to make it convenient for returning a polymesh from a function.
// That design decision could be revisited.
//


#ifndef POLYMESH_H
#define POLYMESH_H

class float3;
class PolyMesh
{
public:
	float3 *verts;
	int verts_count;
	int *indices;
	int indices_count;
	int *sides;
	int sides_count;
};

void PolyMeshFree(PolyMesh &pm); // frees space allocated
void PolyMeshAlloc(PolyMesh &pm,int _verts_count,int _indices_count, int _sides_count); // just allocates space 




#endif
