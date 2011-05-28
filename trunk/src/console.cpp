//
//  todo: this file needs a rename. 
//  console would refer to one of many interfaces to the embedded interpretor
//  
//  original copyright:
//  Console 
//
//           by Stan Melax (c) March 1998
//     
//  This console is small and simple.  
//  It does the job I need.  It works.  Its portable.
//
//  update:
//  At the core is a small lisp implementation and supporting code to bind with the rest of the C application.
//  I hate code bloat but nonetheless I replaced a few dozen lines of code with a few hundred.  
//  Need full expressive power that a language provides.  
//
//  Although the interpretor can do a lot, C/C++ is still 
//  the appropriate language for implementing any service
//  provided by the engine.  
//
// Lisp
// 
//  tend to follow lisp conventions unless the convention is bad or doesn't work.
//
//  Some preprocess parse logic was added to make it easier
//  for example the input 'a=3+2*4 turns into (assign (quote a) (add 3 (mul 2 4))) 
//
//  I've likely done a number of things differently in this implementation.
//  making minimal investment into this part of the infrastructure at this point.
//  will fix or perhaps just replace with an off-the-shelf embeddable language as required.
//
//
//  todo: (in progress) initially all the exposed/bound c funcs took a single param as input.  We'd get a better matching ( CDR vs cdr) if params weren't wrapped in a single arg list.
//  todo:  reflection/serialization to/from xml then finally remove the legacy per object hashes.
//  todo:  s/define/setq/
//  todo:  according to lisp references its:  setq a 7  not:  setq 'a 7   i.e. dont eval the a when you see setq
//  todo:  consider letting a[0] be (car a) and a[2] become (car (cdr (cdr a)))
//  todo:  consider letting a[sym] be an alist lookup
// 
// 
// 
// 
// 
// 

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>  // for INT_MAX
#include <windows.h>  // for the high performance timer

#include "console.h"
#include "vecmath.h"
#include "xmlparse.h"


//---------------------------- LISP ------------------------------------- 
//
//  A tiny implementation of lisp.  
//  The objective is to extend the application with 
//  scripting and a console interpretor
//  
//

enum lisptype 
{
	LUNUSED   = 0,
	LSYM      = 1,
	LINT      = 2,
	LCONS     = 3,
	LNATIVE   = 4,
	LFLOAT    = 5,
	LCLOSURE  = 6,
	LSTRING   = 7,
	LADDR     = 8,
};


int lispcellcount;
EXPORTVAR(lispcellcount);
struct lobj
{
	int refcount;
	int hack;
	void deref()
	{
		--refcount;
		if(! refcount) 	
		{
			hack++;
			 delete this;
		}
	}
	class ptr {  // not sure i like this 'smart' pointer thing, its too many lines of code, adds indirection that makes debugging more cumbersome,  
	  public: 
		lobj *p; 
		ptr():p(NULL){} 
		ptr(char ,char,char){} // dont initialize!!!
		~ptr(){ *this = (lobj *)NULL; }
		ptr(lobj *p_):p(NULL){ *this=p_;} 
		ptr(ptr &pt):p(NULL){ *this=pt.p;} 
		ptr &operator=(lobj *p_){if(p==p_)return *this;refup(p_);refdown(p);p=p_;return *this;}
		ptr &operator=(ptr pt){return (*this=pt.p);}
		lobj& operator*(){ return *p; }
		lobj* operator->(){ return p; }
		operator lobj* (){return p;}
		//operator bool (){return (p!=NULL);}
	};
	lisptype type;
	// definitely have to 'union' the following guys.
	int i;
	float x;
	ptr (*native)(ptr );
	String (*shack)(String);
	String s;
	lobj* car;
	lobj* cdr;
	static void refup(lobj *c){
		if(!c)return;
		assert(!c->hack) ;
		c->refcount++;
	}
	static void refdown(lobj *c){if(c) c->deref();}
	lobj* setcar(lobj*p) {refup(p);refdown(car);car=p;return p;}
	lobj* setcdr(lobj*p) {refup(p);refdown(cdr);cdr=p;return p;}
	ptr params;
	ptr bindings;
	char *addr;
	ClassDesc *cdesc;
	lobj(int r):type(LUNUSED),refcount(r),i(0),x(0.0f),native(NULL),s(""),car(NULL),cdr(NULL),params(NULL),bindings(NULL),addr(NULL),cdesc(NULL),shack(0),hack(0){lispcellcount++;}
	~lobj(){setcar(NULL);setcdr(NULL); params=NULL ; bindings=NULL; type=LUNUSED;lispcellcount--;}
};
typedef lobj::ptr plobj;
static plobj vars('m','n','n');  // all the global and local lisp bindings/variables



plobj CONS(plobj a,plobj d) 
{
	plobj c=new lobj(0);
	c->type=LCONS;
	c->setcar(a);
	c->setcdr(d);
	return c;
}
plobj LIST(plobj a,plobj b) { return CONS(a,CONS(b,NULL));}
plobj LIST(plobj a,plobj b,plobj c) { return CONS(a,LIST(b,c));}
plobj LIST(plobj a,plobj b,plobj c,plobj d) { return CONS(a,LIST(b,c,d));}

plobj CAR(plobj c){ return (c&&c->type==LCONS ) ? c->car:NULL ;}
plobj CDR(plobj c){ return (c&&c->type==LCONS ) ? c->cdr:NULL ;}

plobj lobjint(int n)
{
	plobj b=new lobj(0);
	b->i=n;
	b->x=(float)n;
	b->type=LINT;
	return b;
}
plobj lobjfloat(float n)
{
	plobj b=new lobj(0);
	b->i = (int)n;
	b->x = n;
	b->type=LFLOAT;
	return b;
}
plobj lobjstring(String s)
{
	plobj b=new lobj(0);
	b->type=LSTRING;
	b->s=s;
	return b;
}
plobj symbols('m','n','n');  // tokens
int symsort=1;
EXPORTVAR(symsort);
plobj lispsymbol(String s)
{
	plobj c=symbols;
	plobj prev;
	while(c)
	{
		assert(c->type==LCONS);
		assert(c->car);
		assert(c->car->type==LSYM);
		if(c->car->s==s)
		{
			if(symsort && prev.p) { prev.p->setcdr(c->cdr);c.p->setcdr(symbols);symbols=c;}
			return c->car;
		}
		prev=c;
		c=c->cdr;
	}
	plobj a = new lobj(0);
	a->type = LSYM;
	a->s=s;
	symbols = CONS(a,symbols);
	return a;
}


plobj addbinding(plobj var,plobj val)
{
	plobj entry = CONS(CONS(var,val),NULL);
	lobj** av=&vars.p;
	while(*av)
		av = & ((*av)->cdr);
	*av = entry;
	lobj::refup(*av);
	return (*av)->car;
}
plobj pushbinding(plobj var,plobj val)
{
	vars = CONS(CONS(var,val),vars);
	return vars;
}
plobj popbinding()
{
	assert(vars);
	plobj tmp = vars->car;
	vars=vars->cdr;
	return tmp;
}

plobj lookup(String n)
{
	plobj k=lispsymbol(n);
	plobj v=vars;
	while(v)   // essentially assoc
	{
		assert(v->car);
		assert(v->car->car); 
		assert(v->car->car->type==LSYM); 
		if(v->car->car==k.p)
			return v->car;
		assert(n!=v->car->car->s);
		v=v->cdr;
	}
	return NULL;
}

plobj lobjaddr(void *p,ClassDesc *cd)
{
	if(!p) return NULL;
	assert(cd);
	plobj b=new lobj(0);
	b->addr = (char*) p;
	b->type=LADDR;
	b->cdesc = cd;
	b->s=cd->cname;
	b->setcdr(CONS(lispsymbol(b->s),NULL));
	assert(b->cdr->car->s==cd->cname);
	return b;
}
plobj lobjaddr(void *p,String t)
{
	return lobjaddr(p,GetClass(t));
}

plobj eq(plobj a,plobj b)
{
	if(a==b) return lispsymbol("t"); // something true
	if(!a || !b) return NULL;
	if(a->type != b->type) return NULL;
	if(a->type==LSYM || a->type==LSTRING) return (a->s==b->s)?a:NULL;
	if(a->type==LINT) return (a->i==b->i)?a:NULL;
	if(a->type==LADDR) return (a->addr==b->addr)?a:NULL;
	if(a->type==LFLOAT) return (a->x==b->x)?a:NULL;
	if(a->type==LCONS) return (eq(a->car,b->car) && eq(a->cdr,b->cdr))?a:NULL;
	assert(0);
	return NULL; // probably shouldn't reach this.
}

//------------------------ LISP to/from ASCII --------------------------

String &operator<<(String &s,lobj* a)
{
	String &operator<<(String &s,plobj a) ;
	if(!a)
	{
		//s << " () ";
		return s;
	}
	if(a->type==LSYM)    s << a->s;
	if(a->type==LSTRING) s << '"' << a->s << '"';
	if(a->type==LINT)    s << a->i;
	if(a->type==LFLOAT)  s << a->x;
	if(a->type==LADDR)   { String n; n.sprintf("0x%x %s",a->addr,a->cdesc->cname); s << n;}
	if(a->type==LNATIVE) s << "#NATIVE context: " << a->car ;
	if(a->type==LCONS) 
	{
		if(a->car && a->car->type==LCONS || !a->car)
		{
			s<< "(" << a->car << ")" ;
		}
		else
		{
			s << a->car;
		}
		if(a->cdr) s<< ((a->cdr->type !=LCONS)? " . ":" ")<< a->cdr;
	}
	return s;
}

String &operator<<(String &s,const plobj &a) 
{
	return s<<a.p;
}

static int indent=0; 
static int lformat=0;
EXPORTVAR(lformat);
int linelength=60;
EXPORTVAR(linelength);

String &print(String &s,lobj* a)  // formatted output
{
	if(!a || a->type!=LCONS)
		return s << a;
	assert(a->type==LCONS); 
	String t("");
	t<< "(" << a << ")";
	if(t.Length()<linelength)
		return s<<t;
	// multiline output
	s<< "(";
	while(a) 
	{
		if(a->type!=LCONS)
		{
			s<<" . " << a ;
			break;
		}
		indent++;
		print(s,a->car);
		indent--;
		a=a->cdr;
		s<<"\n";
		for(int i=(a)?-1:0;i<indent;i++) s<<" ";
	}
	s<< ")";
	//for(int i=0;i<indent;i++) s<<" ";
	return s;
}

plobj print(plobj a)
{
	String s;
	return(lobjstring(print(s,a)));
}

int isAlpha(char c) { return (c>='a' && c<='z' || c>='A' && c<='Z' || c=='_' ) ; }
int isDigit(char c) { return (c>='0' && c<='9' ) ; }
char *nextNonAlphaDigit(char *s)
{
	while(isAlpha(*s)||isDigit(*s))s++;
	return s;
}

static String parsequoted(const char *&s,char endquote='\"')
{
	assert( *s == endquote);
	String r;
	s++;
	while( *s &&  *s != endquote) r << *s++;
	if(*s)s++;
	return r;
}

static String parsestring(const char *&s)
{
	assert(isAlpha(*s));
	String r;
	while(  isAlpha(*s)||isDigit(*s) ) r<< *s++;
	return r;
}

static String parsenumber(const char *&s)
{
	assert(isDigit(*s) || *s=='.' || *s=='-');
	String r;
	if(*s=='-') r<< *s++;  // might be '-' at beginning.
	while(  isDigit(*s) || *s=='.'  ) r<< *s++;
	return r;
}

static String parsenumberhex(const char *&s)
{
	assert(isDigit(*s) || *s=='.' || *s=='-');
	String r;
	if(*s=='-') r<< *s++;  // might be '-' at beginning.
	if(s[0]=='0' && s[1]=='x') {r<< *s++;r<< *s++;}
	while(  isDigit(*s) || IsOneOf(*s,"abcdefABCDEF")) r<< *s++;
	return r;
}

char* operators[]=
{
	"--","++",
	"->",
	"&&","||",
	"==",">=","<=","!=",
	">","<",
	"+","-",
	"*","/","%",
	"=",
	";",",","."
};
char* findoperator(const char *s)
{
	for(int i=0;i<sizeof(operators)/sizeof(operators[0]);i++)
		if(!strncmp(s,operators[i],strlen(operators[i])))
			return operators[i];
	return NULL;
}

unsigned long int strtoul2(
	  const char *nptr,
	  const char **endptr,
	  int base)
{
	char *p;
	unsigned long v=strtoul(nptr,&p,base);
	*endptr=p;
	return v;
}

plobj listparse(const char *&s);
plobj llex(const char *&s)
{
	while(IsOneOf(*s," \t\n\0xA\0xD")) s++;
	assert(*s);
	if(!*s) return NULL;
	//assert(s[0]!='(');  // unless dotted notation was used in conjunction with a list 
	if(*s=='(') return listparse(++s);  // note:  do advance s?
	assert(s[0]!=')');  // someone does: (foo . ) when they should have done (foo) or (foo . ())
	if(*s==')') return NULL;  // note:  dont advance s, its needed to close caller?
	if(*s=='\'') return LIST(lispsymbol("quote"),llex(++s));
	if(*s=='`') return LIST(lispsymbol("val"),llex(++s));
	if(*s=='"') return lobjstring(parsequoted(s,'"'));
	if(s[0]=='0' && s[1]=='x')
	{
		return lobjaddr((char*)NULL + strtoul(s,&(char*&)s,0),"unknown");  // strtoul() 2nd param wasn't const
	}
	if(isAlpha(*s))
	{
		return lispsymbol(parsestring(s));
	}
	if(isDigit(*s) || s[0]=='.'&&isDigit(s[1]) || s[0]=='-'&&isDigit(s[1]) || s[0]=='-'&&s[1]=='.' ) 
	{
		String n= parsenumber(s);
		return (IsOneOf('.',n)) ? lobjfloat((float)atof(n)) : lobjint(atoi(n));
	}
	char *op;
	if((op=findoperator(s)))
	{
		s+=strlen(op);
		return lispsymbol(op);
	}
	String c;
	c << *s++;
	return lispsymbol(c);
}

plobj listparse(const char *&s)
{
	while(IsOneOf(*s," \t\n\0xA\0xD")) s++;
	if(!*s || *s==')' && s++) return NULL;  // need to also advance s if closing parenthesis
	plobj car = (*s=='(')?listparse(++s):llex(s) ;
	while(IsOneOf(*s," \t\n\0xA\0xD")) s++;
	return CONS(car  , (s[0]=='.'&&s[1]==' ')?llex(++s):listparse(s) );
}
plobj lispparse(const char * p) {return listparse(p);}



//-------------------- Lisp Func Native Implementation --------------------------

plobj addfunction(const char*n, plobj (*f_native)(plobj ),plobj context=NULL)
{
	plobj c = new lobj(0);
	c->native = f_native;
	c->setcar( context );
	c->type=LNATIVE;
	return addbinding(lispsymbol(n),c);
}
plobj callcfunc(plobj args)
{
	plobj adr = vars->car->cdr;
	assert(adr->type==LINT);
	String (*f)(String);
	(char*&)f = (char *)NULL + adr->i;
	f = adr->shack;
	String a = (args && args->type==LCONS && args->car && args->car->type==LSTRING)? args->car->s : String("") << args;
	String r = f(a);
	//const char *s=r;
	return lispparse(r);
}
ConsoleFnc::ConsoleFnc(char *name,String (*func)(String))
{
	plobj h = lobjint((char*)func-(char*)NULL);
	h->shack = func;
	plobj cf = addfunction(name,callcfunc ,h);
	// functions[_name]=_func;
}


plobj call(plobj (*f)(plobj a0,plobj a1,plobj a2),plobj v)
{
	if(!v || !v->cdr || !v->cdr->cdr || v->cdr->cdr->cdr) 
		return lispsymbol("Should be exactly 3 arguments"); //NULL;
	assert(v);
	assert(v->cdr);
	assert(v->cdr->cdr);
	assert(v->cdr->cdr->cdr==NULL);
	return f(v->car,v->cdr->car,v->cdr->cdr->car);
}
plobj call(plobj (*f)(plobj a0,plobj a1),plobj v)
{
	if(!v || !v->cdr || v->cdr->cdr) // should be 2 and only 2 args (the args themselves could be null)
		return lispsymbol("Should be exactly 2 arguments"); //NULL;
	assert(v);
	assert(v->cdr);
	assert(v->cdr->cdr==NULL);
	return f(v->car,v->cdr->car);
}
plobj call(plobj (*f)(plobj a0),plobj v)
{
	if(!v ||  v->cdr) // should be 1 and only 1 args (the args themselves could be null)
		return lispsymbol("Should be exactly 1 arguments"); //NULL;
	assert(v);
	assert(v->cdr==NULL);
	return f(v->car);
}
class lispregister{public: lispregister(const char*n, plobj (*f)(plobj )){addfunction(n,f);}};
#define LISPVF(F) lispregister lispregister_ ## F (#F,F)
#define LISPF(F) plobj lisp ## F (plobj v) { return call( F , v ); } lispregister lispregister_ ## F (#F,lisp ## F)


plobj cons(plobj a,plobj b) { return CONS(a,b);} 
plobj car(plobj c)  { return CAR(c); }
plobj cdr(plobj c)  { return CDR(c); }

LISPF(cons);
LISPF(car);
LISPF(cdr);

LISPVF(print);

plobj getvars(plobj) {return vars;}
LISPVF(getvars);

plobj dup(plobj a)
{
	if(a&& a->type==LCONS) return CONS(dup(a->car),dup(a->cdr));
	return a;
}
LISPVF(dup);

plobj assoc(plobj k,plobj alist)
{
	plobj v=alist;
	while(v && v->type==LCONS )
	{
		if(eq(CAR(CAR(v)),k))
			return v->car;
		v=v->cdr;
	}
	return NULL;
}
plobj acons(plobj k,plobj v,plobj alist) 
{ 
	//plobj alist = CAR(CDR(CDR(args)));
	plobj b=assoc(k,alist); 
	if(!b)return CONS(CONS(k,v),alist);
	b->setcdr(v);
	return alist;
}
LISPF(assoc);
LISPF(acons);

plobj setcdr(plobj c,plobj d)
{
	assert(c->type==LCONS);
	c->setcdr(d);
	return c;
}
LISPF(setcdr);

plobj keyboard(plobj args) 
{ 
	String ks("");
	if(CAR(CAR(args))==lispsymbol("\\")) return CAR(CDR(CAR(args)));
	ks << CAR(args);
	const char *s=ks;
	if(s[0]=='\0') return NULL;
	if(ks=="SPACE") return lobjint(32); 
	if(ks=="LEFT")  return lobjint(37); 
	if(ks=="RIGHT") return lobjint(39); 
	if(ks=="UP")    return lobjint(38); 
	if(ks=="DOWN")  return lobjint(40); 
	if(ks.Length()>1) return NULL;
	return lobjint( toupper(*s));
}
LISPVF(keyboard);

//-------------------------- C++ Binding Mechanism -----------------------------
// 
// all c vars of any type use the assignref function.  
// Each class/struct/type has an alist type table describing the member type and offset
// types are specified as:  (float3 . (( x 0 float) (y 4 float) (z 8 float)) )
// vars are specified as:  (gravity . #NATIVE context: addr_of_gravity float3 )

plobj deref(plobj e,plobj k) 
{
	if(!e) return NULL;
	if(e->type==LCONS)  // assume alist
	{
		return CAR(CDR(assoc(k,e)));  // this wont work for assignment for syntax such as my_alist.some_member = 3 
	}
	if(e->type!=LADDR)
		return NULL;
	if(!k || k->type!=LSYM) return NULL;
	//assert(e->cdr->car->type==LSYM);
	//plobj t = CDR(assoc(k,CDR(lookup(e->cdr->car->s))));
	//assert(t);
	//if(!t) return NULL;
	//return lobjaddr(e->addr+t->car->i,t->cdr->car->s);
	ClassDesc *cd=NULL;
	char *a=(char*)deref(e->addr,e->cdesc,k->s,cd);
	return lobjaddr(a,cd);
}
LISPF(deref);

plobj tailsymexpand(plobj a)  // used to get full map of a class
{
	if(!a || a->type!=LCONS) return a;
	if(!a->cdr && a->car && a->car->type==LSYM && lookup(a->car->s)) return tailsymexpand(lookup(a->car->s));
	return CONS(tailsymexpand(a->car),tailsymexpand(a->cdr));
}
LISPVF(tailsymexpand);




plobj assignref(void *_p ,ClassDesc *cd , plobj v)
{
	char *p = (char*) _p;
	assert(p);
	assert(cd);
	if(!cd) return NULL;

	assert(!(CAR(v) && v->car->type==LSYM && v->car->s=="."));

	assert(!(CAR(v) && v->car->type==LSYM && v->car->s=="="));
	if(!strcmp(cd->cname,"float"))
	{
		while(v&& v->type != LINT && v->type!=LFLOAT)
			v=v->car;
		if(v)
			*((float*)p) = (v->type == LINT)?(float)v->i:v->x;
		return lobjfloat( *((float*)p) );
	}
	if(!strcmp(cd->cname,"int"))
	{
		while(v&& v->type != LINT && v->type!=LFLOAT)
			v=v->car;
		if(v)
			*((int*)p) = (v->type == LINT)?v->i:(int)v->x;
		return  lobjint( *((int*)p) );

	}
	if(!strcmp(cd->cname,"string"))
	{
		if(v)
		{
			*((String*)p) = (v->type==LSTRING)? v->s : String("") << v;
		}
		return  lobjstring( *((String*)p) );
	}

	plobj rv=CONS(NULL,NULL);
	plobj r=rv;
	for(int i=0;i<cd->members.count;i++)
	{
		r->setcdr(CONS(assignref ( p+cd->members[i].offset,cd->members[i].metaclass ,CAR(v)) , NULL));
		r=r->cdr;
		v= (CDR(v))?CDR(v):CDR(CAR(v));
	}
	return rv->cdr;

}


// System for exposing general C/C++ functions to lisp
// similar to vars, the assignref is used to convert params and return value between lisp and c++
// for each type of function there has to be a call() function 


plobj call(float3 (*f)(const Quaternion&,const float3&),plobj v)
{
	Quaternion q;
	assignref(&q,GetClass(gettype(q)),v->car);
	//assignref(lobjint(0),lispsymbol("float4") ,v->car,(char*)&q);
	float3 a;
	assignref(&a,GetClass(gettype(a)) ,v->cdr->car);
	//assignref(lobjint(0),lispsymbol("float3") ,v->cdr->car,(char*)&a);
	float3 r=f(q,a);
	//return assignref(lobjint(0),lispsymbol("float3") ,NULL,(char*)&r);
	return assignref(&r,GetClass(gettype(r)) ,NULL);
}

plobj call(String (*f)(String),plobj v)
{
	String a;
	assignref(&a,GetClass(gettype(a)) ,v->car);
	String r=f(a);
	return assignref(&r,GetClass(gettype(r)) ,NULL);
}

LISPF(rotate);  // func from math lib

String stest(String p)
{
	return String("got : **") << p << "**";
}
LISPF(stest);

plobj atom(plobj c)  { return (c==NULL || c->type!=LCONS) ? lispsymbol("t") : NULL ; }
LISPF(atom);

plobj isnil(plobj c)  { return (c==NULL) ? lispsymbol("t") : NULL; }
LISPF(isnil);


int lverbose=0;
EXPORTVAR(lverbose);


plobj sum(plobj v)
{
	int   sumi=0;
	float sumf=0.0f;
	int isfloat=0;
	while(v)
	{
		isfloat |=  (v->type==LFLOAT);
		sumi += v->car->i;
		sumf += (v->type==LFLOAT)? v->car->x :  (float)v->car->i;
		v=v->cdr;
	}
	return (isfloat)? lobjfloat(sumf):lobjint(sumi);
}
LISPVF(sum);

float asfloat(lobj* p) { if(!p) return 0.0f; return (p->type==LFLOAT)?p->x:(float)p->i;}
plobj add(plobj a,plobj b)
{
	if(a==NULL)
		return b;
	if(b==NULL) 
		return a;
	if(a->type==LCONS && b->type==LCONS) 
		return CONS(add(a->car,b->car),add(a->cdr,b->cdr));
	if(a->type==LINT && b->type==LINT)
		return lobjint(a->i+b->i);
	return lobjfloat(asfloat(a)+asfloat(b));
}
LISPF(add);

plobj mul(plobj a,plobj b)
{
	if(a==NULL || b==NULL) 
		return NULL;
	if(a->type==LCONS && b->type==LCONS) 
		return CONS(mul(a->car,b->car),mul(a->cdr,b->cdr));
	if(a->type==LCONS)
		return CONS(mul(a->car,b),mul(a->cdr,b));
	if(b->type==LCONS)
		return CONS(mul(a,b->car),mul(a,b->cdr));
	if(a->type==LINT && b->type==LINT)
		return lobjint(a->i*b->i);
	return lobjfloat(asfloat(a)*asfloat(b));
}
LISPF(mul);
plobj div(plobj a,plobj b)
{
	if(a==NULL || b==NULL) 
		return NULL;
	if(a->type==LCONS && b->type==LCONS) 
		return CONS(div(a->car,b->car),div(a->cdr,b->cdr));
	if(a->type==LCONS)
		return CONS(div(a->car,b),div(a->cdr,b));
	if(b->type==LCONS)
		return CONS(div(a,b->car),div(a,b->cdr));
	if(a->type==LINT && b->type==LINT)
		return lobjint(a->i/b->i);
	return lobjfloat(asfloat(a)/asfloat(b));
}
LISPF(div);

plobj ret(plobj v) { return v;} LISPF(ret);

plobj neg(plobj v)
{
	if(!v) 
		return NULL;
	if(v->type==LCONS) 
		return CONS(neg(v->car),neg(v->cdr));
	if(v->type==LINT)
		return lobjint(-v->i);
	if(v->type==LFLOAT)
		return lobjfloat(-v->x);
	return LIST(lispsymbol("neg"),v);
}
LISPF(neg);

LISPF(eq);

plobj ne(plobj a,plobj b)
{
	return eq(a,b) ? NULL : lispsymbol("t"); 
}
LISPF(ne);

plobj gt(plobj a,plobj b)
{
	if(a->type!=LINT  && a->type!=LFLOAT ) return NULL;
	if(b->type!=LINT  && b->type!=LFLOAT ) return NULL;
	if(a->type==LINT) a->x = (float) a->i;
	if(b->type==LINT) b->x = (float) b->i;
	return (a->x > b->x) ?  lispsymbol("t") : NULL; 
}
LISPF(gt);
plobj lt(plobj a,plobj b)
{
	if(a->type!=LINT  && a->type!=LFLOAT ) return NULL;
	if(b->type!=LINT  && b->type!=LFLOAT ) return NULL;
	if(a->type==LINT) a->x = (float) a->i;
	if(b->type==LINT) b->x = (float) b->i;
	return (a->x < b->x) ?  lispsymbol("t") : NULL; 
}
LISPF(lt);

plobj ge(plobj a,plobj b)
{
	return (eq(a,b) || gt(a,b)) ?  lispsymbol("t") : NULL; 
}
LISPF(ge);
plobj le(plobj a,plobj b)
{
	return (eq(a,b) || lt(a,b)) ?  lispsymbol("t") : NULL; 
}
LISPF(le);


plobj assign(plobj v,plobj e)
{
	if(!v) return NULL;
	if(v->type==LCONS) return CONS(assign(v->car,CAR(e)),assign(v->cdr,CDR(e)));
	if(v->type==LADDR)
	{
		return assignref(v->addr,v->cdesc,e);   
		//plobj eval(plobj);
		//return eval(LIST(v,e));
	}
	if(v->type!=LSYM) return NULL;
	plobj c=lookup(v->s);
	if(c)
	{
		c->setcdr(e);
		return c;
	}
	return addbinding(v,e);
}
LISPF(assign);



// my c++ objects reachable from lisp by binding a name to lisp node of type addr
// example of bindings for exposed objects
// (myobj 0x00addr.myclass)
// (barrel 0xFFE30100.rigidbody)
//
lobj* lexposer(String tname,String name,void *address)
{
	assert( lispsymbol(tname) !=NULL );
	return addbinding(lispsymbol(name),lobjaddr((char*)address,tname))->cdr;
}


//-------------------------------- LISP Eval -------------------------------
plobj apply(plobj , plobj );
LISPF(apply);
plobj eval(plobj e,int ap);
plobj eval(plobj e) {  return eval(e,1);}
LISPVF(eval);
plobj eval(plobj e,int ap)
{
	String estring;
	estring << e;
	if(!e) return NULL;
	if(e->type==LSYM)
	{
		plobj binding = lookup(e->s);
		String bstring;
		bstring << ((binding)? binding->cdr : NULL);
		//return (binding)?eval(binding->cdr,ap) : e;
		 return (binding)?binding->cdr : e;
	}
	if(e->type==LINT || e->type==LFLOAT || e->type==LSTRING ) 
		return e;
	if(e->type==LNATIVE) 
		return e; 
	if(e->type==LADDR)
	{
		return e;// (ap)? e : ;
	}
	if(e->type==LCONS)
	{
		if(e->car==NULL) return e; // ???
		if(e->car->s=="lambda") 
		{
			e->bindings=vars;
			return e;
		}
		if(e->car->s=="quote") return (e->cdr)? e->cdr->car :NULL;
		if(e->car->s=="val")   return (CAR(CDR(e))&&e->cdr->car->type==LSYM&&lookup(e->cdr->car->s))? lookup(e->cdr->car->s)->cdr :NULL;
		if(e->car->s=="if")    
			return eval( eval(e->cdr->car) ? (e->cdr->cdr->car) : (CAR(CDR(CDR(CDR(e))))) );
		if(e->car->s=="or")  { plobj rv = eval(CAR(CDR(e))); return (rv)?rv: eval(CAR(CDR(CDR(e)))) ;}
		if(e->car->s=="and") {  return (eval(CAR(CDR(e)))) ? eval(CAR(CDR(CDR(e)))):NULL ;}
		if(e->car->s=="define") return addbinding(e->cdr->car,e->cdr->cdr->car); 
		if(e->car->s=="while")  { plobj rv=NULL; while( eval(CAR(CDR(e)))){rv=eval(CAR(CDR(CDR(e))));} return rv;}
		if(e->car->s=="let") 
		{
			plobj v=CAR(CDR(e));
			int k=0;
			while(v && CAR(CAR(v)))
			{
				pushbinding(CAR(CAR(v)),CAR(CDR(CAR(v))));
				k++;
				v=CDR(v);
			}
			plobj rv = NULL;
			plobj s = CDR(CDR(e));
			while(s)
			{
				rv = eval(CAR(s));
				s=CDR(s);
			}
			while(k--) popbinding();
			return rv; 
		}
		plobj d = eval(e->cdr,0);	
		String dstring,astring;
		dstring << d;
		plobj a = eval(e->car);
		astring << a;
		return (ap==1)?apply(a,d):CONS(a,d);
	}
	return NULL;
}
lobj *rv_lobj=NULL;  // just an experiment on having c funcs returning a addr reference to lisp instead of a string
plobj apply(plobj  f, plobj  v)
{
	String fstring,vstring;
	fstring << f;
	vstring << v;
	if(!f)	
	{
		return CONS(f,v);
	}
	assert(f);
	if(f->type==LADDR)
	{
		return assign(f,v); // uses assignref 
	}
	if(f->type==LNATIVE) // native
	{
		pushbinding(lispsymbol("this"),f->car);
		rv_lobj=NULL;
		plobj rc= f->native(v);
		popbinding();
		return (rv_lobj)?rv_lobj:rc;
	}
	if(f->type==LCONS && f->car && f->car->s=="lambda") // lambda
	{
		plobj p = f->cdr->car;  // params
		plobj e = f->cdr->cdr;  // expressions;
		int k=0;
		
		plobj tmp = vars;
		vars = f->bindings;
		while(p&& p->type==LCONS &&v)
		{
			pushbinding(p->car,v->car);
			p=p->cdr;
			v=v->cdr;
			k++;
		}
		if(p && p->type==LSYM)
		{
			pushbinding(p,v);
			k++;
			p=v=NULL;
		}
		assert(!v);
		while(p)
		{
			assert(0); // something unbound??
			pushbinding(p->car,NULL);
			p=p->cdr;
			k++;
		}
		String bindings;
		bindings <<vars;
		plobj r=NULL;
		while(e)
		{
			r = eval(e->car);
			e=e->cdr;
		}
		String rstring;
		rstring <<r;
		while(k--)
		{
			popbinding();
		}
		vars=tmp;
		return r;
	}
	return CONS(f,v); // not a function.
}

//------------------ REPL ---------------
//

plobj macromathr2l(plobj e,plobj symfunctable)
{
	if(!e || e->type!=LCONS) return e;
	plobj ecar = macromathr2l(e->car,symfunctable);
	plobj ecdr = macromathr2l(e->cdr,symfunctable); // this makes things right to left
	if(!CAR(ecdr) || !assoc(ecdr->car,symfunctable)) return CONS(ecar,ecdr);
	return CONS( LIST( CAR(CDR(assoc(ecdr->car,symfunctable))) ,                          ecar , CAR(CDR(ecdr))),CDR(CDR(ecdr)));
	//return CONS( LIST( CAR(CDR(assoc(ecdr->car,symfunctable))) , LIST(lispsymbol("quote"),ecar), CAR(CDR(ecdr))),CDR(CDR(ecdr)));
}
plobj macromath(plobj e,plobj symfunctable)
{
	if(!e || e->type!=LCONS) return e;
	if(!CAR(e->cdr) || !assoc(e->cdr->car,symfunctable)) return CONS(macromath(e->car,symfunctable),macromath(e->cdr,symfunctable));
	return macromath( CONS(   LIST( CAR(CDR(assoc(e->cdr->car,symfunctable))) , e->car , CAR(CDR(CDR(e))) ), CDR(CDR(CDR(e)))),symfunctable);
}
plobj macroderef(plobj e)
{
	if(!e || e->type!=LCONS) return e;
	if(!CAR(e->cdr) || e->cdr->car->s!="." ) return CONS(macroderef(e->car),macroderef(e->cdr));
	return macroderef( CONS(   LIST( lispsymbol("deref") , e->car , LIST(lispsymbol("quote"),CAR(CDR(CDR(e)))) ), CDR(CDR(CDR(e)))));
}
plobj macroneg(plobj e)
{
	if(!e || e->type!=LCONS) return e;
	if(CAR(e) != lispsymbol("-")) return CONS(macroneg(e->car),macroneg(e->cdr));
	return macroneg( CONS(   LIST( lispsymbol("neg") , CAR(e->cdr) ),CDR(CDR(e)) ));
}
plobj slanguage(plobj e)
{
	//return macromath(macromath(e,LIST(LIST(lispsymbol("*"),lispsymbol("mul")),LIST(lispsymbol("/"),lispsymbol("div")))),CONS(LIST(lispsymbol("+"),lispsymbol("add")),NULL));
	const char * const a[]={"(* mul)(/ div)","(+ add)","(== eq)(> gt)(< lt)(>= ge)(<= le)(!= ne)","(&& and)","(|| or)"};
	e=macroderef(e);
	e=macroneg(e);
	for(int i=0;i<sizeof(a)/sizeof(*a);i++)
		e=macromath(e,lispparse(a[i]));
	e=macromathr2l(e,lispparse("(= assign)"));
	return e;
	// return macromath(macromath(macromath(e,lispparsexx("(. deref)")),lispparsexx("(* mul)(/ div)") ), lispparsexx("(+ add)"));
}


String lisp(String s)
{
	return String( eval(slanguage( lispparse(s))));
}
EXPORTFUNC(lisp);

String lexlisp(String s)
{
	return String(slanguage(lispparse(s)));
}
EXPORTFUNC(lexlisp);

String ycombinator(String)
{
	return String("  (lambda (r)  ((lambda (f) (lambda (x) ((f f) x)))    (lambda (h) ((lambda (g) (g (lambda (z) ((h h) z)))) r  )) ))");
}
EXPORTFUNC(ycombinator);

String lambdafactorial(String)
{
	return String("   (lambda (s)  (lambda (a)   (if (eq a 1) 1 (mul a (s (add a -1 ))) )  )) ");
}
EXPORTFUNC(lambdafactorial);

String yfac(String)
{
	return ycombinator("") + " " + lambdafactorial("") ; 
	// return String("((lambda (h) (lambda (z) ((h h) z)))  (lambda (f)  ((lambda (g) (g (lambda (x) ((f f) x))))  (lambda (s) (lambda (a)  (if (eq a 1) 1 (mul a (s (add a (minus 1)))))))))) ");
}
EXPORTFUNC(yfac);


void keyvent(const char *name,int k)
{
	eval(CDR( assoc(lobjint(k),CDR(lookup(name)) )));
}

String FuncInterp(const char *_s) 
{
	while(*_s == ' ' || *_s=='\t') {_s++;}
	if(!strncmp(_s,"lisp ",5)) _s+=5;
	if(*_s=='\"') _s++;
	return lisp(_s);
}

String FuncPreInterp(const char *_s) {
	while(*_s == ' ' || *_s=='\t') {_s++;}
	return lisp(String("'(") + _s+ ")");
}

void *LVarLookup(String name,ClassDesc *&cdesc_out)
{
		plobj a=CDR(lookup(name));
		if(!a || a->type != LADDR)
			return NULL;
		cdesc_out = a->cdesc;
		return a->addr;
}

void LVarMatches(String prefix,Array<String> &match) 
{
	lobj *v=vars;
	while(v)
	{
		assert(v->car->type==LCONS);
		assert(v->car->car->type==LSYM);
		if(!strncmp(v->car->car->s,prefix,strlen(prefix)))
		{
			match.Add(v->car->car->s);
		}
		v=v->cdr;
	}
}

//----------------
//   Reflection
//
//   provides a dictionary of classes
//   note this doesnt have to be part of the language/interpretor
//

// Alternate non-lisp centric implementation of class dictionary:
Array<ClassDesc*> Classes(-1);
ClassDesc* GetClass(String cname)
{
	if(cname=="") return NULL;
	for(int i=0;i<Classes.count;i++) 
		if(Classes[i]->cname==cname) 
			return Classes[i];
	return Classes.Add(new ClassDesc(cname));
}

ldeclare::ldeclare(String cname,String mname,String tname,int offset)
{
	GetClass(cname)->members.Add(ClassDesc::Member(mname,offset,GetClass(tname)));  // Alternate non-lisp centric implementation of class dictionary
}

LDECLARE(float3,x);
LDECLARE(float3,y);
LDECLARE(float3,z);

LDECLARE(float4,x);
LDECLARE(float4,y);
LDECLARE(float4,z);
LDECLARE(float4,w);

void *deref(void *p,ClassDesc *cdesc,String m,ClassDesc *&memcdesc_out)
{
	assert(cdesc);
	ClassDesc::Member *mb=cdesc->FindMember(m);
	if(!mb) return NULL;
	memcdesc_out = mb->metaclass;
	return ((char*)p) + mb->offset;
}


void xmlimport(void *obj,ClassDesc* t, xmlNode *n)
{
	if(!t || !obj || !n) return;
	if(!stricmp(t->cname,"float"))  { StringIter(n->body) >> *((float*)obj)   ; return;}
	if(!stricmp(t->cname,"int"))    { StringIter(n->body) >> *((int*)  obj)   ; return;}
	if(!stricmp(t->cname,"String")) { *((String*)obj) = n->body               ; return;}
	if(!stricmp(t->cname,"float3")) { StringIter(n->body) >> *((float3*)obj)   ; }  // dont return since you might actually have child xml nodes like <x>1</x> <y>2</y> <z>3</z>
	if(!stricmp(t->cname,"float4")) { StringIter(n->body) >> *((float4*)obj)   ; } 
	for(int i=0;i<n->children.count;i++)
	{
		ClassDesc *ct=NULL;
		void *cobj = deref(obj,t,n->children[i]->tag,ct);
		xmlimport(cobj,ct,n->children[i]);
	}
}
xmlNode *xmlexport(void *obj,ClassDesc* t,const char *name)
{
	assert(t);
	assert(obj );
	xmlNode *n=new xmlNode(name);
	if(!stricmp(t->cname,"float"))  { n->body << *((float*) obj ) ; return n; }
	if(!stricmp(t->cname,"int"))    { n->body << *((int*)   obj ) ; return n; }
	if(!stricmp(t->cname,"String")) { n->body =  *((String*)obj ) ; return n; }
	if(!stricmp(t->cname,"float3")) { n->body << *((float3*)obj ) ; return n; }  
	if(!stricmp(t->cname,"float4")) { n->body << *((float4*)obj ) ; return n; } 
	for(int i=0;i<t->members.count;i++)
	{
		n->children.Add( xmlexport( (unsigned char*) obj + t->members[i].offset , t->members[i].metaclass , t->members[i].mname));
	}
	return n;
}

//-------------------
// simple dofile()

String cfg(const char *filename) 
{

	if(!filename || filename[0]=='\0') return "no file specified.   usage: cfg <filename>";
	FILE *fp=fopen(filename,"r");
	if(!fp) {
		return "No such file exists";
	}
	char buf[1024];
	buf[0]='\0';
	while(fgets(buf,1023,fp)){
		char *s=buf;
		while(*s==' ' || *s=='\t') s++;
		if(*s=='\0' || *s=='#') continue;
		char *e=s+strlen(s)-1;
		while(e>=s && (*e=='\n' || *e=='\r')) *e='\0';
		FuncInterp(s);
	}
	fclose(fp);
	return String("done dofile");
}
EXPORTFUNC(cfg);  // since files might use the cfg extension.

