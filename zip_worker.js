const brotli_size = (async () => {
  let wasm, mem;
  let heap_ptr;
  const env = {
    malloc(bytes) {
      if (heap_ptr == undefined)
        heap_ptr = wasm.__heap_base.value;

      const allocated_ptr = heap_ptr;
      heap_ptr += bytes;
      heap_ptr = Math.ceil(heap_ptr/4)*4;

      const mem_left = mem.byteLength - heap_ptr;
      if (mem_left < 0) {
        wasm.memory.grow(Math.ceil(-mem_left / (1 << 16)));
        mem = wasm.memory.buffer; /* reattach buffer */
      }
      return allocated_ptr;
    },
    free(ptr) {
      /* lol */
    },
    log2: Math.log2,
    exit: () => { throw new Error(); },
  };

  const wasm_src = fetch("build/brotli.wasm");
  const { instance } =
    await WebAssembly.instantiateStreaming(wasm_src, { env });
  wasm = instance.exports;
  mem = wasm.memory.buffer;

  function brotli_size(_fbuf0, _fbuf1) {
    const fbuf0 = new Uint8Array(_fbuf0);
    const fbuf1 = new Uint8Array(_fbuf1);

    heap_ptr = undefined;

    const fbuf_len = fbuf0.length + (fbuf1 ? fbuf1.length : 0);
    const buf_ptr = env.malloc(fbuf_len);
    if (1)
      new Uint8Array(mem, buf_ptr               , fbuf0.length).set(fbuf0);
    if (fbuf1)
      new Uint8Array(mem, buf_ptr + fbuf0.length, fbuf1.length).set(fbuf1);

    const out_size_max = wasm.BrotliEncoderMaxCompressedSize(fbuf_len);
    const out_ptr = env.malloc(out_size_max);

    const out_size_ptr = env.malloc(32/8);
    (new Uint32Array(mem, out_size_ptr, 1))[0] = out_size_max;

    const ret = wasm.BrotliEncoderCompress(
      11,  /* BROTLI_MAX_QUALITY      */
      24,  /* BROTLI_MAX_WINDOW_BITS  */
      0,   /* BROTLI_MODE_GENERIC     */
      fbuf_len,
      buf_ptr,
      out_size_ptr,
      out_ptr
    );
    if (ret == 0) throw new Error("BrotliEncoderCompress failed");
    return (new Uint32Array(mem, out_size_ptr, 1))[0];
  }

  return brotli_size;

})();

const gzipped_size = async (buf0, buf1) => {
  const calc_br_size = await brotli_size;
  return calc_br_size(buf0, buf1);
  /*
  const zipper = new CompressionStream("gzip");
  const writer = zipper.writable.getWriter();
            writer.write(buf0);
  if (buf1) writer.write(buf1);
  writer.close();
  
  let size = 0;
  const reader = zipper.readable.getReader();
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    size += value.length;
  }
  return size;
  */
}

let file_buf;
let gzipped_size_whole;
self.onmessage = async ({ data }) => {
  if (data.fbuf) {
    gzipped_size_whole = await gzipped_size(data.fbuf);
    file_buf = data.fbuf;
    postMessage("ready");
    return;
  }

  if (!file_buf) throw new Error("no fbuf");

  const { idx, len } = data;
  const gzipped_size_without = await gzipped_size(
    file_buf.slice(0, idx),
    file_buf.slice(idx+len, file_buf.byteLength)
  );
  const delta = gzipped_size_whole - gzipped_size_without;
  postMessage({ idx, len, delta })
}
