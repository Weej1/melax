//
//           xmlparse
// 
// A simple parser for loading
// valid xml into a useful tree data structure.
// See the .cpp file for details.
//

#ifndef SM_XMLPARSE_H
#define SM_XMLPARSE_H

#include "array.h"
#include "smstring.h"



class xmlNode
{
public:
	class Attribute
	{
	  public:
		String key;
		String value;
		Attribute(const char *k):key(k){}
	};
	String tag;
	Array<Attribute*> attributes;  
	Array<xmlNode*>  children;
	String body;
	xmlNode *Child(const char *s){for(int i=0;i<children.count;i++)if(s==children[i]->tag)return children[i];return NULL;}
	Attribute *hasAttribute(const char *s){for(int i=0;i<attributes.count;i++)if(s==attributes[i]->key)return attributes[i];return NULL;}
	const Attribute *hasAttribute(const char *s)const{for(int i=0;i<attributes.count;i++)if(s==attributes[i]->key)return attributes[i];return NULL;}
	String &attribute(const char *s){Attribute *a=hasAttribute(s);if(a)return a->value;return (attributes.Add(new Attribute(s)))->value;}
	const String &attribute(const char *s)const;
	xmlNode(const char *_tag,xmlNode *parent=NULL):tag(_tag){if(parent)parent->children.Add(this);}
	xmlNode(){}
	~xmlNode(){while(children.count)delete children.Pop(); while(attributes.count)delete attributes.Pop();}
};


xmlNode *XMLParseFile(const char *filename);
void     XMLSaveFile(xmlNode *elem,const char *filename);




#endif
