#include <jni.h>
#include <android/native_window_jni.h>
#include <axiom/native.h>
#include <string>
#include <vector>
#include <stdexcept>
namespace {
// JNI's GetStringUTFChars/NewStringUTF use modified UTF-8, which corrupts
// supplementary Unicode in JSON. Use Java's standard UTF-8 codec at this seam.
std::string string(JNIEnv* env,jstring text){
 if(!text)throw std::invalid_argument("string");
 const auto type=env->FindClass("java/lang/String");
 const auto method=env->GetMethodID(type,"getBytes","(Ljava/lang/String;)[B");
 const auto encoding=env->NewStringUTF("UTF-8");
 auto data=static_cast<jbyteArray>(env->CallObjectMethod(text,method,encoding));
 env->DeleteLocalRef(encoding);env->DeleteLocalRef(type);
 if(env->ExceptionCheck()||!data)throw std::runtime_error("UTF-8 encoding");
 const auto n=env->GetArrayLength(data);std::string result(n,'\0');
 env->GetByteArrayRegion(data,0,n,reinterpret_cast<jbyte*>(result.data()));env->DeleteLocalRef(data);return result;
}
jstring string(JNIEnv* env,const std::string& text){
 const auto type=env->FindClass("java/lang/String");
 const auto constructor=env->GetMethodID(type,"<init>","([BLjava/lang/String;)V");
 const auto encoding=env->NewStringUTF("UTF-8");const auto data=env->NewByteArray(static_cast<jsize>(text.size()));
 if(!data)throw std::runtime_error("UTF-8 buffer");
 env->SetByteArrayRegion(data,0,static_cast<jsize>(text.size()),reinterpret_cast<const jbyte*>(text.data()));
 auto result=static_cast<jstring>(env->NewObject(type,constructor,data,encoding));
 env->DeleteLocalRef(data);env->DeleteLocalRef(encoding);env->DeleteLocalRef(type);return result;
}
std::vector<std::uint8_t> bytes(JNIEnv* env,jbyteArray value){const auto count=env->GetArrayLength(value);if(count>134217728)throw std::invalid_argument("bytes");std::vector<std::uint8_t> data(count);env->GetByteArrayRegion(value,0,count,reinterpret_cast<jbyte*>(data.data()));return data;}
void failure(JNIEnv* env,const std::exception& e){env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),e.what());}
}
extern "C" {
JNIEXPORT jint JNICALL Java_dev_mostorm_axiom_AxiomNativeModule_initialize(JNIEnv* e,jobject,jstring root){try{const auto path=string(e,root);return axiom_native_initialize(path.data(),path.size());}catch(const std::exception& ex){failure(e,ex);return 1;}}
JNIEXPORT jstring JNICALL Java_dev_mostorm_axiom_AxiomNativeModule_create(JNIEnv* e,jobject,jstring options,jbyteArray snapshot){try{const auto json=string(e,options);const auto data=bytes(e,snapshot);char token[33]{};if(axiom_native_create(json.data(),json.size(),data.data(),data.size(),token))throw std::runtime_error("create rejected");return e->NewStringUTF(token);}catch(const std::exception& ex){failure(e,ex);return nullptr;}}
JNIEXPORT jstring JNICALL Java_dev_mostorm_axiom_AxiomNativeModule_call(JNIEnv* e,jobject,jstring token,jstring request,jbyteArray input){try{const auto s=string(e,token),json=string(e,request);const auto data=bytes(e,input);axiom_native_buffer buffer=0;if(axiom_native_call(s.c_str(),json.data(),json.size(),data.data(),data.size(),&buffer))throw std::runtime_error("call rejected");struct Release{axiom_native_buffer b;~Release(){axiom_native_buffer_release(b);}}release{buffer};size_t n=0;axiom_native_buffer_copy(buffer,nullptr,0,&n);std::string reply(n,'\0');if(axiom_native_buffer_copy(buffer,reply.data(),reply.size(),&n))throw std::runtime_error("buffer copy");return string(e,reply);}catch(const std::exception& ex){failure(e,ex);return nullptr;}}
JNIEXPORT jboolean JNICALL Java_dev_mostorm_axiom_AxiomNativeModule_dispose(JNIEnv* e,jobject,jstring token){try{return axiom_native_close(string(e,token).c_str())==0;}catch(const std::exception& ex){failure(e,ex);return false;}}
JNIEXPORT jlong JNICALL Java_dev_mostorm_axiom_AxiomCanvasView_attach(JNIEnv* e,jobject,jstring token,jobject a,jobject b,jfloat w,jfloat h,jfloat d){try{const auto s=string(e,token);auto* first=ANativeWindow_fromSurface(e,a);auto* second=ANativeWindow_fromSurface(e,b);axiom_native_view view{sizeof(view),1,first,second,w,h,d};std::uint64_t generation=0;const auto code=axiom_native_attach(s.c_str(),&view,&generation);ANativeWindow_release(first);ANativeWindow_release(second);return code?0:static_cast<jlong>(generation);}catch(const std::exception& ex){failure(e,ex);return 0;}}
JNIEXPORT jboolean JNICALL Java_dev_mostorm_axiom_AxiomCanvasView_detach(JNIEnv* e,jobject,jstring t,jlong g){return axiom_native_detach(string(e,t).c_str(),std::uint64_t(g))==0;}
JNIEXPORT jboolean JNICALL Java_dev_mostorm_axiom_AxiomCanvasView_resize(JNIEnv* e,jobject,jstring t,jlong g,jfloat w,jfloat h,jfloat d){return axiom_native_resize(string(e,t).c_str(),std::uint64_t(g),w,h,d)==0;}
JNIEXPORT jboolean JNICALL Java_dev_mostorm_axiom_AxiomCanvasView_frame(JNIEnv* e,jobject,jstring t,jlong g){return axiom_native_frame(string(e,t).c_str(),std::uint64_t(g))==0;}
JNIEXPORT jboolean JNICALL Java_dev_mostorm_axiom_AxiomCanvasView_input(JNIEnv* e,jobject,jstring t,jlong g,jlong pointer,jlongArray time,jfloatArray points,jint phase,jint tool,jboolean pressure){try{const auto n=e->GetArrayLength(time);if(n<1||n>4096||e->GetArrayLength(points)!=n*3)return false;std::vector<jlong> ts(n);std::vector<float> values(n*3);e->GetLongArrayRegion(time,0,n,ts.data());e->GetFloatArrayRegion(points,0,n*3,values.data());std::vector<axiom_native_sample> samples;for(jsize i=0;i<n;++i)samples.push_back({2,std::uint64_t(pointer),std::uint64_t(ts[i]),values[i*3],values[i*3+1],values[i*3+2],0,0,std::uint32_t(phase),std::uint32_t(tool),std::uint32_t(pressure?1:0),0});return axiom_native_input(string(e,t).c_str(),std::uint64_t(g),samples.data(),samples.size())==0;}catch(const std::exception& ex){failure(e,ex);return false;}}
}
