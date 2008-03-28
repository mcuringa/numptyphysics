#ifndef CONFIG_H
#define CONFIG_H

#define CANVAS_WIDTH  800
#define CANVAS_HEIGHT 480
#define CANVAS_GROUND 30
#define CANVAS_WIDTHf  800.0f
#define CANVAS_HEIGHTf 480.0f
#define CANVAS_GROUNDf 30.0f
#define PIXELS_PER_METREf 10.0f
#define CLOSED_SHAPE_THREHOLDf 0.4f
#define SIMPLIFY_THRESHOLDf 1.0f //PIXELs //(1.0/PIXELS_PER_METREf)
#define MULTI_VERTEX_LIMIT (b2_maxShapesPerBody)

#ifdef ARCH_arm //maemo
#  define ITERATION_RATE    60 //fps
#  define RENDER_RATE       20 //fps
#  define SOLVER_ITERATIONS 10
#  define JOINT_TOLERANCE   4.0f //PIXELs
#  define SELECT_TOLERANCE  8.0f //PIXELS_PER_METREf)
#  define CLICK_TOLERANCE   16 //PIXELs 
#else
#  define ITERATION_RATE    60 //fps
#  define RENDER_RATE       30 //fps
#  define SOLVER_ITERATIONS 10
#  define JOINT_TOLERANCE   4.0f //PIXELs
#  define SELECT_TOLERANCE  5.0f //PIXELS_PER_METREf)
#  define CLICK_TOLERANCE   4 //PIXELs 
#endif

#define ITERATION_TIMESTEPf  (1.0f / (float)ITERATION_RATE)
#define RENDER_INTERVAL (1000/RENDER_RATE)

#define HIDE_STEPS ITERATION_RATE

#define COLOUR_RED     0xb80000
#define COLOUR_YELLOW  0xffd700 
#define COLOUR_BLUE    0x000077 
#define COLOUR_GREEN   0x108710
#define COLOUR_BLACK   0x101010
#define COLOUR_BROWN   0x703010

#ifndef INSTALL_BASE_PATH
#  define INSTALL_BASE_PATH "/usr/share/numptyphysics"
#endif
#define DEFAULT_LEVEL_PATH INSTALL_BASE_PATH
#define DEFAULT_RESOURCE_PATH DEFAULT_LEVEL_PATH
#ifndef USER_BASE_PATH
#  define USER_BASE_PATH ".numptyphysics"
#endif
#define USER_LEVEL_PATH USER_BASE_PATH

#define ICON_SCALE_FACTOR 4

class Config
{
 public:
  static int x;
};

#endif //CONFIG_H
