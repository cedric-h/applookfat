mkdir -p build
cd build

clang                            \
  --target=wasm32                \
  -ffreestanding                 \
  -nostdlib                      \
  -Wl,--no-entry                 \
  -Wl,--export=main              \
  -Wl,--import-undefined         \
  -Os                            \
  -Wall                          \
  ../fn_table.c                  \
  -o fn_table.wasm

wasm2wat fn_table.wasm

# -gdwarf                        \
