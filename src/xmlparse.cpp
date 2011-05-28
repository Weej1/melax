//
//               xmlparse
//
// A very simple parser for loading
// valid xml into a useful tree data structure.
// This code does not do xml validation and therefore
// isn't that forgiving with broken input.
//
// 
// 

#include "xmlparse.h"
#include <stdio.h>
#include <windows.h>

/*
int    IsOneOf(const char c,const char *s);  // iff s contains a c
const char*  SkipChars(const char *s,const char *delimeter=" \t");
const char*  SkipToChars(const char *s,const char *tokens);
char*  SkipChars(char *s,const char *delimeter=" \t");
char*  SkipToChars(char *s,const char *tokens);

int IsOneOf(const char e, const char *p) {
	while(*p) if(*p++==e) return 1;
	return 0;
}

const char *SkipChars(const char *s,const char *delimeter) 
{
	// Finds the position of the first character in s
	// that isn't in the list of characters in 'delimeter'
	while(*s&& IsOneOf(*s,delimeter)){ s++;}
	return s;
}

const char *SkipToChars(const char *s,const char *tokens) 
{
	while(*s && !IsOneOf(*s,tokens)) {s++;}
	return s;
}

char *SkipChars(char *s,const char *delimeter) 
{
	while(*s&& IsOneOf(*s,delimeter)){ s++;}
	return s;
}

char *SkipToChars(char *s,const char *tokens) 
{
	while(*s && !IsOneOf(*s,tokens)) {s++;}
	return s;
}
*/


static void indent(FILE *fp,int d) { for(int i=0;i<d;i++) fprintf(fp," ");}



const String &xmlNode::attribute(const char *s)const
{
	static String emptystring("");
	const Attribute *a=hasAttribute(s);
	if(a)return a->value;
	return emptystring;
}


static char *s,*t;
static String token("");
String &NextToken()
{
	s = SkipChars(s," \t\n\r");
	t = s;
	if(!*s) {
		token = String(""); 
		return token;
	}
	if(*t=='\"') {
		s=t=t+1;
		s = SkipToChars(s,"\"");
		token = String(t,s-t);
		if(*s) s++;
		return token;
	}
	if(IsOneOf(*t,"<>!?=/")) {
		s++;
		token = String(t,1);
		return token;
	}
	s = SkipToChars(s,"<>!?=/ \r\t\n");
	token = String(t,s-t);
	s = SkipChars(s," \t\n\r");
	return token;
}

static xmlNode *XMLParse() {
	while(token != "<" || IsOneOf(*s,"!?")) {
		NextToken();
		assert(*s);
	}
	xmlNode *elem = new xmlNode();
	elem->tag = NextToken();
	while(*s && NextToken() != ">" && token != "/") {
		String &newval = elem->attribute(token);
		NextToken();
		assert(token == "=");
		newval = NextToken();
	}
	if(token == "/") {
		NextToken();
		assert(token == ">");
		return elem; // no children
	}
	assert(token == ">");
	NextToken();
	while( token !="<" || *s!='/') {
		if(token == "<") {
			elem->children.Add(XMLParse());
		}
		else {
			s = SkipToChars(s,"<");
			elem->body << String(t,s-t);
			NextToken();
		}
	}
	assert(*t=='<');assert(*s=='/');
	NextToken();
	NextToken();
	assert(token == elem->tag);
	NextToken();
	assert(token ==">");
	NextToken();
	return elem;
}

xmlNode *XMLParseData(char *buf, int len) {
	char *end = buf+len;
	s = t = buf;
	return XMLParse();
}

xmlNode *XMLParseFile(const char *filename) {
	char *buf;
	if(!filename || !*filename) return NULL;
	FILE *fp = fopen(filename,"r");
	if(!fp) fp = fopen(String(filename)+".xml","r");
	if(!fp) return NULL;
	assert(fp);
	fseek(fp,0,SEEK_END);
	long int len = ftell(fp);
	fseek(fp,0,SEEK_SET);
	buf = new char[len+1];
	assert(buf);
	int rc = fread(buf,1,len,fp);
	//assert(rc==len);
	buf[rc]='\0';
	char crap[16];
	rc=fread(crap,1,16,fp);
	assert(rc<=0);
	xmlNode *e = XMLParseData(buf,len);
	delete buf;
	return e;
}


void XMLSaveFile(xmlNode *elem,FILE *fp)
{
	static int depth=0;
	int singleline = (elem->children.count==0 && strlen(elem->body)<60); 
	int i;
	indent(fp,depth);
	fprintf(fp,"<%s",(const char*)elem->tag);
	for(i=0;i<elem->attributes.count;i++) {
		fprintf(fp," %s=\"%s\"",(const char*)elem->attributes[i]->key,(const char*)elem->attributes[i]->value);
	}
	fprintf(fp,(singleline)?">":">\n");
	depth+=2;
	for(i=0;i<elem->children.count;i++) {
		XMLSaveFile(elem->children[i],fp);
	}
	if(strlen((const char*)elem->body)) {
		if(!singleline) indent(fp,depth);
		fprintf(fp,"%s%s",(const char*)elem->body,(singleline)?"":"\n");
	}
	depth-=2;
	if(!singleline) indent(fp,depth);
	fprintf(fp,"</%s>\n",(const char*)elem->tag);
}
void XMLSaveFile(xmlNode *elem,const char *filename)
{
	FILE *fp = fopen(filename,"w");
	assert(fp);
	XMLSaveFile(elem,fp);
	fclose(fp);
}

