package dev.mostorm.axiom.verification;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import com.facebook.react.uimanager.ThemedReactContext;
import dev.mostorm.axiom.AxiomCanvasView;
import dev.mostorm.axiom.AxiomNativeModule;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import org.json.JSONObject;

/** External-consumer regression through the real View/JNI/Runtime/GLES path.
 * Synthetic MotionEvents are supplemental evidence, never physical touch proof.
 * Reflection remains test-only; it adds no production SDK inspection API. */
public final class CanvasInputInstrumentation extends Instrumentation {
  private AxiomCanvasView canvas;
  private AxiomNativeModule module;
  private Method call;
  private String token;

  @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
  private static View find(View view) {
    if(view instanceof AxiomCanvasView)return view;
    if(view instanceof ViewGroup) {
      ViewGroup group=(ViewGroup)view;
      for(int i=0;i<group.getChildCount();++i) {
        View result=find(group.getChildAt(i));if(result!=null)return result;
      }
    }
    return null;
  }
  private JSONObject request(String json) {
    JSONObject[] result={null};Throwable[] failure={null};
    runOnMainSync(()->{
      try {
        result[0]=new JSONObject((String)call.invoke(module,token,json,new byte[0]));
        if(!result[0].getBoolean("ok"))throw new AssertionError(result[0].toString());
      }catch(Throwable ex){failure[0]=ex;}
    });
    if(failure[0]!=null)throw new AssertionError(failure[0]);
    return result[0];
  }
  private JSONObject state() throws Exception { return request("{\"command\":\"state\"}").getJSONObject("value"); }
  private void touch(int action,long down,long time,float x,float y) {
    Throwable[] failure={null};
    runOnMainSync(()->{
      MotionEvent event=MotionEvent.obtain(down,time,action,x,y,0);
      event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
      try {if(!canvas.dispatchTouchEvent(event))throw new AssertionError("View rejected touch");}
      catch(Throwable ex){failure[0]=ex;}
      finally {event.recycle();}
    });
    if(failure[0]!=null)throw new AssertionError(failure[0]);
  }
  @Override public void onStart() {
    Bundle result=new Bundle();
    try {
      Intent intent=getTargetContext().getPackageManager().getLaunchIntentForPackage(getTargetContext().getPackageName());
      intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TASK);
      Activity activity=startActivitySync(intent);
      Throwable[] failure={null};float[] density={1};
      for(int retry=0;retry<100;++retry) {
        runOnMainSync(()->{
          canvas=(AxiomCanvasView)find(activity.getWindow().getDecorView());
          if(canvas!=null)try {
            Field g=AxiomCanvasView.class.getDeclaredField("generation");g.setAccessible(true);
            if(g.getLong(canvas)==0){canvas=null;return;}
            Field t=AxiomCanvasView.class.getDeclaredField("token");t.setAccessible(true);token=(String)t.get(canvas);
            ThemedReactContext context=(ThemedReactContext)canvas.getContext();
            module=context.getReactApplicationContext().getNativeModule(AxiomNativeModule.class);
            call=AxiomNativeModule.class.getDeclaredMethod("call",String.class,String.class,byte[].class);call.setAccessible(true);
            density[0]=canvas.getResources().getDisplayMetrics().density;
          }catch(Throwable ex){failure[0]=ex;}
        });
        if(failure[0]!=null)throw new AssertionError(failure[0]);
        if(canvas!=null)break;
        SystemClock.sleep(100);
      }
      if(canvas==null)throw new AssertionError("RN View did not attach");
      request("{\"command\":\"control\",\"kind\":\"tool\",\"value\":0}");
      request("{\"command\":\"control\",\"kind\":\"preset\",\"profileId\":\"round-handdrawn\",\"revision\":4}");
      int before=state().getInt("objects");float d=density[0];long down=SystemClock.uptimeMillis();
      // MotionEvent.obtain uses Android's zero-based single-finger ID 0.
      touch(MotionEvent.ACTION_DOWN,down,down,60*d,80*d);
      if(state().getInt("activeContacts")!=1)throw new AssertionError("Android pointer ID 0 did not enter Runtime");
      touch(MotionEvent.ACTION_MOVE,down,down+16,120*d,100*d);
      touch(MotionEvent.ACTION_MOVE,down,down+32,180*d,140*d);
      touch(MotionEvent.ACTION_UP,down,down+48,220*d,160*d);
      JSONObject after=state();
      if(after.getInt("activeContacts")!=0||after.getInt("objects")!=before+1)
        throw new AssertionError("Single-finger stroke did not commit: "+after);
      request("{\"command\":\"control\",\"kind\":\"undo\"}");
      if(state().getInt("objects")!=before)throw new AssertionError("Undo lost stroke identity");
      request("{\"command\":\"control\",\"kind\":\"redo\"}");
      if(state().getInt("objects")!=before+1)throw new AssertionError("Redo lost stroke identity");
      long cancel=down+64;
      touch(MotionEvent.ACTION_DOWN,cancel,cancel,80*d,180*d);
      touch(MotionEvent.ACTION_CANCEL,cancel,cancel+16,100*d,200*d);
      JSONObject cancelled=state();
      if(cancelled.getInt("activeContacts")!=0||cancelled.getInt("objects")!=before+1)
        throw new AssertionError("Cancel mutated document or retained contact");
      result.putString("state",cancelled.toString());
      result.putString("status","SYNTHETIC_VIEW_JNI_RUNTIME_PASSED");finish(Activity.RESULT_OK,result);
    }catch(Throwable ex){
      result.putString("status","FAILED");result.putString("error",ex.toString());finish(Activity.RESULT_CANCELED,result);
    }
  }
}
