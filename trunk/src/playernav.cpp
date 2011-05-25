

#include "object.h"
#include "bsp.h"
#include "console.h"
#include "sound.h"
#include "brush.h"


int HitCheckCylinder(float r,float h,Brush *brush,const float3 &_v0,const float3 &_v1,float3 *impact)
{
	// the bsp hitcheckcylinder uses leaves which may not reach cylinder so check point first
	float3 v0=_v0-brush->position;
	float3 v1=_v1-brush->position;
	if(HitCheckCylinder(r,h,brush->bsp,0,v0,v1,impact,float3(0,0,0)))
	{
		*impact += brush->position;
		return 1;
	}
	if(HitCheck(brush->bsp,1,v0,v1,impact))
	{
		*impact += brush->position;
		return 1;
	}
	return 0;
}

int HitCheckCylinder(float r,float h,Brush *brush,const float3 &_v0)
{
	float3 unused; 
	return HitCheckCylinder(r,h,brush,_v0,_v0,&unused);
}



class Player: public Entity
{
  public:
	float3     position;
	float3     positionnew;
	float3     positionold;
	Quaternion orientation;
	float3     velocity;
	float      height;
	float      radius;
	float      headtilt;
	float3     groundnormal;

	Player():Entity("player"),height(1.75f),radius(0.3f)
	{
		LEXPOSEOBJECT(Player,"bob");
		EXPOSEMEMBER(position);
		EXPOSEMEMBER(positionnew);
		EXPOSEMEMBER(positionold);
		EXPOSEMEMBER(orientation);
		EXPOSEMEMBER(velocity);
		EXPOSEMEMBER(height);
		EXPOSEMEMBER(radius);
		EXPOSEMEMBER(headtilt);
	}
	void wasd_fly();
	void spinmove(const float3 &cr);
	void wasd_mlook();
	void move();
	void areacheck(Brush *area);
	Brush *findspace();
};

LDECLARE(Player,position);
LDECLARE(Player,orientation);

Player gplayer;
float3 &PlayerPosition(){return gplayer.position;}
Quaternion &PlayerOrientation(){return gplayer.orientation;}
float& PlayerHeadtilt(){return gplayer.headtilt;}
float& PlayerHeight(){return gplayer.height;}

float3 accelleration;
float3 gravity(0,0,-10.0f);
float  maximumspeed = 10.0f;
float  jumpspeed = 10.0f;
float  dampingground=10.0f;
float  dampingair=1.0f;
int    moveforward;
int    movebackward;
int    moveleft;
int    moveright;
int    movejump;
float  moveturn;
float  movetilt;
int    movedown;

EXPORTVAR(accelleration);
EXPORTVAR(gravity);
EXPORTVAR(maximumspeed);
EXPORTVAR(jumpspeed);
EXPORTVAR(dampingground);
EXPORTVAR(dampingair);
EXPORTVAR(moveforward);
EXPORTVAR(movebackward);
EXPORTVAR(moveleft);
EXPORTVAR(moveright);
EXPORTVAR(movejump);
EXPORTVAR(moveturn);
EXPORTVAR(movetilt);
EXPORTVAR(movedown);

float mouse_sensitivity=0.5f;
EXPORTVAR(mouse_sensitivity);

Sound *jumpsound;


extern float DeltaT;//fixme
extern int centermouse;
extern int focus;
extern float MouseDX;
extern float MouseDY;
extern float &viewangle;

float viewangle_previous;
float viewanglemin = 15.0f;
EXPORTVAR(viewanglemin);
float zoomrate=90.0f;
EXPORTVAR(zoomrate);
int zoommode=0;
float camzoomin(const char*)
{
	if(zoommode==0)
	{
		viewangle_previous = viewangle;
	}
	zoommode=1;
	viewangle = Max(viewangle-zoomrate*DeltaT,viewanglemin);
	return viewangle;
}
EXPORTFUNC(camzoomin);
String camzoomout(const char*)
{
	//currently not capturing every frame key is up...
viewangle = viewangle_previous;  // not smooth here

	if(!zoommode) return "";
	viewangle += zoomrate*DeltaT;
	if(viewangle>=viewangle_previous)
	{
		viewangle=viewangle_previous;
		zoommode=0;
	}
	return String(viewangle);
}
EXPORTFUNC(camzoomout);



//extern BSPNode *area_bsp;
//extern Brush *Area;
Brush *currentroom;
extern float3 HitCheckImpactNormal;
int jumpareacheck=0;
EXPORTVAR(jumpareacheck);
Brush *groundcontact=NULL;

void Player::wasd_mlook() 
{
	//if(!currentroom) currentroom=area;
	if(!jumpsound)jumpsound=SoundCreate("whoosh.wav");

	if(centermouse &&focus ) {
		moveturn = -MouseDX * mouse_sensitivity*viewangle;
		movetilt =  MouseDY * mouse_sensitivity*viewangle;
	}
	float damping = (groundcontact)?dampingground:dampingair;
	//velocity = velocity *  powf((1.0f-damping),DeltaT);
	float3 accelleration;
	if(moveforward-movebackward !=0 || moveright-moveleft!=0)
	{
		Quaternion thrustdomain = (groundcontact)?RotationArc(float3(0,0,1),groundnormal)*orientation:orientation;
		accelleration += normalize((thrustdomain.ydir()*(float)(moveforward-movebackward)+
							 thrustdomain.xdir()*(float)(moveright-moveleft)))*maximumspeed*damping;
	}
	accelleration += gravity;

	float3 accdamping = (velocity-((groundcontact)?groundcontact->velocity:(currentroom)?currentroom->velocity:float3(0,0,0))) * -damping;

	float3 microimpulse;
	if(groundcontact && !movejump && !moveforward && !movebackward && !moveright &&!moveleft) {
		microimpulse = groundnormal * (gravity.z*groundnormal.z) -float3(0,0,gravity.z);
	}
	if(groundcontact) {
		velocity.z = Min(0.0f,velocity.z);
		if(movejump) {
			SoundPlay(jumpsound);	
			velocity.z = jumpspeed;
			groundcontact=NULL;
			jumpareacheck=1;
		}
	}
	accelleration += accdamping;
	accelleration += microimpulse; 
	velocity = velocity + accelleration * DeltaT;
	positionnew = position + velocity *DeltaT;

	orientation = orientation * QuatFromAxisAngle(float3(0,0,1),DegToRad(moveturn));
	headtilt    += movetilt;
	headtilt = Min( 90.0f,headtilt);
	headtilt = Max(-90.0f,headtilt);
	

	movejump=moveforward=movebackward=moveleft=moveright=movedown=0;
	movetilt=moveturn=0.0f;
}
void wasd_mlook(Entity *object) 
{
	assert(&gplayer == object);
	gplayer.wasd_mlook();
}



int playerstuck(Player *e)
{
	if(!currentroom) { return 1;}
	return HitCheckCylinder(e->radius,e->height,currentroom,e->position);
}
char *stuckcheck(const char*)
{
	return (playerstuck(&gplayer))?"WARN: Player Embedded In Solid Matter":"";
}
EXPORTFUNC(stuckcheck);
String playerroom(String)
{
	if(!currentroom) return "Not Currently In A Room";
	return currentroom->id;
}
EXPORTFUNC(playerroom);

void Player::areacheck(Brush *area)
{
	float grounddist=0;
	int wallcontact=0;
	float3 target = positionnew;
	float3 impact;
	const float3 &rp = area->position;
	const float3 &rpn = area->positionnew;
	int hit=0;
	while(hit<5&& HitCheckCylinder(radius,height,area->bsp,0,position-rp,positionnew-rpn,&impact,float3(0,0,0))) {
		impact+=rpn;
		hit++;
		float3 norm = HitCheckImpactNormal;
		if(norm.z > 0.5f){
			groundcontact=area;
			jumpareacheck =0;
			groundnormal = norm;
			grounddist = -dot(groundnormal,impact);
		}
		else if(norm.z > -0.5f){
			wallcontact = 1;
		}
		// slide along plane of impact
		positionnew = PlaneProject(Plane(norm,-dot(norm,impact)),positionnew) + norm*0.00001f;
		if(groundcontact && dot(norm,groundnormal)<0 && dot(groundnormal,positionnew)+grounddist<=0) {
			float3 slide = PlaneProject(Plane(norm,0),groundnormal);
			slide *= -(dot(groundnormal,positionnew)+grounddist)/(dot(slide,groundnormal));
			slide += safenormalize(slide)*0.000001f;
			positionnew += slide;
		}
	}
	if(hit==5) {
		// couldn't resolve it;
		positionnew  = position +rpn-rp;  // TO TEST: adjusmment based on room brush moving.
	}
	if(groundcontact && wallcontact) {
		hit=1;
		float3 positionup = positionnew + float3(0,0,0.4f);
		while(hit<5&&HitCheckCylinder(radius,height,area->bsp,0,positionnew-rpn,positionup-rpn,&positionup,float3(0,0,0))) {
			positionup+=rpn;
			positionup.z -= 0.00001f;
			hit++;
		}
		float3 targetup = target + (positionup-positionnew);
		while(hit<5&& HitCheckCylinder(radius,height,area->bsp,0,positionup-rpn,targetup-rpn,&impact,float3(0,0,0))) {
			impact+=rpn;
			hit++;
			float3 norm = HitCheckImpactNormal;
			// slide along plane of impact
			targetup = PlaneProject(Plane(norm,-dot(norm,impact)),targetup) + norm*0.00001f;
		}			
		float3 positiondrop = targetup - (positionup-positionnew);
		while(hit<5&& HitCheckCylinder(radius,height,area->bsp,0,targetup-rpn,positiondrop-rpn,&positiondrop,float3(0,0,0))) {
			positiondrop +=rpn;
			positiondrop.z += 0.00001f;
			hit++;
		}
		if(hit!=5) {
			// couldn't resolve it;
			positionnew  = positiondrop;
		}
	}
	if(hit) {
		velocity = (positionnew-position)/DeltaT;
	}
}
Brush *Player::findspace()
{
	float3 unused;
	for(int i=0;i<Brushes.count;i++)
	{
		if(Brushes[i] != currentroom && !BSPFinite(Brushes[i]->bsp) && Brushes[i]->collide && !HitCheckCylinder(radius,height,Brushes[i],position))
		{
			return Brushes[i];
		}
	}
	return NULL;
}




void Player::move()
{
	positionold=position;
	float3 unused; // dummy variable to store returned unused impact point
	if(playerstuck(&gplayer)) jumpareacheck=1;
	Brush *newarea;
	if(jumpareacheck && (newarea = findspace()))
	{
		currentroom=newarea;
		jumpareacheck=0;
	}

	groundcontact=NULL;

	extern int blobnav(float r,float h,const float3 &_v0,const float3 &_v1,float3 *impact);
	if(blobnav(radius,height,position,positionnew,&positionnew))
	{
		velocity = (positionnew-position)/DeltaT;
		groundcontact = currentroom;  // so I can jump.
		groundnormal = float3(0,0,1.0f);
	}

	for(int i=0;i<Brushes.count;i++)
	{
		if(!Brushes[i]->collide) continue; // we dont collide with these brushes.
		if(!BSPFinite(Brushes[i]->bsp)) continue;   // only want to collide with finite solid chunks here - not rooms
		if(HitCheckCylinder(radius,height,Brushes[i]->bsp,0,position-Brushes[i]->position,position-Brushes[i]->position,&unused,float3(0,0,0))) continue;  // already interpenetrating - dont restrict movement.
		areacheck(Brushes[i]);  // 
	}
	if(currentroom)
		areacheck(currentroom);  // do this bsp last since we have to remain in a valid position wrt this space.
	else
	{
		positionnew.z = Max(0.01f,positionnew.z); // prevent endless falling
		velocity.z = Min(0.0f,velocity.z);
	}

	position = positionnew;	

	// trigger checks
	for(int i=0;i<Brushes.count;i++)
	{
		int playerinside = HitCheckCylinder(radius,height,Brushes[i]->bsp,0,position-Brushes[i]->position,position-Brushes[i]->position,&unused,float3(0,0,0));
		if(playerinside != Brushes[i]->playerinside)
		{
			Brushes[i]->playerinside = playerinside;
			FuncInterp((playerinside)? Brushes[i]->onenter:Brushes[i]->onexit);
		}
		// else if(playerinside) FuncInterp(brushes[i]->stillinside);
	}
}

void playermove(Entity *object)
{
	assert(object==&gplayer);
	gplayer.move();
}


void Player::wasd_fly()
{
	positionold=position;
	if(playerstuck(&gplayer)) jumpareacheck=1;
	Brush *newarea;
	if(jumpareacheck && (newarea = findspace()))
	{
		currentroom=newarea;
		jumpareacheck=0;
	}


	if(centermouse && focus) {
		moveturn = -MouseDX * mouse_sensitivity*viewangle;
		movetilt =  MouseDY * mouse_sensitivity*viewangle;
	}
	orientation = orientation * QuatFromAxisAngle(float3(0,0,1),DegToRad(moveturn));
	headtilt    += movetilt;
	headtilt = Min( 90.0f,headtilt);
	headtilt = Max(-90.0f,headtilt);
	Quaternion q = orientation * YawPitchRoll(0,headtilt-90.0f,0);

	float damping = dampingground;

	float3 accelleration;
	if(moveforward-movebackward !=0 || moveright-moveleft!=0 || (movejump-movedown)!=0 )
	{
		accelleration += safenormalize((q.zdir()*(float)(moveforward-movebackward)+
							 orientation.xdir()*(float)(moveright-moveleft) + 
							 orientation.zdir()*(float)(movejump-movedown)    ))*maximumspeed*damping;
	}
	else
	{
		velocity = float3(0,0,0);
	}
	float3 accdamping = velocity * -damping;
	accelleration += accdamping;
	velocity = velocity + accelleration * DeltaT;
	position = position + velocity *DeltaT;

	movejump=moveforward=movebackward=moveleft=moveright=movedown=0;
	movetilt=moveturn=0.0f;

}
void wasd_fly(Entity *object)
{
	assert(object==&gplayer);
	gplayer.wasd_fly();
}

void Player::spinmove(const float3 &cr)
{
	positionold=position;
	if(playerstuck(&gplayer)) jumpareacheck=1;
	Brush *newarea;
	if(jumpareacheck && (newarea = findspace()))
	{
		currentroom=newarea;
		jumpareacheck=0;
	}

	Quaternion q0 = orientation * YawPitchRoll(0,headtilt+90,0);
	
	moveturn = -MouseDX * mouse_sensitivity*viewangle;
	movetilt =  MouseDY * mouse_sensitivity*viewangle;
	orientation = orientation * QuatFromAxisAngle(float3(0,0,1),DegToRad(moveturn));
	headtilt    += movetilt;
	headtilt = Min( 90.0f,headtilt);
	headtilt = Max(-90.0f,headtilt);
	Quaternion q1 = orientation * YawPitchRoll(0,headtilt+90,0);
	position = cr + rotate(q1*Inverse(q0),(position+float3(0,0,height)-cr))  - float3(0,0,height);
	moveturn = movetilt = 0.0f;
}
void spinmove(Entity *object,const float3 &cr)
{
	assert(object=&gplayer);
	gplayer.spinmove(cr);
}
