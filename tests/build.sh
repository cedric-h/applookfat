cd brotli

clang \
  --target=wasm32                \
  -ffreestanding                 \
  -nostdlib                      \
  c/common/constants.c           \
  c/common/context.c             \
  c/common/dictionary.c          \
  c/common/platform.c            \
  c/common/shared_dictionary.c   \
  c/common/transform.c           \
  -I c/include \
  -o brotli.wasm
