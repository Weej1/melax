//
//
//  A Small Simple Interpreter and Binding System 
// 
//   by Stan Melax (c) March 1998
//      http://www.melax.com
//      
//   See corresponding .cpp files for more information.
//
//   Global variables and functions are exposed to the  *  interpretor by using the EXPORTVAR and EXPORTFUNC macros.
//   For Example, in your code:
// 
// 				void f(char *s) {
// 					...
// 				}
// 				EXPORTFUNC(f);
// 
// 				int x;
// 				EXPORTVAR(x);
// 
// 
//   The function:
// 			FuncInterp(char *string_to_be_interpreted);
//   is what you use to interpret a given commandline or string.
//   It finds the exported function/variable and calls/sets it.
// 
//

#ifndef SM_CONSOLE_H
#define SM_CONSOLE_H

#ifdef BONZO
// If you cant include the console module in your app you can disable
// existing export macros by defining them to do nothing as follows:
#define EXPORTFUNC(func) ;
#define EXPORTVAR(v) ;
#else

#include "vecmath.h"
#include "array.h"
#include "smstring.h"
#include "stringmath.h"



struct ldeclare
{
	ldeclare(String cname,String mname,String tname,int offset);
};

inline String gettype(float3 &){return "float3";}
inline String gettype(float &){return "float";}
inline String gettype(int &){return "int";}
inline String gettype(String &){return "string";}
inline String gettype(float4 &){return "float4";}
inline String gettype(float4x4 &){return "float4x4";}

#define LDECLARE(C,M) ldeclare  ldeclare_ ## C ## M (#C , #M , gettype( ((C*)NULL)-> M ) , OFFSET(C,M) );

// WIP: contemplating using a class description system that's more explicit and thus less brittle from the c lang: 
class ClassDesc
{
public:
	class Member
	{
	public:
		char mname[24];
		int  offset;
		ClassDesc *metaclass;
		Member(){mname[0]='\0';}
		Member(const char *s,int a,ClassDesc *c):offset(a),metaclass(c){assert(strlen(s)<24); strcpy_s<24>(mname,s);}
	};
	char cname[16];
	Array<Member> members;
	Member *FindMember(String m) {if(!this)return NULL;for(int i=0;i<members.count;i++)if(m==members[i].mname)return &members[i];return NULL;}
	ClassDesc(){cname[0]='\0';}
	ClassDesc(const char *s) {assert(strlen(s)<16); strcpy_s<16>(cname,s);}
};
extern Array<ClassDesc*> Classes;
extern ClassDesc* GetClass(String cname);
void *deref(void *p,ClassDesc *cdesc,String m,ClassDesc *&memcdesc_out);

inline void *deref(void *p,String cn,String m,String &memclass_out)
{
	ClassDesc *md=NULL;
	void *a = deref(p,GetClass(cn),m,md);
	memclass_out = (md)?String(md->cname):"";
	return a;
}

class xmlNode;
extern void     xmlimport(void *obj,ClassDesc* t, xmlNode *n);
extern xmlNode *xmlexport(void *obj,ClassDesc* t, const char *name);


struct lobj;
lobj* lexposer(String cname,String name,void *address);

class lexpose { public: lexpose(String cname,String name,void *address){ lexposer(cname,name,address);}}; // simply invoke function lexposer()
#define LEXPOSEVAR(V) lexpose lexposevar_ ## V(gettype(V),#V,&V); // 
#define LEXPOSEOBJECT(C,N) lexposer(#C, N , this);  // put this in an object's constuctor 

void  LVarMatches(String prefix,Array<String> &match); // returns array of names of bound internal variables that begin with character sequence prefix 
void *LVarLookup(String name,ClassDesc *&cdesc_out);   // looks through internal bindings and returns address and type if it is a c/c++ native variable 
inline void *LVarLookup(String name,String &classname_out) { ClassDesc *c; void *p= LVarLookup(name,c); if(p)classname_out = c->cname; return p;}



class ConsoleFnc{
public:
	ConsoleFnc(char *_name,String (*_func)(String));
};


#define EXPORTVAR(v)  LEXPOSEVAR(v);
#define EXPORTFUNC(func) String CF##func(String p){ return String(func(p));} ConsoleFnc ConF##func(#func,CF##func);

extern String FuncInterp(const char *);  
extern String FuncPreInterp(const char *s);
extern char *CommandCompletion(const char *name,Array<String> &match);

String cfg(const char *filename);


#endif
#endif
