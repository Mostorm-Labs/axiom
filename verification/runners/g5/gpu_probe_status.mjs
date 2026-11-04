export function gpuProbeStatus(native, browserErrors) {
  return native.nativeDone===true&&native.code===0&&Array.isArray(native.errors)&&
    !native.errors.length&&!browserErrors.length?'PASS':'FAIL';
}
