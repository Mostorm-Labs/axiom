package dev.mostorm.axiom

import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReadableArray
import com.facebook.react.bridge.UiThreadUtil
import com.facebook.react.module.annotations.ReactModule

@ReactModule(name = "AxiomNative")
class AxiomNativeModule(context: ReactApplicationContext) : NativeAxiomSpec(context) {
  companion object { init { System.loadLibrary("axiom_react_native_android") } }
  private external fun initialize(root: String): Int
  private external fun create(options: String, snapshot: ByteArray): String
  private external fun call(session: String, request: String, bytes: ByteArray): String
  private external fun dispose(session: String): Boolean
  private fun data(value: ReadableArray): ByteArray {
    require(value.size() <= 134217728)
    return ByteArray(value.size()) { i -> val n=value.getDouble(i);require(n>=0&&n<=255&&n%1.0==0.0);n.toInt().toByte() }
  }
  private fun owner(promise: Promise, action: () -> Any?) {
    UiThreadUtil.runOnUiThread { try { promise.resolve(action()) } catch (e: Exception) { promise.reject("AXIOM_REJECTED", e.message, e) } }
  }
  override fun createSession(options: String, snapshot: ReadableArray, promise: Promise) {
    owner(promise) {
      val bytes=data(snapshot)
      // Assets are copied from the matching SDK release; never a network URL.
      val root=java.io.File(reactApplicationContext.filesDir,"axiom-sdk-0.3.8")
      if (!java.io.File(root,"fonts/catalog.json").isFile) {
        fun copyAssets(relative: String, destination: java.io.File) {
          val children=reactApplicationContext.assets.list(relative) ?: emptyArray()
          if(children.isNotEmpty()) {
            require(destination.isDirectory || destination.mkdirs())
            for(child in children)copyAssets(if(relative.isEmpty())child else "$relative/$child",java.io.File(destination,child))
          } else {
            destination.parentFile?.mkdirs()
            reactApplicationContext.assets.open(relative).use { input -> destination.outputStream().use { input.copyTo(it) } }
          }
        }
        copyAssets("axiom-font.otf",java.io.File(root,"axiom-font.otf"))
        copyAssets("fonts",java.io.File(root,"fonts"))
      }
      require(initialize(root.absolutePath)==0) { "Native initialization failed" }
      create(options,bytes)
    }
  }
  override fun request(session: String, request: String, bytes: ReadableArray, promise: Promise) {
    owner(promise) { call(session,request,data(bytes)) }
  }
  override fun close(session: String, promise: Promise) { owner(promise) { require(dispose(session)) { "Finish IME or drain data custody before close" };null } }
}
