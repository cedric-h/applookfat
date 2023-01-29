// vim: sw=2 ts=2 expandtab smartindent

// use memcpy wasm instruction
#define memcpy __builtin_memcpy

static void dog_talk(char *out) { memcpy(out, "woof", sizeof("woof")); }
static void dog_name(char *out) { memcpy(out, "dog", sizeof("dog")); }
static void cat_talk(char *out) { memcpy(out, "meow", sizeof("meow")); }
static void cat_name(char *out) { memcpy(out, "cat", sizeof("cat")); }

typedef struct {
  void (*talk)(char *out);
  void (*name)(char *out);
} CreatureTable;

static CreatureTable cat_table = { cat_talk, cat_name };
static CreatureTable dog_table = { dog_talk, dog_name };

void main(int creature_index, char *out) {
  CreatureTable *vtable;

  if (creature_index == 0) vtable = &cat_table;
  if (creature_index == 1) vtable = &dog_table;

  vtable->name(out);
  memcpy(out, " goes ", sizeof(" goes "));
  vtable->talk(out);
}
