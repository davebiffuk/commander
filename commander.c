/* commander.c
 * vim:sw=2:ts=2:ai:cindent
 * Dave Holland <dave@biff.org.uk> August 2011
 *
 * based on cubicgrid.c by Vasek Potocek and
 * polyhedra-gl.c by Jamie Zawinski
 */

/*
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 */

#define DEFAULTS   "*delay:         20000         \n" \
                   "*showFPS:       False         \n" \
                   "*wireframe:     False         \n"

#define refresh_commander 0
#include "xlockmore.h"

#ifdef USE_GL

#define DEF_SPEED    "1.0"
#define DEF_ZOOM     "20"
#define DEF_WHICH    "random"
#define DEF_DURATION "60"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "rotator.h"
#include "gltrackball.h"

#include "commander.h"
#include "normals.h"

/*************************************************************************/

static float size;
static float speed;
static char *do_which_str;
static int   duration;

static argtype vars[] = {
  { &speed,    "speed",    "Speed",    DEF_SPEED,    t_Float },
  { &size,     "zoom",     "Zoom",     DEF_ZOOM,     t_Float },
  {&do_which_str,"which",  "Which",    DEF_WHICH,    t_String},
  { &duration, "duration", "Duration", DEF_DURATION, t_Int},
};

static XrmOptionDescRec opts[] = {
  { "-speed",    ".speed",    XrmoptionSepArg, 0 },
  { "-zoom",     ".zoom",     XrmoptionSepArg, 0 },
  { "-which",    ".which",    XrmoptionSepArg, 0 },
  { "-duration", ".duration", XrmoptionSepArg, 0 },
};

ENTRYPOINT ModeSpecOpt commander_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   commander_description =
{ "commander", "init_commander", "draw_commander", "release_commander",
  "draw_commander", "change_commander", NULL, &commander_opts,
  25000, 1, 1, 1, 1.0, 4, "",
  "Shows a rotating 3D spaceship", 0, NULL
};
#endif

typedef struct {
  GLXContext    *glx_context;
  GLfloat       ratio;
  GLint         list;

  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  int npoints;
  int which;
	time_t last_change_time;
} commander_conf;

static commander_conf *commander = NULL;

static const GLfloat zpos = -16.0; /* orig -18.0 */

/*************************************************************************/

ENTRYPOINT Bool
commander_handle_event (ModeInfo *mi, XEvent *event)
{
  commander_conf *cp = &commander[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      cp->button_down_p = True;
      gltrackball_start (cp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      cp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (cp->trackball, event->xbutton.button, 2,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           cp->button_down_p)
    {
      gltrackball_track (cp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static Bool draw_main(commander_conf *cp) 
{
  double x, y, z;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  glTranslatef(0, 0, zpos);

  /* FIXME this is probably a good place to switch ships if
   * appropriate */
	{
		time_t now = time((time_t *) 0);
		if(cp->last_change_time == 0) { cp->last_change_time=now; }
		if(!cp->button_down_p && (now - cp->last_change_time >= duration)) {
			/* printf("change\n"); */
			cp->last_change_time = now;
		}
	}

  gltrackball_rotate (cp->trackball);
  get_rotation (cp->rot, &x, &y, &z, !cp->button_down_p);
  glRotatef (x * 360, 1.0, 0.0, 0.0);
  glRotatef (y * 360, 0.0, 1.0, 0.0);
  glRotatef (z * 360, 0.0, 0.0, 1.0);

  glCallList(cp->list);
  return True;
}

static void init_gl(ModeInfo *mi) 
{

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glDrawBuffer(GL_BACK);
  glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE); /* FIXME interacts badly with original data
														 from shape_to_h */
  /* glFrontFace(GL_CCW); */ /* CCW for original source data */
  glFrontFace(GL_CW); /* CW for "by hand" data */

  /* glPixelStorei(GL_UNPACK_ALIGNMENT, 1); */
  glShadeModel(GL_SMOOTH);

}

static void new_ship(ModeInfo *mi)
{
  commander_conf *cp = &commander[MI_SCREEN(mi)];
  int i;
  GLfloat *this_v;
  int wire = MI_IS_WIREFRAME(mi);
  GLint *p;
  /*GLfloat *this_n;*/
  int count;

  GLfloat bcolor[4] = {0.2, 0.2, 0.2, 0.2};
  GLfloat bspec[4]  = {0.1, 0.1, 0.1, 0.1};
  GLfloat bshiny    = 32.0;
  GLfloat pos[4] = {-8.0, -8.0, -16.0, 0.0}; /* -6 -6 -16 */
  GLfloat amb[4] = {0.2, 0.2, 0.2, 0.4};
  GLfloat dif[4] = {0.2, 0.2, 0.2, 0.2};
  GLfloat spc[4] = {0.0, 0.0, 0.0, 0.0};

	if(!wire) {
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);

		glLightfv(GL_LIGHT0, GL_POSITION, pos);
		glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
		glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
		glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

		glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, bcolor);
		glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, bspec);
		glMateriali  (GL_FRONT_AND_BACK, GL_SHININESS, bshiny);
	}


  cp->list = glGenLists(1);
  glNewList(cp->list, GL_COMPILE);
  if(MI_IS_MONO(mi)) {
    /* FIXME support mono display */
    abort();
  }
  else
  {
		/* http://www.opengl.org/resources/faq/technical/polygonoffset.htm
		 * says "Two passes are then made, once with the model's solid
		 * geometry and once again with the line geometry." -- so this
		 * draws the solid polygons first.
		 * Seconded by http://glprogramming.com/red/chapter06.html
		 */

	if(wire) {
		/* set up the offset for the black fill for hidden line removal */
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glPolygonOffset(1.0, 1.0);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glColor3f(0.0, 0.0, 0.0);
  }

  /* reset pointers */
	p=ship_f[cp->which];
  this_v=ship_v[cp->which];
  /*this_n=ship_n[cp->which];*/

  /* draw the solid shape */
  while(*p != 0) {
    count=*p; p++;
    if(count==1) { glBegin(GL_POINTS); }
    else if(count==2) { /* FIXME this is imperfect */
      glDisable(GL_LIGHTING);
      glColor3f(0.7, 0.7, 0.7);
      glBegin(GL_LINES);
    }
    else {
      if(wire) { glColor3f(0.0, 0.0, 0.0); }
      if(!wire) { glEnable(GL_LIGHTING); }
      glBegin(GL_POLYGON); 
      do_normal(
          this_v[p[0]*3], this_v[p[0]*3+1], this_v[p[0]*3+2],
          this_v[p[1]*3], this_v[p[1]*3+1], this_v[p[1]*3+2],
          this_v[p[2]*3], this_v[p[2]*3+1], this_v[p[2]*3+2]);
			/*glNormal3f(this_n[0],this_n[1],this_n[2]); this_n+=3;*/
      }
    for (i = 0 ; i < count ; i++) {
        glVertex3f(this_v[p[i]*3], this_v[p[i]*3+1], this_v[p[i]*3+2]);
    }
    cp->npoints++;
    p+=count;
    glEnd();
  }



  if(wire) { glDisable(GL_POLYGON_OFFSET_FILL); }

  }

  if(wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(1.0, 1.0, 1.0);

		/* chunky lines :-) */
		glLineWidth(2); 

  	/* reset pointers */
		p=ship_f[cp->which];
		this_v=ship_v[cp->which];
		/*this_n=ship_n[cp->which];*/
		/* draw the wireframe shape */
    while(*p != 0) {
      count=*p; p++;
      if(count==1) { glBegin(GL_POINTS); }
      else if(count==2) { glBegin(GL_LINES); }
      else {
				glBegin(GL_POLYGON); 
        do_normal(this_v[p[0]*3], this_v[p[0]*3+1], this_v[p[0]*3+2],
          this_v[p[1]*3], this_v[p[1]*3+1], this_v[p[1]*3+2],
          this_v[p[2]*3], this_v[p[2]*3+1], this_v[p[2]*3+2]);
				/*glNormal3f(this_n[0],this_n[1],this_n[2]); this_n+=3;*/
			}
      for (i = 0 ; i < count ; i++) {
        glVertex3f(this_v[p[i]*3], this_v[p[i]*3+1], this_v[p[i]*3+2]);
      }
      cp->npoints++;
      p+=count;
      glEnd();
    }
	}
  glEndList();
}

/*************************************************************************/

ENTRYPOINT void reshape_commander(ModeInfo *mi, int width, int height) 
{
  commander_conf *cp = &commander[MI_SCREEN(mi)];
  if(!height) height = 1;
  cp->ratio = (GLfloat)width/(GLfloat)height;
  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, cp->ratio, 0.1, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT void release_commander(ModeInfo *mi) 
{
  if (commander != NULL) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      commander_conf *cp = &commander[screen];
      if (cp->glx_context) {
        cp->glx_context = NULL;
      }
    }
    free((void *)commander);
    commander = NULL;
  }
  FreeAllGL(mi);
}

ENTRYPOINT void init_commander(ModeInfo *mi) 
{
  commander_conf *cp;
	int i;
	int do_which = -1;

  if(!commander) {
    commander = (commander_conf *)calloc(MI_NUM_SCREENS(mi), sizeof(commander_conf));
    if(!commander) return;
  }
  cp = &commander[MI_SCREEN(mi)];

  if ((cp->glx_context = init_GL(mi)) != NULL) {
    init_gl(mi);
    cp->which = -1;
    reshape_commander(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  {
    double spin_speed = 0.7 * speed;
    double spin_accel = 0.1 * speed;

    cp->rot = make_rotator (spin_speed, spin_speed, spin_speed,
                            spin_accel, 0, True);
    cp->trackball = gltrackball_init ();
  }

	/* figure out which ship to display */

	/* do_which=-1 for "random" mode */
	if(!strcasecmp (do_which_str, "random"))
		;
	else {
		for(i=0;i<NUM_ELEM(ship_names);i++) {
			if(!strcasecmp(do_which_str, ship_names[i])) {
					do_which=i;
					cp->which=i;
			}
		}
		if(do_which<0) {
			fprintf(stderr, "%s: no such ship: \"%s\"\n",
					progname, do_which_str);
			exit(1);
		}
	}

	/* FIXME (what?) */
	if(do_which==-1) {
		cp->which=random() % NUM_ELEM(ship_names);
	}

	new_ship(mi);

}

ENTRYPOINT void draw_commander(ModeInfo * mi) 
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  commander_conf *cp;
  if (!commander) return;
  cp = &commander[MI_SCREEN(mi)];
  MI_IS_DRAWN(mi) = True;
  if (!cp->glx_context) return;
  glXMakeCurrent(display, window, *(cp->glx_context));
  if (!draw_main(cp)) {
    release_commander(mi);
    return;
  }
  mi->polygon_count = cp->npoints;
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush(); /* or glFinish(); ??? */
  glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void change_commander(ModeInfo * mi) 
{
  commander_conf *cp = &commander[MI_SCREEN(mi)];
  if (!cp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cp->glx_context));
  init_gl(mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE ("Commander", commander)

#endif
