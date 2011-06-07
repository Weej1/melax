

#include "object.h"
#include "stringmath.h"
#include "xmlparse.h"
#include "console.h"

Array<Entity*> Objects(-1);

Entity *ObjectFind(const char *name)
{
	for(int i=0;i<Objects.count;i++)
	 if(!_stricmp(Objects[i]->id,name)) 
		 return Objects[i];
	return NULL;
}

static String Uniqueify(const char *a)
{
	String s=a;
	int k=0;
	while(ObjectFind(s))
	{
		k++;
		s=String(a)+String(k);
	}
	return s;
}
Entity::Entity(const char *_name,int hashinit):id(Uniqueify(_name))
{
	EXPOSEMEMBER(id);
	Objects.Add(this);
}

Entity::~Entity()
{
	while(trackers.count)
	{
		trackers.Pop()->notify(this);  // note that the trackers may delete themselves and try to remove themselves from trackers array.
	}
	if(Objects.element)  // quick check to ensure global list hasn't been destructed already
		Objects.Remove(this);
}



String objectlist(String s)
{
	extern String html; // output intended for a browser
	html="";
	html << "\n<UL>\n";
	for(int i=0;i<Objects.count;i++) 
	{
		if(s.Length() && strncmp(s,Objects[i]->id,s.Length())!=0)
			continue;
		html  << "<LI> " << Objects[i]->id << "\n";

//		for(int j=0;j<Objects[i]->hash.slots_count ; j++)
//		{
//			if(!Objects[i]->hash.slots[j].used) 
//				continue;
//			html  << "<LI> " << Objects[i]->id << "." << Objects[i]->hash.slots[j].key << " = " << Objects[i]->hash.slots[j].value.Get() << "\n";
//		}
	}
	html << "\n</UL>\n";
	return "html dump of all internal object variables";
}
EXPORTFUNC(objectlist);



String htmlobjects(String s)
{
	extern String html;
	html="";
	html << "\n<UL>\n";
	for(int i=0;i<Objects.count;i++)
	{
			html  << "<LI> <a target=objectview href=\"http://localhost/htmltweaker " << Objects[i]->id << "\"> " << Objects[i]->id << " </a> </LI>\n" ;
	}
	html << "\n</UL>\n";
	html << "<hr>\n";
	html << "<iframe height=\"30%\" id=objectview width=\"100%\" />\n" ;
	return "html list of objects";
}
EXPORTFUNC(htmlobjects);

String htmltweaker(String s)
{
	extern String html;
	html="";
	html << "\n<UL>\n";
	for(int i=0;i<Objects.count;i++)
	{
		if(s.Length() && strncmp(s,Objects[i]->id,s.Length())!=0)
			continue;
//		for(int j=0;j<Objects[i]->hash.slots_count ; j++)
//		{
//			if(!Objects[i]->hash.slots[j].used) 
//				continue;
//			html  << "<LI> <form method=get action=\"http://localhost/script\"> <textarea name=text rows=1 cols=60> " 
//				  << Objects[i]->id << "." << Objects[i]->hash.slots[j].key << " = " << Objects[i]->hash.slots[j].value.Get() 
//				  << "</textarea><button type=SUBMIT>SUBMIT</button></form>\n";
//		}
	}
	html << "\n</UL>\n";
	return "html forms generated for all object console variables";
}
EXPORTFUNC(htmltweaker);
