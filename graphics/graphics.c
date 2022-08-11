#include <sys/types.h>
#include <stdio.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <string.h>
// cd
#include <libcd.h>
#include <malloc.h>

#define VMODE 0 // NTSC
#define SCREENXRES 320
#define SCREENYRES 240
#define CENTERX SCREENXRES/2    // Center of screen on x 
#define CENTERY SCREENYRES/2    // Center of screen on y
#define MARGINX 0
#define MARGINY 32
#define FONTSIZE 8 * 12
#define OTLEN 8
#define CD_SECTOR_SIZE 2048
// converting bites to sectors
#define BtoS(len) ((len + CD_SECTOR_SIZE -1 ) / CD_SECTOR_SIZE)

static char *loadFile;
static char *loadFileTIM;

CdlFILE filePos = {0};
CdlFILE filePosTIM = {0};
// using an array instead of free memory
static unsigned char ramAddr[0x40000];

u_long *dataBuffer;

u_char CtrlResult[8];
int CDreadOK = 0;
int CDreadResult = 0;

DISPENV disp[2];
DRAWENV draw[2];
short db = 0;

u_long ot[2][OTLEN];
char pribuff[2][32768];
char *nextpri;

char fndstr[5] = "NOTFN";

// 4bit TIM
// extern unsigned long _binary____TIM_tornabene4_tim_start[];
// extern unsigned long _binary____TIM_tornabene4_tim_end[];
// extern unsigned long _binary____TIM_tornabene4_tim_length;

// TIM_IMAGE TIM_4;

// helper function to load a texture
void LoadTexture(u_long * tim, TIM_IMAGE * tparam){
    OpenTIM(tim);
    ReadTIM(tparam);
    LoadImage(tparam->prect, tparam->paddr);
    DrawSync(0);
    // if there's a CLUT load it
    if (tparam->mode & 0x8){
        LoadImage(tparam->crect, tparam->caddr);
        DrawSync(0);
    }
}

u_long *LoadFileCD(const char* filename)
{
    CdlFILE file;
    u_long *buffer;

    printf("Reading file %s ... \n", filename);

    //Searching for file
    if (!CdSearchFile(&file, (char*)filename))
    {
        printf("File not found! \n");
        return NULL;
    }

    // Allocate a buffer for the file
    buffer =  (u_long*)malloc(BtoS(file.size) * CD_SECTOR_SIZE);

    // Set seek target
    CdControl(CdlSetloc, (u_char*)&file.pos, 0);

    // read sectors
    CdRead( (int)BtoS(file.size), (u_long *)buffer, CdlModeSpeed);

    // wait until loaded
    CdReadSync(0,0);
    
    printf("Load done\n");
    return buffer;
}

void display() {
    DrawSync(0); // wait for graphics processing to finish

    VSync(0); //wait vsync
    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
    SetDispMask(1); // enable display
    DrawOTag(&ot[db][OTLEN-1]);
    db = !db; // swap buffers
    nextpri = pribuff[db]; // reset next primitive pointer
}

int calc_dirs(int pos_x, int pos_y, int box_width, int box_height, int* dir_x, int* dir_y)
{
    if (pos_x + box_width > SCREENXRES)
    {
        *dir_x = -1;
    }
    if (pos_x < 0) {
        *dir_x = 1;
    }

    if (pos_y + box_height > SCREENYRES)
    {
        *dir_y = -1;
    }
    if (pos_y < 0) {
        *dir_y = 1;
    }

}

void init(void){
    ResetGraph(0);
    // First buffer
    SetDefDispEnv(&disp[0], 0, 0, 320, 240);
    SetDefDrawEnv(&draw[0], 0, 240, 320, 240);
    // Second buffer
    SetDefDispEnv(&disp[1], 0, 240, 320, 240);
    SetDefDrawEnv(&draw[1], 0, 0, 320, 240);

    // load font
    FntLoad(960, 0);
    FntOpen(MARGINX, SCREENYRES - MARGINY - FONTSIZE, SCREENXRES - MARGINX * 2, FONTSIZE, 0, 280 ); // FntOpen(x, y, width, height,  black_bg, max. nbr. chars
    draw[0].isbg = 1; // enable clear
    setRGB0(&draw[0], 63, 0, 127);
    draw[1].isbg = 1;
    setRGB0(&draw[1], 63, 0, 127);
    
    // set initial primitive pointer address
    nextpri = pribuff[0];

    // CD init
    CdInit();
    InitHeap((u_long *)ramAddr, sizeof(ramAddr));
    loadFile = "\\DATA.DAT;1";
    CdSearchFile(&filePos, loadFile);

    loadFileTIM = "\\TORN4.TIM;1";
    
    if (CdSearchFile(&filePosTIM, loadFileTIM))
    {
        (void)strncpy(fndstr, "FOUND", sizeof(fndstr));
    }
}

int main(void)
{
    TILE *tile;
    TILE *second_tile;
    SPRT *sprt_4b;
    DR_TPAGE * tpage_4b; // pointer to DR_TPAGE struct
    int pad = 0;
    PadInit(0);
    int x_tile = 0;
    int y_tile = 0;
    int x_tile2 = 100;
    int y_tile2 = 100;

    int x_dir = 1;
    int y_dir = 1;
    int x_dir2 = 1;
    int y_dir2 = 1;

    int x_tim = 1;
    int y_tim = 1;
    int x_dir_tim = 1;
    int y_dir_tim = 1;
    init();
    // LoadTexture(_binary____TIM_tornabene4_tim_start, &TIM_4);

    // load DATA.DAT block
    dataBuffer = malloc(BtoS(filePos.size) * CD_SECTOR_SIZE);
    CdControl(CdlSetloc, (u_char *)&filePos.pos, CtrlResult);
    CDreadOK = CdRead( (int)BtoS(filePos.size), (u_long *)dataBuffer, CdlModeSpeed);
    CDreadResult = CdReadSync(0, 0);

    //load torn4
    u_long *torn4_image;
    TIM_IMAGE torn4_tim;

    
    torn4_image = LoadFileCD("\\TIM\\TORN4.TIM;1");
    LoadTexture(torn4_image, &torn4_tim);

    while (1) {
        ClearOTagR(ot[db], OTLEN); // clear ordering table

        tile = (TILE*)nextpri; // cast next primitive
        setTile(tile); // initialize the primitive
        calc_dirs(x_tile, y_tile, 64, 64, &x_dir, &y_dir);
        x_tile = x_tile + x_dir;
        y_tile = y_tile + y_dir;
        setXY0(tile, x_tile, y_tile);
        setWH(tile, 64, 64);
        setRGB0(tile, 255, 255, 0); // set yellow
        addPrim(ot[db][OTLEN -1], tile); 
        nextpri += sizeof(TILE); // advance the next primitive pointer

        second_tile = (TILE*)nextpri;
        setTile(second_tile);
        calc_dirs(x_tile2, y_tile2, 32, 32, &x_dir2, &y_dir2);
        x_tile2 = x_tile2 + x_dir2;
        y_tile2 = y_tile2 + y_dir2;
        setXY0(second_tile, x_tile2, y_tile2);
        setWH(second_tile, 32, 32);
        setRGB0(second_tile, 50, 50, 50);
        addPrim(ot[db][OTLEN -2], second_tile);
        nextpri += sizeof(TILE);

        sprt_4b = (SPRT *)nextpri;
        SetSprt(sprt_4b);
        calc_dirs(x_tim, y_tim, 64, 128, &x_dir_tim, &y_dir_tim);
        //x_tim = x_tim + 2*x_dir_tim;
        //y_tim = y_tim + y_dir_tim;
        x_tim = CENTERX - 32;
        y_tim = CENTERY - 64;
        setXY0(sprt_4b, x_tim, y_tim);
        setRGB0(sprt_4b, 128, 128, 128);
        setWH(sprt_4b, 64, 128);
        setClut(sprt_4b, torn4_tim.crect->x, torn4_tim.crect->y);
        addPrim(ot[db], sprt_4b);
        nextpri += sizeof(SPRT);
        tpage_4b = (DR_TPAGE*)nextpri;
        setDrawTPage(tpage_4b, 0, 1, getTPage(torn4_tim.mode&0x3, 0, torn4_tim.prect->x, torn4_tim.prect->y));
        addPrim(ot[db], tpage_4b);
        nextpri += sizeof(DR_TPAGE);

        pad = PadRead(0);
        if(pad& PADLup) {sprt_4b->y0 = y_tim -7;}
        if(pad& PADLdown) {sprt_4b->y0 = y_tim + 7;}
        if(pad& PADLleft) {sprt_4b->x0 = x_tim - 7;}
        if(pad& PADLright) {sprt_4b->x0 = x_tim + 7;}
        printf("direction_x %d \n", x_dir);
        printf("direction_y %d \n", y_dir);

        FntPrint("tile_0: %d, %d \n", x_tile, y_tile);
        FntPrint("tile_1: %d, %d \n", x_tile2, y_tile2);
        FntPrint("sprite_0: %d, %d \n", sprt_4b->x0, sprt_4b->y0);
        FntPrint("\n%s%d\n", (char *)dataBuffer, VSync(-1));
        FntPrint("Heap: %x - Buf: %x\n", ramAddr, dataBuffer);
        FntPrint("CdCtrl: %d\nRead  : %d %d\n", CtrlResult[0], CDreadOK, CDreadResult);
        FntPrint("Size: %dB sectors: %d\n", filePos.size, BtoS(filePos.size));
        FntPrint("tile found state: %s", fndstr);
        
        FntFlush(-1);
        display();

    }
    
    return 0;
}