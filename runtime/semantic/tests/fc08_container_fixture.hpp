#pragma once
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "container_operation_mapping.hpp"
#include "protobuf_object_mapping.hpp"
#include "auditoryworks/axiom/v1/operation.pb.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <google/protobuf/message.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace fc08_test {
using namespace canvas::semantic;
using Json=nlohmann::json;
namespace p=auditoryworks::axiom::v1;
inline const std::filesystem::path root(AXIOM_SOURCE_ROOT);
inline Json read(const std::filesystem::path& path){std::ifstream f(path);return Json::parse(f);}
inline std::vector<std::uint8_t> bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
inline std::string unhex(const std::string& s){if(s.size()%2)throw std::invalid_argument("odd hex");std::string v;for(std::size_t i=0;i<s.size();i+=2)v+=static_cast<char>(std::stoul(s.substr(i,2),nullptr,16));return v;}
inline std::string hex(const std::string& b){std::string v;for(unsigned char c:b){v+="0123456789abcdef"[c>>4U];v+="0123456789abcdef"[c&15U];}return v;}
inline std::string hex(const std::vector<std::uint8_t>& b){return hex(std::string(b.begin(),b.end()));}
// Fixture carrier only: literal fields -> generated DTO. No semantic planning,
// normalization, expected-value derivation or fake runtime behavior lives here.
inline void literal(const Json& value,google::protobuf::Message& message){
 const auto* d=message.GetDescriptor();const auto* r=message.GetReflection();
 for(auto it=value.begin();it!=value.end();++it){
  const auto* f=d->FindFieldByName(it.key());if(!f)throw std::invalid_argument(it.key());
  const auto set=[&](const Json& v){
   using F=google::protobuf::FieldDescriptor;
   switch(f->cpp_type()){
   case F::CPPTYPE_MESSAGE:literal(v,*(f->is_repeated()?r->AddMessage(&message,f):r->MutableMessage(&message,f)));break;
   case F::CPPTYPE_STRING:{auto s=v.get<std::string>();if(f->type()==F::TYPE_BYTES)s=unhex(s);if(f->is_repeated())r->AddString(&message,f,s);else r->SetString(&message,f,s);break;}
   case F::CPPTYPE_DOUBLE:if(f->is_repeated())r->AddDouble(&message,f,v.get<double>());else r->SetDouble(&message,f,v.get<double>());break;
   case F::CPPTYPE_FLOAT:if(f->is_repeated())r->AddFloat(&message,f,v.get<float>());else r->SetFloat(&message,f,v.get<float>());break;
   case F::CPPTYPE_BOOL:if(f->is_repeated())r->AddBool(&message,f,v.get<bool>());else r->SetBool(&message,f,v.get<bool>());break;
   case F::CPPTYPE_UINT32:if(f->is_repeated())r->AddUInt32(&message,f,v.get<std::uint32_t>());else r->SetUInt32(&message,f,v.get<std::uint32_t>());break;
   case F::CPPTYPE_UINT64:if(f->is_repeated())r->AddUInt64(&message,f,v.get<std::uint64_t>());else r->SetUInt64(&message,f,v.get<std::uint64_t>());break;
   case F::CPPTYPE_INT32:if(f->is_repeated())r->AddInt32(&message,f,v.get<std::int32_t>());else r->SetInt32(&message,f,v.get<std::int32_t>());break;
   case F::CPPTYPE_INT64:if(f->is_repeated())r->AddInt64(&message,f,v.get<std::int64_t>());else r->SetInt64(&message,f,v.get<std::int64_t>());break;
   case F::CPPTYPE_ENUM:if(f->is_repeated())r->AddEnumValue(&message,f,v.get<int>());else r->SetEnumValue(&message,f,v.get<int>());break;
   }
  };
  if(f->is_repeated())for(const auto& v:it.value())set(v);else set(it.value());
 }
}
inline Json observed(const google::protobuf::Message& message){
 Json value=Json::object();const auto* r=message.GetReflection();std::vector<const google::protobuf::FieldDescriptor*> fields;r->ListFields(message,&fields);
 for(const auto* f:fields){
  auto get=[&](int i)->Json{
   using F=google::protobuf::FieldDescriptor;const bool repeated=f->is_repeated();
   switch(f->cpp_type()){
   case F::CPPTYPE_MESSAGE:return observed(repeated?r->GetRepeatedMessage(message,f,i):r->GetMessage(message,f));
   case F::CPPTYPE_STRING:{auto v=repeated?r->GetRepeatedString(message,f,i):r->GetString(message,f);return f->type()==F::TYPE_BYTES?hex(v):v;}
   case F::CPPTYPE_DOUBLE:return repeated?r->GetRepeatedDouble(message,f,i):r->GetDouble(message,f);
   case F::CPPTYPE_FLOAT:return repeated?r->GetRepeatedFloat(message,f,i):r->GetFloat(message,f);
   case F::CPPTYPE_BOOL:return repeated?r->GetRepeatedBool(message,f,i):r->GetBool(message,f);
   case F::CPPTYPE_UINT32:return repeated?r->GetRepeatedUInt32(message,f,i):r->GetUInt32(message,f);
   case F::CPPTYPE_UINT64:return repeated?r->GetRepeatedUInt64(message,f,i):r->GetUInt64(message,f);
   case F::CPPTYPE_INT32:return repeated?r->GetRepeatedInt32(message,f,i):r->GetInt32(message,f);
   case F::CPPTYPE_INT64:return repeated?r->GetRepeatedInt64(message,f,i):r->GetInt64(message,f);
   case F::CPPTYPE_ENUM:return repeated?r->GetRepeatedEnumValue(message,f,i):r->GetEnumValue(message,f);
   }throw std::invalid_argument("field type");
  };
  if(f->is_repeated()){auto a=Json::array();for(int i=0;i<r->FieldSize(message,f);++i)a.push_back(get(i));value[f->name()]=std::move(a);}else value[f->name()]=get(0);
 }
 return value;
}
inline Json records(const ObjectStore& store){Json a=Json::array();for(const auto& record:store.allObjects()){p::ObjectRecord dto;if(!canvas::semantic::internal::toProtobufObjectRecord(record,dto))throw std::invalid_argument("observation");a.push_back(observed(dto));}return a;}
inline const Json& manifest(){static const auto v=read(root/".aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json");return v;}
inline std::vector<Json> cases(bool wire){std::vector<Json> v;for(const auto& row:manifest()["variants"]){const auto c=row["case_id"].get<std::string>();if(wire?(c=="FC08-W03"||c=="FC08-W04"||c=="FC08-W05"||c=="FC08-W06"):(c>="FC08-T01"&&c<="FC08-T06"))v.push_back(row);}return v;}
inline std::vector<std::uint8_t> carrier(const Json& model){p::Operation dto;literal(model,dto);const auto s=dto.SerializeAsString();return {s.begin(),s.end()};}
inline std::vector<std::uint8_t> wireFor(const Json& row,const std::string& label){for(const auto& w:row["wire"])if(w["label"]==label)return bytes(root/w["path"].get<std::string>());return {};}
inline ObjectId id(const std::string& s){ObjectId v;const auto b=unhex(s);if(b.size()!=16)throw std::invalid_argument("id");std::copy(b.begin(),b.end(),v.bytes.begin());return v;}
template<class Store> struct Runtime {
 Store objects;AppliedOperationLedger ledger;SemanticGenerationState generation;CanonicalCommitClock clock{RuntimeEpoch(37)};OperationEngine engine;
 ApplyResult apply(const std::vector<std::uint8_t>& wire){const auto d=SemanticCodec::decodeProtobufOperation(wire);if(!d.ok())return {};return engine.apply(d.operation,ApplySource::kLocalInteraction,objects,ledger,generation,clock);}
 void seed(const Json& values){
  if(values.empty())return;
  const Json model={{"operation_id",{{"value","fffffffffffffffffffffffffffffffe"}}},{"document_id",{{"value","00000000000000000000000000000064"}}},{"schema_version",1},{"payload_version",1},{"payload",{{"insert_objects",{{"objects",values}}}}}};
  if(apply(carrier(model)).disposition!=ApplyDisposition::kApplied)throw std::invalid_argument("Fixture setup rejected by real InsertObjects");
 }
};
} // namespace fc08_test
