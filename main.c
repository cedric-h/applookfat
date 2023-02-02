// vim: sw=2 ts=2 expandtab smartindent

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#include <stdint.h>
#ifndef __wasm__
  #include <stdlib.h>
  #include <stdio.h>
#else
  extern void abort();
#endif

typedef struct { char *name; int len; int unknown; } NameSpan;

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

int64_t leb_decode(uint8_t *bytes, int *i) {
  int64_t result = 0;
  int shift = 0;

  /* the size in bits of the result variable, e.g., 64 if result's type is int64_t */
  int size = 64;

  uint8_t byte;
  do {
    byte = bytes[*i];
    *i = *i + 1;

    result |= ((byte & ~(1 << 7)) << shift);
    shift += 7;
  } while (((1 << 7) & byte) != 0);

  /* sign bit of byte is second high-order bit (0x40) */
  if ((shift < size) && ((1 << 6) & byte))
    /* sign extend */
    result |= (~0ul << shift);

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
      if (nsi->sub_i == nsi->end_i) return 0;

      nsi->fn_idx = uleb_decode(nsi->bytes, &nsi->sub_i);
      nsi->name_size = uleb_decode(nsi->bytes, &nsi->sub_i);

      nsi->name_i = nsi->sub_i;
      dbgf("func[%d]: <%.*s>\n",
          nsi->fn_idx, nsi->name_size,
          nsi->bytes + nsi->name_i);

      nsi->sub_i += nsi->name_size;

      return 1;
    } break;
  }
  return 0;
}

static NameSpan custom_sec_name(uint8_t *bytes, int *sec_i) {
  int name_len = uleb_decode(bytes, sec_i);
  return (NameSpan) { .name = (char *)(bytes + *sec_i), .len = name_len };
}

static int name_sec_find(
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
  ImportKind_Func   = 0x00,
  ImportKind_Table  = 0x01,
  ImportKind_Mem    = 0x02,
  ImportKind_Global = 0x03,
} ImportKind;
typedef enum {
  ImportSecStage_Init,
  ImportSecStage_Body,
} ImportSecStage;
typedef struct {
  ImportSecStage stage;
  uint8_t *bytes;
  int sec_size, end_i, sec_i;

  int import_count, import_i;
  int import_name; uint32_t import_name_len;
  int module_name; uint32_t module_name_len;
  ImportKind import_kind;
} ImportSecIter;

static int import_sec_iter(ImportSecIter *esi) {
  if (esi->stage == ImportSecStage_Body) esi->import_i++;

  switch (esi->stage) {
    case (ImportSecStage_Init): {
      esi->end_i = esi->sec_i + esi->sec_size;

      esi->import_count = uleb_decode(esi->bytes, &esi->sec_i);
      dbgf("sec size: %d\n", esi->sec_size);
      dbgf("import count: %d\n", esi->import_count);

      esi->stage = ImportSecStage_Body;

      /* fallthrough */
    };

    case (ImportSecStage_Body): {

      if (esi->sec_i == esi->end_i) return 0;

      esi->module_name_len = uleb_decode(esi->bytes, &esi->sec_i);
      esi->module_name = esi->sec_i;
      esi->sec_i += esi->module_name_len;
      dbgf("mod name len: %d\n", esi->module_name_len);
      dbgf("end_i: %d, sec_i: %d\n", esi->end_i, esi->sec_i);

      esi->import_name_len = uleb_decode(esi->bytes, &esi->sec_i);
      esi->import_name = esi->sec_i;
      esi->sec_i += esi->import_name_len;
      dbgf("import name len: %d\n", esi->import_name_len);
      dbgf("end_i: %d, sec_i: %d\n", esi->end_i, esi->sec_i);

      esi->import_kind = esi->bytes[esi->sec_i++];

      if (esi->import_kind == ImportKind_Func  ) {
        dbg("ImportKind_Func");
        uleb_decode(esi->bytes, &esi->sec_i); /* func sig typeid */
      }
      if (esi->import_kind == ImportKind_Table) {
        dbg("ImportKind_Table");

        esi->sec_i++; /* ref type */

        int has_max = esi->bytes[esi->sec_i++];
        dbgf("has_max: %d\n", has_max);
                     uleb_decode(esi->bytes, &esi->sec_i);
        if (has_max) uleb_decode(esi->bytes, &esi->sec_i);
      }
      if (esi->import_kind == ImportKind_Mem   ) {
        dbg("ImportKind_Mem");

        int has_max = esi->bytes[esi->sec_i++];
        dbgf("has_max: %d\n", has_max);
                     uleb_decode(esi->bytes, &esi->sec_i);
        if (has_max) uleb_decode(esi->bytes, &esi->sec_i);
      }
      if (esi->import_kind == ImportKind_Global) {
        dbg("ImportKind_Global");
        esi->sec_i += 2; /* valtype + mutability */
      }

      dbgf("---\n end_i: %d, sec_i: %d\n", esi->end_i, esi->sec_i);

      return 1;
    } break;
  }

  return 0;
}

typedef enum {
  ExportKind_Func   = 0x00,
  ExportKind_Table  = 0x01,
  ExportKind_Mem    = 0x02,
  ExportKind_Global = 0x03,
} ExportKind;
typedef enum {
  ExportSecStage_Init,
  ExportSecStage_Body,
} ExportSecStage;
typedef struct {
  ExportSecStage stage;
  uint8_t *bytes;
  int sec_size, end_i, sec_i;

  int export_count, export_i;
  int export_name; uint32_t export_name_len;

  ExportKind export_kind;
  int export_index;
} ExportSecIter;

static int export_sec_iter(ExportSecIter *esi) {
  if (esi->stage == ExportSecStage_Body) esi->export_i++;

  switch (esi->stage) {
    case (ExportSecStage_Init): {
      esi->end_i = esi->sec_i + esi->sec_size;

      esi->export_count = uleb_decode(esi->bytes, &esi->sec_i);
      dbgf("sec size: %d\n", esi->sec_size);
      dbgf("export count: %d\n", esi->export_count);

      esi->stage = ExportSecStage_Body;

      /* fallthrough */
    };

    case (ExportSecStage_Body): {

      if (esi->sec_i == esi->end_i) return 0;

      esi->export_name_len = uleb_decode(esi->bytes, &esi->sec_i);
      esi->export_name = esi->sec_i;
      esi->sec_i += esi->export_name_len;
      dbgf("export name len: %d\n", esi->export_name_len);
      dbgf("end_i: %d, sec_i: %d\n", esi->end_i, esi->sec_i);

      esi->export_kind = esi->bytes[esi->sec_i++];
      esi->export_index = uleb_decode(esi->bytes, &esi->sec_i);

      return 1;
    } break;
  }

  return 0;
}

typedef enum {
  CodeSecStage_Init,
  CodeSecStage_Body,
} CodeSecStage;
typedef struct {
  CodeSecStage stage;
  uint8_t *bytes; int file_size;
  int sec_size, end_i, sec_i;

  int fn_count, fn_idx, fn_size;
} CodeSecIter;

static int count_imported_fn(uint8_t *bytes, int file_size);
static uint8_t code_sec_iter(CodeSecIter *csi) {
  if (csi->stage == CodeSecStage_Body) csi->fn_idx++;

  switch (csi->stage) {
    case (CodeSecStage_Init): {
      csi->end_i = csi->sec_i + csi->sec_size;

      csi->fn_count = uleb_decode(csi->bytes, &csi->sec_i);
      dbgf("sec size: %d\n", csi->sec_size);
      dbgf("fn count: %d\n", csi->fn_count);

      csi->stage = CodeSecStage_Body;
      csi->fn_idx = count_imported_fn(csi->bytes, csi->file_size);

      /* fallthrough */
    };

    case (CodeSecStage_Body): {
      csi->sec_i += csi->fn_size;

      if (csi->sec_i == csi->end_i) return 0;

      csi->fn_size = uleb_decode(csi->bytes, &csi->sec_i);

      dbgf("code subsec size: %d\n", csi->fn_size);
      dbgf("sec_i: %d\n", csi->sec_i);
      dbgf("end_i: %d\n", csi->end_i);

      return 1;
    } break;
  }

  return 0;
}

typedef enum {
  FnBodyStage_Init,
  FnBodyStage_Body,
} FnBodyStage;
typedef struct {
  FnBodyStage stage;
  uint8_t *bytes;
  int fn_size, end_i, fn_i;
  int local_count;

  int scopes;
} FnBodyIter;

static uint8_t fn_body_iter(FnBodyIter *fei) {
  switch (fei->stage) {
    case (FnBodyStage_Init): {
      fei->end_i = fei->fn_i + fei->fn_size;

      fei->local_count = uleb_decode(fei->bytes, &fei->fn_i);
      dbgf("found function with %d locals\n", fei->local_count);

      for (int i = 0; i < fei->local_count; i++) {
        uleb_decode(fei->bytes, &fei->fn_i); /* index */
        fei->fn_i++;                         /* valtype */
      }
      dbgf("bytes left in func body: %d\n",
             fei->end_i - fei->fn_i);

      fei->stage = FnBodyStage_Body;
      fei->fn_i--;

      /* fallthrough */
    };

    case (FnBodyStage_Body): {

      int bytes_left = fei->end_i - fei->fn_i;
      if (bytes_left == 0) return 0;

      uint8_t instr = fei->bytes[fei->fn_i++];
      switch (instr) {
        case (0x45):
        case (70):
        case (71):
        case (72):
        case (73):
        case (74):
        case (75):
        case (76):
        case (77):
        case (78):
        case (79):
        case (80):
        case (81):
        case (82):
        case (83):
        case (84):
        case (85):
        case (86):
        case (87):
        case (88):
        case (89):
        case (90):
        case (91):
        case (92):
        case (93):
        case (94):
        case (95):
        case (96):
        case (97):
        case (98):
        case (99):
        case (100):
        case (101):
        case (102):
        case (103):
        case (104):
        case (105):
        case (106):
        case (107):
        case (108):
        case (109):
        case (110):
        case (111):
        case (112):
        case (113):
        case (114):
        case (115):
        case (116):
        case (117):
        case (118):
        case (119):
        case (120):
        case (121):
        case (122):
        case (123):
        case (124):
        case (125):
        case (126):
        case (127):
        case (128):
        case (129):
        case (130):
        case (131):
        case (132):
        case (133):
        case (134):
        case (135):
        case (136):
        case (137):
        case (138):
        case (139):
        case (140):
        case (141):
        case (142):
        case (143):
        case (144):
        case (145):
        case (146):
        case (147):
        case (148):
        case (149):
        case (150):
        case (151):
        case (152):
        case (153):
        case (154):
        case (155):
        case (156):
        case (157):
        case (158):
        case (159):
        case (160):
        case (161):
        case (162):
        case (163):
        case (164):
        case (165):
        case (166):
        case (167):
        case (168):
        case (169):
        case (170):
        case (171):
        case (172):
        case (173):
        case (174):
        case (175):
        case (176):
        case (177):
        case (178):
        case (179):
        case (180):
        case (181):
        case (182):
        case (183):
        case (184):
        case (185):
        case (186):
        case (187):
        case (188):
        case (189):
        case (190):
        case (191):
        case (192):
        case (193):
        case (194):
        case (195):
        case (0xC4):
        case (0x1A): case (0x1B):
        case (0xD1):
        case (0x00): case (0x01): case (0x05): case (0x0F):
          break;

        case (0x02): case (0x03): case (0x04): {
          dbgf("0x%02x at 0x%x scopin up %d -> %d\n",
              instr, fei->fn_i, fei->scopes, fei->scopes+1);
          fei->scopes++;

          leb_decode(fei->bytes, &fei->fn_i);
        } break;

        case (0x3F): case (0x40):
        case (0xD0): {
          fei->fn_i++;
        } break;

        case (0x41): leb_decode(fei->bytes, &fei->fn_i); break;
        case (0x42): leb_decode(fei->bytes, &fei->fn_i); break;

        /* floats */
        case (0x43): {
          fei->fn_i += 4;
        } break;
        case (0x44): {
          fei->fn_i += 8;
        } break;

        case (0x20): case (0x21): case (0x22): case (0x23): case (0x24):
        case (0x25): case (0x26):
        case (0xD2):
        case (0x10): /* call */
        case (0x0C): case (0x0D): {
          uleb_decode(fei->bytes, &fei->fn_i);
        } break;

        case (0x28):
        case (41):
        case (42):
        case (43):
        case (44):
        case (45):
        case (46):
        case (47):
        case (48):
        case (49):
        case (50):
        case (51):
        case (52):
        case (53):
        case (54):
        case (55):
        case (56):
        case (57):
        case (58):
        case (59):
        case (60):
        case (61):
        case (0x3E):
        case (0x11): {
          uleb_decode(fei->bytes, &fei->fn_i);
          uleb_decode(fei->bytes, &fei->fn_i);
        } break;

        case (0x1C): {
          uint32_t valtype_count = uleb_decode(fei->bytes, &fei->fn_i);
          fei->fn_i += valtype_count;
        } break;

        case (0x0E): {
          uint32_t lvec_len = uleb_decode(fei->bytes, &fei->fn_i);
          for (int i = 0; i < lvec_len; i++) {
            uleb_decode(fei->bytes, &fei->fn_i);
          }
          uleb_decode(fei->bytes, &fei->fn_i);
        } break;

        case (0xFC): {
          uint32_t immediate = uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 8 ) uleb_decode(fei->bytes, &fei->fn_i),
                               fei->fn_i++;
          if (immediate == 9 ) uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 10) fei->fn_i++, fei->fn_i++;
          if (immediate == 11) fei->fn_i++;
          if (immediate == 12) uleb_decode(fei->bytes, &fei->fn_i) ,
                               uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 13) uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 14) uleb_decode(fei->bytes, &fei->fn_i) ,
                               uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 15) uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 16) uleb_decode(fei->bytes, &fei->fn_i);
          if (immediate == 17) uleb_decode(fei->bytes, &fei->fn_i);
        } break;

        case (0xFD): {
          uint32_t immediate = uleb_decode(fei->bytes, &fei->fn_i);
          switch (immediate) {
            case (12): {
              fei->fn_i += 16;
            } break;

            case (13): {
              for (int i = 0; i < 16; i++)
                uleb_decode(fei->bytes, &fei->fn_i);
            } break;
            
            case (21):
            case (22):
            case (23):
            case (24):
            case (25):
            case (26):
            case (27):
            case (28):
            case (29):
            case (30):
            case (31):
            case (32):
            case (33):
            case (34):
            {
              uleb_decode(fei->bytes, &fei->fn_i);
            } break;
            
            case (0):
            case (1):
            case (2):
            case (3):
            case (4):
            case (5):
            case (6):
            case (7):
            case (8):
            case (9):
            case (10):
            case (11):
            case (92):
            case (93):
            {
              uleb_decode(fei->bytes, &fei->fn_i);
              uleb_decode(fei->bytes, &fei->fn_i);
            } break;

            case (84):
            case (85):
            case (86):
            case (87):
            case (88):
            case (89):
            case (90):
            case (91): {
              uleb_decode(fei->bytes, &fei->fn_i);
              uleb_decode(fei->bytes, &fei->fn_i);
              uleb_decode(fei->bytes, &fei->fn_i);
            } break;
          }
        } break;

        /* END */
        case (0x0B): {
          dbgf("0x0B scopin down %d -> %d\n", fei->scopes, fei->scopes-1);

          if (fei->scopes-- == 0) {
            dbgf("reached end, %d bytes left\n", bytes_left);
            dbgf("cursor: 0x%0x\n", fei->fn_i);
            if (bytes_left != 1) abort();
          }
        } break;

        default: dbgf("unknown instr %d\n", instr); abort(); break;
      }

      return 1;
    } break;
  }

  return 0;
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

static int wasm_iter(WasmIter *wmi) {
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
      if (wmi->file_i == wmi->file_size) return 0;

      wmi->sec_id = bytes[wmi->file_i++];
      wmi->sec_size = uleb_decode(bytes, &wmi->file_i);

      dbgf("id: %d, size: %d(0x%x)\n",
          wmi->sec_id, wmi->sec_size, wmi->sec_size);
      wmi->sec_i = wmi->file_i;
      
      dbgf("0x%x <-> 0x%x\n",
          wmi->sec_i, wmi->file_i + wmi->sec_size);

      wmi->file_i += wmi->sec_size;
      dbg("");

      return 1;
    }
  }
  return 0;
}

/* imports occupy first part of function index space */
static int count_imported_fn(uint8_t *bytes, int file_size) {
  int imported_fn_count = 0;

  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  while (wasm_iter(&wmi)) {
    if (wmi.sec_id == SecId_Import) {
      ImportSecIter isi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      while (import_sec_iter(&isi)) {
        if (isi.import_kind == ImportKind_Func  )
          imported_fn_count++;
      }
    }
  }

  return imported_fn_count;
}

/* I feel somewhat bad for looping through the entire module every time
 * I want a function name, but you can put this entire thing in front of a
 * cachetable anyway soooooo lmao
 *
 * "I start by doing everything from scratch every frame, then profile if it's slow"
 *    - Ryan Fleury */
static NameSpan fn_name_get(uint8_t *bytes, int file_size, int fn_idx) {
  int imported_fn_count = count_imported_fn(bytes, file_size);

  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  NameSecIter nsi = {0};

  while (wasm_iter(&wmi)) {
    if (wmi.sec_id == SecId_Import && fn_idx < imported_fn_count) {
      ImportSecIter isi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      int imported_fns = 0;
      while (import_sec_iter(&isi))
        if (isi.import_kind == ImportKind_Func) {
          if (imported_fns == fn_idx)
            return (NameSpan) {
              .name = (char *)(bytes + isi.import_name),
              .len = isi.import_name_len
            };
          imported_fns++;
        }

    }

    if (wmi.sec_id == SecId_Custom) {
      if (!name_sec_find(wmi.bytes, wmi.sec_size, wmi.sec_i, &nsi))
        continue;

      while (name_sec_iter(&nsi)) {
        if (nsi.fn_idx == fn_idx)
          return (NameSpan) {
            .name = (char *)(bytes + nsi.name_i),
            .len = nsi.name_size
          };
      }
    }
  }

  return (NameSpan) {
    .name =        "???",
    .len  = sizeof("???")-1,
    .unknown = 1,
  };
}

typedef enum {
  FnCallIterStage_Init,
  FnCallIterStage_Body,
} FnCallIterStage;
typedef struct {
  FnCallIterStage stage;
  FnBodyIter fei;

  uint8_t *bytes;
  int file_size, fn_index;
  int called_fn_index;
} FnCallIter;
static int fn_call_iter(FnCallIter *fci) {

  switch (fci->stage) {
    case (FnCallIterStage_Init): {
      WasmIter wmi = { .bytes = fci->bytes, .file_size = fci->file_size };
      while (wasm_iter(&wmi)) {
        if (wmi.sec_id == SecId_Code) {
          CodeSecIter csi = {
            .bytes = fci->bytes,
            .file_size = fci->file_size,
            .sec_size = wmi.sec_size,
            .sec_i    = wmi.sec_i   ,
          };

          while (code_sec_iter(&csi)) {
            if (csi.fn_idx == fci->fn_index) {
              fci->stage = FnCallIterStage_Body;
              fci->fei = (FnBodyIter) {
                .bytes = fci->bytes,
                .fn_i = csi.sec_i,
                .fn_size = csi.fn_size,
              };
              return fn_call_iter(fci);
            }
          }
        }
      }

      dbgf("ERROR: FN %d NOT FOUND\n", fci->fn_index);
      dbgf("(%d imported fns)\n", count_imported_fn(fci->bytes, fci->file_size));
      return 0;
    };

    case (FnCallIterStage_Body): {
      while (fn_body_iter(&fci->fei)) {
        int fn_i = fci->fei.fn_i;
        int instr = fci->bytes[fn_i++];

        if (instr == 0x10) {
          fci->called_fn_index = uleb_decode(fci->bytes, &fn_i);
          fn_body_iter(&fci->fei);

          return 1;
        }
#if 0
        if (instr == 0x11) {
          int sig   = uleb_decode(fci->bytes, &fn_i);
          int table = uleb_decode(fci->bytes, &fn_i);
          dbgf("FN CALLS INDIRECTLY (typeidx: %d, tableidx: %d)\n",
              sig, table);
        }
#endif
      }
      return 0;
    } break;
  }

  return 0;
}

/* returns number of non-import fns */
static int count_fns(uint8_t *bytes, int file_size) {
  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  while (wasm_iter(&wmi)) {
    if (wmi.sec_id == SecId_Code) {
      CodeSecIter csi = {
        .bytes = bytes,
        .file_size = file_size,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      code_sec_iter(&csi);

      return csi.fn_count;
    }
  }

  return 0;
}

#ifdef __wasm__
extern uint8_t __heap_base;

#define memset __builtin_memset
#define memcpy __builtin_memcpy

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS   1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS       0
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS       1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS   0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS      0
#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#ifdef NULL
#undef NULL
#endif
#define NULL ((void *)0)
#define memset __builtin_memset
#define memcpy __builtin_memcpy

typedef struct { float r, g, b, a; } Clr;
typedef struct { float x, y; float u, v; Clr color; } Vert;
#define CLR_RED     ((Clr) { 1.0f, 0.0f, 0.0f, 1.0f })
#define CLR_MAGENTA ((Clr) { 1.0f, 0.0f, 1.0f, 1.0f })
#define CLR_WHITE   ((Clr) { 1.0f, 1.0f, 1.0f, 1.0f })
#define CHAR_WH_RATIO (0.54)
#define UI_SCALE (1)
#define UI_TEXT_SIZE (20)
#define UI_TEXT_PAD  (6)
/* spritesheet */
extern void ss_clear(void);
extern void ss_font(int text_px, int text_pad);
/* triangle pusher (gl) */
extern float gl_vbuf(Vert     *vbuf, int len);
extern float gl_ibuf(uint16_t *ibuf, int len);
extern float gl_clear(float r, float g, float b);
/* maffs */
#define M_PI 3.141592653589793
#define GOLDEN_RATIO 1.618033988749894
extern float cosf(float);
extern float sinf(float);
extern float sqrtf(float);
extern float atan2f(float, float);
extern float fmodf(float, float);
extern int file_section_size(int index, int len);
extern int file_whole_size(void);
#define abs(a) (((a) < 0) ? -(a) : (a))
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
extern void eputs(char *str, int len);

static struct {
  float mouse_x, mouse_y;

  Vert     vbuf[1 << 20];
  uint16_t ibuf[1 << 21];
  int vrt_i;
  int idx_i;
} state = {0};

void mouse(int mode, float x, float y) { state.mouse_x = x, state.mouse_y = y; }
void key(int mode, char code) { }
void wheel(int mode, float x, float y, float dy) { }

typedef struct {
  float radius;
  float start_rads, end_rads;
  Clr clr;
  float x, y;
} DrawNgonIn;
static void draw_ngon(DrawNgonIn *in) {
  float radius     = in->radius;
  float start_rads = in->start_rads;
  float end_rads   = in->end_rads;
  Clr   c          = in->clr;
  float x          = in->x;
  float y          = in->y;

  Vert     *vbuf = state.vbuf + state.vrt_i;
  uint16_t *ibuf = state.ibuf + state.idx_i;

  int edge_count = (int)((end_rads - start_rads) / 0.1f);
  if (edge_count < 1) edge_count = 1;
  if ((state.vrt_i + 2+edge_count) >= ARR_LEN(state.vbuf)) return;
  if ((state.idx_i + edge_count*3) >= ARR_LEN(state.ibuf)) return;

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

  for (int i = 1; i <= edge_count; i++) {
    float theta = start_rads + (end_rads - start_rads)*(i/(float)edge_count);
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

static void draw_rect(float size_x, float size_y, Clr clr, int x, int y) {
  Vert     *vbuf = state.vbuf + state.vrt_i;
  uint16_t *ibuf = state.ibuf + state.idx_i;

  if ((state.vrt_i + 4) >= ARR_LEN(state.vbuf)) return;
  if ((state.idx_i + 6) >= ARR_LEN(state.ibuf)) return;

  float cx      = UI_TEXT_PAD;
  float cy      = UI_TEXT_PAD;
  float csize_x = UI_TEXT_SIZE;
  float csize_y = UI_TEXT_SIZE;

  *vbuf++ = (Vert) { .x =  x +  size_x*0.0,
                     .y =  y +  size_y*0.0,
                     .u = cx + csize_x*0.0,
                     .v = cy + csize_y*0.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*0.0,
                     .y =  y +  size_y*1.0,
                     .u = cx + csize_x*0.0,
                     .v = cy + csize_y*1.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*1.0,
                     .y =  y +  size_y*1.0,
                     .u = cx + csize_x*1.0,
                     .v = cy + csize_y*1.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*1.0,
                     .y =  y +  size_y*0.0,
                     .u = cx + csize_x*1.0,
                     .v = cy + csize_y*0.0,
                     .color = clr };

  {
    int i = state.vrt_i;
    *ibuf++ = i+3; *ibuf++ = i+2; *ibuf++ = i+1;
    *ibuf++ = i+3; *ibuf++ = i+1; *ibuf++ = i+0;
  }

  state.vrt_i += 4;
  state.idx_i += 6;
}

static void draw_char(char c, int wsize, Clr clr, int x, int y) {
  Vert     *vbuf = state.vbuf + state.vrt_i;
  uint16_t *ibuf = state.ibuf + state.idx_i;

  if ((state.vrt_i + 4) >= ARR_LEN(state.vbuf)) return;
  if ((state.idx_i + 6) >= ARR_LEN(state.ibuf)) return;

  float csize = UI_SCALE * UI_TEXT_SIZE;
  float leap = UI_TEXT_PAD + csize + UI_TEXT_PAD;
  float csize_x = csize * CHAR_WH_RATIO;
  float csize_y = csize;

  float cx = (float)(c % 12) * leap + UI_TEXT_PAD;
  float cy = (float)(c / 12) * leap + UI_TEXT_PAD;

  float size_x = wsize * CHAR_WH_RATIO;
  float size_y = wsize;
  *vbuf++ = (Vert) { .x =  x +  size_x*0.0,
                     .y =  y +  size_y*0.0,
                     .u = cx + csize_x*0.0,
                     .v = cy + csize_y*0.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*0.0,
                     .y =  y +  size_y*1.0,
                     .u = cx + csize_x*0.0,
                     .v = cy + csize_y*1.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*1.0,
                     .y =  y +  size_y*1.0,
                     .u = cx + csize_x*1.0,
                     .v = cy + csize_y*1.0,
                     .color = clr };
  *vbuf++ = (Vert) { .x =  x +  size_x*1.0,
                     .y =  y +  size_y*0.0,
                     .u = cx + csize_x*1.0,
                     .v = cy + csize_y*0.0,
                     .color = clr };

  {
    int i = state.vrt_i;
    *ibuf++ = i+3; *ibuf++ = i+2; *ibuf++ = i+1;
    *ibuf++ = i+3; *ibuf++ = i+1; *ibuf++ = i+0;
  }

  state.vrt_i += 4;
  state.idx_i += 6;
}

static void draw_str(char *str, int len, float f_x, float f_y) {
  // int wsize = (int)(UI_SCALE * UI_TEXT_SIZE);
  // Clr clr = { 1.0f, 1.0f, 1.0f, 1.0f };
  // int x = (int)(UI_SCALE * f_x);
  // int y = (int)(UI_SCALE * (f_y + UI_TEXT_SIZE/5));
  int wsize = (int)(UI_SCALE * UI_TEXT_SIZE);
  Clr clr = { 1.0f, 1.0f, 1.0f, 1.0f };
  int x = (int)(f_x);
  int y = (int)((f_y + UI_TEXT_SIZE/5));

  if (len == -1)
    for (len = 0; str[len]; len++);

  int size_x = len*wsize*CHAR_WH_RATIO;
  for (int i = 0; i < len; i++)
    draw_char(
      str[i],
      wsize,
      clr,
      x + i*wsize*CHAR_WH_RATIO - size_x/2,
      y - wsize/2
    );
}

static uint8_t *bytes;
static     int file_size;
void takefile(uint8_t *_bytes, int _file_size) {
  bytes     = _bytes;
  file_size = _file_size;
}
void init(void) {
  ss_clear();
  ss_font(UI_TEXT_SIZE*UI_SCALE, UI_TEXT_PAD);
  gl_vbuf(state.vbuf, ARR_LEN(state.vbuf));
  gl_ibuf(state.ibuf, ARR_LEN(state.ibuf));
}

static float _hue_to_rgb(float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f/6.f) return p + (q - p) * 6.f * t;
    if (t < 1.f/2.f) return q;
    if (t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
    return p;
}
static Clr clr_from_hsl(float h, float s, float l) {
    float r, g, b;

    if (s == 0.f) {
      r = g = b = l; // achromatic
    } else {
      float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
      float p = 2 * l - q;
      r = _hue_to_rgb(p, q, h + 1.f/3.f);
      g = _hue_to_rgb(p, q, h);
      b = _hue_to_rgb(p, q, h - 1.f/3.f);
    }
    if (r > 1) r = 1;
    if (g > 1) g = 1;
    if (b > 1) b = 1;
    return (Clr) { r, g, b, 1.0f };
}

static void draw_size_str(
  char cbuf[], int cbuf_len,
  int n_bytes,
  int x, int y
) {
  int kb = n_bytes / (1 << 10);
  if (kb > 2)
    draw_str(
      cbuf,
      npf_snprintf(
        cbuf, cbuf_len,
        "%dkb",
        kb
      ),
      state.mouse_x,
      state.mouse_y + UI_SCALE*UI_TEXT_SIZE
    );
  else
    draw_str(
      cbuf,
      npf_snprintf(
        cbuf, cbuf_len,
        "%d bytes",
        n_bytes
      ),
      state.mouse_x,
      state.mouse_y + UI_SCALE*UI_TEXT_SIZE
    );
}

static int fn_is_export(uint8_t *bytes, int file_size, int fn_index) {
  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  while (wasm_iter(&wmi)) {
    if (wmi.sec_id == SecId_Export) {
      ExportSecIter esi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      while (export_sec_iter(&esi))
        if (esi.export_kind == ExportKind_Func)
          if (esi.export_index == fn_index)
            return 1;
    }
  }

  return 0;
}

typedef enum {
  DrawWasmLayer_Base,
  DrawWasmLayer_Hover,
  DrawWasmLayer_Text,

  DrawWasmLayer_HoveredFn,
} DrawWasmLayer;
static int draw_wasm(
  DrawWasmLayer layer, int hovered_fn_idx,
  uint8_t *bytes, int file_size,
  float radius, float pie_x, float pie_y
) {
  int layer_base       = layer == DrawWasmLayer_Base;
  int layer_hover      = layer == DrawWasmLayer_Hover;
  int layer_text       = layer == DrawWasmLayer_Text;
  int layer_hovered_fn = layer == DrawWasmLayer_HoveredFn;

  float mouse_rads;
  {
    float dy = state.mouse_y - pie_y;
    float dx = state.mouse_x - pie_x;
    mouse_rads = atan2f(dy, dx);
    if (mouse_rads < 0) mouse_rads = M_PI*2 + mouse_rads;
  }

  char kb_chars[1 << 7] = {0};
  WasmIter wmi = { .bytes = bytes, .file_size = file_size };
  float prog = 0;
  for (int si = 0; wasm_iter(&wmi); si++) {
    int child_text = 0;

    if (wmi.sec_id == SecId_Code, 0) {
      CodeSecIter csi = {
        .bytes = bytes,
        .file_size = file_size,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      float code_prog = prog;
      for (int ci = 0; code_sec_iter(&csi); ci++) {
        // NameSpan fn_name = fn_name_get(bytes, file_size, csi.fn_idx);
        
        float code_chunk = (float)csi.fn_size / (float) wmi.file_size;
        DrawNgonIn dnin = {
          .radius = radius,
          .start_rads = (code_prog +          0)*M_PI*2,
          .end_rads   = (code_prog + code_chunk)*M_PI*2,
          .x = pie_x,
          .y = pie_y
        };
        code_prog += code_chunk;
        int hover = dnin.start_rads < mouse_rads && dnin.end_rads > mouse_rads;
        if (layer_hovered_fn && hover) return csi.fn_idx;
        if (!hover && code_chunk < 0.001f) continue;
        int hide_text = code_chunk < 0.01f;

        int called_by_hovered = 0;
        FnCallIter fci = { .bytes = bytes, .file_size = file_size, .fn_index = hovered_fn_idx };
        while (fn_call_iter(&fci))
          if (fci.called_fn_index == csi.fn_idx)
            called_by_hovered = 1;

        if (called_by_hovered || fn_is_export(bytes, file_size, csi.fn_idx)) {
          float angle = rads_lerp(dnin.start_rads, dnin.end_rads, 0.5f);
          dnin.x += cosf(angle) * radius * (called_by_hovered ? 0.1f : 0.2f);
          dnin.y += sinf(angle) * radius * (called_by_hovered ? 0.1f : 0.2f);
        }

        if (layer_base) {
          float hue = 0.2 + 0.6*fmodf((si/2.0f + ci) * GOLDEN_RATIO, 1.0f);
          dnin.clr = clr_from_hsl(hue, 0.6f, called_by_hovered ? 0.3f : 0.6f);
          draw_ngon(&dnin);
        }

        NameSpan fn_name = fn_name_get(bytes, file_size, csi.fn_idx);
        if (layer_hover && hover) {
          float a = dnin.clr.a = 0.15f;
          dnin.clr.r = dnin.clr.g = dnin.clr.b = a*0.1f;
          draw_ngon(&dnin);
        }

        if (layer_text && hover) {
          child_text = 1;
          draw_str(
            fn_name.name,
            fn_name.len,
            state.mouse_x,
            state.mouse_y
          );
          draw_size_str(
            kb_chars, sizeof(kb_chars),
            csi.fn_size,
            state.mouse_x,
            state.mouse_y + UI_SCALE*UI_TEXT_SIZE
          );

          if (fn_is_export(bytes, file_size, csi.fn_idx)) {
            draw_str(
                     "EXPORTED" ,
              sizeof("EXPORTED")-1,
              state.mouse_x,
              state.mouse_y + 2*UI_SCALE*UI_TEXT_SIZE
            );
          }

        }

        if (!hide_text && layer_text) {
          float center_rads = rads_lerp(dnin.start_rads, dnin.end_rads, 0.5f);
          draw_str(
            fn_name.name,
            fn_name.len,
            pie_x + cosf(center_rads) * radius*0.9,
            pie_y + sinf(center_rads) * radius*0.9
          );
        }
      }
    }

    /* render base-level chunk */
    {
      int sec_size = file_section_size(wmi.sec_i, wmi.sec_size);
      int whole_size = file_whole_size();
      float chunk = (float)sec_size / (float)whole_size;
      DrawNgonIn dnin = {
        .radius = (wmi.sec_id == SecId_Code) ? radius*0.833 : radius,
        .start_rads = (prog +     0)*M_PI*2,
        .end_rads   = (prog + chunk)*M_PI*2,
        .x = pie_x,
        .y = pie_y
      };
      prog += chunk;

      int hover = dnin.start_rads < mouse_rads && dnin.end_rads > mouse_rads;
      if (!hover && chunk < 0.001f) continue;
      int hide_text = chunk < 0.01f;

      if (layer_base) {
        float hue = fmodf(si * GOLDEN_RATIO, 1);
        dnin.clr = clr_from_hsl(hue, 0.5f, 0.65f);
        draw_ngon(&dnin);
      }

      if (layer_hover && hover) {
        float a = dnin.clr.a = 0.15f;
        dnin.clr.r = dnin.clr.g = dnin.clr.b = a*0.5f;
        draw_ngon(&dnin);
      }

      if (layer_text && hover && child_text == 0) {
        draw_str(
          module_sec_name[wmi.sec_id],
          -1,
          state.mouse_x,
          state.mouse_y
        );
        draw_size_str(
          kb_chars, sizeof(kb_chars),
          sec_size,
          state.mouse_x,
          state.mouse_y + UI_SCALE*UI_TEXT_SIZE
        );
      }

      if (!hide_text && layer_text) {
        float center_rads = dnin.start_rads + 0.5*(dnin.end_rads - dnin.start_rads);
        float dist = radius * ((chunk > 0.1) ? 0.63 : 1.2);
        draw_str(
          module_sec_name[wmi.sec_id],
          -1,
          pie_x + cosf(center_rads) * dist,
          pie_y + sinf(center_rads) * dist
        );

        if (wmi.sec_id == SecId_Custom) {
          int sec_i = wmi.sec_i;
          NameSpan ns = custom_sec_name(wmi.bytes, &sec_i);
          draw_str(
            ns.name,
            ns.len,
            pie_x + cosf(center_rads) * dist,
            pie_y + sinf(center_rads) * dist - UI_SCALE*UI_TEXT_SIZE
          );
        }
      }
    }
    /* render base-level chunk */

  }

  return 0;
}

void frame(int width, int height) {
  float pie_x =  width/2;
  float pie_y = height/2;
  float radius = height/3;
  int hovered_fn_idx = draw_wasm(DrawWasmLayer_HoveredFn, -1, bytes, file_size, radius, pie_x, pie_y);
  draw_wasm(DrawWasmLayer_Base , hovered_fn_idx, bytes, file_size, radius, pie_x, pie_y);
  draw_wasm(DrawWasmLayer_Hover, hovered_fn_idx, bytes, file_size, radius, pie_x, pie_y);
  draw_wasm(DrawWasmLayer_Text , hovered_fn_idx, bytes, file_size, radius, pie_x, pie_y);

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

  Clr clear = clr_from_hsl(0.57, 0.9, 0.65);
  gl_clear(clear.r, clear.g, clear.b);
}
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static void fn_call_recurse(
  uint8_t *bytes,
  int file_size,
  uint8_t *visited,
  int n,
  int parent_fn_index,
  int fn_index
) {
  NameSpan fn_name = {
    .name =        "root",
    .len  = sizeof("root"),
  };
  if (n > 0) {
    visited[fn_index]++;
    if (visited[fn_index] > 1) return;

    fn_name = fn_name_get(bytes, file_size, parent_fn_index);
  }

  NameSpan called_fn_name = fn_name_get(bytes, file_size, fn_index);

  int imported_fn_count = count_imported_fn(bytes, file_size);

  for (int i = 0; i < n; i++) putchar(' ');
  if (n == 0) {
    printf("ROOT CALLS \"%.*s\" (fn %d)\n", called_fn_name.len, called_fn_name.name, fn_index);
  } else if (fn_index < imported_fn_count) {
    if (called_fn_name.unknown || fn_name.unknown)
      printf("- FN %d CALLS FN %d (import) \n", parent_fn_index, fn_index);
    else
      printf(
        "FN \"%.*s\" CALLS FN \"%.*s\" (import) \n",
        fn_name.len,
        fn_name.name,
        called_fn_name.len,
        called_fn_name.name
      );
  } else {
    if (called_fn_name.unknown || fn_name.unknown)
      printf("- FN %d CALLS FN %d          \n", parent_fn_index, fn_index);
    else
      printf(
        "FN \"%.*s\" CALLS FN \"%.*s\"\n",
        fn_name.len,
        fn_name.name,
        called_fn_name.len,
        called_fn_name.name
      );
  }

  FnCallIter fci = { .bytes = bytes, .file_size = file_size, .fn_index = fn_index };
  while (fn_call_iter(&fci))
    if (fci.called_fn_index >= imported_fn_count)
      fn_call_recurse(bytes, file_size, visited, n + 1, fn_index, fci.called_fn_index);
}

int main(int argc, char **argv) {
  FILE *f = fopen("tests/build/big_call_stack.wasm", "rb");
  // FILE *f = fopen("build/ex.wasm", "rb");

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

    if (wmi.sec_id == SecId_Export) {
      ExportSecIter esi = {
        .bytes = bytes,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      int fn_count = count_fns(bytes, file_size);
      uint8_t *visited = malloc(fn_count);
      memset(visited, 0, fn_count);

      while (export_sec_iter(&esi)) {
        NameSpan ns = {
          .name = (char *)(bytes + esi.export_name),
          .len = esi.export_name_len
        };
        if (esi.export_kind == ExportKind_Func) {
          printf(
            "found export: \"%.*s\" (fn %d)\n",
            ns.len,
            ns.name,
            esi.export_index
          );

          fn_call_recurse(bytes, file_size, visited, 0, -1, esi.export_index);
        }
      }

      for (int i = 0; i < fn_count; i++) {
        if (visited[i] == 0) continue;
        NameSpan ns = fn_name_get(bytes, file_size, i);
          printf(
            "\"%.*s\" (called %d times)\n",
            ns.len,
            ns.name,
            visited[i]
          );
      }
    }

  }
#if 0
    if (wmi.sec_id == SecId_Code) {
      CodeSecIter csi = {
        .bytes = bytes,
        .file_size = file_size,
        .sec_size = wmi.sec_size,
        .sec_i    = wmi.sec_i   ,
      };

      while (code_sec_iter(&csi)) {
        NameSpan fn_name = fn_name_get(bytes, file_size, csi.fn_i);

        printf(
          "       - %20.*s: %4d bytes = %5.1f%%\n",
          fn_name.len,
          fn_name.name,
          csi.fn_size,
          100 * ((float)csi.fn_size / (float) wmi.file_size)
        );
      }
    }
#endif
  return 0;
}
#endif
