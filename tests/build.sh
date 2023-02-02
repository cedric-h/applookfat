mkdir -p build
cd build

# clang                            \
#   --target=wasm32                \
#   -ffreestanding                 \
#   -nostdlib                      \
#   -Wl,--no-entry                 \
#   -Wl,--export=main              \
#   -Wl,--import-undefined         \
#   -Os                            \
#   -Wall                          \
#   ../fn_table.c                  \
#   -o fn_table.wasm

clang                            \
  --target=wasm32                \
  -ffreestanding                 \
  -nostdlib                      \
  -Wl,--no-entry                 \
  -Wl,--export=main              \
  -Wl,--import-undefined         \
  -Os                            \
  -Wall                          \
  ../hello_world.c               \
  -o hello_world.wasm

# clang                            \
#   --target=wasm32                \
#   -ffreestanding                 \
#   -nostdlib                      \
#   -Wl,--no-entry                 \
#   -Wl,--export=main              \
#   -Wl,--import-undefined         \
#   -O0                            \
#   -Wall                          \
#   ../big_call_stack.c            \
#   -o big_call_stack.wasm

# wasm2wat big_call_stack.wasm

# -gdwarf                        \
