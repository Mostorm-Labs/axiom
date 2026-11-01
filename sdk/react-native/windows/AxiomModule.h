#pragma once
#include <NativeModules.h>
#include <axiom/native.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cmath>
#include <winrt/Windows.ApplicationModel.h>
namespace MostormAxiom {
REACT_MODULE(AxiomNative,L"AxiomNative")
struct AxiomNative {
 Microsoft::ReactNative::ReactContext context;
 REACT_INIT(Initialize)
 void Initialize(Microsoft::ReactNative::ReactContext const& value)noexcept{context=value;}
 REACT_METHOD(createSession)
 void createSession(std::string options,std::vector<double> snapshot,Microsoft::ReactNative::ReactPromise<std::string> promise)noexcept{
  context.UIDispatcher().Post([options=std::move(options),snapshot=std::move(snapshot),promise]()mutable{
   try{wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length||length>=32768)throw std::runtime_error("module location");
    const auto root=std::filesystem::path(executable).parent_path()/L"AxiomAssets";
    const auto utf8=root.u8string();if(axiom_native_initialize(reinterpret_cast<const char*>(utf8.data()),utf8.size()))throw std::runtime_error("initialization");
    std::vector<std::uint8_t> bytes;for(auto v:snapshot){if(v<0||v>255||std::floor(v)!=v)throw std::invalid_argument("byte");bytes.push_back(static_cast<std::uint8_t>(v));}
    char token[33]{};if(axiom_native_create(options.data(),options.size(),bytes.data(),bytes.size(),token))throw std::runtime_error("create rejected");promise.Resolve(std::string(token));
   }catch(const std::exception& e){promise.Reject(e.what());}
  });
 }
 REACT_METHOD(request)
 void request(std::string token,std::string request,std::vector<double> input,Microsoft::ReactNative::ReactPromise<std::string> promise)noexcept{
  context.UIDispatcher().Post([token=std::move(token),request=std::move(request),input=std::move(input),promise]()mutable{
   try{std::vector<std::uint8_t> bytes;for(auto v:input){if(v<0||v>255||std::floor(v)!=v)throw std::invalid_argument("byte");bytes.push_back(static_cast<std::uint8_t>(v));}
    axiom_native_buffer buffer=0;if(axiom_native_call(token.c_str(),request.data(),request.size(),bytes.data(),bytes.size(),&buffer))throw std::runtime_error("call rejected");
    struct Release{axiom_native_buffer b;~Release(){axiom_native_buffer_release(b);}}release{buffer};size_t n=0;axiom_native_buffer_copy(buffer,nullptr,0,&n);std::string reply(n,'\0');if(axiom_native_buffer_copy(buffer,reply.data(),n,&n))throw std::runtime_error("copy");promise.Resolve(reply);
   }catch(const std::exception& e){promise.Reject(e.what());}
  });
 }
 REACT_METHOD(close)
 void close(std::string token,Microsoft::ReactNative::ReactPromise<void> promise)noexcept{context.UIDispatcher().Post([token=std::move(token),promise](){if(axiom_native_close(token.c_str()))promise.Reject("CLOSE_REJECTED: finish IME or drain data custody");else promise.Resolve();});}
};
}
