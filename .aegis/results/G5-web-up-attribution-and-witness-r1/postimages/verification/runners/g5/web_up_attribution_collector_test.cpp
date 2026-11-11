#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
using namespace canvas::foundation::internal::g5;
using Json=nlohmann::json;
void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 need(control(2,914)==0,"initialize");unsigned roots=0,ends=0,aggregates=0,total=0;
 // Validate fixed-capacity record-slot reuse after each fully drained pair.
 // Exercise 12 Up roots, including 40 object
 // queries, many value comparisons and short-lived charged preparation leases.
 for(unsigned pair=0;pair<6;++pair){
  for(unsigned up=0;up<2;++up){need(control(1,0)==0,"on");{
   Scope input{Tag::input,Purpose::input};Scope root{Tag::eraser_up};Scope exact{Tag::exact_prepare};
   canvas::resources::ResourceBudgetManager budget({8*1024*1024,8*1024*1024});
   for(unsigned copy=0;copy<40;++copy){Scope query{Tag::pathops};
    {Scope history{Tag::wa_history_lookup};for(unsigned mask=0;mask<8;++mask){Scope value{Tag::wa_history_value};}}
    {Scope path{Tag::wa_path_lookup};Scope build{Tag::wa_path_build};}
    {Scope evaluate{Tag::wa_exact_eval};for(unsigned mask=0;mask<8;++mask){Scope difference{Tag::wa_difference};}}
    for(unsigned lease=0;lease<160;++lease){const auto token=budget.reserve(canvas::resources::ResourceClass::kRecorded,1024,true);need(bool(token),"reserve");budget.release(*token);}
    emit(Kind::Counter,Tag::wa_history_lookup,{1,8388608,1024,1,8,0,0,160,0,copy,0},false);
   }
  }need(control(0,0)==0,"off");}
  for(unsigned page=0;page<4096;++page){need(control(4,0)==0,"drain");const auto p=Json::parse(bytes(),bytes()+size());
   need(p["loss"]["critical"]==0&&p["loss"]["statistical"]==0,"cumulative observation overflow");
   for(const auto& r:p["records"]){++total;roots+=r["kind"]==unsigned(Kind::Begin)&&r["tag"]==unsigned(Tag::eraser_up);
    ends+=r["kind"]==unsigned(Kind::End)&&r["tag"]==unsigned(Tag::eraser_up);aggregates+=r["kind"]==unsigned(Kind::Counter)&&r["values"][0]=="9";}
   if(p["finished"].get<bool>())break;
  }
 }
 need(roots==12&&ends==12&&aggregates>0,"root/aggregate lost");
 std::cout<<Json{{"roots",roots},{"ends",ends},{"aggregates",aggregates},{"records",total},{"critical_loss",0},{"statistical_loss",0},{"array_slots_reused_by_drain",true}}.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
