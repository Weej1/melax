/*
 *              A Small Simple Console 
 *
 *           by Stan Melax (c) March 1998
 *           http://www.melax.com
 *     
 *
 *  Global variables and functions are exposed to the 
 *  interpretor by using the EXPORTVAR and EXPORTFUNC macros.
 *  For Example, in your code:
 *
 *				void f(char *s) {
 *					...
 *				}
 *				EXPORTFUNC(f);
 *
 *				int x;
 *				EXPORTVAR(x);
 *
 *
 *  The function:
 *			FuncInterp(char *string_to_be_interpreted);
 *  is what you use to interpret a given commandline or string.
 *  It finds the exported function/variable and calls/sets it.
 *
 */

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
#include "object.h"

class ConsoleVar{
public:
	ConsoleVar(char *_name,Reference r);
};
class ConsoleFnc{
public:
	ConsoleFnc(char *_name,String (*_func)(char *));
};

#define EXPORTVAR(v) ConsoleVar ConV##v(#v,v);
#define EXPORTFUNC(func) String CF##func(char *p){ return String(func(p));} ConsoleFnc ConF##func(#func,CF##func);

extern char *FuncInterp(const char *);  
extern char *FuncPreInterp(const char *s);
extern char *CommandCompletion(const char *name,Array<const char *> &match);
Reference GetVar(String varname);

char *dofile(const char *filename);

class Profile
{
public:
	//static Profile* current;
	//Profile *parent;
	__int64 start;
	const char *name;
	inline Profile(const char *_name):name(_name)
	{
		//parent=current;
		//current=this;
		__int64 _start;
		__asm
		{
			cpuid;                // Force completion of out-of-order processing
			rdtsc;                // Read timestamp counter
			mov DWORD PTR [_start    ], eax;  // Copy counter value into variable
			mov DWORD PTR [_start + 4], edx 
		}
		start=_start;
	}
	inline ~Profile()
	{
		__int64 finish;
		__asm
		{
			cpuid;                // Force completion of out-of-order processing
			rdtsc;                // Read timestamp counter
			mov DWORD PTR [finish    ], eax;  // Copy counter value into variable
			mov DWORD PTR [finish + 4], edx 
		}
		addrecord(name,(float) (finish-start));
		//current=parent;
	}
	void addrecord(const char *name,float cycles);
};
void ProfileReset();
#define PROFILE(n) Profile _pf ## n (#n);

#endif
#endif
