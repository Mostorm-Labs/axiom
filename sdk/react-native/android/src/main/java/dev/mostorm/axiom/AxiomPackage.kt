package dev.mostorm.axiom
import com.facebook.react.BaseReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.model.ReactModuleInfo
import com.facebook.react.module.model.ReactModuleInfoProvider
import com.facebook.react.uimanager.ViewManager
class AxiomPackage: BaseReactPackage() {
 override fun getModule(name: String, context: ReactApplicationContext): NativeModule?=if(name=="AxiomNative")AxiomNativeModule(context)else null
 override fun getReactModuleInfoProvider()=ReactModuleInfoProvider {mapOf("AxiomNative" to ReactModuleInfo("AxiomNative","AxiomNative",false,false,false,true))}
 override fun createViewManagers(context: ReactApplicationContext): List<ViewManager<*,*>> = listOf(AxiomCanvasViewManager())
}
