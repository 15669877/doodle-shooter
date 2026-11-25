/* doodle_shooter.c -- "白纸+铅笔涂鸦" 横版飞机射击
 * 纯 C99 / 单文件 / 零外部资源(无图片·无字体文件·无音频)
 * Linux/macOS: gcc -O2 -Wall -Wextra -o game game.c -lX11 -lm
 * Windows MSVC: cl /W4 /O2 game.c
 * Windows GCC : gcc -O2 -Wall -Wextra -mwindows -o game.exe game.c -lgdi32 -luser32
 * 模式: game | --selftest | --sim | --preview
 * 美术: 纸白#F7F7F3 墨黑#111111 灰阶#E4E4E4/#B4B4B4/#6E6E6E;
 *       直线法线扰动+正弦微弯, 圆为两段随机起始角的不闭合弧,
 *       精灵像素 1/9 概率偏移±1px, 笔画随机断墨。 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#ifdef _WIN32
#  include <windows.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "user32.lib")
#    pragma comment(lib, "gdi32.lib")
#  endif
#else
#  include <X11/Xlib.h>
#  include <time.h>
#endif

/* =================== 1. 常量与全局 =================== */
#define SCR_W 640                    /* 屏幕宽(像素) */
#define SCR_H 480                    /* 屏幕高(像素) */
#define TPS   60                     /* 逻辑帧率 */
#define SPR   3                      /* 精灵放大倍数(玩家) */
#define MAXPT 128                    /* 单笔最多采样点 */
#define MAX_BULLET  64               /* 容量: 静态数组+alive标记, 零动态分配 */
#define MAX_EBULLET 48
#define MAX_ENEMY   40
#define MAX_DEBRIS  256
#define MAX_RING    24
#define NCLOUD      6
#define C_PAPER 0xF7F7F3u
#define C_LIGHT 0xE4E4E4u
#define C_MID   0xB4B4B4u
#define C_DARK  0x6E6E6Eu
#define C_INK   0x111111u
#define C_WHITE 0xFFFFFFu
enum { P_INK = 0, P_DARK, P_MID, P_LIGHT, P_PAPER, P_WHITE, P_COUNT };  /* 按亮度递增 */
static const unsigned PAL[P_COUNT] = { C_INK, C_DARK, C_MID, C_LIGHT, C_PAPER, C_WHITE };
enum { ST_TITLE = 0, ST_PLAY, ST_OVER };
enum { K_LEFT = 0, K_RIGHT, K_UP, K_DOWN, K_FIRE, K_COUNT };
/* 后端无关的原始键码: 两个后端都归一到这张表再查逻辑键 */
enum { KEY_L = 256, KEY_R, KEY_U, KEY_DN, KEY_SPACE, KEY_A, KEY_D, KEY_W, KEY_S };

typedef struct { float x, y, vx, vy; int alive; } Bullet;
typedef struct { float x, y, base_y, phase, spd; int alive, jam, hp, scale; } Enemy;
typedef struct { float x, y, vx, vy, ang, spin; int alive, life, maxlife, len, pi; } Debris;
typedef struct { float x, y, r0; int alive, life, maxlife; } Ring;
typedef struct { float x, y, r; int alive; } Cloud;

/* ---------- 美术资源(全部代码内生成) ---------- */
static const char *ART_PLANE[18] = {
    "......K......","......W......",".....KWK.....",".....KWK.....","....KMWMK....","....KMWMK....",
    "...KKMWMKK...","..KMMMWMMMK..",".KMMMMMMMMMK.","KMMMMMMMMMMMK","KMMMMMMMMMMMK",".KMMMMMMMMMK.",
    ".KMMMMMMMMMK.","..KMMMLMMMK..","...KDDDDDK...","....KKKKK....","....KLLLK....",".....KLK.....",
};
static const char *ART_ENEMY[13] = {   /* 17x13, 机头朝左迎面飞来 */
    "..KK.............","..KWK............","..KWWWK..........","..KWWWWK.........","..KWWWWWK........","..KWWWWWWK.......","KWWWWWWWMWWWWWWWK",
    "..KWWWWWWK.......","..KWWWWK.........","..KWWWK..........","..KWWK...........","..KWK............","..KK.............",
};
static const char *ART_BULLET[9] = {   /* 5x9 胶囊形子弹 */
    ".KKK.","KWWWK","KWWWK","KWWWK","KWWWK","KWWWK","KWWWK","KWWWK",".KKK.",
};
/* 5x7 点阵字: 列优先, 每字节 7 位, bit0 = 顶行 */
static const char FONT_CHARS[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-!/>";
static const unsigned char FONT[43][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x01,0x01}, {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03},
    {0x61,0x51,0x49,0x45,0x43}, {0x00,0x60,0x60,0x00,0x00}, {0x00,0x36,0x36,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x00,0x5F,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02}, {0x41,0x22,0x14,0x08,0x00},
};
/* ---------- 全局状态 ---------- */
static Bullet g_bul[MAX_BULLET]; static Bullet g_ebul[MAX_EBULLET]; static Enemy  g_en[MAX_ENEMY]; static Debris g_deb[MAX_DEBRIS];
static Ring   g_ring[MAX_RING]; static Cloud  g_cloud[NCLOUD]; static float g_px, g_py, g_pvx, g_pvy;   /* 玩家位置/速度 */
static int   g_plean, g_pinv;            /* 侧倾 / 无敌剩余帧 */ static int   g_state, g_score, g_high, g_frame, g_deaths;
static float g_diff;                     /* 难度计数 */ static int   g_spawn_cd, g_spawn_n, g_over_t, g_shake_t; static int   g_key[K_COUNT];
static int   g_sx, g_sy;                 /* 震屏偏移 */

/* =================== 2. 工具: 随机 / 时间 / 颜色 =================== */
static unsigned g_rng_game = 0x13579BDFu, g_rng_art = 0x2468ACE0u; static unsigned rnd_next(unsigned *s)
{
    unsigned x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}
/* 逻辑随机与美术随机分流: 画面抖动不影响玩法手感 */
static float rnd_game(void) { return (float)(rnd_next(&g_rng_game) >> 8) / 16777216.0f; }
static float rnd_art(void)  { return (float)(rnd_next(&g_rng_art)  >> 8) / 16777216.0f; }
static int   rnd_art_i(int n) { return n <= 1 ? 0 : (int)(rnd_next(&g_rng_art) % (unsigned)n); }
static double now_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
#endif
}
static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (long)(ms % 1000) * 1000000L; nanosleep(&ts, NULL);
#endif
}
/* 灰阶淡出: 沿 墨->暗->中->亮->纸 阶梯回退, 不引入新颜色 */
static int shade_idx(int pi, float t)
{
    float f = (float)pi + t * (float)(P_PAPER - pi);   /* 沿灰阶阶梯回退到纸色 */ int i = (int)(f + 0.5f);
    return i < 0 ? 0 : (i > P_COUNT-1 ? P_COUNT-1 : i);
}

/* 统一的按键映射表: raw code -> 逻辑键(两个后端共用) */
static int key_lookup(int raw)
{
    switch (raw) {                              /* 内部码 -> 逻辑键(两后端共用) */
    case KEY_L: case KEY_A: return K_LEFT;
    case KEY_R: case KEY_D: return K_RIGHT;
    case KEY_U: case KEY_W: return K_UP;
    case KEY_DN: case KEY_S: return K_DOWN;
    case KEY_SPACE: return K_FIRE;
    default: break;
    }
#ifdef _WIN32
    switch (raw) {                              /* Win32 虚拟键码 -> 逻辑键 */
    case VK_LEFT: return K_LEFT;  case VK_RIGHT: return K_RIGHT;
    case VK_UP: return K_UP;      case VK_DOWN: return K_DOWN;
    case VK_SPACE: return K_FIRE;
    case 'A': case 'a': return K_LEFT;  case 'D': case 'd': return K_RIGHT;
    case 'W': case 'w': return K_UP;    case 'S': case 's': return K_DOWN;
    default: break;
    }
#endif
    return -1;
}

/* =================== 3. 图形后端 =================== */
#ifdef _WIN32
static HWND g_hwnd; static HDC g_memdc; static HBITMAP g_bmp; static HBRUSH g_br[P_COUNT]; static HPEN g_pen[P_COUNT];
static int g_cur_pen = -1; static POINT s_poly[MAXPT];

/* 窗口过程: 自行处理 WM_PAINT/WM_ERASEBKGND, 否则默认背景会把画面整块擦白 */
static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND) return 1;                /* 禁止背景擦除, 防闪烁 */
    if (msg == WM_PAINT) {
        /* 必须走 BeginPaint/EndPaint: 只有这样内容才会进入 DWM 合成并真正显示在屏幕上。
           直接 GetDC+BitBlt 画到窗口在现代 Windows 上会被合成器丢弃(屏幕一片空白)。 */
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr;
        GetClientRect(h, &cr);
        if (g_memdc) BitBlt(dc, 0, 0, cr.right - cr.left, cr.bottom - cr.top, g_memdc, 0, 0, SRCCOPY);
        EndPaint(h, &ps); return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(h); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(h, msg, wp, lp);
}

static void gfx_init(void)
{
    WNDCLASSEX wc; RECT r; int i; HDC dc; int sw, sh; SetProcessDPIAware(); memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc; wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL); wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = "DoodleShooter"; RegisterClassExA(&wc);
    r.left = 0; r.top = 0; r.right = SCR_W; r.bottom = SCR_H;
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    sw = GetSystemMetrics(SM_CXSCREEN); sh = GetSystemMetrics(SM_CYSCREEN); g_hwnd = CreateWindowExA(0, "DoodleShooter", "Doodle Shooter",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, (sw - (r.right - r.left)) / 2, (sh - (r.bottom - r.top)) / 2,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    dc = GetDC(g_hwnd); g_memdc = CreateCompatibleDC(dc); g_bmp = CreateCompatibleBitmap(dc, SCR_W, SCR_H); SelectObject(g_memdc, g_bmp);
    ReleaseDC(g_hwnd, dc);
    for (i = 0; i < P_COUNT; i++) {
        COLORREF cr = RGB((PAL[i] >> 16) & 0xFF, (PAL[i] >> 8) & 0xFF, PAL[i] & 0xFF);
        g_br[i] = CreateSolidBrush(cr); g_pen[i] = CreatePen(PS_SOLID, 1, cr);
    }
}
static void gfx_use(int pi) { if (pi != g_cur_pen) { g_cur_pen = pi; SelectObject(g_memdc, g_pen[pi]); } }
static void gfx_fill(int x, int y, int w, int h, int pi)
{
    RECT rc;
    if (w <= 0 || h <= 0) return;
    rc.left = x + g_sx; rc.top = y + g_sy; rc.right = rc.left + w; rc.bottom = rc.top + h;
    SelectObject(g_memdc, g_br[pi]); FillRect(g_memdc, &rc, g_br[pi]);
}
static void gfx_point(int x, int y, int pi) { gfx_fill(x, y, 1, 1, pi); }
static void gfx_poly(const int *pts, int n, int pi)
{
    int i;
    if (n < 2 || n > MAXPT) return;
    for (i = 0; i < n; i++) { s_poly[i].x = pts[i*2] + g_sx; s_poly[i].y = pts[i*2+1] + g_sy; }
    gfx_use(pi); Polyline(g_memdc, s_poly, n);
}
/* 呈现: 走标准重绘路径。必须 InvalidateRect + UpdateWindow, 让 WM_PAINT 被真正派发,
   由窗口过程用 BeginPaint/EndPaint 完成实际绘制 —— 直接 GetDC+BitBlt 在 DWM 合成下
   不会真正显示到屏幕(症状: 窗口一片空白)。 */
static void gfx_present(void)
{
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);                /* 立即触发 WM_PAINT, 不等下一帧 */
}
static void gfx_shutdown(void)
{
    int i;
    for (i = 0; i < P_COUNT; i++) { DeleteObject(g_br[i]); DeleteObject(g_pen[i]); }
    DeleteObject(g_bmp); DeleteDC(g_memdc);
}
static int gfx_poll(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        int down, raw, k;
        if (msg.message == WM_QUIT) return 1;
        if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) {
            down = (msg.message == WM_KEYDOWN); raw = (int)msg.wParam;
            if (raw >= 'a' && raw <= 'z') raw -= 32;       /* 小写归一 */
            k = key_lookup(raw);
            if (k >= 0) g_key[k] = down;
            continue;
        }
        /* 其余消息(WM_PAINT 等)必须派发给窗口过程, 否则绘制区域永不被验证, 屏幕空白 */
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
#else /* --------------------- X11 --------------------- */
static Display *g_dpy; static Window g_win; static GC g_gc; static Pixmap g_pix;
static int g_scr, g_dep; static unsigned long g_px[P_COUNT]; static int g_last_pi = -1; static XPoint s_poly[MAXPT];
static void gfx_init(void)
{
    XSetWindowAttributes wa; Atom a; int i; g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) { fprintf(stderr, "cannot open display\n"); exit(1); }
    g_scr = DefaultScreen(g_dpy); g_dep = DefaultDepth(g_dpy, g_scr); memset(&wa, 0, sizeof wa); wa.background_pixel = WhitePixel(g_dpy, g_scr);
    wa.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask;
    g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, g_scr), 0, 0, SCR_W, SCR_H, 0, g_dep,
                          InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &wa);
    XStoreName(g_dpy, g_win, "Doodle Shooter"); a = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &a, 1); XMapWindow(g_dpy, g_win); g_gc = XCreateGC(g_dpy, g_win, 0, NULL);
    g_pix = XCreatePixmap(g_dpy, g_win, SCR_W, SCR_H, g_dep);
    for (i = 0; i < P_COUNT; i++) {
        XColor col; col.red = (unsigned short)((PAL[i] >> 16) & 0xFF); col.green = (unsigned short)((PAL[i] >> 8) & 0xFF);
        col.blue = (unsigned short)(PAL[i] & 0xFF); col.flags = DoRed | DoGreen | DoBlue;
        if (!XAllocColor(g_dpy, DefaultColormap(g_dpy, g_scr), &col))
            XAllocNamedColor(g_dpy, DefaultColormap(g_dpy, g_scr), "white", &col, NULL);
        g_px[i] = col.pixel;
    }
}
static void gfx_use(int pi)
{
    if (pi == g_last_pi) return;
    g_last_pi = pi; XSetForeground(g_dpy, g_gc, g_px[pi]);
}
static void gfx_fill(int x, int y, int w, int h, int pi)
{
    if (w <= 0 || h <= 0) return;
    gfx_use(pi); XFillRectangle(g_dpy, g_pix, g_gc, x + g_sx, y + g_sy, (unsigned)w, (unsigned)h);
}
static void gfx_point(int x, int y, int pi) { gfx_fill(x, y, 1, 1, pi); }
static void gfx_poly(const int *pts, int n, int pi)
{
    int i;
    if (n < 2 || n > MAXPT) return;
    for (i = 0; i < n; i++) {
        s_poly[i].x = (short)(pts[i*2] + g_sx); s_poly[i].y = (short)(pts[i*2+1] + g_sy);
    }
    gfx_use(pi); XDrawLines(g_dpy, g_pix, g_gc, s_poly, n, CoordModeOrigin);
}
static void gfx_present(void)
{
    XCopyArea(g_dpy, g_pix, g_win, g_gc, 0, 0, 0, 0, SCR_W, SCR_H); XFlush(g_dpy);
}
static void gfx_shutdown(void)
{
    XFreePixmap(g_dpy, g_pix); XFreeGC(g_dpy, g_gc); XDestroyWindow(g_dpy, g_win); XCloseDisplay(g_dpy);
}
static int gfx_poll(void)
{
    XEvent ev;
    while (XPending(g_dpy)) {
        XNextEvent(g_dpy, &ev);
        if (ev.type == KeyPress || ev.type == KeyRelease) {
            int down = (ev.type == KeyPress), k; KeySym ks = XLookupKeysym(&ev.xkey, 0); int raw;
            switch (ks) {                          /* X11 键码归一到统一 raw code */
            case XK_Left: raw = KEY_L; break;   case XK_Right: raw = KEY_R; break;
            case XK_Up: raw = KEY_U; break;      case XK_Down: raw = KEY_DN; break;
            case XK_space: raw = KEY_SPACE; break;
            case XK_a: raw = KEY_A; break;       case XK_d: raw = KEY_D; break;
            case XK_w: raw = KEY_W; break;       case XK_s: raw = KEY_S; break;
            default: raw = -1; break;
            }
            k = key_lookup(raw);
            if (k >= 0) g_key[k] = down;
        } else if (ev.type == ClientMessage) {
            return 1;
        }
    }
    return 0;
}
#endif

/* =================== 4. 铅笔笔触 =================== */
/* 采样点缓冲 + 断墨折线: 直线与弧线共用, 避免重复实现断笔逻辑 */
static int g_pts[MAXPT*2], g_cnt;
static void path_begin(void) { g_cnt = 0; }
static void path_add(int x, int y) { if (g_cnt < MAXPT) { g_pts[g_cnt*2] = x; g_pts[g_cnt*2+1] = y; g_cnt++; } }
static void path_draw(int pi, float brk)
{
    int i, run = -1;
    for (i = 0; i < g_cnt; i++) {
        if (rnd_art() < brk) {                            /* 断笔 */
            if (run >= 0 && i - run >= 2) gfx_poly(g_pts + run*2, i - run, pi);
            run = -1; continue;
        }
        if (run < 0) run = i;
    }
    if (run >= 0 && g_cnt - run >= 2) gfx_poly(g_pts + run*2, g_cnt - run, pi);
}
/* 抖动的直线: 沿法线随机扰动 + 正弦微弯 */
static void stroke_jitter(int x0, int y0, int x1, int y1, int pi, float amp)
{
    float dx = (float)(x1-x0), dy = (float)(y1-y0); float len = sqrtf(dx*dx + dy*dy), nx, ny, bend; int n, i;
    if (len < 0.5f) { gfx_point(x0, y0, pi); return; }
    nx = -dy/len; ny = dx/len; bend = (rnd_art()*2.0f - 1.0f) * amp * 1.4f; n = (int)(len/5.0f) + 2;
    if (n > MAXPT) n = MAXPT;
    path_begin();
    for (i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i/(float)(n-1) : 0.0f; float off = sinf(t*3.14159265f*2.0f)*bend + (rnd_art()*2.0f - 1.0f)*amp;
        path_add(x0 + (int)(dx*t + nx*off), y0 + (int)(dy*t + ny*off));
    }
    path_draw(pi, 0.07f);
}
/* 抖动的弧线(角度制), 半径随机起伏 */
static void arc_jitter(int cx, int cy, float r, int pi, float a0, float a1, float amp)
{
    float span = (a1-a0)*3.14159265f/180.0f; int n = (int)(fabsf(span)*r/5.0f) + 3, i;
    if (n > MAXPT) n = MAXPT;
    path_begin();
    for (i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i/(float)(n-1) : 0.0f; float a = a0*3.14159265f/180.0f + span*t; float rr = r + (rnd_art()*2.0f - 1.0f)*amp;
        path_add(cx + (int)(rr*cosf(a)), cy + (int)(rr*sinf(a)));
    }
    path_draw(pi, 0.08f);
}
/* 手绘圆: 两段不闭合的弧, 起始角随机 */
static void circle_sketch(int cx, int cy, float r, int pi, float amp)
{
    float s1 = rnd_art()*360.0f, s2 = s1 + 150.0f + rnd_art()*90.0f; arc_jitter(cx, cy, r, pi, s1, s1 + 185.0f + rnd_art()*35.0f, amp);
    arc_jitter(cx, cy, r, pi, s2, s2 + 170.0f + rnd_art()*40.0f, amp*0.8f);
}

/* =================== 5. 精灵与文字 =================== */
static int art_color(char ch, int jam)
{
    switch (ch) {
    case 'K': return P_INK;
    case 'W': return jam ? P_LIGHT : P_WHITE;
    case 'M': return jam ? P_DARK  : P_MID;
    case 'D': return P_DARK;
    case 'L': return P_LIGHT;
    default:  return P_PAPER;
    }
}
/* 画精灵: 每像素 1/9 概率偏移 ±1px(铅笔毛边); 同色同偏移的连续像素合并成一次填充 */
static void sprite_blit(const char *const *art, int rows, int cols,
                        int ox, int oy, int scale, int jam, int lean)
{
    int r, c;
    for (r = 0; r < rows; r++) {
        const char *line = art[r]; int sh = lean, run_start = -1, run_jx = 0, run_jy = 0; char run_ch = '.';
        for (c = 0; c <= cols; c++) {
            char ch = (c < cols) ? line[c] : '.'; int jx = 0, jy = 0;
            if (ch != '.' && ch != ' ' && rnd_art() < 0.1111f) {
                switch (rnd_art_i(4)) {
                case 0: jx = -1; break;  case 1: jx = 1; break;
                case 2: jy = -1; break;  default: jy = 1; break;
                }
            }
            if (ch != '.' && ch != ' ' && run_start >= 0 &&
                ch == run_ch && jx == run_jx && jy == run_jy) continue;
            if (run_start >= 0) {                           /* 收束横带 */
                gfx_fill(ox + run_start*scale + sh + run_jx, oy + r*scale + run_jy,
                         (c - run_start)*scale, scale, art_color(run_ch, jam));
                run_start = -1;
            }
            if (ch != '.' && ch != ' ') { run_start = c; run_jx = jx; run_jy = jy; run_ch = ch; }
        }
    }
}
static int text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }
static void draw_text(int x, int y, const char *s, int scale, int pi)
{
    int i, j, k;
    for (i = 0; s[i]; i++) {
        int idx = -1;
        for (k = 0; FONT_CHARS[k]; k++) if (FONT_CHARS[k] == s[i]) { idx = k; break; }
        if (idx < 0) continue;
        for (j = 0; j < 5; j++)
            for (k = 0; k < 7; k++)
                if (FONT[idx][j] & (1u << k))
                    gfx_fill(x + j*scale, y + k*scale, scale, scale, pi);
        x += 6 * scale;
    }
}
static void draw_text_center(int y, const char *s, int scale, int pi) { draw_text((SCR_W - text_width(s, scale))/2, y, s, scale, pi); }

/* =================== 6. 游戏逻辑 =================== */
/* 生成间隔: diff/(diff+35) 平滑收敛 0.95s -> 0.26s */
static float spawn_interval(void)
{
    float t = g_diff/(g_diff + 35.0f), iv = 0.95f - 0.69f*t; return iv < 0.26f ? 0.26f : iv;
}
static void spawn_enemy(void)
{
    int i, slot = -1, jam; Enemy *e; float base;
    for (i = 0; i < MAX_ENEMY; i++) if (!g_en[i].alive) { slot = i; break; }
    if (slot < 0) return;
    jam = (g_spawn_n % 6 == 5);            /* 每 6 只出 1 只干扰机 */ g_spawn_n++; e = &g_en[slot]; memset(e, 0, sizeof *e);
    base = 1.15f + g_diff*0.005f;          /* 水平速度随时间轻微提升 */
    if (base > 2.90f) base = 2.90f;        /* 并设上限 */
    e->alive = 1; e->jam = jam; e->scale = jam ? 4 : 3; e->hp = jam ? 3 : 1; e->spd = jam ? base*0.55f : base;     /* 干扰机更慢 */
    e->x = (float)(SCR_W + 40) + rnd_game()*30.0f; e->base_y = 60.0f + rnd_game()*(float)(SCR_H - 170);
    e->y = e->base_y; e->phase = rnd_game()*6.2831853f;
}
static void fire_player(void)
{
    int i;
    for (i = 0; i < MAX_BULLET; i++) {
        if (!g_bul[i].alive) {
            g_bul[i].alive = 1; g_bul[i].x = g_px; g_bul[i].y = g_py - 26.0f; g_bul[i].vx = 0.0f; g_bul[i].vy = -6.5f; return;
        }
    }
}
static void fire_enemy(Enemy *e)
{
    int i; float ang, sp;
    for (i = 0; i < MAX_EBULLET; i++) {
        if (!g_ebul[i].alive) {
            ang = atan2f(g_py - e->y, g_px - e->x) + (rnd_game()*2.0f - 1.0f)*0.42f; sp = 2.1f + rnd_game()*0.5f; g_ebul[i].alive = 1;
            g_ebul[i].x = e->x - (float)(e->scale*17)*0.5f; g_ebul[i].y = e->y; g_ebul[i].vx = cosf(ang)*sp; g_ebul[i].vy = sinf(ang)*sp; return;
        }
    }
}
static void spawn_debris(float x, float y, int n, int pi)
{
    int i, k = 0;
    for (i = 0; i < MAX_DEBRIS && k < n; i++) {
        Debris *d = &g_deb[i];
        if (d->alive) continue;
        d->alive = 1; d->x = x; d->y = y; d->pi = pi; d->vx = (rnd_game()*2.0f - 1.0f)*2.6f; d->vy = (rnd_game()*2.0f - 1.0f)*2.2f - 0.6f;
        d->ang = rnd_game()*6.2831853f; d->spin = (rnd_game()*2.0f - 1.0f)*0.35f; d->maxlife = 22 + (int)(rnd_game()*22.0f); d->life = d->maxlife;
        d->len = 2 + (int)(rnd_game()*3.0f); k++;
    }
}
static void spawn_ring(float x, float y, float r)
{
    int i;
    for (i = 0; i < MAX_RING; i++) {
        if (g_ring[i].alive) continue;
        g_ring[i].alive = 1; g_ring[i].x = x; g_ring[i].y = y; g_ring[i].r0 = r; g_ring[i].maxlife = 26; g_ring[i].life = g_ring[i].maxlife; return;
    }
}
/* 圆-矩形近似判定 */
static int circle_hit_rect(float cx, float cy, float r, int rx, int ry, int rw, int rh)
{
    float nx = cx, ny = cy;
    if (cx < (float)rx) nx = (float)rx;
    else if (cx > (float)(rx+rw)) nx = (float)(rx+rw);
    if (cy < (float)ry) ny = (float)ry;
    else if (cy > (float)(ry+rh)) ny = (float)(ry+rh);
    return (cx-nx)*(cx-nx) + (cy-ny)*(cy-ny) <= r*r;
}
static void kill_enemy(Enemy *e)
{
    e->alive = 0; g_score += e->jam ? 30 : 10; spawn_debris(e->x, e->y, 8 + (int)(rnd_game()*9.0f), e->jam ? P_DARK : P_INK);
    spawn_ring(e->x, e->y, 6.0f + (float)(e->scale*4));
}
static void player_hit(void)
{
    g_deaths++; g_state = ST_OVER; g_over_t = 0; g_shake_t = 8;     /* 震屏 8 帧 */ spawn_debris(g_px, g_py, 16, P_INK);
    spawn_ring(g_px, g_py, 8.0f);
    if (g_score > g_high) g_high = g_score;
}
static void reset_game(void)
{
    int i;
    for (i = 0; i < MAX_BULLET; i++)  g_bul[i].alive = 0;
    for (i = 0; i < MAX_EBULLET; i++) g_ebul[i].alive = 0;
    for (i = 0; i < MAX_ENEMY; i++)   g_en[i].alive = 0;
    for (i = 0; i < MAX_DEBRIS; i++)  g_deb[i].alive = 0;
    for (i = 0; i < MAX_RING; i++)    g_ring[i].alive = 0;
    g_px = SCR_W*0.5f; g_py = (float)(SCR_H-70); g_pvx = g_pvy = 0.0f; g_plean = 0; g_pinv = 80;                        /* 开场 80 帧无敌 */
    g_score = 0; g_diff = 0.0f; g_frame = 0; g_spawn_cd = 40; g_spawn_n = 0; g_shake_t = 0; g_state = ST_PLAY;
}
static void update_player(void)
{
    float ax = 0.0f, ay = 0.0f;
    if (g_key[K_LEFT])  ax -= 1.0f;
    if (g_key[K_RIGHT]) ax += 1.0f;
    if (g_key[K_UP])    ay -= 1.0f;
    if (g_key[K_DOWN])  ay += 1.0f;
    if (ax != 0.0f && ay != 0.0f) { ax *= 0.7071f; ay *= 0.7071f; }
    g_pvx += ax*0.62f;                               /* 加速度 */ g_pvy += ay*0.62f;
    g_pvx *= 0.85f;  g_pvy *= 0.85f;                 /* 0.85 摩擦阻尼 */ g_px += g_pvx; g_py += g_pvy;
    if (g_px < 18.0f) { g_px = 18.0f; g_pvx = 0.0f; }
    if (g_px > (float)(SCR_W-18)) { g_px = (float)(SCR_W-18); g_pvx = 0.0f; }
    if (g_py < 52.0f) { g_py = 52.0f; g_pvy = 0.0f; }
    if (g_py > (float)(SCR_H-30)) { g_py = (float)(SCR_H-30); g_pvy = 0.0f; }
    if (g_pvx > 0.35f)       g_plean = 1;             /* 左右移动时机身 1px 侧倾 */
    else if (g_pvx < -0.35f) g_plean = -1;
    else                     g_plean = 0;
    if (g_pinv > 0) g_pinv--;
    if ((g_frame % 7) == 0) fire_player();            /* 每 7 帧一发 */
}
static void update_enemies(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &g_en[i];
        if (!e->alive) continue;
        e->x -= e->spd; e->y = e->base_y + sinf((float)g_frame*0.05f + e->phase)*20.0f;  /* 正弦浮动 */
        if (e->x < -60.0f) { e->alive = 0; continue; }
        if (e->jam && rnd_game() < 0.006f) fire_enemy(e);
    }
}
/* 两类子弹共用同一套积分与越界回收 */
static void move_bullets(Bullet *arr, int n, int is_enemy)
{
    int i;
    for (i = 0; i < n; i++) {
        Bullet *b = &arr[i];
        if (!b->alive) continue;
        b->x += b->vx; b->y += b->vy;
        if (b->y < -20.0f || b->x < -20.0f || b->x > (float)(SCR_W+20) ||
            (is_enemy && b->y > (float)(SCR_H+20))) b->alive = 0;
    }
}
static void update_bullets(void)
{
    move_bullets(g_bul, MAX_BULLET, 0); move_bullets(g_ebul, MAX_EBULLET, 1);
}
static void update_fx(void)
{
    int i;
    for (i = 0; i < MAX_DEBRIS; i++) {
        Debris *d = &g_deb[i];
        if (!d->alive) continue;
        d->x += d->vx; d->y += d->vy; d->vx *= 0.965f; d->vy *= 0.965f; d->ang += d->spin;
        if (--d->life <= 0) d->alive = 0;
    }
    for (i = 0; i < MAX_RING; i++) {
        Ring *rg = &g_ring[i];
        if (rg->alive && --rg->life <= 0) rg->alive = 0;
    }
}
static void update_collisions(void)
{
    int i, j;
    for (i = 0; i < MAX_BULLET; i++) {                 /* 玩家子弹 -> 敌人 */
        Bullet *b = &g_bul[i];
        if (!b->alive) continue;
        for (j = 0; j < MAX_ENEMY; j++) {
            Enemy *e = &g_en[j]; int w = 17*e->scale, h = 13*e->scale;
            if (!e->alive) continue;
            if (circle_hit_rect(b->x, b->y, 4.0f, (int)e->x-w/2, (int)e->y-h/2, w, h)) {
                b->alive = 0; e->hp--;
                if (e->hp <= 0) kill_enemy(e);
                break;
            }
        }
    }
    if (g_pinv > 0) return;                            /* 无敌期间免疫 */
    for (j = 0; j < MAX_ENEMY; j++) {                  /* 敌人 -> 玩家 */
        Enemy *e = &g_en[j]; int w = 17*e->scale, h = 13*e->scale;
        if (!e->alive) continue;
        if (circle_hit_rect(g_px, g_py-3.0f, 8.0f, (int)e->x-w/2, (int)e->y-h/2, w, h)) {
            e->alive = 0; player_hit(); return;
        }
    }
    for (i = 0; i < MAX_EBULLET; i++) {                /* 敌弹 -> 玩家 */
        Bullet *b = &g_ebul[i];
        if (!b->alive) continue;
        if (circle_hit_rect(b->x, b->y, 4.0f, (int)g_px-12, (int)g_py-16, 24, 32)) {
            b->alive = 0; player_hit(); return;
        }
    }
}
static void tick(void)
{
    g_frame++;
    if (g_state == ST_TITLE) {                        /* 标题: 飞机悬停 */
        g_px = SCR_W*0.5f + sinf((float)g_frame*0.04f)*40.0f; g_py = (float)(SCR_H*0.55f) + sinf((float)g_frame*0.07f)*6.0f;
        g_plean = (sinf((float)g_frame*0.04f) > 0.0f) ? 1 : -1;
        if (g_key[K_FIRE]) { g_key[K_FIRE] = 0; reset_game(); }
        return;
    }
    if (g_state == ST_PLAY) {
        g_diff += 0.045f;                              /* 难度随时间增长 */
        if (--g_spawn_cd <= 0) {
            spawn_enemy(); g_spawn_cd = (int)(spawn_interval()*(float)TPS);
        }
        update_player(); update_enemies(); update_bullets(); update_fx(); update_collisions();
        if (g_score > g_high) g_high = g_score;        /* 最高分本局内保留 */
    } else {                                          /* 失败: 2 秒后可重开 */
        g_over_t++; update_bullets(); update_fx();
        if (g_over_t > 120 && g_key[K_FIRE]) { g_key[K_FIRE] = 0; reset_game(); }
    }
}

/* =================== 7. 绘制 =================== */
static void draw_clouds(void)
{
    static const float off[5][3] = {
        {-1.55f,0.10f,0.80f},{-0.70f,-0.35f,1.00f},{0.20f,-0.52f,1.12f},
        {1.05f,-0.18f,0.86f},{1.55f,0.16f,0.62f}
    };
    int i, k;
    for (i = 0; i < NCLOUD; i++) {
        Cloud *c = &g_cloud[i];
        if (!c->alive) continue;
        for (k = 0; k < 5; k++)                          /* 多个抖动圆拼成云 */
            circle_sketch((int)(c->x + off[k][0]*c->r), (int)(c->y + off[k][1]*c->r),
                          off[k][2]*c->r, P_LIGHT, 1.3f);
        stroke_jitter((int)(c->x-1.7f*c->r), (int)(c->y+0.15f*c->r),
                      (int)(c->x+1.7f*c->r), (int)(c->y+0.12f*c->r), P_LIGHT, 1.1f);
    }
}
static void draw_debris(void)
{
    int i;
    for (i = 0; i < MAX_DEBRIS; i++) {
        Debris *d = &g_deb[i]; float t, ex, ey; int pi;
        if (!d->alive) continue;
        t = 1.0f - (float)d->life/(float)d->maxlife; pi = shade_idx(d->pi, t*0.95f); ex = d->x + cosf(d->ang)*(float)d->len;         /* 随机旋转的小横线 */
        ey = d->y + sinf(d->ang)*(float)d->len; stroke_jitter((int)d->x, (int)d->y, (int)ex, (int)ey, pi, 0.8f);
        if (rnd_art() < 0.35f) gfx_point((int)d->x, (int)d->y, pi);
    }
}
static void draw_rings(void)
{
    int i, k;
    for (i = 0; i < MAX_RING; i++) {
        Ring *rg = &g_ring[i]; float t, grow; int pi;
        if (!rg->alive) continue;
        t = 1.0f - (float)rg->life/(float)rg->maxlife; grow = t*34.0f; pi = shade_idx(P_INK, t*0.9f);
        for (k = 0; k < 3; k++) {                       /* 3 层不同半径的铅笔圈 */
            float r = rg->r0 + grow + (float)k*7.0f; circle_sketch((int)rg->x, (int)rg->y, r, pi, 1.4f + (float)k*0.3f);
        }
    }
}
static void draw_bullets(void)
{
    int i;
    for (i = 0; i < MAX_BULLET; i++) {                 /* 玩家子弹: 胶囊 + 极短尾迹 */
        Bullet *b = &g_bul[i];
        if (!b->alive) continue;
        gfx_fill((int)b->x-2, (int)b->y+11, 4, 3, P_LIGHT); gfx_fill((int)b->x-1, (int)b->y+14, 2, 2, P_MID);
        sprite_blit(ART_BULLET, 9, 5, (int)b->x-5, (int)b->y-9, 2, 0, 0);
    }
    for (i = 0; i < MAX_EBULLET; i++) {                /* 敌弹: 手绘小圆点 */
        Bullet *b = &g_ebul[i];
        if (!b->alive) continue;
        circle_sketch((int)b->x, (int)b->y, 3.2f, P_DARK, 0.9f);
    }
}
static void draw_enemies(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &g_en[i]; int w, h;
        if (!e->alive) continue;
        w = 17*e->scale; h = 13*e->scale; sprite_blit(ART_ENEMY, 13, 17, (int)e->x - w/2, (int)e->y - h/2, e->scale, e->jam, 0);
    }
}
static void draw_player(void)
{
    int w = 13*SPR, h = 18*SPR;
    if (g_state == ST_PLAY && g_pinv > 0 && ((g_frame/3) & 1)) return;   /* 无敌闪烁 */
    sprite_blit(ART_PLANE, 18, 13, (int)g_px - w/2, (int)g_py - h/2, SPR, 0, g_plean);
}
/* HUD 与状态文字: 都不参与震屏 */
static void draw_ui(void)
{
    char buf[64]; int sx = g_sx, sy = g_sy; g_sx = 0; g_sy = 0; snprintf(buf, sizeof buf, "SCORE %06d", g_score);
    draw_text(14, 12, buf, 2, P_INK); snprintf(buf, sizeof buf, "HI %06d", g_high);
    draw_text(SCR_W - text_width(buf, 2) - 14, 12, buf, 2, P_DARK); stroke_jitter(12, 34, SCR_W-12, 34, P_LIGHT, 1.0f);
    if (g_state == ST_TITLE) {
        draw_text_center(96, "DOODLE AIR RAID", 3, P_INK); draw_text_center(140, "PAPER SKY - INK PILOT", 2, P_MID);
        if ((g_frame/22) & 1) draw_text_center(300, "PRESS SPACE", 3, P_INK);
        draw_text_center(344, "ARROWS OR WASD - AUTO FIRE", 1, P_DARK);
    } else if (g_state == ST_OVER) {
        draw_text_center(150, "GAME OVER", 3, P_INK); snprintf(buf, sizeof buf, "SCORE %d", g_score); draw_text_center(200, buf, 2, P_MID);
        if (g_over_t > 120 && ((g_frame/20) & 1)) draw_text_center(250, "PRESS SPACE", 2, P_DARK);
    }
    g_sx = sx; g_sy = sy;
}
static void render(void)
{
    int i; gfx_fill(-16, -16, SCR_W+32, SCR_H+32, P_PAPER);
    for (i = 0; i < NCLOUD; i++) {                     /* 云: 缓慢上飘 + 轻微左移 */
        Cloud *c = &g_cloud[i]; c->y -= 0.055f; c->x -= 0.018f;
        if (c->y < -50.0f) { c->y = (float)(SCR_H+40); c->x = rnd_game()*(float)SCR_W; }
        if (c->x < -90.0f) c->x = (float)(SCR_W+60);
    }
    draw_clouds(); stroke_jitter(6, 42, SCR_W-6, 42, P_LIGHT, 1.2f); stroke_jitter(6, SCR_H-6, SCR_W-6, SCR_H-6, P_LIGHT, 1.2f);
    draw_rings(); draw_enemies(); draw_bullets(); draw_debris(); draw_player(); draw_ui();
}

/* =================== 8. 自检 / 模拟 / 预览 =================== */
static int g_checks, g_fails; static void check(int cond, const char *what)
{
    g_checks++;
    if (!cond) { g_fails++; printf("  FAIL: %s\n", what); }
}
static int art_row_ok(const char *const *art, int rows, int cols, const char *name)
{
    int r, ok = 1;
    for (r = 0; r < rows; r++)
        if ((int)strlen(art[r]) != cols) {
            printf("  FAIL: %s row %d width %d != %d\n", name, r, (int)strlen(art[r]), cols); ok = 0;
        }
    return ok;
}
/* 统计数组中 alive 的个数, 用于验证容量上限 */
static int count_alive(const void *arr, int n, size_t step, size_t off)
{ int i, c = 0; for (i = 0; i < n; i++) if (*(const int *)((const char *)arr + (size_t)i*step + off)) c++; return c; }
static int run_selftest(void)
{
    int i, iv; float prev, cur; printf("doodle_shooter selftest\n");
    /* 精灵字符画尺寸: 行列必须严格一致 */
    check(art_row_ok(ART_PLANE, 18, 13, "plane"), "plane art 13x18"); check(art_row_ok(ART_ENEMY, 13, 17, "enemy"), "enemy art 17x13");
    check(art_row_ok(ART_BULLET, 9, 5, "bullet"), "bullet art 5x9");
    check((int)strlen(FONT_CHARS) == 43 && sizeof(FONT)/sizeof(FONT[0]) == 43, "font table size 43");
    /* 调色板亮度必须严格递增(保证素描层次) */
    for (i = 1; i < P_COUNT; i++)
        check(((PAL[i-1] >> 16) & 0xFF) < ((PAL[i] >> 16) & 0xFF), "palette luminance order");
    /* 圆-矩形碰撞边界 */
    check(circle_hit_rect(10, 10, 5, 0, 0, 20, 20) && !circle_hit_rect(100, 100, 5, 0, 0, 20, 20),
          "circle inside/outside rect");
    check(circle_hit_rect(23, 10, 5, 0, 0, 20, 20) && !circle_hit_rect(27, 10, 5, 0, 0, 20, 20),
          "circle edge boundary");
    /* 按键映射: 每个逻辑键都必须能被至少一种原始键码触发(防止映射表断裂) */
    check(key_lookup(KEY_L) == K_LEFT && key_lookup(KEY_R) == K_RIGHT &&
          key_lookup(KEY_U) == K_UP && key_lookup(KEY_DN) == K_DOWN &&
          key_lookup(KEY_SPACE) == K_FIRE, "key_lookup maps all five logical keys");
    check(key_lookup(KEY_A) == K_LEFT && key_lookup(KEY_D) == K_RIGHT &&
          key_lookup(KEY_W) == K_UP && key_lookup(KEY_S) == K_DOWN, "key_lookup maps WASD aliases");
    check(key_lookup(0xFFFF) == -1, "key_lookup rejects unknown codes");
#ifdef _WIN32
    /* Win32 后端: 虚拟键码必须先归一到内部码, 否则方向键/空格全部失效 */
    check(key_lookup(VK_LEFT) == K_LEFT && key_lookup(VK_RIGHT) == K_RIGHT &&
          key_lookup(VK_UP) == K_UP && key_lookup(VK_DOWN) == K_DOWN &&
          key_lookup(VK_SPACE) == K_FIRE, "VK codes map to logical keys");
    check(key_lookup('A') == K_LEFT && key_lookup('D') == K_RIGHT &&
          key_lookup('W') == K_UP && key_lookup('S') == K_DOWN, "VK letters map to logical keys");
#endif
    /* 难度曲线: 单调收敛到 0.26s */
    g_diff = 0.0f; prev = spawn_interval(); check(fabsf(prev - 0.95f) < 1e-4f, "spawn interval starts at 0.95s");
    for (i = 0; i < 400; i++) {
        g_diff += 10.0f; cur = spawn_interval(); check(cur <= prev + 1e-6f, "spawn interval monotonically decreasing");
        check(cur >= 0.26f - 1e-6f, "spawn interval >= 0.26s"); prev = cur;
    }
    /* 容量上限: 反复生成也不得越界 */
    g_diff = 0.0f; g_spawn_n = 0;
    for (i = 0; i < 5000; i++) spawn_enemy();
    check(count_alive(g_en, MAX_ENEMY, sizeof(Enemy), offsetof(Enemy, alive)) <= MAX_ENEMY, "enemy capacity respected");
    for (i = 0; i < 5000; i++) fire_player();
    check(count_alive(g_bul, MAX_BULLET, sizeof(Bullet), offsetof(Bullet, alive)) <= MAX_BULLET, "bullet capacity respected");
    for (i = 0; i < 5000; i++) spawn_debris(320, 240, 16, P_INK);
    check(count_alive(g_deb, MAX_DEBRIS, sizeof(Debris), offsetof(Debris, alive)) <= MAX_DEBRIS, "debris capacity respected");
    /* 淡出阶梯不得越界 */
    for (i = 0; i < P_COUNT; i++) {
        iv = shade_idx(i, 0.0f); check(iv >= 0 && iv < P_COUNT, "shade index in range");
        iv = shade_idx(i, 1.0f); check(iv >= 0 && iv < P_COUNT, "shade index in range (t=1)");
    }
    printf("  checks=%d fails=%d -> %s\n", g_checks, g_fails,
           g_fails == 0 ? "SELFTEST OK" : "SELFTEST FAILED");
    return g_fails == 0 ? 0 : 1;
}
static int run_sim(void)
{
    int game, f, i, total = 0, best = 0; unsigned seed = 0xC0FFEEu; printf("doodle_shooter headless sim (20 games x 1800 ticks)\n");
    g_deaths = 0;
    for (game = 0; game < 20; game++) {
        memset(g_key, 0, sizeof g_key); reset_game();
        for (f = 0; f < 1800; f++) {
            for (i = 0; i < K_COUNT; i++)              /* 随机输入 */
                g_key[i] = (int)((rnd_next(&seed) >> 7) & 3u) ? 1 : 0;
            tick();
            if (g_state == ST_OVER && g_over_t > 130) g_key[K_FIRE] = 1;
            if (g_px < 0.0f || g_px > (float)SCR_W || g_py < 0.0f || g_py > (float)SCR_H) {
                printf("  FAIL: player out of bounds at tick %d\n", f); return 1;
            }
            if (g_score < 0) { printf("  FAIL: negative score\n"); return 1; }
        }
        total += g_score;
        if (g_score > best) best = g_score;
    }
    printf("  games=20 deaths=%d avg_score=%d best=%d -> SIM OK\n", g_deaths, total/20, best); return 0;
}
static void print_art(const char *title, const char *const *art, int rows)
{ int r; printf("== %s ==\n", title); for (r = 0; r < rows; r++) printf("  %s\n", art[r]); }
static int run_preview(void)
{
    int i, c, r; print_art("plane 13x18", ART_PLANE, 18); print_art("enemy 17x13", ART_ENEMY, 13); print_art("bullet 5x9", ART_BULLET, 9);
    printf("== font sample ==\n");
    for (r = 0; r < 7; r++) {
        for (i = 0; i < 43; i++) {
            for (c = 0; c < 5; c++) printf("%c", (FONT[i][c] >> r) & 1u ? '#' : '.');
            printf(" ");
        }
        printf("\n");
    }
    return 0;
}

/* =================== 9. 主循环 =================== */
enum { M_GAME = 0, M_SELFTEST, M_SIM, M_PREVIEW };
static int mode_from_str(const char *s)
{
    if (!s) return M_GAME;
    if (strstr(s, "--selftest")) return M_SELFTEST;
    if (strstr(s, "--sim")) return M_SIM;
    if (strstr(s, "--preview")) return M_PREVIEW;
    return M_GAME;
}
static int mode_from_args(int argc, char **argv)
{
    int i, m;
    for (i = 1; i < argc; i++) { m = mode_from_str(argv[i]); if (m != M_GAME) return m; }
    return M_GAME;
}
static int app_main(int mode)
{
    int i; double next;
    if (mode == M_SELFTEST) return run_selftest();
    if (mode == M_SIM) return run_sim();
    if (mode == M_PREVIEW) return run_preview();
    for (i = 0; i < NCLOUD; i++) {                    /* 初始云分布 */
        g_cloud[i].alive = 1; g_cloud[i].x = rnd_game()*(float)SCR_W; g_cloud[i].y = rnd_game()*(float)SCR_H;
        g_cloud[i].r = 24.0f + rnd_game()*22.0f;
    }
    g_state = ST_TITLE; g_px = SCR_W*0.5f; g_py = (float)(SCR_H*0.55f); g_high = 0; gfx_init(); next = now_ms();
    for (;;) {
        if (gfx_poll()) break;
        tick();
        if (g_shake_t > 0) {                           /* 震屏 */
            g_shake_t--; g_sx = (int)(rnd_game()*7.0f) - 3; g_sy = (int)(rnd_game()*7.0f) - 3;
        } else { g_sx = 0; g_sy = 0; }
        render(); gfx_present(); next += 1000.0/(double)TPS;                    /* 固定步长 ~60FPS */
        {
            double now = now_ms();
            if (next < now) next = now;
            sleep_ms((int)(next - now));
        }
    }
    gfx_shutdown(); return 0;
}
#ifdef _WIN32
int main(int argc, char **argv) { return app_main(mode_from_args(argc, argv)); }
/* -mwindows 入口: 附加父控制台, 让 --selftest 等模式仍有输出 */
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cl, int cs)
{
    int mode; (void)hi; (void)hp; (void)cs; mode = mode_from_str(cl ? cl : "");
    if (mode != M_GAME && AttachConsole(ATTACH_PARENT_PROCESS)) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h != INVALID_HANDLE_VALUE) SetStdHandle(STD_OUTPUT_HANDLE, h);
        h = GetStdHandle(STD_ERROR_HANDLE);
        if (h != INVALID_HANDLE_VALUE) SetStdHandle(STD_ERROR_HANDLE, h);
    }
    return app_main(mode);
}
#else
int main(int argc, char **argv) { return app_main(mode_from_args(argc, argv)); }
#endif
