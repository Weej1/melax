// for linking be sure to add library ws2_32.lib
// there's likely no stdin, so select() will return SOCKET_ERROR (-1) instead of 0 in that case.
//

#include <stdio.h>
#include <assert.h>

#ifdef SGI

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  include <bstring.h>
#  include <sys/time.h>
#  include "sgi.h"
#  define INVALID_SOCKET (-1)
   void WSACleanup(){};
   typedef int SOCKET;
   void closesocket(int s) {shutdown(s,2);}
#  define SOCKADDR_IN  struct sockaddr_in
#  define FAR  
#  define DEFAULT_PORT  2132
#else
#  include <io.h>
#  include <winsock.h>
   void perror(char *){printf("%d is the error", WSAGetLastError());}
#  define DEFAULT_PORT  80
#endif

#include "d3dfont.h"
#include "array.h"
#include "console.h"
//#include "stringy.h"

int verbose=0;  // perhaps should be a global
EXPORTVAR(verbose);

int sockstuff =0;
int network=0;
SOCKET listen_sock=INVALID_SOCKET;
char *http_trailer="HTTP/1.0";
Array<SOCKET> Players;


int Startup() {
	if(sockstuff) return sockstuff; // already initialized
	int status;
	WSADATA shit;
	if ((status = WSAStartup(MAKEWORD(1,1), &shit)) != 0) {
         //sprintf(szTemp, "%d is the err", status);
         //MessageBox( hWnd, szTemp, "Error", MB_OK);
		  return 0;
	}
	sockstuff=1;
	return sockstuff;
}


int Listen(int port=DEFAULT_PORT) {
	if(!Startup()) {
		// I guess not
		printf("ERROR couldn't start up\n");
		return INVALID_SOCKET;
	}


	SOCKADDR_IN listen_addr;  /* Local socket - internet style */

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	listen_addr.sin_port = htons(port);        /* Convert to network ordering */
	listen_sock = socket( AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) {
		perror("socket:  ");
		return 0;
	}
	if (bind( listen_sock, (struct sockaddr FAR *) &listen_addr, sizeof(listen_addr)) == -1) {
		perror("ERROR:  bind:  ");
		listen_sock = INVALID_SOCKET;
		return 0;
	}
	if (listen( listen_sock, 4 ) < 0) {
		perror("listen:  ");
		listen_sock = INVALID_SOCKET;
        	// printf("%d is the error", WSAGetLastError());
	 	WSACleanup();
 		return 0;
	}
	return 1;
}



SOCKET Accept() {
	SOCKADDR_IN accept_addr;    /* Accept socket address - internet style */
	int accept_addr_len;        /* Accept socket address length */
	accept_addr_len = sizeof(accept_addr);

	SOCKET accept_sock;
	accept_sock = accept( listen_sock,(struct sockaddr FAR *) &accept_addr,
           (int FAR *) &accept_addr_len );
	if (accept_sock < 0) {
		perror("accept:  ");
		// printf("%d is the error", WSAGetLastError());
		WSACleanup();
		return INVALID_SOCKET;
	}
	return accept_sock;
}


int PollRead(SOCKET s){
	if(s==INVALID_SOCKET) return 0;
	fd_set re,wr,ex;
	struct timeval tm;
	tm.tv_sec=0;
	tm.tv_usec=0;
	FD_ZERO(&re);
	FD_ZERO(&wr);
	FD_ZERO(&ex);
	FD_SET(s,&re);
	int rc=select(s+1, &re, &wr,&ex,&tm);
	if(rc==SOCKET_ERROR ) return 0;
	//printf("select:  rc=%d  readfdset=%d\n",rc,FD_ISSET(s,&re));
	return (rc && FD_ISSET(s,&re));
}
int PollWrite(SOCKET s){
	if(s==INVALID_SOCKET) return 0;
	fd_set re,wr,ex;
	struct timeval tm;
	tm.tv_sec=0;
	tm.tv_usec=0;
	FD_ZERO(&re);
	FD_ZERO(&wr);
	FD_ZERO(&ex);
	FD_SET(s,&wr);
	select(1, &re, &wr,&ex,&tm);
	return (FD_ISSET(s,&wr));
}

void DoConsoleCommand(char *command,int s=-1) {
printf("Got a command:\n%s\n-----------\n",command);

	// strip off http shit, decode it and try to extract the console command
	// if the http stuff isn't there, then just execute what ya got
	if(!strncmp("GET /",command,5)) {
		command+=5;
	}
	if(!strncmp("script?text=",command,12)) {
		command+=12;
	}
	char *commandend=command;
	while((*commandend != '\0') && (*commandend != '\n') && (*commandend != '\r')) {
		commandend++;
	}
	int len = strlen(http_trailer);
	if((commandend-command)>=len && !strncmp(commandend-len,http_trailer,len)) {
		commandend-=len;
	}

	char exbuffer[512];
	int k=0;
	while(command<commandend) {
		if(*command == '=') {
			exbuffer[k++]=' ';
			command++;
		}
		if(*command == '?') {
			exbuffer[k++]=' ';
			command++;
		}
		if(*command == '+') {
			exbuffer[k++]=' ';
			command++;
		}
		else if(*command == '%'){
			char hx[5];
			sprintf(hx,"0x__");
			hx[2]=command[1];
			hx[3]=command[2];
			hx[4]='\0';
			int n;
			sscanf(hx,"%x",&n);
			exbuffer[k++] = (char) n;
			command+=3;
		}
		else {
			exbuffer[k++] = *command;
			command++;
		}
	}
	exbuffer[(k++)-1] = '\0';
	assert(k<=512);
	//printf("Got from network:  %s\n",exbuffer);
	if(s!=INVALID_SOCKET) {
		int rc=send(s,exbuffer,strlen(exbuffer),0);
		if(rc==-1) {
			perror("send:  ");
		}
		char outbuf[256];
		sprintf(outbuf,"\n</pre></code>Response:<code><pre>\n");
		send(s,outbuf,strlen(outbuf),0);
	}
	int se=dup(2);
	int so=dup(1);
	dup2(s,1); // send stdout to socket
	dup2(s,2); // send stderr to socket
	char *rs=FuncInterp(exbuffer);
	dup2(so,1);
	dup2(se,2);
	close(so);
	close(se);
	PostString(exbuffer,0,1,5);
	PostString(rs,0,2,5);
	if(s!=INVALID_SOCKET) {
		send(s,rs,strlen(rs),0);
	}
}

void NetworkSend(char *buf) {
	char shit[1024];
	memset(shit,'\0',1024);
	strcpy(shit,buf);
	//int num_bytes = strlen(buf)+1;
	int num_bytes = 1024;
	// broadcast command
	int i;
	char ds[1024]; //debugstring
	ds[0]='\0';
	//sprintf(ds+strlen(ds),"Write: (%d bytes)  ",);
	sprintf(ds+strlen(ds),"Write: (%d bytes)  ",num_bytes);
	for(i=0;i<Players.count;i++) {
		sprintf(ds+strlen(ds),"  player %d: ",i);
		if(PollWrite(Players[i])) {
			int rc=send(Players[i], buf,num_bytes,0);
			if(rc != num_bytes) {
				sprintf(ds+strlen(ds),"%2d/%2d  ",rc,num_bytes);
				//printf("only sent %d/%d bytes\n",rc,num_bytes);
			}
			else {
				sprintf(ds+strlen(ds),"OK     ");
			}
		}
		else {
			//printf("Problem with player %d\n",i);
			sprintf(ds+strlen(ds),"fail   ");
		}
	}
	PostString(ds,0,3,0);
}
void GetOtherPlayerInfo() {
	int junk=0;
	int i;
	char ds[1024]; //debugstring
	ds[0]='\0';
	sprintf(ds+strlen(ds),"Read:  ");
	for(i=0;i<Players.count;i++) {
		sprintf(ds+strlen(ds),"  player %d: ",i);
		int pkts=0;
		int bytes=0;
		int error=0;
		while(PollRead(Players[i]) && !error) {
			char buf[1025];
			int rc=0;
			rc = recv(Players[i],buf,1024,  0);
			if(rc<=0){ 
				error=1;
				sprintf(ds+strlen(ds),"ERR %2d  ",rc);
				//printf("problem reading (%d) from player %d\n",rc,i);
				continue;
			}
			if(rc>0) {
				pkts++;
				junk++;
				bytes+=rc;
				// things worked, we got some bytes
				buf[rc]='\0';
				//printf("read %d bytes from socket: %s\n",rc,buf);
				DoConsoleCommand(buf);
			}
		}
		sprintf(ds+strlen(ds),"pkts=%2d  bytes=%d  %s  ",pkts,bytes,(error)?"ERR":"OK ");
	}
	PostString(ds,0,4,0);
	if(junk)	PostString(ds,0,5,5);
}

void DoStdinStuff() {
	if(!PollRead(0)) return;
	char buf[1024];
        char *rs=NULL;
	//String com;
	while(PollRead(0)) {
		buf[0]='\0';
		gets(buf+strlen(buf));
		//com+=buf;
		//com+="\n";
	}
	PostString(buf,0,3,5);  
	PostString(rs=FuncInterp(buf),0,4,5);
	printf("%s\n",rs);
}
void DoNetworkStuff(){
	DoStdinStuff();
	// check for guy connecting and deal with it if so
	if(!sockstuff || listen_sock==INVALID_SOCKET) {
		return;
	}
	if(Players.count) {GetOtherPlayerInfo();}
	assert(listen_sock != INVALID_SOCKET);
	if(!PollRead(listen_sock)) { return;}
	SOCKET s=Accept();
	if(s==INVALID_SOCKET) {return;}
	char buf[2048];
	int rc=recv(s,buf,8,  0);
	buf[rc]='\0';
	if(rc==8 && !stricmp(buf,"PERSIST")) {
		Players.Add(s);
		printf("Hey %d Joined the game\n",s);
		return;
	}
	if(rc!=8) {
		printf("what the heck is going on %d\n",rc);
	}
	int bc=0;
	while(PollRead(s) && (bc=recv(s,buf+rc,1024-rc,  0)) >0) {
		rc  = rc+bc;
	}
	buf[rc]='\0';
	

	char outbuf[256];
	sprintf(outbuf,"HTTP/1.0 200 OK\nContent Type: text/html\n\n");
	rc = send(s,outbuf,strlen(outbuf),0);
	sprintf(outbuf,"<html>  <body BGCOLOR=\"#60a060\">");
	rc = send(s,outbuf,strlen(outbuf),0);
	if(rc==-1) {
		perror("send:  ");
	}
	if(verbose) {
		sprintf(outbuf,"You Submitted:<code><pre>\n");
		send(s,outbuf,strlen(outbuf),0);
		send(s,buf,strlen(buf),0);
		sprintf(outbuf,"\n</pre></code>");
		send(s,outbuf,strlen(outbuf),0);
	}
	sprintf(outbuf,"Parsed:<code><pre>\n");
	send(s,outbuf,strlen(outbuf),0);
	
	DoConsoleCommand(buf,s);

	sprintf(outbuf,"\n</pre></code>\n\n click <b>BACK</b> in your web browser.\n<hr></body></html>\n");
	send(s,outbuf,strlen(outbuf),0);

	closesocket(s);
}


String disablenetwork(String) {
	if(listen_sock != INVALID_SOCKET) {
		closesocket(listen_sock);
		listen_sock=INVALID_SOCKET;
	}
	if(sockstuff) {
		WSACleanup();
		sockstuff=0;
	}
	return "networking off";
}
EXPORTFUNC(disablenetwork);

String enablenetwork(String) {
	// set up network listening stuff if you can
	if(listen_sock==INVALID_SOCKET) {
		Listen();
	}
	if(listen_sock==INVALID_SOCKET) {
		return "ERROR:  unable to get sockets happenging\n";
	}
	return "ok networking on";
}
EXPORTFUNC(enablenetwork);

	



#ifdef BONZO

//--------------- client side ------------------------

SOCKET ConnectTCP(char *machine,int port=80) {
  struct  hostent *phe;
  SOCKADDR_IN addr;  /* DESTination Socket INternet */
  SOCKET sock=INVALID_SOCKET;

  if(!Startup()) {
	  return INVALID_SOCKET;
  }

  sock = socket( AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
    printf( "socket() failed");
	return INVALID_SOCKET;
  }

  addr.sin_family = AF_INET;
  
  if (isdigit(machine[0]))
  {
	unsigned int address = inet_addr(machine); 
    phe =gethostbyaddr ((char *)&address,4,AF_INET);
	if(phe) {
		memcpy((char FAR *)&(addr.sin_addr), phe->h_addr,phe->h_length);
	}
	else {
		// just hack it in
		memcpy((char FAR *)&(addr.sin_addr),(char *)&address,4);
	}
  } else
  {
    phe = gethostbyname(machine);
    if (phe == NULL) {
	  printf("unable to get hostbyname %s\n. Error %d",machine,WSAGetLastError());
	  closesocket(sock);
      return INVALID_SOCKET;
    }
    memcpy((char FAR *)&(addr.sin_addr), phe->h_addr,phe->h_length);
  }

  addr.sin_port = htons(port);        /* Convert to network ordering */

  if (connect( sock, (PSOCKADDR) &addr, sizeof(addr )) < 0) {
      closesocket( sock );
      printf("connect() failed");
	  return INVALID_SOCKET;
  }
  return sock;
}

void URL(char *url)
{
  int port=80;
  char machine[255], command[255];
  SOCKET sock;

  //Strip out http://
  if (!strnicmp(url,"http://", 7))
  {
	  url+=7;
  }

  //get machine nummber
  strcpy(machine, url);
  int i=0;
  while (machine[i]!='/' && machine[i]!=' ' && machine[i]!='\x00')
  {
	  i++;
  }
  machine[i]='\0';


  //get console command
  strcpy(command, url+ strlen(machine));
  i=0;
  while (machine[i]!=':' && machine[i]!='\x00')
  {
	  i++;
  }

  //get optional port
  if (machine[i]==':')
  {
	  sscanf(machine+i+1,"%d",&port);
  }
  machine[i]='\0';




  sock = ConnectTCP(machine,port);
  if(sock == INVALID_SOCKET) {
	  return;
  }
  // else:  great we got it
  char buf[1024];
  sprintf(buf,"GET %s%s %s\n\n",(*command=='/')?"":"/",command,http_trailer);
  send(sock,buf,strlen(buf)+1,0);
  int rc;
  while((rc=recv(sock,buf,1024,  0))>0){
	  buf[rc]='\0';
	  printf("read %d bytes from socket: \n%s\n-----------\n",rc,buf);
  }
  closesocket(sock);
}




void JoinGame(char *parameterstring)
{
  int port=80;
  char machine[255];
  SOCKET sock;

  machine[0]='\0';
  sscanf(parameterstring,"%s",machine);

  sock = ConnectTCP(machine,port);
  if(sock == INVALID_SOCKET) {
	  printf("Unable to join game %s\n",machine);
	  return;
  }
  printf("Joined game %s\n",machine);
  // else:  great we got it
  char buf[8];
  sprintf(buf,"PERSIST");
  send(sock,buf,strlen(buf)+1,0);
  Players.Add(sock);
}


#ifndef _BUILDER
EXPORTFUNC(URL);
EXPORTFUNC(JoinGame);
#endif


// a hack to make sure sockets get clened up
class Junk{
public:
	Junk(){}
	~Junk();
} junk;
Junk::~Junk(){
	if(listen_sock != INVALID_SOCKET) {
		closesocket(listen_sock);
		listen_sock=INVALID_SOCKET;
	}
	if(sockstuff) {
		WSACleanup();
	}
	sockstuff=0;
}
//REGISTER(network)

#endif

