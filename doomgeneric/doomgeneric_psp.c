//doomgeneric for psp

#include "doomkeys.h"
#include "doomgeneric.h"
#include "i_video.h"

#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psprtc.h>

#include <stdio.h>
#include <time.h>
#include <math.h>

#define PSP_VIRTUALWIDTH  4096
#define PSP_VIRTUALHEIGHT 4096
#define PSP_SCREENWIDTH   480
#define PSP_SCREENHEIGHT  272
#define FRAMEBUFFER_WIDTH 512
#define BYTES_PER_PIXEL   4
#define FRAMEBUFFER_SIZE  (FRAMEBUFFER_WIDTH*PSP_SCREENHEIGHT*BYTES_PER_PIXEL)

#define BUTTONQUEUE_SIZE 16

PSP_MODULE_INFO("doomgeneric", 0, 1, 0);
PSP_HEAP_SIZE_KB(16*1024); // 16KB

struct Vertex
{
	float u,v;
	float x,y,z;
};

struct Viewport
{
  float x, y;
  float w, h;
};

static uint64_t s_startTic;
static uint64_t s_ticFrequency;
static uint32_t	__attribute__((aligned(16))) s_displayList[262144];

static SceCtrlData s_inputData;
static SceCtrlData s_oldInputData;
static uint16_t s_buttonQueue[BUTTONQUEUE_SIZE];
static uint32_t s_buttonQueueWriteIndex = 0;
static uint32_t s_buttonQueueReadIndex = 0;
static uint32_t s_buttonsToCheck[] = {
  PSP_CTRL_START,
  PSP_CTRL_SELECT,
  PSP_CTRL_UP,
  PSP_CTRL_DOWN,
  PSP_CTRL_LEFT,
  PSP_CTRL_RIGHT,
  PSP_CTRL_LTRIGGER,
  PSP_CTRL_RTRIGGER,
  PSP_CTRL_CROSS,
  PSP_CTRL_SQUARE,
  PSP_CTRL_TRIANGLE
};

static struct Viewport s_viewport;

static int exitCallback(int arg1, int arg2, void* common)
{
  sceKernelExitGame();
  return 0;
}

static int callbackThread(SceSize args, void* argp) 
{
  int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();

  return 0;
}

static int setupCallbacks(void) 
{
  int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
  if (thid >= 0) {
    sceKernelStartThread(thid, 0, NULL);
  }

  return thid;
}

static void setupViewport()
{
  int realW = PSP_SCREENWIDTH;
  int realH = PSP_SCREENHEIGHT;
  int logicalW = SCREENWIDTH;
  int logicalH = SCREENHEIGHT;

  // DOOM native resolution has a narrower aspect ratio, so we will scale
  // it up to the PSP by adding black sidebars.
  float scale = (float)realH / logicalH;
  s_viewport.y = 0.0f;
  s_viewport.h = realH;
  s_viewport.w = ceilf((float)logicalW * scale);
  s_viewport.x = (realW - s_viewport.w) / 2.0f;
}

static void setupGraphics()
{
  sceKernelDcacheWritebackAll();

  setupViewport();

  sceGuInit();

  sceGuStart(GU_DIRECT, s_displayList);
  sceGuDrawBuffer(GU_PSM_8888, (void*)NULL, FRAMEBUFFER_WIDTH);
  sceGuDispBuffer(PSP_SCREENWIDTH, PSP_SCREENHEIGHT, (void*)FRAMEBUFFER_SIZE, FRAMEBUFFER_WIDTH);

  // Center the virtual coordinate range. PSP has virtual coordinate space of 4096x4096.
  sceGuOffset(PSP_VIRTUALWIDTH/2 - (PSP_SCREENWIDTH/2), PSP_VIRTUALHEIGHT/2 - (PSP_SCREENHEIGHT/2));
  sceGuViewport(PSP_VIRTUALWIDTH/2, PSP_VIRTUALHEIGHT/2, PSP_SCREENWIDTH, PSP_SCREENHEIGHT);

  // Discard pixels outside of the screen bounds.
  sceGuScissor(0, 0, PSP_SCREENWIDTH, PSP_SCREENHEIGHT);
  sceGuEnable(GU_SCISSOR_TEST);

  // Setup backface culling.
  sceGuFrontFace(GU_CW);
  sceGuEnable(GU_CULL_FACE);

  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuClear(GU_COLOR_BUFFER_BIT);

  sceGuFinish();
  sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
  sceDisplayWaitVblankStart();
  sceGuDisplay(GU_TRUE);
}

void DG_Init()
{
  setupCallbacks();

  // Redirect stdout,stderr to files for the PSP.
  freopen("doomgeneric-stdout.txt", "w", stdout);
  freopen("doomgeneric-stderr.txt", "w", stderr);

  sceRtcGetCurrentTick(&s_startTic);
  s_ticFrequency = sceRtcGetTickResolution();

  setupGraphics();

  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

static uint8_t convertToDoomKey(uint32_t button)
{
  uint8_t doomKey = 0;

  switch (button)
  {
    case PSP_CTRL_START:
      doomKey = KEY_ENTER;
      break;
    case PSP_CTRL_SELECT:
      doomKey = KEY_ESCAPE;
      break;
    case PSP_CTRL_UP:
      doomKey = KEY_UPARROW;
      break;
    case PSP_CTRL_DOWN:
      doomKey = KEY_DOWNARROW;
      break;
    case PSP_CTRL_LEFT:
      doomKey = KEY_LEFTARROW;
      break;
    case PSP_CTRL_RIGHT:
      doomKey = KEY_RIGHTARROW;
      break;
    case PSP_CTRL_RTRIGGER:
      doomKey = KEY_FIRE;
      break;
    case PSP_CTRL_LTRIGGER:
      doomKey = KEY_RSHIFT;
      break;
    case PSP_CTRL_SQUARE:
      doomKey = KEY_USE;
      break;
    case PSP_CTRL_TRIANGLE:
      doomKey = KEY_TAB;
      break;
    case PSP_CTRL_CROSS:
    default:
      break;
    }

  return doomKey;
}

static void addButtonToQueue(int pressed, uint32_t button)
{
  uint8_t key = convertToDoomKey(button);

  uint16_t buttonData = (pressed << 8) | key;

  s_buttonQueue[s_buttonQueueWriteIndex] = buttonData;
  s_buttonQueueWriteIndex++;
  s_buttonQueueWriteIndex %= BUTTONQUEUE_SIZE;
}

static void handleInput()
{
  s_oldInputData = s_inputData;

  sceCtrlPeekBufferPositive(&s_inputData, 1);

  int buttons = sizeof(s_buttonsToCheck) / sizeof(uint32_t);
  for (int i = 0; i < buttons; i++)
  {
    uint32_t button = s_buttonsToCheck[i];
    if (s_inputData.Buttons & button && !(s_oldInputData.Buttons & button))
    {
      addButtonToQueue(1, button);
    }
    else if (!(s_inputData.Buttons & button) && s_oldInputData.Buttons & button)
    {
      addButtonToQueue(0, button);
    }
  }
}

void DG_DrawFrame()
{
  sceKernelDcacheWritebackAll();

  sceGuStart(GU_DIRECT, s_displayList);

  sceGuClearColor(0xFF000000);
  sceGuClear(GU_COLOR_BUFFER_BIT);

  // Setup and load the color palette (32*8, 256 colors).
  sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
  sceGuClutLoad(32, colors);

  // Setup and load the indexed texture map.
  sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
  sceGuTexImage(0, FRAMEBUFFER_WIDTH, 256, SCREENWIDTH, I_VideoBuffer);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
  sceGuTexFilter(GU_NEAREST, GU_LINEAR);

  // Setup the vertices for the doom framebuffer.
  struct Vertex *vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));
  vertices[0].u = 0; vertices[0].v = 0;
  vertices[0].x = s_viewport.x; vertices[0].y = s_viewport.y; vertices[0].z = 0;
  vertices[1].u = SCREENWIDTH; vertices[1].v = SCREENHEIGHT;
  vertices[1].x = s_viewport.x + s_viewport.w; vertices[1].y = s_viewport.y + s_viewport.h; vertices[1].z = 0;

  sceGumDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, NULL, vertices);

  sceGuFinish();
  sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();

  handleInput();
}

void DG_SleepMs(uint32_t ms)
{
  sceKernelDelayThread(ms * 1000);
}

uint32_t DG_GetTicksMs()
{
  uint64_t currentTic;
  sceRtcGetCurrentTick(&currentTic);

  double elapsed = (double)((currentTic - s_startTic)*1000) / s_ticFrequency;
  return (uint32_t)elapsed;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
  if (s_buttonQueueReadIndex == s_buttonQueueWriteIndex)
  {
    return 0;
  }
  else
  {
    uint16_t buttonData = s_buttonQueue[s_buttonQueueReadIndex];
    s_buttonQueueReadIndex++;
    s_buttonQueueReadIndex %= BUTTONQUEUE_SIZE;

    *pressed = buttonData >> 8;
    *doomKey = buttonData & 0xFF;

    return 1;
  }

  return 0;
}

void DG_SetWindowTitle(const char * title)
{
}
