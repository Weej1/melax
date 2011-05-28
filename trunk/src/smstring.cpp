//
//      String
//
// More convenient than using character arrays (char*). 
// Its your typical String class - nuff said.
//
//
//

#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <windows.h>  // for file directory search routine
#include <stdlib.h>

#include "smstring.h"
#include "array.h"
#include "console.h"


String::String()
{
	buf=NULL;
	size=length=0;
	prev=next=this;
}

String::String(const String &s)
{
	buf=NULL;
	size=length=0;
	prev=next=this;
	*this = s;
}

String &String::operator=(const String& s)
{
	if(next != this) 
	{
		prev->next=next;
		next->prev=prev;
		next=prev=this;
	}
	else if (buf) 
	{
        delete[] buf;
    }
	buf=NULL;
	size=length=0;
	if(s.buf==NULL || s[0]=='\0')
	{
		return *this;
	}
	buf    = s.buf;
	length = s.length;
	size   = s.size;
	prev   = &s;
	next   = s.next;
	s.next->prev = this;
	s.next = this;
	return *this;
}

String::String(const char *s)
{
	buf=NULL;
	next=prev=this;
	size=length=0;
	*this = s;
}


String &String::operator=(const char *s)
{
	if(s==*this) return *this;
	if(next != this) 
	{
		prev->next=next;
		next->prev=prev;
		next=prev=this;
		buf=NULL;
		size=length=0;
	}
	if(buf) buf[0]='\0';
	length=0;
	*this << s;
	return *this;
}

void String::resize(int _newsize)
{
	detach();  
	char *newbuf = new char[_newsize];
	for(int i=0;buf && i<size && i<_newsize;i++)
	{
		newbuf[i]=buf[i];
		if(buf[i]=='\0') break;  
	}
	if(buf) delete []buf;
	buf=newbuf;
	size=_newsize;
}

String::operator const char *() const{ 
	static char *rv="";
	return buf?buf:"\0";
} 


//----------------------------------------------
// Concatenating operators.  Note '+=' is not always a member.
//
String &String::operator<<(const char *b)
{
	if(!b) return *this;
	int blen = (int)strlen(b);
	if(blen==0) 
	{
		return *this;
	}
	detach();
	if(size < length+blen +1)
	{
		int newsize=16;
		while(newsize < length+blen +1) newsize *=2;
		assert( b-buf <0 || b-buf >=size);  // implement this case later, adding part of onesself so cant delete buf during resize.
		resize(newsize);
	}
	strcpy(buf+length,b);
	length+=blen;
	return *this;
}




String::String(const char *s,int slen)
{
	buf=NULL;
	next=prev=this;
	if(slen==0)
	{
		*this = "";
		return;
	}
	assert(slen>0);
	length = slen;
	size   = length+1;
	buf=new char[size];
	if(s) strncpy(buf,s,slen);
	else  buf[0]='\0';
	buf[length] = '\0';
	return;
}

void String::detach()
{
	if(this==next) 
	{
		if(!buf)
		{
			size=16;
			buf=new char[size];
			buf[0]='\0';
		}
		return;
	}
	if(size==0) size=16;
	buf=new char[size];
	strncpy(buf,(const char*)*next,size);
	prev->next=next;
	next->prev=prev;
	next=prev=this;
}

String::~String() 
{
	if(next != this) 
	{
		prev->next=next;
		next->prev=prev;
		next=prev=this;
		buf=NULL;
		size=length=0;
	}
    if (buf) {
        delete[] buf;
		buf=NULL;
		size=length=0;
    }
}


int String::Asint() const{
	int a=0;
	sscanf_s(*this,"%d",&a);
	return a;
}
float String::Asfloat() const{
	float f=0.0f;
	sscanf_s(*this,"%f",&f);
	return f;
}



String operator+(const String &a,const String &b){
	String s(a);
	s << b;
	return s; 
}

//----------------------------------------------
// The obvious operators for comparing strings
// Dont you wish strcmp() was called strdiff().
//
int operator==(const String &a,const String &b){
	return (!stricmp(a,b));
}
int operator!=(const String &a,const String &b){
	return (stricmp(a,b)); 
}
int operator>(const String &a,const String &b){
	return (stricmp(a,b)>0);  
}
int operator<(const String &a,const String &b){
	return (stricmp(a,b)<0);  
}
int operator>=(const String &a,const String &b){
	return (stricmp(a,b)>=0); 
}
int operator<=(const String &a,const String &b){
	return (stricmp(a,b)<=0); 
}
int operator==(const String &a,const char *b){
	return (!stricmp(a,b));
}
int operator!=(const String &a,const char *b){
	return (stricmp(a,b));
}
int operator> (const String &a,const char *b){
	return (stricmp(a,b)>0);  
}
int operator< (const String &a,const char *b){
	return (stricmp(a,b)<0);  
}
int operator>=(const String &a,const char *b){
	return (stricmp(a,b)>=0); 
}
int operator<=(const String &a,const char *b){
	return (stricmp(a,b)<=0); 
}
int operator==(const char *a,const String &b){
	return (!stricmp(a,b));
}
int operator!=(const char *a,const String &b){
	return (stricmp(a,b));
}
int operator> (const char *a,const String &b){
	return (stricmp(a,b)>0);  
}
int operator< (const char *a,const String &b){
	return (stricmp(a,b)<0);  
}
int operator>=(const char *a,const String &b){
	return (stricmp(a,b)>=0); 
}
int operator<=(const char *a,const String &b){
	return (stricmp(a,b)<=0); 
}

String &String::sprintf(char *format,...) 
{
	detach();
	va_list vl;
	va_start(vl, format);
	if (buf) {	
		delete[] buf;
		buf=NULL;
	}
	int  n;
	char *tmp = NULL;
    int tmpsize = 128;
	while( (n = _vsnprintf((tmp=new char[tmpsize]), tmpsize-1, format, vl))<0) {
		delete []tmp;
		tmpsize *=2;
	}
    buf = new char[n+1];
	assert(buf);
	strcpy(buf,tmp);
	delete []tmp;
	va_end(vl);
	return *this;
}
	


//------------------------------------------------
// Returns true if c is one of the characters in s
int IsOneOf(const char e, const char *p) {
	while(*p) if(*p++==e) return 1;
	return 0;
}

//------------------------------------------------
// Finds the position of the first character in s
// that isn't in the list of characters in 'delimeter'
const char *SkipChars(const char *s,const char *delimeter) {
	while(*s&& IsOneOf(*s,delimeter)){ s++;}
	return s;
}

const char *SkipToChars(const char *s,const char *tokens) {
	while(*s && !IsOneOf(*s,tokens)) {s++;}
	return s;
}

char *SkipChars(char *s,const char *delimeter) {
	while(*s&& IsOneOf(*s,delimeter)){ s++;}
	return s;
}

char *SkipToChars(char *s,const char *tokens) {
	while(*s && !IsOneOf(*s,tokens)) {s++;}
	return s;
}

String &String::DropFirstChars(char *delimeter){
	if(!buf) {
		return *this;
	}
	(*this) = SkipChars(buf,delimeter);
	return *this;
}

char *DeleteTrailingChars(char *s,const char *delimeter)
{
	if(!s) return NULL;
	char *p=s+strlen(s)-1;
	while(p>=s && IsOneOf(*p,delimeter))
		*p--='\0';
	return s;
}

String &String::DropLastChars(char *delimeter){
	if(!buf) {
		return *this;
	}
	this->detach();
	DeleteTrailingChars(this->buf,delimeter);
	this->length = strlen(this->buf);
	return *this;
}

//---------------------------------------------------
// PopFirstWord - removes and returns first word in
// the supplied string.  
// Note that input parameter 'line' will be modified.
String PopFirstWord(String &line,char *delimeter) {
	Array<char> f;
	const char *s = line;
	s = SkipChars(s,delimeter);
	while((*s) && !IsOneOf(*s,delimeter)) {
		f.Add(*s);
		s++;
	}
	f.Add('\0');
	s = SkipChars(s,delimeter);
	line = s;
	return String(f.element);		
}

String PopLastWord(String &line) {
	Array<char> f;
	const char *s = line+strlen(line);
	assert(! *s);
	while(s>(const char*)line && IsOneOf(*(s-1)," \t")) {
		s--;
	}
	f.Add('\0');
	while((s>(const char*)line) && !IsOneOf(*(s-1)," \t\n\r")){
		s--;
		f.Insert((*s),0);
	}
	while(s>(const char*)line && IsOneOf(*(s-1)," \t")){
		s--;
	}
	line.operator[](s-(const char *)line) ='\0';
	return String(f.element);		
}
 
String ToLower(const char *b)
{
	String s(b);
	int i=0;
	while(s[i]) {s[i]=tolower(s[i]);i++;}
	return s;
}

int hashpos(const String &s,int number_of_slots)
{
	int sum=0;
	const char *p = s;
	while(*p){sum+=(unsigned char)(*p++);}
	return sum%number_of_slots;  
}


int fileexists(const char *filename)
{
	if(!filename || !*filename) return 0;
	FILE *fp;
	fp=fopen(filename,"r");
	if(fp)fclose(fp);
	return (fp!=NULL);
}

String splitpathfname(String filename)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
	drive[0]=dir[0]=fname[0]=ext[0]='\0';
	_splitpath(filename, drive, dir, fname, ext);
	return fname;
}
EXPORTFUNC(splitpathfname);

String splitpathext(String filename)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
	drive[0]=dir[0]=fname[0]=ext[0]='\0';
	_splitpath(filename, drive, dir, fname, ext);
	return ext;
}
EXPORTFUNC(splitpathext);

String splitpathdir(String filename)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
	drive[0]=dir[0]=fname[0]=ext[0]='\0';
	_splitpath(filename, drive, dir, fname, ext);
	return dir;
}
EXPORTFUNC(splitpathdir);


String filefind(const String &filename,String pathlist,const String &extentions)
{
	if(filename=="") return String("");
	if(fileexists(filename)) return filename;
	String fn;
	if(splitpathdir(filename)!="")
	{
		String ex=extentions;
		while(ex != "")
		{
			String e = PopFirstWord(ex," .;,\t");
			fn = filename + "." + e;
			if(fileexists(fn)) return fn;
		}
		return String("");
	}
	while(pathlist[0])
	{
		String p = PopFirstWord(pathlist," ;,\t");
		p.DropLastChars("/\\");
		p << "/";
		fn = p+filename;
		if(fileexists(fn)) return fn;
		String ex=extentions;
		while(ex != "")
		{
			String e = PopFirstWord(ex," .;,\t");
			fn = p + filename + "." + e;
			if(fileexists(fn)) return fn;
		}
	}
	return String("");
}

int filesearch(const char *searchstring,Array<String> &filenames)
{
	WIN32_FIND_DATA d;
	HANDLE h;
	
	h = FindFirstFile(searchstring,&d);
	if(h==INVALID_HANDLE_VALUE) 
	{
		printf("WARN: Unable to find any files in %s \n",searchstring);
		return(0);
	}
	do {
		//printf(" %s  ...",d.cFileName); fflush(stdout);
		char drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
		fname[0]=ext[0]='\0';
		_splitpath(d.cFileName, drive, dir, fname, ext);
		if(!*fname) continue;
		filenames.Add(String(fname)+ext);
	} while(FindNextFile(h,&d)) ;
	FindClose(h);
	return filenames.count;
}


