#ifdef _MSC_VER //kill some warnings
#define _CRT_SECURE_NO_WARNINGS
#endif

//g++ main.cpp rotate.cpp -L/usr/X11R6/lib -lX11 -lXi -lXmu -lGL -lglut -lGLU ../../INMOST.a -O5
// press space - explode mesh to see connection 
#include "inmost.h"
#include "rotate.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdarg.h>
#include "inc_glut.h"
#include <iomanip>

#include "color.h"
#include "coord.h"
#include "octree.h"
#include "streamline.h"
#include "svg_line.h"
#include "face2gl.h"
#include "color_bar.h"
#include "tga.h"
#include "screenshot.h"
#include "volumetric.h"
#include "vector.h"
#include "input.h"
#include "picker.h"
#include "clipper.h"

inline static unsigned int flip(const unsigned int * fp)
{
	unsigned int mask = -((int)(*fp >> 31)) | 0x80000000;
	return *fp ^ mask;
}
#define _0(x)	(x & 0x7FF)
#define _1(x)	(x >> 11 & 0x7FF)
#define _2(x)	(x >> 22 )

void draw_screen();
void svg_draw(std::ostream & file);

using namespace INMOST;
Mesh * mesh;
int interactive = 0;
double zoom = 1;
int width = 800, height = 800;
double sleft = 1e20, sright = -1e20, sbottom = 1e20, stop = -1e20, sfar = -1e20, snear = 1e20;
double shift[3] = {0,0,0};
bool perspective = false;
int drawedges = 0, draw_orphan = true;
bool boundary = true, planecontrol = false, clipupdate = false, bndupdate = true, clipboxupdate = false, draw_volumetric = false, elevation = false;
bool drawaxis = true, drawcolorbar = true;
Element disp_e;
Mesh::GeomParam table;
ElementArray<Element> orphans;
int is_material_defined = 0;


Storage::real p[3] = {0,0,0}, n[3] = {0,0,1};
ElementArray<Element> boundary_faces;
ElementArray<Edge> added_edges;
std::map<int,std::vector<HandleType> > material_faces;
std::map<int,double> material_x;
std::map<int,double> material_y;
std::map<int,double> material_z;
std::vector<double> harmonic_points, dual_harmonic_points, conormals;




static void GetBox(Element e, Storage::real min[3], Storage::real max[3])
{
  min[0] = min[1] = min[2] = 1.0e20;
  max[0] = max[1] = max[2] = -1.0e20;
	ElementArray<Node> nodes = e->getNodes();
	for (ElementArray<Node>::iterator it = nodes.begin(); it != nodes.end(); it++)
	{
		Storage::real_array c = it->Coords();
		for (int i = 0; i < (int)c.size(); i++) 
    {
			if (max[i] < c[i]) max[i] = c[i]; //max
			if (min[i] > c[i]) min[i] = c[i]; //min
		}
	}
  for(int i = 0; i < 3; ++i)
  {
    if( max[i] < min[i] )
    {
      max[i] = 0.0001;
      min[i] = -0.0001;
    }
    else if( max[i] == min[i] )
    {
      max[i] += 0.0001;
      min[i] += -0.0001;
    }
  }
}

double amplitude = 10;
double radius = 25;
char visualization_prompt[8192];
int visualization_prompt_active = 0;
Tag visualization_tag;
ElementType visualization_type;
bool visualization_smooth = false;


std::vector<Streamline> streamlines;


class segment
{
	coord v[2];
public:
	segment(coord a, coord b) 
	{
		v[0] = a; 
		v[1] = b;
	}
	segment(const segment & b)
	{
		v[0] = b.v[0];
		v[1] = b.v[1];
	}
	segment & operator =(segment const & b)
	{
		v[0] = b.v[0];
		v[1] = b.v[1];
		return *this;
	}
	void Draw()
	{
		glVertex3dv(v[0].data());
		glVertex3dv(v[1].data());
	}
	void SVGDraw(std::ostream & file, double modelview[16], double projection[16], int viewport[4])
	{
		double * v0 = v[0].data();
		double * v1 = v[1].data();
		svg_line(file, v0[0], v0[1], v0[2], v1[0], v1[1], v1[2], modelview, projection, viewport);
	}
};

std::vector<segment> segments;



const int name_width = 32;
const int type_width = 14;
const int elems_width = 10;
const int sparse_width = 10;
const int length_width = 10;

void PrintTag(Tag t)
{
	std::cout << std::setw(name_width) << t.GetTagName() << std::setw(type_width) << DataTypeName(t.GetDataType());
	int num = 0;
	char elems[7] = "NEFCSM";
	std::string print = "";
	for(ElementType etype = NODE; etype <= MESH; etype = etype << 1)
	{
		if( t.isDefined(etype) )
		{
			print = print + elems[ElementNum(etype)];
			num++;
		}
	}
	std::cout << std::setw(elems_width) << print;
	print = "";
	for(ElementType etype = NODE; etype <= MESH; etype = etype << 1)
	{
		if( t.isSparse(etype) )
		{
			print = print + elems[ElementNum(etype)];
			num++;
		}
	}
	std::cout << std::setw(sparse_width) << print;
	if( t.GetSize() == ENUMUNDEF )
		std::cout << std::setw(length_width) << "VAR" << std::endl;
	else
		std::cout << std::setw(length_width) << t.GetSize() << std::endl;
}

void PrintTags(Mesh * m, ElementType etypes)
{
	std::cout << std::setw(name_width) << "Name" << std::setw(type_width) << "Type" << std::setw(elems_width) << "Element" << std::setw(sparse_width) << "Sparse" << std::setw(length_width) << "Length" << std::endl;
	for(Mesh::iteratorTag t = m->BeginTag(); t != m->EndTag(); ++t )
	{
		bool print = false;
		for(ElementType etype = NODE; etype <= MESH; etype = etype << 1) if( (etype&etypes) && t->isDefined(etype) ) {print = true; break;}
		if( print ) PrintTag(*t);
	}
}






std::vector<face2gl> all_boundary;
std::vector<face2gl> added_faces;
std::vector<face2gl> clip_boundary;



volumetric * CommonVolumetricView;
Vectors * CommonVectors = NULL;



clipper * oclipper = NULL;
bnd_clipper * bclipper = NULL;
picker * current_picker = NULL;


void set_matrix3d()
{
	double aspect = (double)width/(double)height;
	double side = std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
	//double center[3] = { (sleft+sright)*0.5, (sbottom+stop)*0.5, (sfar+snear)*0.5};
	const double sc = 2;
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	//~ glOrtho(center[0]-sc*side*zoom*aspect,center[0]+sc*zoom*side*aspect,
		//~ center[1]-sc*side*zoom,center[1]+sc*zoom*side,
		//~ center[2]-sc*side*100,center[2]+sc*side*100);
	if( !perspective )
	{
		glOrtho(-sc*side*zoom*aspect,sc*side*zoom*aspect,
				-sc*side*zoom,sc*side*zoom,
				-sc*side*100,sc*side*100);
	}
	else
	{
		const double pi = 3.1415926535897932384626433832795;
		const double znear = zoom*2*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.1;
		const double zfar  = 1000000.0;
		const double fH = znear * tan( 60.0 / 360.0 * pi);
		double fW = fH * aspect;
		glFrustum(-fW,fW,-fH,fH,znear,zfar);
	}
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity();
}

void set_matrix2d()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1,1,-1,1,-1,1);
	glMatrixMode(GL_MODELVIEW);
}

void reshape(int w, int h)
{
	width = w;
	height = h;
	set_matrix3d();
	glViewport(0, 0, w, h);
}


int actionstate  = 0;
double mymx = 0;
double mymy = 0;

void myclickmotion(int nmx, int nmy) // Mouse
{
	double lmx = 2.*(nmx/(double)width - 0.5),lmy = 2.*(0.5 - nmy/(double)height), dmx = lmx-mymx, dmy = lmy - mymy;
	if( actionstate == 1 ) //middle button
	{
		double shiftmod[3] = {0,0,0};
		shiftmod[0] += dmx*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear);
		shiftmod[1] += dmy*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear);
		if( planecontrol )
		{
			
			rotatevector_from_stack((double*)shiftmod);
			p[0] += shiftmod[0];
			p[1] += shiftmod[1];
			p[2] += shiftmod[2];
			clipupdate = true;
		}
		else
		{
			//shiftmod[0] += dmx*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
			//shiftmod[1] += dmy*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			bndupdate = true;
		}
		glutPostRedisplay();
		mymx = lmx;
		mymy = lmy;
	}
	else if( actionstate == 2 ) //right button
	{
		if( planecontrol )
		{
			double shiftmod[3] = {0,0,0};
			shiftmod[2] -= dmy*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear);
			rotatevector_from_stack((double*)shiftmod);
			p[0] += shiftmod[0];
			p[1] += shiftmod[1];
			p[2] += shiftmod[2];
			clipupdate = true;
		}
		else
		{
			zoom *= expf(-dmy);
			reshape(width,height);
			double shiftmod[3] = {0,0,0};
			shiftmod[2] += dmx*zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear);
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			bndupdate = true;
		}
		glutPostRedisplay();
		mymx = lmx;
		mymy = lmy;
	}
	else if( actionstate == 3 ) //left buttion
	{
		clickmotion(nmx,nmy);
		if( planecontrol )
		{
			//quatget(n);
			reverse_rotatevector_from_stack((double *)n);
			reverse_rotatevector((double *)n);
			rotatevector_from_stack((double *)n);
			clipupdate = true;
			quatinit();
		}
		else bndupdate = true;
	}
}
void mymotion(int nmx, int nmy) // Mouse
{
	motion(nmx,nmy);
	mymx = 2.*(nmx/(double)width - 0.5);
	mymy = 2.*(0.5 - nmy/(double)height);
	if( current_picker != NULL ) glutPostRedisplay();
}

void myclick(int b, int s, int nmx, int nmy) // Mouse
{
	if( b == GLUT_LEFT_BUTTON )
	{
		if( s == GLUT_DOWN )
		{
			actionstate = 3;
		}
		else
		{
			actionstate = 0;
		}
		click(b,s,nmx,nmy);
	}
	else if( b == GLUT_MIDDLE_BUTTON )
	{
		if( s == GLUT_DOWN )
		{
			actionstate = 1;
			interactive = true;
		}
		else
		{
			actionstate = 0;
			interactive = false;
		}
		mymx = 2.*(nmx/(double)width - 0.5);
		mymy = 2.*(0.5 - nmy/(double)height);
	}
	else if( b == GLUT_RIGHT_BUTTON )
	{
		if( s == GLUT_DOWN )
		{
			actionstate = 2;
			interactive = true;
		}
		else
		{
			actionstate = 0;
			interactive = false;
		}
		mymx = 2.*(nmx/(double)width - 0.5);
		mymy = 2.*(0.5 - nmy/(double)height);
	}
	
	
	glutPostRedisplay();
}


Input * CommonInput = NULL;



void keyboard(unsigned char key, int x, int y)
{
	(void) x;
	(void) y;
	printf("%d %d\n",(int)key,(int)glutGetModifiers());
	if( glutGetModifiers() & (GLUT_ACTIVE_CTRL) )
		std::cout << "pressed " << ((char)(key)) << " int " << ((int)key) << " ctrl " << (glutGetModifiers() & GLUT_ACTIVE_CTRL ? "yes" : "no") << " shift " << (glutGetModifiers() & GLUT_ACTIVE_SHIFT ? "yes" : "no") << " alt " << (glutGetModifiers() & GLUT_ACTIVE_ALT ? "yes" : "no") << std::endl;
	if( CommonInput != NULL )
	{
		CommonInput->KeyPress(key);
		return;
	}
	if( key == 27 )
	{
		if( oclipper ) delete oclipper;
		if( bclipper ) delete bclipper;
		if( current_picker ) delete current_picker;
		delete mesh;
		exit(-1);
	}
	else if( key == '=' || key == '+')
	{
		zoom /= 1.1;
		reshape(width,height);
		glutPostRedisplay();
		interactive = true;
		//reset_timer = Timer();
	}
	else if( key == '_' || key == '-')
	{
		zoom *= 1.1;	
		reshape(width,height);
		glutPostRedisplay();
		interactive = true;
		//reset_timer = Timer();
	}
  else if( key == 'e' )
  {
    drawedges = (drawedges+1)%(4+is_material_defined);
    glutPostRedisplay();
  }
	else if( key == 'w' )
	{
		double shiftmod[3] = {0,0,0};
		shiftmod[1] -= 0.03f*expf(zoom-1)*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
		if( !planecontrol )
		{
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			interactive = true;
			bndupdate = true;
		}
		else
		{
			rotatevector_from_stack((double*)shiftmod);
			p[0] -= shiftmod[0];
			p[1] -= shiftmod[1];
			p[2] -= shiftmod[2];
			clipupdate = true;
		}
		glutPostRedisplay();
	}
	else if( key == 's' )
	{
		double shiftmod[3] = {0,0,0};
		shiftmod[1] += 0.03f*expf(zoom-1)*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
		if( !planecontrol )
		{
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			interactive = true;
			bndupdate = true;
		}
		else
		{
			rotatevector_from_stack((double*)shiftmod);
			p[0] -= shiftmod[0];
			p[1] -= shiftmod[1];
			p[2] -= shiftmod[2];
			clipupdate = true;
		}
		glutPostRedisplay();
	}
	else if( key == 'a' )
	{
		double shiftmod[3] = {0,0,0};
		shiftmod[0] += 0.03f*expf(zoom-1)*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
		if( !planecontrol )
		{
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			interactive = true;
			bndupdate = true;
		}
		else
		{
			rotatevector_from_stack((double*)shiftmod);
			p[0] -= shiftmod[0];
			p[1] -= shiftmod[1];
			p[2] -= shiftmod[2];
			clipupdate = true;
		}
		glutPostRedisplay();
	}
	else if( key == 'd' )
	{
		double shiftmod[3] = {0,0,0};
		shiftmod[0] -= 0.03f*expf(zoom-1)*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5;
		if(!planecontrol)
		{
			rotatevector((double*)shiftmod);
			shift[0] += shiftmod[0];
			shift[1] += shiftmod[1];
			shift[2] += shiftmod[2];
			interactive = true;
			bndupdate = true;
		}
		else
		{	rotatevector_from_stack((double*)shiftmod);
			p[0] -= shiftmod[0];
			p[1] -= shiftmod[1];
			p[2] -= shiftmod[2];
			clipupdate = true;
		}
		glutPostRedisplay();
	}
	else if( key == 'r' )
	{
		if( planecontrol )
		{
			p[0] = (sleft+sright)*0.5;
			p[1] = (sbottom+stop)*0.5;
			p[2] = (sfar+snear)*0.5;
			n[0] = 0;
			n[1] = 0;
			n[2] = 1;
			quatinit();
			clipupdate = true;
		}
		else
		{
			shift[0] = 0.0f;
			shift[1] = 0.0f;
			shift[2] = 0.0f;
			zoom = 1;
			quatinit();
			bndupdate = true;
		}
		glutPostRedisplay();
		interactive = true;
	}
	else if( key == 'z' )
	{
		int ret;
		printf("Enter point of plane (x,y,z):\n");
		ret = scanf("%lf %lf %lf",p,p+1,p+2);
		printf("Enter normal of plane (x,y,z):\n");
		ret = scanf("%lf %lf %lf",n,n+1,n+2);
		Storage::real l = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
		if( l )
		{
			n[0] /= l;
			n[1] /= l;
			n[2] /= l;
		}
		if( oclipper ) 
		{
			oclipper->clip_plane(p,n);
			clipboxupdate = true;
		}
		if( bclipper )
		{
			bclipper->clip_plane(p,n);
			clipboxupdate = true;
		}
			
		glutPostRedisplay();
	}
	else if( key == 'x' )
	{
		draw_volumetric = !draw_volumetric;
		glutPostRedisplay();
	}
	else if( key == 'b' )
	{
		boundary = !boundary;
		glutPostRedisplay();
	}
	else if( key == 'l' )
	{
		planecontrol = !planecontrol;
		if( planecontrol ) 
		{
			quatpush();
			quatinit();
		}
		else quatpop();
	}
	else if( key == 'n' )
	{
		if( CommonInput == NULL ) CommonInput = new Input(&amplitude, "Amplitude");
		glutPostRedisplay();
	}
	else if( key == 'm' )
	{
		if( CommonInput == NULL ) CommonInput = new Input(&radius, "Radius");
		glutPostRedisplay();
	}
	else if( key == 'h' )
	{
		draw_orphan = !draw_orphan;
		glutPostRedisplay();
	}
	else if( key == ',' || key == '<' )
	{
		std::cout << "positive shift" << std::endl;
		Storage::real radius2 = radius*radius;
		for(Mesh::iteratorNode it = mesh->BeginNode(); it != mesh->EndNode(); ++it)
		{
			Storage::real_array coords = it->Coords();
			Storage::real proj[3] = {0,0,0};
			Storage::real d = -(n[0]*(coords[0]-p[0]) + n[1]*(coords[1]-p[1]) + n[2]*(coords[2]-p[2]));
			proj[0] = coords[0] + d*n[0];
			proj[1] = coords[1] + d*n[1];
			proj[2] = coords[2] + d*n[2];
			d = (n[0]*(proj[0]-p[0]) + n[1]*(proj[1]-p[1]) + n[2]*(proj[2]-p[2]));
			assert(fabs(d) < 1.0e-7); // check that it is indeed a projection
			Storage::real dist = sqrt((p[0]-proj[0])*(p[0]-proj[0])+(p[1]-proj[1])*(p[1]-proj[1])+(p[2]-proj[2])*(p[2]-proj[2]));
			Storage::real delta = exp(-dist*dist/radius2);
			coords[0] += n[0]*delta*amplitude;
			coords[1] += n[1]*delta*amplitude;
			coords[2] += n[2]*delta*amplitude;
		}
		all_boundary.clear();
		INMOST_DATA_ENUM_TYPE pace = std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,(unsigned)boundary_faces.size()/100));
		for(INMOST_DATA_ENUM_TYPE k = 0; k < boundary_faces.size(); k++) 
		{
			all_boundary.push_back(DrawFace(boundary_faces[k]));
			if( k%pace == 0 ) all_boundary.back().set_flag(true);
		}
		if( oclipper ) delete oclipper;
		if( bclipper ) delete bclipper;
		bclipper = new bnd_clipper(mesh,boundary_faces.data(),(int)boundary_faces.size());
		if( mesh->GetDimensions() != 2 )
			oclipper = new clipper(mesh);
		clipupdate = true;
		glutPostRedisplay();
	}
	else if( key == '.' || key == '>' )
	{
		std::cout << "negative shift" << std::endl;
		Storage::real radius2 = radius*radius;
		for(Mesh::iteratorNode it = mesh->BeginNode(); it != mesh->EndNode(); ++it)
		{
			Storage::real_array coords = it->Coords();
			Storage::real proj[3] = {0,0,0};
			Storage::real d = -(n[0]*(coords[0]-p[0]) + n[1]*(coords[1]-p[1]) + n[2]*(coords[2]-p[2]));
			proj[0] = coords[0] + d*n[0];
			proj[1] = coords[1] + d*n[1];
			proj[2] = coords[2] + d*n[2];
			d = (n[0]*(proj[0]-p[0]) + n[1]*(proj[1]-p[1]) + n[2]*(proj[2]-p[2]));
			assert(fabs(d) < 1.0e-7); // check that it is indeed a projection
			Storage::real dist = sqrt((p[0]-proj[0])*(p[0]-proj[0])+(p[1]-proj[1])*(p[1]-proj[1])+(p[2]-proj[2])*(p[2]-proj[2]));
			Storage::real delta = exp(-dist*dist/radius2);
			coords[0] -= n[0]*delta*amplitude;
			coords[1] -= n[1]*delta*amplitude;
			coords[2] -= n[2]*delta*amplitude;
		}
		all_boundary.clear();
		INMOST_DATA_ENUM_TYPE pace = std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,(unsigned)boundary_faces.size()/100));
		for(INMOST_DATA_ENUM_TYPE k = 0; k < boundary_faces.size(); k++) 
		{
			all_boundary.push_back(DrawFace(boundary_faces[k]));
			if( k%pace == 0 ) all_boundary.back().set_flag(true);
		}
		if( oclipper ) delete oclipper;
		if( bclipper ) delete bclipper;
		bclipper = new bnd_clipper(mesh,boundary_faces.data(),(unsigned)boundary_faces.size());
		if( mesh->GetDimensions() != 2 )
			oclipper = new clipper(mesh);
		clipupdate = true;
		glutPostRedisplay();
	}
	else if( key == 'p' )
	{
		perspective = !perspective;
		glutPostRedisplay();
	}
	else if( key == 'o' )
	{
		elevation = !elevation;
		clipupdate = true;
		glutPostRedisplay();
	}
	else if( key == 'v' )
	{
		if( CommonInput == NULL ) 
		{
			PrintTags(mesh,CELL|FACE|EDGE|NODE);

			CommonInput = new Input(visualization_prompt, "Enter data for visualization as Element:Name:Component or ElementType:Number");
			visualization_prompt_active = 1;
			clipupdate = true;
			//if( visualization_tag.isValid() ) visualization_tag =  mesh->DeleteTag(visualization_tag);
			//if( disp_e.isValid() ) disp_e = InvalidElement();
		}
		glutPostRedisplay();
	}
	else if( key == 'c' )
	{
		if( CommonInput == NULL ) 
		{
			CommonInput = new Input(visualization_prompt, "Enter data for color bounds as min:max");
			visualization_prompt_active = 2;
		}
		glutPostRedisplay();
	}
	else if( key == 'q' )
	{
		mesh->Save("mesh.vtk");
		mesh->Save("mesh.pmf");
		mesh->Save("mesh.xml");
		mesh->Save("mesh.gmv");
	}
	else if( key == 't' )
	{
		screenshot(4);
		std::fstream fout("screenshot.svg",std::ios::out);
		svg_draw(fout);
		fout.close();
	}
}

void keyboard2(unsigned char key, int x, int y)
{
	(void) x;
	(void) y;
	if( key == '=' || key == '+' ||  key == '_' || key == '-' || key == 'w' || key == 's' || key == 'a' || key == 'd' || key == 'r' || key == 'p' || key == 'z')
	{
		//printf("depressed\n");
		interactive = false;
		glutPostRedisplay();
	}
	
}

Tag face_center;

void DrawElement(Element e, color_t face, color_t edge, color_t node, bool print_adj)
{
	glPointSize(4);
	if( e.GetElementType() == NODE )
	{
		node.set_color();
		glColor3f(1,1,0);
		glBegin(GL_POINTS);
		glVertexNdv(e->getAsNode()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
	}
	if( e.GetElementType() == EDGE && e.GetElementDimension() == 0)
	{
		node.set_color();
		glColor3f(1,1,0);
		glBegin(GL_POINTS);
		glVertexNdv(e->getNodes()[0]->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
	}
	else if( e.GetElementType() == EDGE && e.GetElementDimension() == 1 )
	{
		edge.set_color();
		glLineWidth(4);
		glBegin(GL_LINES);
		glVertexNdv(e->getAsEdge()->getBeg()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glVertexNdv(e->getAsEdge()->getEnd()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
		node.set_color();
		glBegin(GL_POINTS);
		glVertexNdv(e->getAsEdge()->getBeg()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glVertexNdv(e->getAsEdge()->getEnd()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
		glLineWidth(1);

		if (print_adj)
		{
			glRasterPos3dv(e->getAsEdge().getBeg().Coords().data());
			printtext("%d", e->getAsEdge().getBeg().LocalID());
			glRasterPos3dv(e->getAsEdge().getEnd().Coords().data());
			printtext("%d", e->getAsEdge().getEnd().LocalID());
		}
	}
	else if(  e.GetElementType() == FACE && e.GetElementDimension() == 1 )
	{
		edge.set_color();
		glLineWidth(4);
		glBegin(GL_LINES);
		glVertexNdv(e->getAsFace()->getBeg()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glVertexNdv(e->getAsFace()->getEnd()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
		node.set_color();
		glBegin(GL_POINTS);
		glVertexNdv(e->getAsFace()->getBeg()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glVertexNdv(e->getAsFace()->getEnd()->Coords().data(),e.GetMeshLink()->GetDimensions());
		glEnd();
		glLineWidth(1);

		if (print_adj)
		{
			glRasterPos3dv(e->getAsFace().getBeg().Coords().data());
			printtext("%d", e->getAsFace().getBeg().LocalID());
			glRasterPos3dv(e->getAsFace().getEnd().Coords().data());
			printtext("%d", e->getAsFace().getEnd().LocalID());
		}
	}
	else if( e.GetElementType() == FACE || (e.GetElementType() == CELL && e.GetElementDimension() == 2) )
	{
		face2gl f = DrawFace(e);
		face.set_color();
		glBegin(GL_TRIANGLES);
		f.draw();
		glEnd();
		edge.set_color();
		glBegin(GL_LINES);
		f.drawedges();
		glEnd();
		node.set_color();
		ElementArray<Node> nodes = e->getNodes();
		ElementArray<Edge> edges = e->getEdges();
		glBegin(GL_POINTS);
		for(ElementArray<Node>::iterator it = nodes.begin(); it != nodes.end(); ++it)
			glVertexNdv(it->Coords().data(),it->GetMeshLink()->GetDimensions());
		glEnd();
		glColor3f(0, 0, 0);
		//glDisable(GL_DEPTH_TEST);
		if (print_adj)
		{
			for (ElementArray<Edge>::iterator it = edges.begin(); it != edges.end(); ++it)
			{
				double cnt[3];
				it->Centroid(cnt);
				//for (int k = 0; k < 3; ++k) cnt[k] = cnt[k] * 0.99 + campos[k] * 0.01;
				glRasterPos3dv(cnt);
				printtext("%d", it->LocalID());
			}
		}
		//glEnable(GL_DEPTH_TEST);
	}
	else if( e.GetElementType() == CELL )
	{
		ElementArray<Face> dfaces = e.getFaces();
		face.set_color();
		glBegin(GL_TRIANGLES);
		for(ElementArray<Face>::iterator it = dfaces.begin(); it != dfaces.end(); ++it)
		{
			face2gl f = DrawFace(it->self());
			f.draw();
		}
		glEnd();
		glColor3f(0, 0, 0);
		edge.set_color();
		glBegin(GL_LINES);
		for(ElementArray<Face>::iterator it = dfaces.begin(); it != dfaces.end(); ++it)
		{
			face2gl f = DrawFace(it->self());
			f.drawedges();
		}
		glEnd();
		node.set_color();
		ElementArray<Node> nodes = e->getNodes();
		glBegin(GL_POINTS);
		for(ElementArray<Node>::iterator it = nodes.begin(); it != nodes.end(); ++it)
			glVertexNdv(it->Coords().data(),it->GetMeshLink()->GetDimensions());
		glEnd();

		if (print_adj)
		{
			for (ElementArray<Face>::iterator it = dfaces.begin(); it != dfaces.end(); ++it)
			{
				double cnt[3];
				it->Centroid(cnt);
				//for (int k = 0; k < 3; ++k) cnt[k] = cnt[k] * 0.99 + campos[k] * 0.01;
				glRasterPos3dv(cnt);
				printtext("%d", it->LocalID());
			}
		}
	}
	glPointSize(1);
	if (e.GetElementType() == ESET)
	{
		ElementSet s = e.getAsSet();
		for(ElementSet::iterator it = s.Begin(); it != s.End(); ++it)
			DrawElement(it->self(),face,edge,node,false);

		if (print_adj)
		{
			glDisable(GL_DEPTH_TEST);
			for (ElementSet::iterator it = s.Begin(); it != s.End(); ++it)
			{
				double cnt[3];
				it->Centroid(cnt);
				glRasterPos3dv(cnt);
				printtext("%d", it->LocalID());
			}
			glEnable(GL_DEPTH_TEST);
		}
	}
	
}

void whereami(double & cx, double & cy, double & cz)
{
   // Get the viewing matrix
   GLdouble modelview[16],projection[16];
   GLint viewport[4] = {0,0,1,1};
   glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
   glGetDoublev(GL_PROJECTION_MATRIX, projection);
   
   GLdouble outx, outy, outz;  // Var's to save the answer in

   gluUnProject(0.5, 0.5, 0.,
               modelview, projection, viewport,
               &outx, &outy, &outz);

   // Return the result.
   cx = outx;
   cy = outy;
   cz = outz;
}


void pick_mouse(double origin[3], double direction[3])
{
   // Get the viewing matrix
   GLdouble modelview[16],projection[16];
   GLint viewport[4] = {0,0,1,1};
   glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
   glGetDoublev(GL_PROJECTION_MATRIX, projection);
   
   GLdouble outx, outy, outz;  // Var's to save the answer in

   gluUnProject((mymx+1.0)/2.0, (mymy+1.0)/2.0, 0.,
               modelview, projection, viewport,
               &outx, &outy, &outz);

   // Return the result.
   origin[0] = outx;
   origin[1] = outy;
   origin[2] = outz;
   
   gluUnProject((mymx+1.0)/2.0, (mymy+1.0)/2.0, 1.,
               modelview, projection, viewport,
               &outx, &outy, &outz);
   direction[0] = outx - origin[0];
   direction[1] = outy - origin[1];
   direction[2] = outz - origin[2];
   double l = sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
   if( l ) 
   {
	   direction[0] /= l;
	   direction[1] /= l;
	   direction[2] /= l;
   }
}

//returns position of bottom
double display_elem_info(Element e, double top, double left, double interval)
{
	double bottom = top-interval;
	for(Mesh::iteratorTag t = mesh->BeginTag(); t != mesh->EndTag(); ++t) if( t->isDefined(e->GetElementType()) )
		if( e->HaveData(*t) )
			bottom -= interval;
	
	glDisable(GL_DEPTH_TEST);
	glLoadIdentity();
	set_matrix2d();
	
	
	glColor4f(1,1,1,0.65);
	glEnable(GL_BLEND);
	glBegin(GL_QUADS);
	glVertex2f(left-0.01,bottom-0.01);
	glVertex2f(left-0.01,top-0.01);
	glVertex2f(0.99,top-0.01);
	glVertex2f(0.99,bottom-0.01);
	glEnd();
	glDisable(GL_BLEND);
	glColor3f(0,0,0);
	glBegin(GL_LINE_LOOP);
	glVertex2f(left-0.01,bottom-0.01);
	glVertex2f(left-0.01,top-0.01);
	glVertex2f(0.99,top-0.01);
	glVertex2f(0.99,bottom-0.01);
	glEnd();
	
	
	glColor3f(0.2,0.2,0.2);
	top -= interval;
	glRasterPos2f(left,top);
	printtext("%s %d",ElementTypeName(e->GetElementType()),e->LocalID());
	if( e->GetElementType() == ESET ) printtext(" size %d",e->getAsSet().Size());
	glColor3f(0.2,0.2,0.2);
	for(Mesh::iteratorTag t = mesh->BeginTag(); t != mesh->EndTag(); ++t) if( t->isDefined(e->GetElementType()) )
	{
		if( e->HaveData(*t) )
		{
			char str[131072];
			char temp[131072];
			str[0] = '\0';
			int dsize;
			switch(t->GetDataType())
			{
				case DATA_INTEGER:
				{
					Storage::integer_array arr = e->IntegerArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						sprintf(temp,"%s %d",str,arr[k]);
						strcpy(str,temp);
					}
					break;
				}
				case DATA_REAL:
				{
					Storage::real_array arr = e->RealArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						sprintf(temp,"%s %lf",str,arr[k]);
						strcpy(str,temp);
					}
					break;
				}
				case DATA_BULK:
				{
					Storage::bulk_array arr = e->BulkArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						sprintf(temp,"%s %d",str,arr[k]);
						strcpy(str,temp);
					}
					break;
				}
				case DATA_REFERENCE:
				{
					Storage::reference_array arr = e->ReferenceArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						if(arr.at(k) == InvalidHandle()) sprintf(temp,"%s NULL",str);
						else sprintf(temp,"%s %s:%d",str,ElementTypeName(arr[k]->GetElementType()),arr[k]->LocalID());
						strcpy(str,temp);
					}
					break;
				}
				case DATA_REMOTE_REFERENCE:
				{
					Storage::remote_reference_array arr = e->RemoteReferenceArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						if(arr.at(k).first == NULL || arr.at(k).second == InvalidHandle()) sprintf(temp,"%s NULL",str);
						else sprintf(temp,"%s %p:%s:%d",str,arr[k]->GetMeshLink(),ElementTypeName(arr[k]->GetElementType()),arr[k]->LocalID());
						strcpy(str,temp);
					}
					break;
				}
#if defined(USE_AUTODIFF)
				case DATA_VARIABLE:
				{
					Storage::var_array arr = e->VariableArray(*t);
					dsize = arr.size();
					for(INMOST_DATA_ENUM_TYPE k = 0; k < arr.size(); k++)
					{
						std::stringstream stream;
						stream << arr[k].GetValue() << " {[" << arr[k].GetRow().Size() << "] ";
						for(INMOST_DATA_ENUM_TYPE q = 0; q < arr[k].GetRow().Size(); ++q)
						{
							stream << "(" << arr[k].GetRow().GetValue(q) << "," << arr[k].GetRow().GetIndex(q) << ") ";
						}
						stream << "}";
						sprintf(temp,"%s %s",str,stream.str().c_str());
						strcpy(str,temp);
					}
					break;
				}
#endif
			}
			sprintf(temp,"%s[%d] %s %s",t->GetTagName().c_str(),dsize,DataTypeName(t->GetDataType()),str);
			strcpy(str,temp);
			top -= interval;
			glRasterPos2f(left,top);
			printtext(str);
			
		}
	}
	glEnable(GL_DEPTH_TEST);
	return top-2*interval;
}

void draw_screen()
{
	//glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	set_matrix3d();

	Storage::real mult = zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5*0.1;
	if( perspective )
		glTranslated(0,0,-zoom*2*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5);
	if( planecontrol )
		rotate_from_stack();
	else
		rotate();

	//axis
	if( drawaxis )
	{
		glLineWidth(3.0);
		glBegin(GL_LINES);
		glColor3f(1,0,0);
		glVertex3d(0,0,0);
		glVertex3d(mult,0,0);
		glColor3f(0,1,0);
		glVertex3d(0,0,0);
		glVertex3d(0,mult,0);
		glColor3f(0,0,1);
		glVertex3d(0,0,0);
		glVertex3d(0,0,mult);
		glEnd();
		glLineWidth(1.0);
	}
	//glPointSize(1);
	
	//glTranslated(-(sleft+sright)*0.5+shift[0],-(sbottom+stop)*0.5 + shift[1],-(snear+sfar)*0.5 + shift[2]);
	
	glTranslated(shift[0],shift[1],shift[2]);


	//if( planecontrol )
	if( drawaxis )
	{
		glColor3f(0.6,0.4,0.2);
		glPointSize(5);
		glBegin(GL_POINTS);
		glVertex3dv(p);
		glEnd();
		glPointSize(1);

		glColor3f(0.2,0.4,0.6);
		glLineWidth(3.0);
		glBegin(GL_LINES);
		glVertex3dv(p);
		glVertex3d(p[0]+n[0]*mult*2,p[1]+n[1]*mult*2,p[2]+n[2]*mult*2);
		glEnd();
		glLineWidth(1.0);
	}

	

	double campos[3] = {0.5,0.5,0}, pickp[3], pickd[3];
	whereami(campos[0], campos[1], campos[2]);
	int picked = -1;

	if (draw_orphan)
	for (int k = 0; k < (int)orphans.size(); ++k)
		DrawElement(orphans[k], color_t(1, 0, 1), color_t(0, 1, 1), color_t(0, 0, 1), true);

	if (disp_e.isValid())
		DrawElement(disp_e, color_t(1, 1, 0), color_t(1, 0, 0), color_t(0, 0, 1), true);


	//glTranslated((l+r)*0.5,(b+t)*0.5,(near+far)*0.5);
	if( drawedges == 4 )
	{
		clip_boundary.clear();
		for(std::map<int,std::vector<HandleType> >::iterator it = material_faces.begin(); it != material_faces.end(); ++it)
		{
			for(std::vector<HandleType>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				clip_boundary.push_back(DrawFace(Face(mesh,*jt)));
				clip_boundary.back().shift(material_x[it->first],material_y[it->first],material_z[it->first]);
				clip_boundary.back().set_color(0.6,0.6,0.6,1);
			}
		}
		if (!(drawedges == 2 || drawedges == 3))
			draw_faces(clip_boundary);
		glColor4f(0,0,0,1);
		if (drawedges && drawedges != 2) 
			draw_edges(clip_boundary);
		clipboxupdate = true;
	}
	else
	{
		
		if( oclipper || bclipper )
		{
			
			if( clipupdate ) 
			{
				if( current_picker != NULL ) {delete current_picker; current_picker = NULL;}
				if( oclipper ) oclipper->clip_plane(p,n);
				clipupdate = false;
				clipboxupdate = true;
			}
		
			if( !interactive && clipboxupdate )
			{
				clip_boundary.clear();
				if( oclipper ) oclipper->gen_clip(clip_boundary,n,elevation);
				bclipper->clip_plane(p,n);
				bclipper->gen_clip(clip_boundary,p,n,elevation);
				clipboxupdate = false;
				for(int k = 0; k < (int)orphans.size(); ++k)
				{
					if( orphans[k]->GetElementType() == FACE )
						clip_boundary.push_back(DrawFace(orphans[k]->getAsFace()));
					else if( orphans[k]->GetElementType() == CELL )
					{
						ElementArray<Face> faces = orphans[k].getFaces();
						for(int q = 0; q < faces.size(); ++q)
							clip_boundary.push_back(DrawFace(faces[q]));
					}
				}


				if( current_picker != NULL ) {delete current_picker; current_picker = NULL;}
				if( !clip_boundary.empty() ) current_picker = new picker(clip_boundary);
			}

			

			if( current_picker != NULL )
			{
				pick_mouse(pickp,pickd);
				picked = current_picker->select(pickp,pickd);
			}
			
	
			if( interactive && clipboxupdate )
			{
				//printf("draw1 %d %d\n",interactive, clipboxupdate);
				INMOST_DATA_ENUM_TYPE opace = !planecontrol ? std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,oclipper->size()/100)) : 1;
				INMOST_DATA_ENUM_TYPE bpace = std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,bclipper->size()/100));
				glColor4f(0.6,0.6,0.6,1);
				if (isColorBarEnabled()) GetColorBar()->BindTexture();
				if( oclipper ) oclipper->draw_clip(opace,n,elevation);
				bclipper->draw_clip(bpace,p,n);
				if (isColorBarEnabled()) GetColorBar()->UnbindTexture();
				glColor4f(0,0,0,1); 
				if (drawedges)
				{
					if( oclipper ) oclipper->draw_clip_edges(opace, n,elevation);
					bclipper->draw_clip_edges(bpace);
				}
				
			}
			else
			{
				//printf("draw2 %d %d\n",interactive, clipboxupdate);
				if( interactive )
				{
					if (!(drawedges == 2 || drawedges == 3)) 
						draw_faces_interactive(clip_boundary);
					glColor4f(0,0,0,1); 
					if (drawedges && drawedges != 2) 
						draw_edges_interactive(clip_boundary);
				}
				else
				{
					if (!(drawedges == 2 || drawedges == 3))
						draw_faces(clip_boundary,picked);
					glColor4f(0,0,0,1); 
					if (drawedges && drawedges != 2) 
						draw_edges(clip_boundary, picked);
				}
			}
		}
		
		


		if( draw_volumetric )
		{
			if( bndupdate ) CommonVolumetricView->camera(campos,interactive);
			CommonVolumetricView->draw(interactive);
		}
		
		if( CommonVectors )
			CommonVectors->Draw(interactive);


		for(int k = 0; k < streamlines.size(); ++k)
			streamlines[k].Draw(true);//interactive);

		glColor4f(0, 0, 0, 1);
		glBegin(GL_LINES);
		for (int k = 0; k < segments.size(); ++k)
			segments[k].Draw();
		glEnd();

		if( boundary )
		{
			glEnable(GL_BLEND);
			if( !interactive && bndupdate)
			{
				for(INMOST_DATA_ENUM_TYPE q = 0; q < all_boundary.size() ; q++) 
					all_boundary[q].compute_dist(campos);
				//std::sort(all_boundary.rbegin(),all_boundary.rend());
				face2gl::radix_sort_dist(all_boundary);


				for(INMOST_DATA_ENUM_TYPE q = 0; q < added_faces.size() ; q++) 
					added_faces[q].compute_dist(campos);
				//std::sort(all_boundary.rbegin(),all_boundary.rend());
				face2gl::radix_sort_dist(added_faces);
			}
			glColor4f(0,0,0,0.25); 
			if (interactive) 
			{
				if (drawedges && drawedges != 2) 
					draw_edges_interactive(all_boundary);
			}
			else
			{
				if (drawedges && drawedges != 2)
					draw_edges(all_boundary);
			}
			

			if (interactive)
			{
				if (!(drawedges == 2 || drawedges == 3))
					draw_faces_interactive_nc(all_boundary);
			}
			else
			{
				if (!(drawedges == 2 || drawedges == 3))
					draw_faces_nc(all_boundary);
			}
			
			glDisable(GL_BLEND);
		}

		if( !interactive && bndupdate) bndupdate = false;
	}





	glEnable(GL_BLEND);
	glColor4f(0,0,0,0.25); 
	if (drawedges && drawedges != 2) 
		draw_edges(added_faces);
	if (!(drawedges == 2 || drawedges == 3))
		draw_faces_nc(added_faces);
	glDisable(GL_BLEND);


	if( !added_edges.empty() )
	{
		glColor3f(0,0,0);
		glBegin(GL_LINES);
		for(ElementArray<Edge>::iterator it = added_edges.begin(); it != added_edges.end(); ++it)
		{
			glVertexNdv(it->getBeg()->Coords().data(),it->GetMeshLink()->GetDimensions());
			glVertexNdv(it->getEnd()->Coords().data(),it->GetMeshLink()->GetDimensions());
		}
		glEnd();
	}

	glLineWidth(2.0);

	glBegin(GL_LINES);
	for(int k = 0; k < conormals.size(); k+=3)
	{
		glVertex3dv(&conormals[k]);
	}
	glEnd();

	glLineWidth(1.0);

	glPointSize(5.0);

	glColor3f(0,0,1);
	glBegin(GL_POINTS);
	for(int k = 0; k < harmonic_points.size(); k+=3)
		glVertex3dv(&harmonic_points[k]);
	glEnd();

	glColor3f(1,0,0);
	glBegin(GL_POINTS);
	for(int k = 0; k < dual_harmonic_points.size(); k+=9)
		glVertex3dv(&dual_harmonic_points[k+3]);
	glEnd();

	glPointSize(1.0);

	glColor3f(0.5,0.5,0.5);
	glBegin(GL_LINES);
	for(int k = 0; k < dual_harmonic_points.size(); k+=9)
	{
		glVertex3dv(&dual_harmonic_points[k+0]);
		glVertex3dv(&dual_harmonic_points[k+3]);
		glVertex3dv(&dual_harmonic_points[k+3]);
		glVertex3dv(&dual_harmonic_points[k+6]);
	}
	glEnd();





	double top = 0.96;
	if( picked != -1 )
	{

		Element e = clip_boundary[picked].get_elem(mesh);
		top = display_elem_info(e,0.96,0.0,0.04);
	}

	if( disp_e.isValid() )
		top = display_elem_info(disp_e,top+0.04,0.0,0.04);

	if( CommonInput != NULL )
	{
		glDisable(GL_DEPTH_TEST);
		glLoadIdentity();
		set_matrix2d();
		CommonInput->Draw();
		if( CommonInput->Done() )
		{
			if( ! CommonInput->Canceled() )
			{
				if( visualization_prompt_active == 2 )
				{
					int k = 0, slen = (int)strlen(visualization_prompt);
					for(k = 0; k < slen; ++k) 
					{
						if( visualization_prompt[k] == ':' )
						{
							visualization_prompt[k] = '\0';
							break;
						}
					}
					if( k < slen && k+1 < slen )
					{
						double minv, maxv;
						visualization_prompt[k] = ':';
						minv = atof(visualization_prompt);
						maxv = atof(visualization_prompt+k+1);
						GetColorBar()->set_min(minv);
						GetColorBar()->set_max(maxv);
						clipupdate = true;
					}
					else printf("malformed string %s for color map bounds\n",visualization_prompt);
					visualization_prompt_active = 0;
				}
				else if( visualization_prompt_active == 1 )
				{
					char typen[1024],name[1024];
					unsigned comp;
					bool correct_input = false;
					int k = 0,l, slen = (int)strlen(visualization_prompt);
					for(k = 0; k < slen; ++k) 
					{
						if( visualization_prompt[k] == ':' )
						{
							visualization_prompt[k] = '\0';
							break;
						}
					}
					for(l = k+1; l < slen; ++l) 
					{
						if( visualization_prompt[l] == ':' )
						{
							visualization_prompt[l] = '\0';
							break;
						}
					}

					//std::cout << "k: " << k << " l: " << l << std::endl;

					if( k == slen && l == slen+1 )
					{
						for(int q = 0; q < slen; ++q)
							visualization_prompt[q] = tolower(visualization_prompt[q]);
						if( std::string(visualization_prompt) == "clear" )
						{
							disp_e = InvalidElement();
							if( visualization_tag.isValid() )
							{
								visualization_tag =  mesh->DeleteTag(visualization_tag);
								color_bar::UnsetVisualizationTag();
								clipupdate = true;
							}
							correct_input = true;
							streamlines.clear();
							glutPostRedisplay();
							if( CommonVectors )
							{
								delete CommonVectors;
								CommonVectors = NULL;
							}

						}
					}

					if( k < slen && l == slen ) //ElementType:Number format
					{
						strcpy(typen, visualization_prompt);
						std::string stype(typen);
						visualization_type = NONE;
						for (size_t q = 0; q < stype.size(); ++q)
							stype[q] = tolower(stype[q]);
						if (stype == "scale")
						{
							double scale = atof(visualization_prompt + k + 1);
							std::cout << "input scale: " << scale << std::endl;
							if( CommonVectors )
								CommonVectors->SetScale(scale);
							correct_input = true;
							glutPostRedisplay();
						}
						else
						{
							if (stype == "node") visualization_type = NODE;
							else if (stype == "edge") visualization_type = EDGE;
							else if (stype == "face") visualization_type = FACE;
							else if (stype == "cell") visualization_type = CELL;
							else if (stype == "eset") visualization_type = ESET;
							if (visualization_type == NONE)
								printf("unknown element type %s\n", typen);
							else
							{
								disp_e = InvalidElement();
								bool is_number = true;
								for (l = k + 1; l < slen; ++l)
								if (!isdigit(visualization_prompt[l]))
									is_number = false;
								if (is_number)
								{
									comp = atoi(visualization_prompt + k + 1);
									visualization_prompt[k] = ':';


									correct_input = true;

									if (mesh->isValidElement(visualization_type, comp))
									{
										printf("Display data for %s:%d\n", typen, comp);
										disp_e = mesh->ElementByLocalID(visualization_type, comp);
									}
									else
										printf("No valid element at %s:%d\n", typen, comp);
								}
								else if (visualization_type == ESET)
								{
									std::string name = std::string(visualization_prompt + k + 1);
									visualization_prompt[k] = ':';
									correct_input = true;
									disp_e = mesh->GetSet(name);
									if (disp_e.isValid())
										printf("Display data for %s:%d\n", typen, disp_e.LocalID());
									else
										printf("Cannot find set with name %s\n", name.c_str());
								}

								if (disp_e.isValid())
								{
									if (disp_e.GetElementType() != ESET)
									{
										disp_e->Centroid(shift);
										for (int r = 0; r < 3; ++r)
											shift[r] = -shift[r];
									}
									else
									{
										shift[0] = shift[1] = shift[2] = 0;
										ElementSet s = disp_e.getAsSet();
										int nelem = 0;
										for (ElementSet::iterator it = s.Begin(); it != s.End(); ++it)
										{
											double cnt[3];
											it->Centroid(cnt);
											shift[0] += cnt[0];
											shift[1] += cnt[1];
											shift[2] += cnt[2];
											nelem++;
										}
										shift[0] /= (double)nelem;
										shift[1] /= (double)nelem;
										shift[2] /= (double)nelem;
										for (int r = 0; r < 3; ++r)
											shift[r] = -shift[r];
									}
								}
							}
						}
					}

					if( k < slen && l < slen && l+1 < slen )
					{
						bool is_number = true;
						for(int r = l+1; r < slen; ++r)
							if( !isdigit(visualization_prompt[r]) )
								is_number = false;
						if( !is_number ) for(int r = l+1; r < slen; ++r) visualization_prompt[r] = tolower(visualization_prompt[r]);

						strcpy(typen,visualization_prompt);
						strcpy(name,visualization_prompt+k+1);
						if( is_number )
							comp = atoi(visualization_prompt+l+1);
						else if( std::string(visualization_prompt+l+1) == "mag" )
							comp = ENUMUNDEF;
						else if (std::string(visualization_prompt + l + 1) == "streamline")
							comp = ENUMUNDEF-2;
						else if (std::string(visualization_prompt + l + 1) == "vector" ||
								 std::string(visualization_prompt + l + 1) == "vec")
							comp = ENUMUNDEF-3;
						else
						{
							std::cout << "unknown name for component, expected number or 'mag' or 'vec' or 'streamline'" << std::endl;
							comp = ENUMUNDEF-1;
						}
						visualization_prompt[k] = ':';
						visualization_prompt[l] = ':';
						printf("type %s name %s comp %d\n",typen,name,comp);
						std::string stype(typen), sname(name);
						if (mesh->HaveTag(sname) && (comp == ENUMUNDEF - 2 || comp == ENUMUNDEF - 3))
						{
							ElementType vel_def = NONE;
							for (size_t q = 0; q < stype.size(); ++q)
							{
								stype[q] = tolower(stype[q]);
								typen[q] = tolower(typen[q]);
							}
							if (stype == "node") vel_def = NODE;
							else if (stype == "edge") vel_def = EDGE;
							else if (stype == "face") vel_def = FACE;
							else if (stype == "cell") vel_def = CELL;
							
							if( comp == ENUMUNDEF - 2 )
							{
								streamlines.clear();
								BuildStreamlines(mesh,mesh->GetTag(sname),vel_def,streamlines);
							}
							else if( comp == ENUMUNDEF - 3 )
							{
								if( CommonVectors ) delete CommonVectors;
								CommonVectors = new Vectors(mesh,mesh->GetTag(sname),vel_def);
							}
						}
						else if( mesh->HaveTag(sname) && comp != ENUMUNDEF-1)
						{
							Tag source_tag = mesh->GetTag(sname);
							if ((comp >= 0 && comp < source_tag.GetSize()) || comp == ENUMUNDEF)
							{
								visualization_type = NONE;
								for (size_t q = 0; q < stype.size(); ++q)
								{
									stype[q] = tolower(stype[q]);
									typen[q] = tolower(typen[q]);
								}
								if (stype == "node") visualization_type = NODE;
								else if (stype == "edge") visualization_type = EDGE;
								else if (stype == "face") visualization_type = FACE;
								else if (stype == "cell")
								{
									visualization_type = CELL;
									visualization_smooth = false;
								}
								else if (stype == "smooth_cell")
								{
									visualization_type = CELL;
									visualization_smooth = true;
								}

								if (visualization_type != NONE)
								{
									if (source_tag.isDefined(visualization_type))
									{
										if (source_tag.GetDataType() == DATA_REAL || source_tag.GetDataType() == DATA_INTEGER || source_tag.GetDataType() == DATA_BULK || source_tag.GetDataType() == DATA_VARIABLE)
										{
											if (source_tag.GetDataType() == DATA_VARIABLE)
												printf("I can show only value for data of type variable\n");
											float min = 1.0e20, max = -1.0e20;
											printf("prepearing data for visualization\n");
											if (visualization_tag.isValid())
											{
												visualization_tag = mesh->DeleteTag(visualization_tag);
												color_bar::UnsetVisualizationTag();
											}
											visualization_tag = mesh->CreateTag("VISUALIZATION_TAG", DATA_REAL, NODE, NONE, 1);
											color_bar::SetVisualizationTag(visualization_tag,visualization_type,visualization_smooth);
											for (Mesh::iteratorNode it = mesh->BeginNode(); it != mesh->EndNode(); ++it)
											{
												ElementArray<Element> elems = it->getAdjElements(visualization_type);
												Storage::real_array coords = it->Coords();
												Storage::real cnt[3], dist, wgt;
												Storage::real val = 0.0, vol = 0.0, res;
												for (ElementArray<Element>::iterator jt = elems.begin(); jt != elems.end(); ++jt) if (jt->HaveData(source_tag) && (jt->GetDataSize(source_tag) > comp || comp == ENUMUNDEF))
												{
													jt->Centroid(cnt);
													if( mesh->GetDimensions() == 2 )
														dist = (cnt[0] - coords[0])*(cnt[0] - coords[0]) + (cnt[1] - coords[1])*(cnt[1] - coords[1]);
													else
														dist = (cnt[0] - coords[0])*(cnt[0] - coords[0]) + (cnt[1] - coords[1])*(cnt[1] - coords[1]) + (cnt[2] - coords[2])*(cnt[2] - coords[2]);
													wgt = 1.0 / (dist + 1.0e-8);
													if (source_tag.GetDataType() == DATA_REAL)
													{
														Storage::real_array v = jt->RealArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q] * v[q];
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * v[comp];
													}
													else if (source_tag.GetDataType() == DATA_INTEGER)
													{
														Storage::integer_array v = jt->IntegerArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q] * v[q];
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * static_cast<double>(v[comp]);
													}
													else if (source_tag.GetDataType() == DATA_BULK)
													{
														Storage::bulk_array v = jt->BulkArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q)
															{
																double g = static_cast<double>(v[q]);
																l += g * g;
															}
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * static_cast<double>(v[comp]);
													}
													else if (source_tag.GetDataType() == DATA_VARIABLE)
													{
														Storage::var_array v = jt->VariableArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q].GetValue() * v[q].GetValue();
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * static_cast<double>(v[comp].GetValue());
													}
													vol += wgt;
												}
												res = val / vol;
												if (res < min) min = res;
												if (res > max) max = res;
												it->RealDF(visualization_tag) = res;
											}
											visualization_tag = mesh->CreateTag("VISUALIZATION_TAG", DATA_REAL, CELL, NONE, 1);
											for (Mesh::iteratorCell it = mesh->BeginCell(); it != mesh->EndCell(); ++it)
											{
												ElementArray<Element> elems = it->getAdjElements(visualization_type);
												Storage::real coords[3];
												it->Centroid(coords);
												Storage::real cnt[3], dist, wgt;
												Storage::real val = 0.0, vol = 0.0, res;
												for (ElementArray<Element>::iterator jt = elems.begin(); jt != elems.end(); ++jt) if (jt->HaveData(source_tag) && (jt->GetDataSize(source_tag) > comp || comp == ENUMUNDEF))
												{
													jt->Centroid(cnt);
													dist = (cnt[0] - coords[0])*(cnt[0] - coords[0]) + (cnt[1] - coords[1])*(cnt[1] - coords[1]) + (cnt[2] - coords[2])*(cnt[2] - coords[2]);
													wgt = 1.0 / (dist + 1.0e-8);
													if (source_tag.GetDataType() == DATA_REAL)
													{
														Storage::real_array v = jt->RealArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q] * v[q];
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * v[comp];
													}
													else if (source_tag.GetDataType() == DATA_INTEGER)
													{
														Storage::integer_array v = jt->IntegerArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q] * v[q];
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * v[comp];
													}
													else if (source_tag.GetDataType() == DATA_BULK)
													{
														Storage::bulk_array v = jt->BulkArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q)
															{
																double g = static_cast<double>(v[q]);
																l += g*g;
															}
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * static_cast<double>(v[comp]);
													}
													else if (source_tag.GetDataType() == DATA_VARIABLE)
													{
														Storage::var_array v = jt->VariableArray(source_tag);
														if (comp == ENUMUNDEF)
														{
															double l = 0;
															for (unsigned q = 0; q < v.size(); ++q) l += v[q].GetValue() * v[q].GetValue();
															l = sqrt(l);
															val += wgt * l;
														}
														else val += wgt * v[comp].GetValue();
													}
													vol += wgt;
												}
												res = val / vol;
												if (res < min) min = res;
												if (res > max) max = res;
												it->RealDF(visualization_tag) = res;
											}

											GetColorBar()->set_min(min);
											GetColorBar()->set_max(max);
											char comment[1024];
											sprintf(comment, "%s[%d] on %s, [%g:%g]", name, comp, typen, min, max);
											GetColorBar()->set_comment(comment);
											clipupdate = true;
											/*
											all_boundary.clear();
											INMOST_DATA_ENUM_TYPE pace = std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,(unsigned)boundary_faces.size()/100));
											for(INMOST_DATA_ENUM_TYPE k = 0; k < boundary_faces.size(); k++)
											{
											all_boundary.push_back(DrawFace(boundary_faces[k]));
											if( k%pace == 0 ) all_boundary.back().set_flag(true);
											}
											*/
										}
										else if (source_tag.GetDataType() == DATA_REFERENCE)
										{
											segments.clear();
											if (comp == ENUMUNDEF)
											{
												for (Mesh::iteratorElement it = mesh->BeginElement(visualization_type); it != mesh->EndElement(); ++it) if (it->HaveData(source_tag))
												{
													coord cnt1, cnt2;
													it->Centroid(cnt1.data());
													Storage::reference_array arr = it->ReferenceArray(source_tag);
													for (Storage::reference_array::iterator jt = arr.begin(); jt != arr.end(); ++jt) if (jt->isValid())
													{
														jt->Centroid(cnt2.data());
														segments.push_back(segment(cnt1, cnt2));
													}
												}
											}
											else for (Mesh::iteratorElement it = mesh->BeginElement(visualization_type); it != mesh->EndElement(); ++it) if (it->HaveData(source_tag))
											{
												coord cnt1, cnt2;
												it->Centroid(cnt1.data());
												Storage::reference_array arr = it->ReferenceArray(source_tag);
												if( arr.size() > comp )
												{
													arr[comp]->Centroid(cnt2.data());
													segments.push_back(segment(cnt1, cnt2));
												}
											}
										}
										else printf("tag %s is not real or integer or bulk or variable or reference\n", name);
									}
									else printf("tag %s is not defined on element type %s\n", name, typen);
								}
								else printf("do not understand element type %s, should be: node, edge, face, cell, smooth_cell\n", typen);
							}
							else printf("component is out of range for tag %s of size %u\n", name, source_tag.GetSize());
						}
						else printf("mesh do not have tag with name %s\n",name);
					}
					else if(!correct_input) printf("malformed string %s for visualization\n",visualization_prompt);
					visualization_prompt_active = 0;
					//visualization_prompt[0] = '\0';

				}

				glutPostRedisplay();
			}
			delete CommonInput;
			CommonInput = NULL;
		}
		glEnable(GL_DEPTH_TEST);
	}

	if( isColorBarEnabled() && drawcolorbar )
	{
		glDisable(GL_DEPTH_TEST);
		glLoadIdentity();
		set_matrix2d();
		GetColorBar()->Draw();
		glEnable(GL_DEPTH_TEST);
	}
}

void draw()
{
  draw_screen();
	glutSwapBuffers();
}



void svg_draw(std::ostream & file)
{
	//file << "<?xml version=\"1.0\" stanfalone=\"no\">" << std::endl;
	//file << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">" << std::endl;
	file << "<svg width=\"" << width << "\" height=\"" << height << "\" version=\"1.1\">" << std::endl;

	//glDepthMask(GL_TRUE);
	glLoadIdentity();
	set_matrix3d();


	Storage::real mult = zoom*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5*0.1;
	if( perspective )
		glTranslated(0,0,-zoom*2*std::max(std::max( sright-sleft, stop-sbottom ), sfar-snear)*0.5);
	if( planecontrol )
		rotate_from_stack();
	else
		rotate();

	int viewport[4];
	double projection[16];
	double modelview[16]; 
	glGetDoublev (GL_PROJECTION_MATRIX, projection);
	glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
	glGetIntegerv(GL_VIEWPORT,viewport);

	//axis
	/*
	{
		file << "<g stroke-width=\"3\">" << std::endl;
		file << "<g stroke=\"red\">" << std::endl;
		svg_line(file, mult,0,0, 0,0,0, modelview,projection,viewport);
		file << "</g>" << std::endl;
		file << "<g stroke=\"green\">" << std::endl;
		svg_line(file, 0,mult,0, 0,0,0, modelview,projection,viewport);
		file << "</g>" << std::endl;
		file << "<g stroke=\"blue\">" << std::endl;
		svg_line(file, 0,0,mult, 0,0,0, modelview,projection,viewport);
		file << "</g>" << std::endl;
		file << "</g>" << std::endl;
	}
	*/
	//glPointSize(1);
	
	//glTranslated(-(sleft+sright)*0.5+shift[0],-(sbottom+stop)*0.5 + shift[1],-(snear+sfar)*0.5 + shift[2]);
	
	glTranslated(shift[0],shift[1],shift[2]);

	glGetDoublev (GL_PROJECTION_MATRIX, projection);
	glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
	

	double campos[3] = {0.5,0.5,0}, pickp[3], pickd[3];
	whereami(campos[0],campos[1],campos[2]);
	int picked = -1;

	
	//glTranslated((l+r)*0.5,(b+t)*0.5,(near+far)*0.5);
	if( drawedges == 4 )
	{
		std::vector<face2gl> sorted_clip_boundary;
		for(std::map<int,std::vector<HandleType> >::iterator it = material_faces.begin(); it != material_faces.end(); ++it)
		{
			for(std::vector<HandleType>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				sorted_clip_boundary.push_back(DrawFace(Face(mesh,*jt)));
				sorted_clip_boundary.back().shift(material_x[it->first],material_y[it->first],material_z[it->first]);
				sorted_clip_boundary.back().compute_dist(campos);
				sorted_clip_boundary.back().set_color(1,1,1,1);
			}
		}
		face2gl::radix_sort_dist(sorted_clip_boundary);
		//std::sort(sorted_clip_boundary.rbegin(),sorted_clip_boundary.rend());
		if (!(drawedges == 2 || drawedges == 3))
			svg_draw_faces(file, sorted_clip_boundary, ::drawedges && ::drawedges != 2, modelview, projection, viewport);
	}
	else
	{
		
		if( oclipper || bclipper ) 
		{
			/*
			std::vector<face2gl> temp_boundary;
			oclipper->gen_clip(temp_boundary,n);
			for(INMOST_DATA_ENUM_TYPE q = 0; q < temp_boundary.size() ; q++)
				temp_boundary[q].compute_dist(campos);
			face2gl::radix_sort_dist(temp_boundary);
			if (!(drawedges == 2 || drawedges == 3))
				svg_draw_faces(file,:drawedges && ::drawedges != 2,temp_boundary,modelview,projection,viewport);
			 */
			
			std::vector<face2gl> sorted_clip_boundary(clip_boundary);
			for(INMOST_DATA_ENUM_TYPE q = 0; q < sorted_clip_boundary.size() ; q++)
				sorted_clip_boundary[q].compute_dist(campos);
			face2gl::radix_sort_dist(sorted_clip_boundary);
			if (!(drawedges == 2 || drawedges == 3))
				svg_draw_faces(file, sorted_clip_boundary, ::drawedges && ::drawedges != 2, modelview, projection, viewport);
			
			//file << "<g stroke=\"black\">" << std::endl;
			//if (drawedges && drawedges != 2)svg_draw_edges(file,sorted_clip_boundary,modelview,projection,viewport);
			//file << "</g>" << std::endl;
		}
		
		


		if( draw_volumetric )
		{
			//if( bndupdate ) CommonVolumetricView->camera(campos,interactive);
			//CommonVolumetricView->draw(interactive);
		}
		
		if( CommonVectors )
			CommonVectors->SVGDraw(file,modelview,projection,viewport);


		for(int k = 0; k < streamlines.size(); ++k)
			streamlines[k].SVGDraw(file,modelview,projection,viewport);//interactive);

		file << "<g stroke=\"black\">" << std::endl;
		for (int k = 0; k < segments.size(); ++k)
			segments[k].SVGDraw(file, modelview, projection, viewport);
		file << "</g>" << std::endl;

		if( boundary )
		{
			file << "<g fill=\"black\" fill-opacity=\"0.25\">" << std::endl;
			//if (drawedges && drawedges != 2)svg_draw_edges(file,all_boundary,modelview,projection,viewport);
			if (!(drawedges == 2 || drawedges == 3))
				svg_draw_faces_nc(file, all_boundary, ::drawedges && ::drawedges != 2, modelview, projection, viewport);
			file << "</g>" << std::endl;
		}
	}

	
	if( isColorBarEnabled() )
	{
		glLoadIdentity();
		set_matrix2d();

		glGetDoublev (GL_PROJECTION_MATRIX, projection);
		glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
		glGetIntegerv(GL_VIEWPORT,viewport);

		GetColorBar()->DrawSVG(file, modelview, projection, viewport);
	}
	
	file << "</svg>" << std::endl;
}


int main(int argc, char ** argv)
{
	double tt;
	table[CENTROID]    = CELL | FACE;
	table[NORMAL]      = FACE;
	table[ORIENTATION] = FACE;
	table[MEASURE]     = CELL|FACE;
	mesh = new Mesh();
	printf("Started loading mesh.\n");
	tt = Timer();
  //mesh->SetCommunicator(INMOST_MPI_COMM_WORLD);
  mesh->SetParallelFileStrategy(0);
  mesh->SetParallelStrategy(1);
	mesh->SetFileOption("VERBOSITY","2");
	if( argc < 2 )
	{
		printf("Usage: %s mesh_file [dims]\n",argv[0]);
		return 0;
	}
	//try
	{
		if( argc > 2 )	mesh->SetFileOption("VTK_GRID_DIMS",argv[2]);
		mesh->Load(argv[1]);
	} 
	/*
	catch(...)
	{
		printf("Error during loading %s\n",argv[1]);
		return -1;
	}
	*/
	printf("Done. Time %lg\n",Timer()-tt);
	//int fixed = 0;
	//tt = Timer();
	//delete mesh;
	//printf("Delete %lg\n",Timer()-tt);
	//return 0;

	//Mesh::GeomParam param;
	//param[MEASURE] = CELL;
	//param[ORIENTATION] = FACE;
	//mesh->RemoveGeometricData(param);
	//mesh->PrepareGeometricData(param);

	std::map<Element::GeometricType,int> elems;

	for(Mesh::iteratorElement it = mesh->BeginElement(CELL|FACE|EDGE|NODE); it != mesh->EndElement(); ++it)
		elems[it->GetGeometricType()]++;

	for(std::map<Element::GeometricType,int>::iterator it = elems.begin(); it != elems.end(); ++it )
		std::cout << Element::GeometricTypeName(it->first) << ": " << it->second << std::endl;


	for(ElementType mask = NODE; mask <= CELL; mask = mask << 1)	
		std::cout << ElementTypeName(mask) << " " << mesh->NumberOf(mask) << std::endl;
	
	//return 0;
	//for(Mesh::iteratorFace f = mesh->BeginFace(); f != mesh->EndFace(); f++)
	//	if( Geometry::FixNormaleOrientation(&*f) ) fixed++;
	//printf("fixed: %d\n",fixed);
	printf("Computing geometric quantities\n");
	tt = Timer();
	mesh->PrepareGeometricData(table);
	printf("Done. %lg\n",Timer()-tt);
	
	printf("Calculating bounds.\n");
	tt = Timer();
	if( mesh->GetDimensions() == 2 )
	{
		for(Mesh::iteratorNode n = mesh->BeginNode(); n != mesh->EndNode(); n++)
		{
			Storage::real_array c = n->Coords();
			if( c[0] > sright ) sright = c[0];
			if( c[0] < sleft ) sleft = c[0];
			if( c[1] > stop ) stop = c[1];
			if( c[1] < sbottom ) sbottom = c[1];
		}
		sfar = -1, snear = 1;
	}
	else
	{
		for(Mesh::iteratorNode n = mesh->BeginNode(); n != mesh->EndNode(); n++)
		{
			Storage::real_array c = n->Coords();
			if( c[0] > sright ) sright = c[0];
			if( c[0] < sleft ) sleft = c[0];
			if( c[1] > stop ) stop = c[1];
			if( c[1] < sbottom ) sbottom = c[1];
			if( c[2] > sfar ) sfar = c[2];
			if( c[2] < snear ) snear = c[2];
		}
	}
	printf("Done. Time %lg\n",Timer()-tt);
	printf("%g:%g %g:%g %g:%g\n",sleft,sright,sbottom,stop,snear,sfar);

	printf("Gathering boundary faces.\n");
	tt = Timer();
	boundary_faces.SetMeshLink(mesh);
	for(Mesh::iteratorCell f = mesh->BeginCell(); f != mesh->EndCell(); f++) 
	{
		if( f->GetElementDimension() == 2 )
			boundary_faces.push_back(*f);
	}
	for(Mesh::iteratorFace f = mesh->BeginFace(); f != mesh->EndFace(); f++) if( f->GetElementDimension() == 2 )
	{
		if( f->Boundary() )
			boundary_faces.push_back(*f);
	}
	printf("Done. Time %lg\n",Timer()-tt);

	if( boundary_faces.empty() )
	{
		printf("Haven't found any boundary elements of the mesh. Nothing to display.\n");
		return -1;
	}

	printf("Prepearing set of boundary faces for drawing.\n");
	tt = Timer();
	INMOST_DATA_ENUM_TYPE pace = std::max<INMOST_DATA_ENUM_TYPE>(1,std::min<INMOST_DATA_ENUM_TYPE>(15,(unsigned)boundary_faces.size()/100));
	for(INMOST_DATA_ENUM_TYPE k = 0; k < boundary_faces.size(); k++) 
	{
		all_boundary.push_back(DrawFace(boundary_faces[k]));
		if( k%pace == 0 ) all_boundary.back().set_flag(true);
	}
	printf("Done. Time %g\n",Timer() - tt);

  if(mesh->HaveTag("ADDED_ELEMENTS") )
  {
    Tag add = mesh->GetTag("ADDED_ELEMENTS");
	  
    if( add.isDefined(EDGE) ) for(Mesh::iteratorEdge it = mesh->BeginEdge(); it != mesh->EndEdge(); ++it) if( it->Integer(add) ) added_edges.push_back(it->self());
    if( add.isDefined(FACE) )  for(Mesh::iteratorFace it = mesh->BeginFace(); it != mesh->EndFace(); ++it) if( it->Integer(add) && !it->Boundary() ) added_faces.push_back(DrawFace(it->self()));
  }
  if(mesh->HaveTag("CONORMALS"))
  {
    Tag cnrmls = mesh->GetTag("CONORMALS");
    conormals.reserve(mesh->NumberOfFaces()*2*3);
    Storage::real side = 1.0e20;
    for(Mesh::iteratorCell it = mesh->BeginCell(); it != mesh->EndCell(); ++it)
    {
      Storage::real max[3],min[3];
      GetBox(it->self(),min,max);
      side = std::min(side,max[0]-min[0]);
      side = std::min(side,max[1]-min[1]);
      side = std::min(side,max[2]-min[2]);
    }
    side *=0.25;
    for(Mesh::iteratorFace it = mesh->BeginFace(); it != mesh->EndFace(); ++it)
    {
      Storage::real_array fconormals = it->RealArray(cnrmls);
      Storage::real cnt[3];
      it->Centroid(cnt);
      conormals.push_back(cnt[0]);
      conormals.push_back(cnt[1]);
      conormals.push_back(cnt[2]);
      conormals.push_back(cnt[0]+side*fconormals[0]);
      conormals.push_back(cnt[1]+side*fconormals[1]);
      conormals.push_back(cnt[2]+side*fconormals[2]);
      conormals.push_back(cnt[0]);
      conormals.push_back(cnt[1]);
      conormals.push_back(cnt[2]);
      conormals.push_back(cnt[0]+side*fconormals[3]);
      conormals.push_back(cnt[1]+side*fconormals[4]);
      conormals.push_back(cnt[2]+side*fconormals[5]);
    }
  }
  if(mesh->HaveTag("HARMONIC_POINT"))
  {
    Tag h1 = mesh->GetTag("HARMONIC_POINT");
    harmonic_points.reserve(mesh->NumberOfFaces()*3);
    for(Mesh::iteratorFace it = mesh->BeginFace(); it != mesh->EndFace(); ++it)
    {
      Storage::real_array h1a = it->RealArray(h1);
      harmonic_points.insert(harmonic_points.end(),h1a.begin(),h1a.end());
    }
  }
  std::cout << "Harmonic points array size: " << harmonic_points.size()/3 << std::endl;

  if(mesh->HaveTag("DUAL_HARMONIC_POINT"))
  {
    Tag h2 = mesh->GetTag("DUAL_HARMONIC_POINT");
    for(Mesh::iteratorElement it = mesh->BeginElement(CELL|FACE); it != mesh->EndElement(); ++it) if( it->HaveData(h2) )
    {
      Storage::real_array h2a = it->RealArray(h2);
      dual_harmonic_points.insert(dual_harmonic_points.end(),h2a.begin(),h2a.end());
    }
  }

  std::cout << "Boundary harmonic points array size: " << dual_harmonic_points.size()/9 << std::endl;

	printf("Prepearing interactive mesh clipper.\n");
	//tt = Timer();
	if( mesh->GetDimensions() != 2 )
		oclipper = new clipper(mesh);
	bclipper = new bnd_clipper(mesh,boundary_faces.data(),(unsigned)boundary_faces.size());
	clipupdate = true;
	//printf("Done. Time %g\n",Timer() - tt);

	CommonVolumetricView = new volumetric(mesh);
	//CommonVolumetricView = new volumetric2(mesh);
	
	shift[0] = -(sleft+sright)*0.5;
	shift[1] = -(sbottom+stop)*0.5;
	shift[2] =  -(sfar+snear)*0.5;


	p[0] = (sleft+sright)*0.5;
	p[1] = (sbottom+stop)*0.5;
	p[2] = (sfar+snear)*0.5;

  if( mesh->HaveTag("FACE_CENTER") && mesh->GetTag("FACE_CENTER").isDefined(FACE) && mesh->GetTag("FACE_CENTER").GetSize() == 3 )
  {
    std::cout << "Have face centers!" << std::endl;
    face_center = mesh->GetTag("FACE_CENTER");
  }


  //if (mesh->HaveTag("VELOCITY") && mesh->GetTag("VELOCITY").isDefined(CELL))
	//	  BuildStreamlines(mesh,mesh->GetTag("VELOCITY"),streamlines);

  {
	  std::map<ElementType,int> num_orphans, num_topo;
	  
	  for(Mesh::iteratorElement it = mesh->BeginElement(FACE|EDGE|NODE); it != mesh->EndElement(); ++it)
		  if( it->nbAdjElements(CELL) == 0 ) 
		  {
			  orphans.push_back(it->self());
			  num_orphans[it->GetElementType()]++;
		  }
	   
		  printf("number of orphan elements: %lu\n",orphans.size());
		  for(std::map<ElementType,int>::iterator it = num_orphans.begin(); it != num_orphans.end(); ++it)
			  printf("%s %d\n",ElementTypeName(it->first),it->second);
	  int was = orphans.size();
	  if(mesh->TopologyErrorTag().isValid())
	  {
		  for(Mesh::iteratorElement it = mesh->BeginElement(FACE|EDGE|NODE); it != mesh->EndElement(); ++it)
			  if( it->HaveData(mesh->TopologyErrorTag()) )
			  {
				  orphans.push_back(it->self());
				  num_topo[it->GetElementType()]++;
			  }
		  printf("number of elements with topology error: %lu\n",orphans.size()-was);
		  for(std::map<ElementType,int>::iterator it = num_topo.begin(); it != num_topo.end(); ++it)
			  printf("%s %d\n",ElementTypeName(it->first),it->second);
		  for(int k = was; k < orphans.size(); ++k)
			  std::cout << ElementTypeName(orphans[k]->GetElementType()) << ":" << orphans[k]->LocalID() << std::endl;
	  }
  }
	
  if( mesh->HaveTag("MATERIAL") )
  {
	  is_material_defined = 1;
	  Tag mat = mesh->GetTag("MATERIAL");
	  for(Mesh::iteratorFace it = mesh->BeginFace(); it != mesh->EndFace(); ++it)
	  {
		  int m1 = -1, m2 = -1;
		  if( it->BackCell().isValid()  ) m1 = it->BackCell()->Integer(mat);
		  if( it->FrontCell().isValid() ) m2 = it->FrontCell()->Integer(mat);
		  if( m1 != m2 )
		  {
			  if( m1 >= 0 ) material_faces[m1].push_back(it->GetHandle());
			  if( m2 >= 0 ) material_faces[m2].push_back(it->GetHandle());
		  }
	  }
	  std::map<int,double> material_v;
	  double gx = 0, gy = 0, gz = 0, gv = 0;
	  for(Mesh::iteratorCell it = mesh->BeginCell(); it != mesh->EndCell(); ++it)
	  {
		  double cnt[3], v = it->Volume();
		  it->Centroid(cnt);
		  int m = it->Integer(mat);
		  material_x[m] += cnt[0]*v;
		  material_y[m] += cnt[1]*v;
		  material_z[m] += cnt[2]*v;
		  material_v[m] += v;
		  gx += cnt[0]*v;
		  gy += cnt[1]*v;
		  gz += cnt[2]*v;
		  gv += v;
	  }
	  gx /= gv;
	  gy /= gv;
	  gz /= gv;
	  for(std::map<int,double>::iterator it = material_v.begin(); it != material_v.end(); ++it)
	  {
		  double & x = material_x[it->first];
		  double & y = material_y[it->first];
		  double & z = material_z[it->first];
		  x /= it->second;
		  y /= it->second;
		  z /= it->second;
		  double dx = x - gx;
		  double dy = y - gy;
		  double dz = z - gz;
		  x = dx * 0.65;
		  y = dy * 0.65;
		  z = dz * 0.65;
	  }
	  
  }

	quatinit();
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(width, height);
	glutInitWindowPosition (100, 100);
	glutCreateWindow("Graph");
	
	glDepthFunc(GL_LEQUAL);
	glClearDepth(1.f);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);


	color_bar::InitColorBar();
	
	glHint(GL_POLYGON_SMOOTH_HINT,GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
	
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	glClearColor (1.0f, 1.0f, 1.0f, 1.f);
	glutDisplayFunc(draw);
	glutReshapeFunc(reshape);
	
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboard2);
	glutMouseFunc(myclick);
	glutMotionFunc(myclickmotion);
	glutPassiveMotionFunc(mymotion);
	
	//glutIdleFunc(idle);
	
	glutPostRedisplay();
	glutMainLoop();
}
