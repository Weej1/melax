//
//      String
//
// More convenient than using character arrays (char*). 
// Its your typical String class - nuff said.
// This is a general use class.  Not NovodeX specific.
// This file has no dependancies on any other data types.
//
//

#ifndef SM_STRING_H
#define SM_STRING_H

#include <stdio.h>
#include <string.h>
#include "array.h"


class String 
{
  public:
				String();
				String(const String &t);
				String(const char *s);
				String(const char *s,int len);
	template<class T>
	explicit	String(const T &a){buf=NULL;prev=next=this;length=size=0;*this="";*this+=a;}
				~String();
	String &	operator =(const String &s);
	String &	operator =(const char *s);
	String &	operator+=(const char *b);
	String &	sprintf(char *format,...);
	const char  operator[](int i) const {return buf?buf[i]:'\0';}
	char &		operator[](int i){detach(); return buf[i];}
				operator const char *() const;
	//			operator char *() { detach(); return buf;}
	String &	DropFirstChars(char *delimeter=" \t");
	String &	DropLastChars(char *delimeter=" \t");
	void		detach();  // unshare data with siblings copy if necessary
	void		resize(int _newsize);
	int			Asint() const;
	float		Asfloat() const;
	int			Length() const {return length;}
	int			correctlength(){if(buf)length=(int)strlen(buf);return length;}
private:
	char		*buf;
	int			length; // strlen
	int			size;   // memory allocated
protected:
	mutable const String	*prev;  // loop of siblings sharing the same data
	mutable const String	*next;  // loop of siblings sharing the same data
};

String operator+(const String &a,const String &b);

inline String &operator+=(String &s,float a)
{
	char buf[128];
	sprintf_s(buf,sizeof(buf),"%g",a);
	return s+=buf;
}

inline String &operator+=(String &s,int a)
{
	char buf[32];
	sprintf_s(buf,sizeof(buf),"%d",a);
	return s+=buf;
}
inline String &operator+=(String &s,char a)
{
	char buf[32];
	sprintf_s(buf,sizeof(buf),"%c",a);
	return s+=buf;
}
inline String &operator+=(String &s,unsigned int a)
{
	char buf[32];
	sprintf_s(buf,sizeof(buf),"%d",a);
	return s+=buf;
}
template<class T>
inline String &operator << (String &s,const T &t)
{
	return s+=t;
}

template <class T>
inline String &append(String &s,T* a,int n,const char *delimeter="\n")
{
	for(int i=0;i<n;i++) 
	{
		s+=a[i];
		s+=delimeter;
	}
	return s;
}

int operator==(const String &a,const String &b);
int operator!=(const String &a,const String &b);
int operator> (const String &a,const String &b);
int operator< (const String &a,const String &b);
int operator>=(const String &a,const String &b);
int operator<=(const String &a,const String &b);
int operator==(const String &a,const char   *b);
int operator!=(const String &a,const char   *b);
int operator> (const String &a,const char   *b);
int operator< (const String &a,const char   *b);
int operator>=(const String &a,const char   *b);
int operator<=(const String &a,const char   *b);
int operator==(const char   *a,const String &b);
int operator!=(const char   *a,const String &b);
int operator> (const char   *a,const String &b);
int operator< (const char   *a,const String &b);
int operator>=(const char   *a,const String &b);
int operator<=(const char   *a,const String &b);


int    IsOneOf(const char c,const char *s);  // iff s contains a c
const char*  SkipChars(const char *s,const char *delimeter=" \t");
const char*  SkipToChars(const char *s,const char *tokens);
char*  SkipChars(char *s,const char *delimeter=" \t");
char*  SkipToChars(char *s,const char *tokens);
char*  DeleteTrailingChars(char *s,const char *tokens);

String PopFirstWord(String &line,char *delimeter=" \t\n\r");
String PopLastWord(String &line);
int    hashpos(const String &s,int number_of_slots);
inline int    hashpos(String &s,int number_of_slots) {return hashpos((const String)s,number_of_slots);}
String ToLower(const char *b);

int fileexists(const char *filename);
String filefind(const String &filename,String pathlist,const String &extentions);
int filesearch(const char *searchstring,Array<String> &filenames); // search using expressions star dot star, returns all found fname with ext only
String splitpathdir(String filename);
String splitpathext(String filename);
String splitpathfname(String filename);


#endif
