// Browser file facts only. DebugControlRouter constructs the existing Runtime
// intent after the asynchronous read and revalidates its original target.
export function installImageInput({module, getHandle, schedule, restoreFocus}) {
  const picker = document.createElement('input');
  picker.type = 'file';
  picker.accept = 'image/png,image/jpeg,image/webp,.png,.jpg,.jpeg,.webp';
  picker.hidden = true;
  document.body.append(picker);
  let pending = 0n, disposed = false;
  const finish = (token, {bytes, mime = '', error = '', cancelled = false}) => {
    const handle = getHandle();
    if (disposed || !handle || !token || token !== pending) return;
    pending = 0n;
    let data = 0, type = 0, message = 0;
    try {
      if (bytes?.length) {
        data = module._malloc(bytes.length);
        if (data) module.HEAPU8.set(bytes, data);
        else { error = 'Image file buffer allocation failed'; bytes = undefined; }
      }
      type = module.stringToNewUTF8(mime);
      message = module.stringToNewUTF8(error);
      module._axiom_web_debug_image_result(handle, token, data, bytes?.length || 0, type, message, cancelled ? 1 : 0);
    } finally {
      module._free(data); module._free(type); module._free(message);
      restoreFocus(); schedule();
    }
  };
  picker.addEventListener('cancel', () => finish(pending, {cancelled:true}));
  picker.addEventListener('change', async () => {
    const token = pending, file = picker.files?.[0];
    if (!file) { finish(token, {cancelled:true}); return; }
    try {
      if (!file.size || file.size > 128 * 1024 * 1024) throw new Error('Image file is empty or exceeds 128 MiB');
      const bytes = new Uint8Array(await file.arrayBuffer());
      const matches = (offset, values) => values.every((value, i) => bytes[offset+i] === value);
      const mime = matches(0, [137,80,78,71,13,10,26,10]) ? 'image/png'
        : matches(0, [255,216,255]) ? 'image/jpeg'
        : matches(0, [82,73,70,70]) && matches(8, [87,69,66,80]) ? 'image/webp' : '';
      if (!mime) throw new Error('Selected file is not a supported PNG, JPEG or WebP image');
      const extension = file.name.split('.').at(-1).toLowerCase();
      const declared = {png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',webp:'image/webp'}[extension];
      if (declared && declared !== mime) throw new Error('File extension does not match its image data');
      finish(token, {bytes,mime});
    } catch (error) { finish(token, {error:String(error)}); }
  });
  return {
    // Call synchronously after ImGui input dispatch, within the trusted browser
    // activation. A later animation frame cannot reliably open a native picker.
    process() {
      if (disposed || pending || !getHandle()) return;
      const token = module._axiom_web_debug_image_request(getHandle());
      if (!token) return;
      pending = token; picker.value = '';
      try {
        if (picker.showPicker) picker.showPicker(); else picker.click();
      } catch (error) { finish(token, {error:`Image file picker failed: ${error}`}); }
    },
    dispose() { disposed = true; pending = 0n; picker.remove(); },
  };
}
