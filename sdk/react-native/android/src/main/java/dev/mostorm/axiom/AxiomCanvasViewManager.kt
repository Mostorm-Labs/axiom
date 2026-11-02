package dev.mostorm.axiom
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.annotations.ReactProp
class AxiomCanvasViewManager: SimpleViewManager<AxiomCanvasView>() {
 override fun getName()="AxiomCanvasView"
 override fun createViewInstance(context: ThemedReactContext)=AxiomCanvasView(context)
 @ReactProp(name="sessionToken") fun token(view: AxiomCanvasView, value: String?) {view.setSessionToken(value?:"")}
 override fun onDropViewInstance(view: AxiomCanvasView){view.releaseSurface();super.onDropViewInstance(view)}
}
