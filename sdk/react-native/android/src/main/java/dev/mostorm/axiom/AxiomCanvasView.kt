package dev.mostorm.axiom

import android.content.Context
import android.graphics.PixelFormat
import android.view.Choreographer
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout

class AxiomCanvasView(context: Context): FrameLayout(context), Choreographer.FrameCallback {
  private val canonical=SurfaceView(context)
  private val preview=SurfaceView(context)
  private var canonicalSurface: Surface?=null
  private var previewSurface: Surface?=null
  private var generation=0L
  private var token=""
  private var running=false
  private var boundWidth=0
  private var boundHeight=0
  companion object { init { System.loadLibrary("axiom_rn") } }
  private external fun attach(token: String, canonical: Surface, preview: Surface, width: Float, height: Float, dpr: Float): Long
  private external fun detach(token: String, generation: Long): Boolean
  private external fun resize(token: String, generation: Long, width: Float, height: Float, dpr: Float): Boolean
  private external fun frame(token: String, generation: Long): Boolean
  private external fun input(token: String, generation: Long, pointer: Long, times: LongArray, values: FloatArray, phase: Int, tool: Int, pressure: Boolean): Boolean
  init {
    addView(canonical,LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.MATCH_PARENT))
    preview.setZOrderMediaOverlay(true);preview.holder.setFormat(PixelFormat.TRANSLUCENT)
    addView(preview,LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.MATCH_PARENT))
    fun listen(view: SurfaceView, isPreview: Boolean) {
      view.holder.addCallback(object: SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) { if(isPreview)previewSurface=holder.surface else canonicalSurface=holder.surface; bind() }
        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) { bind() }
        override fun surfaceDestroyed(holder: SurfaceHolder) { releaseSurface();if(isPreview)previewSurface=null else canonicalSurface=null }
      })
    }
    listen(canonical,false);listen(preview,true)
  }
  fun setSessionToken(value: String) { if(token!=value){releaseSurface();token=value;bind()} }
  private fun bind() {
    val a=canonicalSurface?:return;val b=previewSurface?:return
    if(token.isEmpty()||width<=0||height<=0||!a.isValid||!b.isValid)return
    val d=resources.displayMetrics.density
    if(generation==0L)generation=attach(token,a,b,width/d,height/d,d)
    else if(width!=boundWidth||height!=boundHeight)resize(token,generation,width/d,height/d,d)
    boundWidth=width;boundHeight=height
    if(generation!=0L&&!running){running=true;Choreographer.getInstance().postFrameCallback(this)}
  }
  fun releaseSurface() { if(generation!=0L)detach(token,generation);generation=0;boundWidth=0;boundHeight=0;running=false;Choreographer.getInstance().removeFrameCallback(this) }
  override fun onAttachedToWindow() { super.onAttachedToWindow();bind() }
  override fun onDetachedFromWindow() { releaseSurface();super.onDetachedFromWindow() }
  override fun onSizeChanged(w: Int,h: Int,oldw: Int,oldh: Int) { super.onSizeChanged(w,h,oldw,oldh);bind() }
  override fun doFrame(time: Long) { if(running&&generation!=0L){frame(token,generation);Choreographer.getInstance().postFrameCallback(this)} }
  override fun onInterceptTouchEvent(event: MotionEvent)=true
  override fun onTouchEvent(e: MotionEvent): Boolean {
    if(generation==0L)return false
    parent.requestDisallowInterceptTouchEvent(true)
    val action=e.actionMasked
    if(action !in listOf(MotionEvent.ACTION_DOWN,MotionEvent.ACTION_POINTER_DOWN,MotionEvent.ACTION_MOVE,MotionEvent.ACTION_UP,MotionEvent.ACTION_POINTER_UP,MotionEvent.ACTION_CANCEL))return false
    val indices=if(action==MotionEvent.ACTION_MOVE||action==MotionEvent.ACTION_CANCEL)(0 until e.pointerCount).toList() else listOf(e.actionIndex)
    val phase=when(action){MotionEvent.ACTION_DOWN,MotionEvent.ACTION_POINTER_DOWN->0;MotionEvent.ACTION_UP,MotionEvent.ACTION_POINTER_UP->2;MotionEvent.ACTION_CANCEL->3;else->1}
    val d=resources.displayMetrics.density
    for(i in indices){
      val history=if(phase==1)e.historySize else 0
      val times=LongArray(history+1)
      val points=FloatArray((history+1)*3)
      for(h in 0..history){val past=h<history;times[h]=(if(past)e.getHistoricalEventTime(h) else e.eventTime)*1000000
        points[h*3]=(if(past)e.getHistoricalX(i,h) else e.getX(i))/d;points[h*3+1]=(if(past)e.getHistoricalY(i,h) else e.getY(i))/d;points[h*3+2]=if(past)e.getHistoricalPressure(i,h) else e.getPressure(i)}
      val pen=e.getToolType(i)==MotionEvent.TOOL_TYPE_STYLUS||e.getToolType(i)==MotionEvent.TOOL_TYPE_ERASER
      input(token,generation,e.getPointerId(i).toLong(),times,points,phase,if(pen)2 else 1,pen)
    }
    return true
  }
}
