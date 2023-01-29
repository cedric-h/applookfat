mkdir -p build
cd build

clang                            \
  --target=wasm32                \
  -ffreestanding                 \
  -nostdlib                      \
  -Wl,--no-entry                 \
  -Wl,--export=init              \
  -Wl,--export=takefile          \
  -Wl,--export=frame             \
  -Wl,--export=mouse             \
  -Wl,--export=key               \
  -Wl,--export=wheel             \
  -Wl,--export=__heap_base       \
  -Wl,--import-undefined         \
  -Os                            \
  -Wall                          \
  ../main.c                      \
  -o alf.wasm

# -gdwarf                        \
