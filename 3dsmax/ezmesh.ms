--
--   EZM - easy mesh exporter for simple rendering in demos
--   XML Export Untility Functions
--
--   by S Melax (c) 2003 (updated 2007 and 2009) - no rights reserved
--   www.melax.com
--
--  this version does not use the plugin ezmesh.dlm normally found in the plugins subdirectory.
--  I was having max version issues 8 vs9 vs etc..
--
-- This is an extra module to export display mesh data
-- in a simple xml format.
--
-- Installation instructions:  put this file in (getdir #maxroot)/scripts/startup
--
-- This collection of functions is not specific any single mesh format 
-- and could easily be used for other exporting apps.  
-- Feel free to use any of the ideas/code here for your own work.
--
-- Sure, you can easily write importers and exporters with a lot of 
-- print statements and sscanf calls all over the place, but that's 
-- not always the cleanest way to do things.  Ideally you want to 
-- set up systems so that when you change your data structures you
-- dont have to go and update your i/o modules.  I also suggest 
-- designing your datafile formats, your c++ classes, and your max 
-- rollouts so that they are reflections of each other.  Then it 
-- will be trivial to automate everything. 
--
-- The xml export system is based on max structs
-- i.e. define structs to define your data model.
-- Each struct corresponds to an xml node type.  
-- To distinguish between xml node attributes and children 
-- simply add a "_" at the beginning of any struct's member name 
-- that is to be an attrib.  The leading "_" is not exported.
--
-- for example, the code:
--
--   struct HockeyPlayer ( _name , position , mass, jersey);
--   wayne = HockeyPlayer "Gretzky" [3,5,0] 190 99;
--   xmlexport wayne
--
-- produces:
--
--   <HockeyPlayer name="Gretzky">
--    <jersey>99</jersey>
--    <mass>190</mass>
--    <position>3.0 5.0 0.0</position>
--   </HockeyPlayer>
--
--  Yeah, its alphabetical.  Max internally randomizes the order of struct 
--  members so you cant get the order you want.  xml parsers shouldn't care.
-- 
--  The xml export facilities handle arrays and nested structures too.
--
--  You dont have to stuff all your data into structures.  You can separate your
--  calls to begin and end the export of an xml node, thereby allowing you to spit
--  out whatever data inbetween.  
-- 

global ezm_outfile      --  whether a window or a file, we send output to it
ezm_outfile = listener  --  if not specified, then output to maxscript listener window F11

global xmlVecDelimeter;   -- some people like <v>x,y,z</v> others like <v>x y z</v>
global xmlObjDelimeter;   -- To delimit array elements within an xml body
global xmlindentstring;   -- you can change this to "\t" or to "" for no indentation.
global xmlIgnorePrefix;   -- if you want your xml tokens to strip of a prefix on the struct's names
global xmlDelimLastObj;
global xmlindentamount=0;

if xmlVecDelimeter==undefined then xmlVecDelimeter= " "; 
if xmlObjDelimeter==undefined then xmlObjDelimeter= ","; 
if xmlindentstring==undefined then xmlindentstring=" ";  -- for readability of raw ascii file
if xmlIgnorePrefix==undefined then xmlIgnorePrefix="ezm_";   
if xmlDelimLastObj==undefined then xmlDelimLastObj=true;

function xmlindent =
(
	local i
	for i = 1 to xmlindentamount do
	  format xmlindentstring to:ezm_outfile
)

function xmlStringFriendly b=
(
	local d = xmlVecDelimeter
	if(classof b == array) then
	(
		local s = xmlStringFriendly(b[1])
		local i
		for i = 2 to b.count do (
			s+=d ;
			s += xmlStringFriendly b[i]
		)
		return s;
	)
	if (classof b == point3) then return (b.x as string) + d  +  (b.y as string) + d  + (b.z as string);
	if (classof b == point4) then return (b.x as string) + d  +  (b.y as string) + d  + (b.z as string) + d  + (b.w as string);
	if (classof b == quat) then return (b.x as string) + d  +  (b.y as string) + d  + (b.z as string) + d  + (b.w as string);
	if (classof b == matrix3) then 
	(  
		return((b[1].x as string) + d + (b[1].y as string) + d + (b[1].z as string) + d + "0" + d +
		       (b[2].x as string) + d + (b[2].y as string) + d + (b[2].z as string) + d + "0" + d +
		       (b[3].x as string) + d + (b[3].y as string) + d + (b[3].z as string) + d + "0" + d +
		       (b[4].x as string) + d + (b[4].y as string) + d + (b[4].z as string) + d + "1"      );
	)
	return b as string;
)

function xmlExportunit t b =
(
	xmlindent();
	format "<%>" t to:ezm_outfile
	format "%" (xmlStringFriendly b) to:ezm_outfile
	format "</%>\n" t to:ezm_outfile
)
function xmlIsAttrib p =
(
	return (p as string)[1] == "_"
)
function xmlDeriveStructTag s = 
(
	--  we support stipping a prefix from the classname
	--  when determining the xml tags
	--  This is so you can have xml tokens like shape and box
	--  without conflicting with preexisting max classes
	local tag = (filterstring s ":(")[2] 
	local i;
	local match = true;
	if xmlIgnorePrefix.count==0 then match=false;
	for i = 1 to xmlIgnorePrefix.count do
	(
		if tag[i]!= xmlIgnorePrefix[i] then match=false;
	)
	if(match) then
	(
		oldtag=tag;
		tag=""
		for i=xmlIgnorePrefix.count+1 to oldtag.count do
		(
			tag += oldtag[i]
		)
	)
	return tag;
)

function xmlExport x = (messagebox "WTF" ) -- function declaration, actual function defined below

function xmlExportBegin x =
(
	-- x has to be a struct
	local tag = xmlDeriveStructTag (classof x as string)
	local props = getpropnames x;
	sort props
	local p
	local v
	xmlindent();
	format "<%" tag  to:ezm_outfile
	-- output attributes  
	for p in props do if(xmlIsAttrib p) then   -- for each attrib
	(
		v = getproperty x p 
		if(v!=undefined) then format " %=\"%\"" (trimleft (p as string) "_") (xmlStringFriendly v) to:ezm_outfile
	)
	format ">\n" to:ezm_outfile
	xmlindentamount+=1;
	for p in props do if(not xmlIsAttrib(p)) do   -- for each child xml node
	(
		v = getproperty x p 
		if(v == undefined) then continue; 
		local a
		local e
		if(classof v == Array) then a = v;
		else a = #( v )
		for e in a do
		(
			if(classof e==MAXScriptFunction) then e(x);  --- not sure the best way to provide args to this
			if(classof(classof e) == structdef) then xmlExport e;
			else xmlExportunit (p as string) e
		)
	)
)

function xmlExportEnd x =
(
	local tag = xmlDeriveStructTag (classof x as string)
	xmlindentamount-=1;
	xmlindent();
	format "</%>\n" tag to:ezm_outfile
	return x
)

function xmlExport x =
(
	xmlExportBegin x;
	xmlExportEnd   x;
)

-- now this is going overboard:
rollout xmlutilrollout "XML Exporter Settings"
(
	label xmllab "Global XML Settings"
	edittext xvd "xmlVecDelimeter"  align:#right fieldwidth:60 bold:true
	edittext xod "xmlObjDelimeter"  align:#right fieldwidth:60 bold:true
	edittext xis "xmlIndentString"  align:#right fieldwidth:60 bold:true
	edittext xip "xmlIgnorePrefix"  align:#right fieldwidth:60 bold:true
	checkbox xtd "Obj Delimeter After Last" 
	button   cls "Close"
	on xmlutilrollout open do
	(
		xvd.text = xmlVecDelimeter
		xod.text = xmlObjDelimeter
		xis.text = xmlindentstring
		xip.text = xmlIgnorePrefix
		xtd.state= xmlDelimLastObj
	)
	on cls pressed do removerollout xmlutilrollout;
	on xvd changed  s do xmlVecDelimeter = s ;
	on xod changed  s do xmlObjDelimeter = s ;
	on xis changed  s do xmlIndentString = s ;
	on xip changed  s do xmlIgnorePrefix = s ;
	on xtd changed  s do xmlDelimLastObj = s ;
)
utility xmlutils "xml exporter utils" 
(
	on xmlutils open do
	(
		addrollout xmlutilrollout 
	)
	on xmutils closed do
	(
		removerollout xmlutilrollout 
	)
)


-- "EZMesh XML Utility Functions Loaded"
-- now create a mesh exporter using the above xml routines


-- assume xmlIgnorePrefix="ezm_"     -- remove "ezm_" prefix from xml tokens

-- assume persistent global ezm_exportscale = 1.0  --   0.01 would convert cm to meters.  gets applied to positions and offsets - not normals.
ezm_exportscale = 0.01

persistent global ezm_exportanim = false;   -- exports anim frames when exportings skin bones.

struct ezm_skeleton (_count , bone)
struct ezm_bone (_name,_parent , _position, _orientation)
struct ezm_scene(_name)
struct ezm_model (_name, skeleton)
struct ezm_mesh(material ,_semantic="3dsmaxtrimesh")
struct ezm_verts(_count,_semantic="position");
struct ezm_vertexbuffer(_count,_semantic="position orientation texcoord bones weights");
struct ezm_indexbuffer(_count);
struct ezm_tverts(_count,_semantic="texcoord");
struct ezm_faces(_count,_semantic="verts tverts smoothinggroup matid");
struct ezm_animation(_name, _trackcount, _framecount, _duration, _dtime)
struct ezm_animTrack(_name, _count)

function ezm_ripspaces s = 
(
	local i
	for i=1 to s.count do if s[i]==" " then s[i]="_"
	s
)

function ezm_findobject s =
(
	for o in objects do if (o.name == s) then return o; 
	return undefined;	
)
function ezm_getpositionlocal n =
(
	local p = [0,0,0]
	try ( p=n.controller.position ) catch (
		if(n.parent == undefined) then 
		(
			p= getposition n;
		)
		else
		(
			local pp = getposition n.parent
			local pr = getrotation n.parent
			local mp = getposition n
			p= (mp-pp) * ((pr) as matrix3) 
		)
	)
	return p
)


function ezm_getrotationlocal n =
(
	local q=(quat 0 0 0 1)
	try( q = conjugate n.controller.rotation) catch (
		if(n.parent == undefined) then 
		(
			q = getrotation n;
		)
		else
		(
			local pr = getrotation n.parent
			local mr = getrotation n
			q= (conjugate pr) * mr 
		)
	)
	return  q
)

function ezm_exportboneanim n =
(
	local i,t
	select n
	local sm = n.modifiers["skin"]
	local pah = ezm_animation("animation")
	pah._trackcount = skinops.getnumberbones sm
	kt = #();
	for i = 1 to skinops.getnumberbones sm do
	(
		local bn = skinops.getbonename sm i 0 
		local bo = ezm_findobject bn;
		for k in bo.rotation.controller.keys do AppendIfUnique kt k.time
	)
	sort kt
	pah._framecount = kt.count ; --1+(animationrange.end-animationrange.start).ticks / ticksperframe
	pah._dtime      = 0.033
	pah._duration   = (kt[kt.count] - kt[1] ).ticks / ticksperframe * pah._dtime
	xmlExportBegin pah
	for i = 1 to skinops.getnumberbones sm do
	(
		local bn = skinops.getbonename sm i 0 
		local bo = ezm_findobject bn;
		local pat  = ezm_animTrack bn
		pat._count =  pah._framecount
		xmlExportBegin pat
		for  t in kt do -- = animationrange.start to animationrange.end by 5 do 
		(
			slidertime = t
			local pos = [0,0,0]
			local rot = (quat 0 0 0 1)
			try ( pos = ezm_getpositionlocal bo * ezm_exportscale  ; rot = ezm_getrotationlocal bo ;) catch( )
			xmlindent()
			format " %  %  %\n" ((t-animationrange.start)  as float / 4800.0) (xmlStringFriendly(pos))  (xmlStringFriendly(rot)) to: ezm_outfile 
		)
		xmlExportEnd pat
	)
	xmlExportEnd pah
)


function ezm_materialdiffusemapname mat = 
(
	local filename = "NULL"
	try( filename =(getfilenamefile mat.diffusemap.filename)) catch()
	return filename
)

function ezm_getfaceorientation m i = 
(
	local v0 = getvert m (getface m i)[1]
	local v1 = getvert m (getface m i)[2]
	local v2 = getvert m (getface m i)[3]
	local n = cross(v1-v0) (v2-v1)
	local t0 = gettvert m (gettvface m i)[1]
	local t1 = gettvert m (gettvface m i)[2]
	local t2 = gettvert m (gettvface m i)[3]
	local a = length n
	n=normalize n;
	local b = (v2-v0)*(t1.x-t0.x) - (v1-v0)*(t2.x-t0.x)  --  binormal contrib
	local t = cross (normalize b)  n
	b = cross n t
	local q = conjugate((matrix3 t b n [0,0,0]) as quat)
	return [q.x ,q.y,q.z,q.w] * a;  -- weighted by area
)


function ezm_exportdisplaymeshskin n =
(
	if classof ezm  !=ReferenceTarget or classof ezm.exportskin != InterfaceFunction then
	(
		-- no maxsdk, i cant recompile the c code
		--return messagebox "unable to locate needed dll function to export vertexbuffer/indexbuffer";
	)
	local i,j
	max modify mode
	select n
	local sm = n.modifiers["skin"]
	if sm==undefined then  return messagebox "No skin on node - cant export as skinned displaymesh";
	local dm = ezm_model (ezm_ripspaces n.name)
	local m = n.mesh	
	local skeleton = #()
	for i = 1 to skinops.getnumberbones sm do
	(
		local bn = skinops.getbonename sm i 0 
		local bo = ezm_findobject bn;
		local bp, pos,rot
		try ( pos = ezm_getpositionlocal bo * ezm_exportscale  ; rot = ezm_getrotationlocal bo ;bp = (ezm_ripspaces bo.parent.name) ; ) catch( )
		local b  = ezm_bone (ezm_ripspaces bn) bp pos rot
		append skeleton b
	)
	dm.skeleton = ezm_skeleton skeleton.count skeleton

	local orientations=#()
	local texcoord=#()
	local sw = #()
	local si = #()
	for i= 1 to m.numverts do
	(
		orientations[i] = [0,0,0,0];
		texcoord[i]=[0,0,0];
		local w = #(1.0, 0.0, 0.0, 0.0)
		local k = #(0,0,0,0)
		for j = 1 to (skinops.getvertexweightcount sm i) do  if j<5 do
		(
			k[j] = ((skinops.getvertexweightboneid sm i j) - 1)  -- fix max's index by 1 to be C like
			w[j] = (skinops.getvertexweight sm i j)
		)
		w[1] = 1 - (w[2]+w[3]+w[4])
		-- for debugging: format "skinvert %    % % % %   % % % %\n" i  k[1] k[2] k[3] k[4] w[1] w[2] w[3] w[4] to:ezm_outfile
		join sw w
		join si k
	)
	
	xmlExportBegin dm
	if(ezm_exportanim) then ezm_exportboneanim n
	local ms = ezm_mesh (ezm_materialdiffusemapname n.material)  
	xmlExportBegin ms
	
	local fs=ezm_indexbuffer ( getNumFaces m ) 
	xmlExportBegin fs
	for i=1 to ( getNumFaces m ) do
	(
		local orn = ezm_getfaceorientation m i
		for j=1 to 3 do if (length orn) > (length orientations[(getface m i)[j]])  then texcoord[(getface m i)[j]] = gettvert m (gettvface m i)[j];
		for j=1 to 3 do orientations[(getface m i)[j]] += orn
		local t = (getface m i) - [1,1,1]
		xmlindent();
		format "% % %,\n"  (t.x as integer) (t.y as integer) (t.z as integer)  to:ezm_outfile
	)
	xmlExportEnd fs

	local vb= ezm_vertexbuffer   (getnumverts m)
	xmlExportBegin vb
	for i=1 to (getnumverts m) do 
	(
		local w = #(1.0, 0.0, 0.0, 0.0)
		local b = #(0,0,0,0)
		for j = 1 to (skinops.getvertexweightcount sm i) do  if j<5 do
		(
			b[j] = ((skinops.getvertexweightboneid sm i j) - 1)  -- fix max's index by 1 to be C like
			w[j] = (skinops.getvertexweight sm i j)
		)
		w[1] = 1 - (w[2]+w[3]+w[4])

		xmlindent();
		local v = (getvert m i )*ezm_exportscale
		local o =  normalize orientations[i]
		local t =  texcoord[i] -- gettvert m i 
		format "% % %  % % % %  % %   "  v.x v.y v.z  o.x o.y o.z o.w  t.x t.y  to:ezm_outfile
		format "% % % %  % % % %"   b[1] b[2] b[3] b[4]  w[1] w[2] w[3] w[4] to:ezm_outfile
		format ",\n"  to:ezm_outfile		
	)
	xmlExportEnd vb
	xmlExportEnd ms
	xmlExportEnd dm
)


function ezm_exportdisplaymeshnoskin n =
(
	if classof ezm !=ReferenceTarget or classof ezm.exportskin != InterfaceFunction then
	(
		return messagebox "unable to locate needed dll function to export vertexbuffer/indexbuffer";
	)
	local i,j
	local dm = ezm_model (ezm_ripspaces n.name)
	local m = n.mesh	
	local pos = ezm_getpositionlocal n * ezm_exportscale  
	local rot = ezm_getrotationlocal n 
	local b  =  ezm_bone (ezm_ripspaces n.name) undefined pos rot
	dm.skeleton = ezm_skeleton 1 b
	xmlExportBegin dm
	if(ezm_exportanim) then ezm_exportboneanim n
	local ms = ezm_mesh (ezm_materialdiffusemapname n.material)
	xmlExportBegin ms
	ezm.exportmesh ezm_outfile m ezm_exportscale 
	xmlExportEnd ms
	xmlExportEnd dm
)
function ezm_exportdisplayscene scenename = 
(
	local n
	local g = $selection
	local sg = ezm_scene scenename
	xmlExportBegin sg
	for n in g do
	(
		if (superclassof n) != GeometryClass then continue
		if n.modifiers["skin"] == undefined then 
		(
			ezm_exportdisplaymeshnoskin n
			
		)
		else 
		(
			ezm_exportdisplaymeshskin n
		)
	)
	xmlExportEnd   sg
	try (select g) catch ()
)


function ezm_MeshExport n basename=
(
	local nodelist = #()
	format "<?xml version=\"1.0\"?>\n" to: ezm_outfile
	ezm_exportdisplayscene basename
)

function ezm_MeshToFile n =
(
	local filename
	local basename = "untitled";
	if(maxfilename !=undefined and maxfilename != "") then basename = (filterstring maxfilename ".")[1];
	else if (classof $) != ObjectSet and $ != undefined then basename = n.name

	filename = (getdir #export) + "\\" + basename  + ".ezm"
	filename = getsavefilename filename:filename types:"EZM-XML(*.ezm)|*.ezm"
	if (filename == undefined) then ( print "cancelled"; return 0;)
	ezm_outfile = createfile filename
	if(ezm_outfile == #undefined) then (
		print "unable to open file: " + filename
		return undefined
	)
	ezm_MeshExport n basename
	close ezm_outfile
	ezm_outfile = listener -- or set back to #undefined
	print ("Wrote " + filename)
	return filename
)
function ezm_MeshToWindow n =
(
	ezm_outfile = listener
	ezm_MeshExport n "test_output"
)


if ( classof (ezm_meshex) == RolloutClass) then 
(
	max utility mode
	removerollout ezm_meshex 
	max create mode
	print "Previous EZMesh Rollout Removed."
)

utility ezm_meshex "EZM Eazy Mesh Exporter"
(
		label sel "Selected: (current selection)" align:#left
		label dirpath "save directory" 
		checkbox exportanim "Export Animation Frames" checked:false
		spinner exportscalespinner "Export Scale" range:[0.001,10000,1] 
		button towindow "Export To Window" width:120 tooltip:"writes the data into the maxscript listener window"
		button tofile   "Export To File"   width:120 
		on ezm_meshex open do
		(
			dirpath.text = (getdir #export) 
			exportscalespinner.value = ezm_exportscale;
			ezm_exportanim = false
			exportanim.checked = false
		)
		on exportanim changed s do ezm_exportanim = exportanim.checked

		on ezm_meshex close do
		(
		)
		on exportscalespinner changed val do
		(
			ezm_exportscale = val
		)
		on towindow pressed do 
		(
			local n = $
			ezm_MeshToWindow n
		)
		on tofile pressed do 
		(
			local n=$
			local f = ezm_MeshToFile n
		)
		function setselectionstring s = ( sel.text = "Selected: "+s;)
)

max utility mode
addrollout ezm_meshex 
max create mode

try ( ezm.setstream listener ) catch ( print "EZMesh Warning:  unable to call function ezm.setstream from EZMESH.DLM"; )

"EZMesh Exporter Loaded"


