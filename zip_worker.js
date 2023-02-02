const gzipped_size = async (buf0, buf1) => {
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
}

(async () => {
  const env = { hello: () => console.log("hi") };

  const wasm_src = fetch("build/hello_world.wasm");
  const { instance } =
    await WebAssembly.instantiateStreaming(wasm_src, { env });

  console.log(instance.exports.main());
})();

let file_buf;
self.onmessage = ({ data }) => {
  if (data.fbuf) {
    file_buf = data.fbuf;
    return;
  }

  if (!file_buf) throw new Error("no fbuf");

  const { idx, len } = data;
  gzipped_size(
    file_buf.slice(0, idx),
    file_buf.slice(idx+len, file_buf.length)
  ).then(gzipped_size_without => {
    postMessage({ idx, len, gzipped_size_without })
  });
}
