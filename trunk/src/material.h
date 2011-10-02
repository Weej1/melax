//  
// by: s melax (c) 2004
// note: needs some refactoring here
//  
//


#ifndef SM_MATERIAL_H
#define SM_MATERIAL_H

#include <d3dx9.h>
#include "hash.h"
#include "smstring.h"
#include "array.h"
#include "xmlparse.h"

class Material;

extern Array<Material*> Materials;
Material *MaterialGet(xmlNode *matelem);
LPD3DXEFFECT MaterialSetup(Material *mat);
LPD3DXEFFECT MaterialSetup(int k);
LPD3DXEFFECT MaterialUpload(Material *mat);
LPD3DXEFFECT MaterialUpload(int k);
void MaterialUnload(int k);
const char *MaterialGetName(int matid);
int MaterialFind(const char *matname);
int MaterialNextRegular(int i);
int MaterialSpecial(int k);  

#endif

