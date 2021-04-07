//doomgeneric for psp

#include "doomkeys.h"
#include "doomgeneric.h"
#include "i_video.h"

#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <psprtc.h>
#include <stdio.h>
#include <time.h>

#define PSP_VIRTUALWIDTH  4096
#define PSP_VIRTUALHEIGHT 4096
#define PSP_SCREENWIDTH   480
#define PSP_SCREENHEIGHT  272
#define FRAMEBUFFER_WIDTH 512
#define BYTES_PER_PIXEL   4
#define FRAMEBUFFER_SIZE  (FRAMEBUFFER_WIDTH*PSP_SCREENHEIGHT*BYTES_PER_PIXEL)

PSP_MODULE_INFO("doomgeneric", 0, 1, 0);
PSP_HEAP_SIZE_KB(16*1024); // 16KB

struct Vertex
{
	float u,v;
	float x,y,z;
};

static uint64_t s_startTic;
static uint64_t s_ticFrequency;
static uint32_t	__attribute__((aligned(16))) s_displayList[262144];

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

static void setupGraphics()
{
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
  vertices[0].x = 0; vertices[0].y = 0; vertices[0].z = 0;
  vertices[1].u = SCREENWIDTH; vertices[1].v = SCREENHEIGHT;
  vertices[1].x = PSP_SCREENWIDTH; vertices[1].y = PSP_SCREENHEIGHT; vertices[1].z = 0;

  sceGumDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, NULL, vertices);

  sceGuFinish();
  sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();
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
  return 0;
}

void DG_SetWindowTitle(const char * title)
{
}
