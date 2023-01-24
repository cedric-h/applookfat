// vim: sw=2 ts=2 expandtab smartindent

#include <stdint.h>

#if 0
#define dbg puts
#define dbgf printf
#else
#define dbg(...)
#define dbgf(...)
#endif

uint32_t uleb_decode(uint8_t *bytes, int *i) {
  uint32_t result = 0;
  int shift = 0;
  while (1) {
    uint8_t byte = bytes[*i];
    *i = *i + 1;

    result |= (byte & ~(1 << 7)) << shift;
    if (((1 << 7) & byte) == 0)
      break;
    shift += 7;
  }
  return result;
}

typedef enum {
  NameSecIterStage_Init,
  NameSecIterStage_Body,
} NameSecIterStage;
typedef struct {
  NameSecIterStage stage;
  uint8_t *bytes, *name_ptr;
  int end_i,
      sub_size, sub_i,
      fn_idx, name_i, name_size;
} NameSecIter;
static int name_sec_iter(NameSecIter *nsi) {
  switch (nsi->stage) {
    case (NameSecIterStage_Init): {
      nsi->end_i = nsi->sub_i + nsi->sub_size;

      int fn_count = uleb_decode(nsi->bytes, &nsi->sub_i);
      (void)fn_count;

      dbg("name sec: fn subsec!");
      dbgf("fn count: %d\n", fn_count);

      nsi->stage = NameSecIterStage_Body;
      /* fallthrough */
    };

    case (NameSecIterStage_Body): {
      nsi->fn_idx = uleb_decode(nsi->bytes, &nsi->sub_i);
      nsi->name_size = uleb_decode(nsi->bytes, &nsi->sub_i);

      nsi->name_i = nsi->sub_i;
      dbgf("func[%d]: <%.*s>\n",
          nsi->fn_idx, nsi->name_size,
          nsi->bytes + nsi->name_i);

      nsi->sub_i += nsi->name_size;

      return nsi->sub_i < nsi->end_i;
    } break;
  }
}

static int custom_sec_find_name(
  uint8_t *bytes, int sec_size, int sec_i,
  NameSecIter *out
) {
  int end_i = sec_i + sec_size;

  int name_len = uleb_decode(bytes, &sec_i);
  if (name_len != 4) return 0;

  if (bytes[sec_i++] != 'n' ||
      bytes[sec_i++] != 'a' ||
      bytes[sec_i++] != 'm' ||
      bytes[sec_i++] != 'e' )
    return 0;

  dbg("name sec!");

  while (sec_i < end_i) {
    int sub_id = bytes[sec_i++];
    int sub_size = uleb_decode(bytes, &sec_i);

         if (sub_id == 0) dbg("name sec: module subsec!");
    else if (sub_id == 1) {
      *out = (NameSecIter) {
        .bytes = bytes,
        .sub_size = sub_size,
        .sub_i = sec_i,
      };
      return 1;
    }
    else if (sub_id == 2) dbg("name sec: local subsec!");
    else dbgf("unknown name sec: %d\n", sub_id);

    sec_i += sub_size;
  }

  return 0;
}

typedef enum {
  CodeSecStage_Init,
  CodeSecStage_Body,
} CodeSecStage;
typedef struct {
  CodeSecStage stage;
  uint8_t *bytes;
  int sec_size, end_i, sec_i;

  int fn_count, fn_i, fn_size;
} CodeSecIter;

static uint8_t code_sec_iter(CodeSecIter *csi) {
  switch (csi->stage) {
    case (CodeSecStage_Init): {
      csi->end_i = csi->sec_i + csi->sec_size;

      csi->fn_count = uleb_decode(csi->bytes, &csi->sec_i);
      dbgf("count: %d\n", csi->fn_count);

      csi->stage = CodeSecStage_Body;

      csi->fn_i = -1;
      /* fallthrough */
    };

    case (CodeSecStage_Body): {
      csi->fn_i++;
      csi->fn_size = uleb_decode(csi->bytes, &csi->sec_i);
      dbgf("code subsec size: %d\n", csi->fn_size);

      csi->sec_i += csi->fn_size;

      return csi->sec_i < csi->end_i;
    } break;
  }
}

typedef enum {
  SecId_Custom    =  0,
  SecId_Type      =  1,
  SecId_Import    =  2,
  SecId_Function  =  3,
  SecId_Table     =  4,
  SecId_Memory    =  5,
  SecId_Global    =  6,
  SecId_Export    =  7,
  SecId_Start     =  8,
  SecId_Element   =  9,
  SecId_Code      = 10,
  SecId_Data      = 11,
  SecId_DataCount = 12,
  SecId_MAX,
} SecId;

static char *module_sec_name[] = {
  [SecId_Custom   ] = "Custom"   ,
  [SecId_Type     ] = "Type"     ,
  [SecId_Import   ] = "Import"   ,
  [SecId_Function ] = "Function" ,
  [SecId_Table    ] = "Table"    ,
  [SecId_Memory   ] = "Memory"   ,
  [SecId_Global   ] = "Global"   ,
  [SecId_Export   ] = "Export"   ,
  [SecId_Start    ] = "Start"    ,
  [SecId_Element  ] = "Element"  ,
  [SecId_Code     ] = "Code"     ,
  [SecId_Data     ] = "Data"     ,
  [SecId_DataCount] = "DataCount",
};

typedef enum {
  WasmIterStage_Init,
  WasmIterStage_Body,
} WasmIterStage;
typedef struct {
  WasmIterStage stage;
  uint8_t *bytes;
  int file_i, file_size,
       sec_i,  sec_size, sec_id;
} WasmIter;

int wasm_iter(WasmIter *wmi) {
  uint8_t *bytes = wmi->bytes;

  switch (wmi->stage) {

    case (WasmIterStage_Init): {
      if (bytes[wmi->file_i++] != 0x00 ||
          bytes[wmi->file_i++] != 0x61 ||
          bytes[wmi->file_i++] != 0x73 ||
          bytes[wmi->file_i++] != 0x6d)
        dbg("nomag");

      if (bytes[wmi->file_i++] != 0x01 ||
          bytes[wmi->file_i++] != 0x00 ||
          bytes[wmi->file_i++] != 0x00 ||
          bytes[wmi->file_i++] != 0x00)
        dbg("unknown wasm version");

      wmi->stage = WasmIterStage_Body;

      /* fallthrough */
    };

    case (WasmIterStage_Body): {
      wmi->sec_id = bytes[wmi->file_i++];
      wmi->sec_size = uleb_decode(bytes, &wmi->file_i);

      dbgf("id: %d, size: %d(0x%x)\n",
          wmi->sec_id, wmi->sec_size, wmi->sec_size);
      wmi->sec_i = wmi->file_i;
      
      dbgf("0x%x <-> 0x%x\n",
          wmi->sec_i, wmi->file_i + wmi->sec_size);

      wmi->file_i += wmi->sec_size;
      dbg("");

      return wmi->file_i != wmi->file_size;
    }
  }
}

/* I feel somewhat bad for looping through the entire module every time
 * I want a function name, but you can put this entire thing in front of a
 * cachetable anyway soooooo lmao
 *
 * "I start by doing everything from scratch every frame, then profile if it's slow"
 *    - Ryan Fleury */
typedef struct { char *name; int len; } FnName;
static FnName fn_name_get(uint8_t *bytes, int file_size, int fn_idx) {
  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  NameSecIter nsi = {0};

  while (wasm_iter(&wmi)) {
    if (wmi.sec_id == SecId_Custom) {
      if (!custom_sec_find_name(wmi.bytes, wmi.sec_size, wmi.sec_i, &nsi))
        continue;

      while (name_sec_iter(&nsi)) {
        if (nsi.fn_idx == fn_idx)
          return (FnName) {
            .name = (char *)(bytes + nsi.name_i),
            .len = nsi.name_size
          };
      }
    }
  }

  return (FnName) {
    .name =        "???",
    .len  = sizeof("???")-1,
  };
}

#ifdef __wasm__
extern uint8_t __heap_base;

#define NULL 0
#define memset __builtin_memset
#define memcpy __builtin_memcpy
#define ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

typedef struct { float r, g, b, a; } Clr;
typedef struct { float x, y; float u, v; Clr color; } Vert;
#define CLR_RED     ((Clr) { 1.0f, 0.0f, 0.0f, 1.0f })
#define CLR_MAGENTA ((Clr) { 1.0f, 0.0f, 1.0f, 1.0f })
#define CLR_WHITE   ((Clr) { 1.0f, 1.0f, 1.0f, 1.0f })
#define CHAR_WH_RATIO (0.54)
#define UI_SCALE (1.5)
#define UI_TEXT_SIZE (12)
#define UI_TEXT_PAD  (5)
/* spritesheet */
extern void ss_font(int text_px, int text_pad);
/* triangle pusher (gl) */
extern float gl_vbuf(Vert     *vbuf, int len);
extern float gl_ibuf(uint16_t *ibuf, int len);
/* maffs */
#define M_PI 3.141592653589793
extern float cosf(float);
extern float sinf(float);
extern float fmodf(float, float);
static float rads_dist(float a, float b) {
  float difference = fmodf(b - a, M_PI*2.0f),
        distance = fmodf(2.0f * difference, M_PI*2.0f) - difference;
  return distance;
}
static float rads_lerp(float a, float b, float t) {
  float difference = fmodf(b - a, M_PI*2.0f),
        distance = fmodf(2.0f * difference, M_PI*2.0f) - difference;
  return a + distance * t;
}
/* dbg */
extern void abort();
extern void eputs(char *str, int len);

static struct {
  Vert     vbuf[1 << 13];
  uint16_t ibuf[1 << 13];
  int vrt_i;
  int idx_i;
} state = {0};

void mouse(int mode, float x, float y) { }
void key(int mode, char code) { }
void wheel(int mode, float x, float y, float dy) { }

static void draw_ngon(
  float radius,
  float start_rads, float end_rads,
  Clr c,
  float x, float y
) {
  Vert     *vbuf = state.vbuf + state.vrt_i;
  uint16_t *ibuf = state.ibuf + state.idx_i;

  float cx      = UI_TEXT_PAD;
  float cy      = UI_TEXT_PAD;
  float csize_x = UI_TEXT_SIZE;
  float csize_y = UI_TEXT_SIZE;

  *vbuf++ = (Vert) { .x =  x              ,
                     .y =  y              ,
                     .u = cx + csize_x*0.0,
                     .v = cy + csize_y*1.0,
                     .color = c };
  uint16_t idx_center = (vbuf - state.vbuf)-1;

  {
    float bx = cosf(start_rads) * radius;
    float by = sinf(start_rads) * radius;
    *vbuf++ = (Vert) { .x =  x +      bx*1.0,
                       .y =  y +      by*1.0,
                       .u = cx + csize_x*0.0,
                       .v = cy + csize_y*1.0,
                       .color = c };
  }

  int edge_count = (int)(rads_dist(start_rads, end_rads) / 0.1f);
  if (edge_count < 1) edge_count = 1;

  for (int i = 1; i <= edge_count; i++) {
    float theta = rads_lerp(start_rads, end_rads, i/(float)edge_count);
    float bx = cosf(theta) * radius;
    float by = sinf(theta) * radius;

    *vbuf++ = (Vert) { .x =  x +      bx*1.0,
                       .y =  y +      by*1.0,
                       .u = cx + csize_x*1.0,
                       .v = cy + csize_y*0.0,
                       .color = c };

    *ibuf++ = (vbuf - state.vbuf)-1;
    *ibuf++ = (vbuf - state.vbuf)-2;
    *ibuf++ = idx_center;
  }

  state.vrt_i = (vbuf - state.vbuf);
  state.idx_i = (ibuf - state.ibuf);
}

static uint8_t *bytes;
static     int file_size;
void init(uint8_t *_bytes, int _file_size) {
  ss_font(UI_TEXT_SIZE*UI_SCALE, UI_TEXT_PAD);
  gl_vbuf(state.vbuf, ARR_LEN(state.vbuf));
  gl_ibuf(state.ibuf, ARR_LEN(state.ibuf));

  bytes     = _bytes;
  file_size = _file_size;
}

void frame(int width, int height) {

  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  float prog = 0;
  int circs = 0;
  while (wasm_iter(&wmi)) {

    if (circs++ < 500) {
      // float chunk = 1.0f/(float)(5*SecId_MAX);
      float chunk = (float)wmi.sec_size / (float)wmi.file_size;
      Clr clr = { prog, 0.3f, 0.4f, 1.0f };
      draw_ngon(width/2, prog*M_PI*2, (prog + chunk)*M_PI*2, clr, width/2, height/2);
      prog += chunk;
    }

    if (wmi.sec_id == SecId_Code) {
      CodeSecIter csi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      // while (code_sec_iter(&csi)) {
      //   // FnName fn_name = fn_name_get(bytes, file_size, csi.fn_i);
      // }
    }
  }

  Vert *vbuf = state.vbuf + state.vrt_i;
  for (Vert *p = state.vbuf; p != vbuf; p++) {
    /* it's in 0...width/height, we need it in -1...1 */

    p->x /= (float) width;
    p->y /= (float)height;

    p->u /= 1 << 10;
    p->v /= 1 << 10;

    p->x = 1 - 2*(1 - p->x);
    p->y = 1 - 2*(    p->y);

    // p->u = 1 - p->u;
    // p->v = 1 - p->v;
  }

  gl_vbuf(state.vbuf, state.vrt_i);
  gl_ibuf(state.ibuf, state.idx_i);
  state.vrt_i = state.idx_i = 0;
}
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void main(void) {
  FILE *f = fopen("build/ex.wasm", "rb");

  fseek(f, 0, SEEK_END);
  int file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *bytes = malloc(file_size);
  fread(bytes, 1, file_size, f);

  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  while (wasm_iter(&wmi)) {
    printf(
      "%8s: %5.1f%%\n",
      module_sec_name[wmi.sec_id],
      100 * ((float)wmi.sec_size / (float) wmi.file_size)
    );

    if (wmi.sec_id == SecId_Code) {
      CodeSecIter csi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      while (code_sec_iter(&csi)) {
        FnName fn_name = fn_name_get(bytes, file_size, csi.fn_i);

        printf(
          "       - %20.*s: %5.1f%%\n",
          fn_name.len,
          fn_name.name,
          100 * ((float)csi.fn_size / (float) wmi.file_size)
        );
      }
    }
  }
}
#endif
