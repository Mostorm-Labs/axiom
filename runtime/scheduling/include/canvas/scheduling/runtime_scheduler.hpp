#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <map>
#include <optional>
#include <vector>
namespace canvas::scheduling {
enum class WorkPriority { kVisible,kNear,kPrefetch,kBackground };
struct WorkDependency final {
 std::uint64_t id=0,generation=1;
 WorkDependency(std::uint64_t value,std::uint64_t version=1):id(value),generation(version){}
 bool operator==(const WorkDependency&)const=default;
 bool operator==(std::uint64_t value)const noexcept{return id==value;}
};
struct WorkItem final {
 std::uint64_t id=0,generation=0;WorkPriority priority=WorkPriority::kBackground;
 std::uint64_t deadline=0;std::vector<WorkDependency> dependencies;std::size_t estimatedBytes=0;
};
// Owner-driven cooperative executor. No private pool or platform VSync owner.
class RuntimeScheduler final {
 public:
 explicit RuntimeScheduler(std::size_t capacity=128):capacity_(capacity){}
 bool submit(WorkItem item){
  if(!item.id||!item.generation||std::find(item.dependencies.begin(),item.dependencies.end(),item.id)!=item.dependencies.end())return false;
  for(const auto dep:item.dependencies)if(!dep.id||!dep.generation)return false;
  // A cycle would permanently pin visible work. Check before replacing live work.
  std::set<std::uint64_t> visited;
  const auto reaches=[&](auto&& self,std::uint64_t id)->bool {
   if(id==item.id)return true;
   if(!visited.insert(id).second)return false;
   const auto it=work_.find(id);if(it==work_.end())return false;
   for(auto dep:it->second.item.dependencies)if(self(self,dep.id))return true;
   return false;
  };
  for(auto dep:item.dependencies)if(reaches(reaches,dep.id))return false;
  auto found=work_.find(item.id);
  if(found!=work_.end()&&found->second.item.generation>item.generation)return false;
  if(found!=work_.end()&&found->second.item.generation==item.generation)return found->second.item.dependencies==item.dependencies;
  if(found==work_.end()&&work_.size()>=capacity_){
   auto victim=work_.end();for(auto it=work_.begin();it!=work_.end();++it)
    if(!it->second.running&&!needed(it->first)&&it->second.item.priority>item.priority&&(victim==work_.end()||it->second.item.priority>victim->second.item.priority))victim=it;
   if(victim==work_.end())return false;
   work_.erase(victim);
  }
  work_[item.id]={std::move(item),false};return true;
 }
 std::optional<WorkItem> take(std::uint64_t now=0){
  auto chosen=work_.end();WorkPriority best=WorkPriority::kBackground;
  for(auto it=work_.begin();it!=work_.end();++it){
   if(it->second.running)continue;
   if(it->second.item.deadline&&it->second.item.deadline<now)continue;
   bool ready=true;for(auto dep:it->second.item.dependencies)if(work_.contains(dep.id)||!completed_.contains(dep.id)||completed_.at(dep.id)!=dep.generation){ready=false;break;}
   if(!ready)continue;
   auto priority=it->second.item.priority;
   // Transitive priority inheritance, bounded by the live queue size.
   std::vector<std::uint64_t> targets{it->first};
   for(std::size_t i=0;i<targets.size()&&i<work_.size();++i)for(const auto& [id,row]:work_)
    if(std::find(row.item.dependencies.begin(),row.item.dependencies.end(),targets[i])!=row.item.dependencies.end()){
     priority=std::min(priority,row.item.priority);if(std::find(targets.begin(),targets.end(),id)==targets.end())targets.push_back(id);
    }
   if(chosen==work_.end()||priority<best){chosen=it;best=priority;}
  }
  if(chosen==work_.end())return {};
  chosen->second.running=true;return chosen->second.item;
 }
 bool finish(std::uint64_t id,std::uint64_t generation){auto it=work_.find(id);if(it==work_.end()||!it->second.running||it->second.item.generation!=generation)return false;completed_[id]=generation;work_.erase(it);trim();return true;}
 bool cancel(std::uint64_t id,std::uint64_t generation){auto it=work_.find(id);if(it==work_.end()||it->second.item.generation!=generation)return false;work_.erase(it);return true;}
 std::size_t size()const noexcept{return work_.size();}
 private:
 struct Row{WorkItem item;bool running=false;};
 std::size_t capacity_;std::map<std::uint64_t,Row> work_;std::map<std::uint64_t,std::uint64_t> completed_;
 bool needed(std::uint64_t key)const{for(const auto& [id,row]:work_){(void)id;if(std::find(row.item.dependencies.begin(),row.item.dependencies.end(),key)!=row.item.dependencies.end())return true;}return false;}
 void trim(){if(completed_.size()>capacity_*2)for(auto it=completed_.begin();it!=completed_.end()&&completed_.size()>capacity_;){bool needed=false;for(const auto& [id,row]:work_){(void)id;needed|=std::find(row.item.dependencies.begin(),row.item.dependencies.end(),it->first)!=row.item.dependencies.end();}if(needed)++it;else it=completed_.erase(it);}}
};
}
