
namespace {
#if defined(AXIOM_MASK_PRIMITIVE_OBSERVATION)
void mpsGauge(const std::shared_ptr<NavigationMaskPlan>&p){if(!p)return;const auto&u=*p->usage;const auto src=p->pool->statistics();const auto rec=p->budget->snapshot(resources::ResourceClass::kRecorded);auto&a=mps::state.usage;a={p->capacity,u.used(),u.bytes,p->indexBytes,p->pathBytes,p->budget->used(),src.bytes,src.indexBytes};if(u.owner)for(unsigned i=0;i<7;++i)a[8+i]=u.owner->categories[i];if(auto*parts=mps::part(&u))for(unsigned i=0;i<3;++i)a[15+i]=parts->bytes[i];a[18]=rec.staging;a[19]=rec.pendingRelease;}
#endif

bool navigationPoint(semantic::Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=std::numeric_limits<float>::max()&&std::abs(p.y)<=std::numeric_limits<float>::max();}
SkRect navigationBounds(const semantic::EraseCubicSegment& s){
 SkRect r=SkRect::MakeLTRB(float(std::min({s.p0.position.x,s.p1.position.x,s.control1.x,s.control2.x})),float(std::min({s.p0.position.y,s.p1.position.y,s.control1.y,s.control2.y})),float(std::max({s.p0.position.x,s.p1.position.x,s.control1.x,s.control2.x})),float(std::max({s.p0.position.y,s.p1.position.y,s.control1.y,s.control2.y})));
 r.outset(float(std::max(s.p0.radius,s.p1.radius)),float(std::max(s.p0.radius,s.p1.radius)));
 r.fLeft=std::nextafter(r.fLeft,-INFINITY);r.fTop=std::nextafter(r.fTop,-INFINITY);r.fRight=std::nextafter(r.fRight,INFINITY);r.fBottom=std::nextafter(r.fBottom,INFINITY);return r;
}
bool navigationSegment(const semantic::EraseCubicSegment&s){return navigationPoint(s.p0.position)&&navigationPoint(s.p1.position)&&navigationPoint(s.control1)&&navigationPoint(s.control2)&&std::isfinite(s.p0.radius)&&std::isfinite(s.p1.radius)&&s.p0.radius>0&&s.p1.radius>0;}
bool navigationCommand(const semantic::PathCommand& c,SkRect* bounds=nullptr){
 bool valid=true;std::visit([&](const auto& v){const auto add=[&](semantic::Vec2 p){valid&=navigationPoint(p);if(bounds){const auto point=SkRect::MakeLTRB(float(p.x),float(p.y),std::nextafter(float(p.x),INFINITY),std::nextafter(float(p.y),INFINITY));bounds->join(point);}};if constexpr(requires{v.point;})add(v.point);if constexpr(requires{v.end;})add(v.end);if constexpr(requires{v.control;})add(v.control);if constexpr(requires{v.control1;}){add(v.control1);add(v.control2);}},c);return valid;
}
void navigationAppend(SkPathBuilder& b,const semantic::PathCommand& command){std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,semantic::MoveTo>)b.moveTo(float(v.point.x),float(v.point.y));else if constexpr(std::is_same_v<T,semantic::LineTo>)b.lineTo(float(v.end.x),float(v.end.y));else if constexpr(std::is_same_v<T,semantic::QuadTo>)b.quadTo(float(v.control.x),float(v.control.y),float(v.end.x),float(v.end.y));else if constexpr(std::is_same_v<T,semantic::CubicTo>)b.cubicTo(float(v.control1.x),float(v.control1.y),float(v.control2.x),float(v.control2.y),float(v.end.x),float(v.end.y));else if constexpr(std::is_same_v<T,semantic::ClosePath>)b.close();},command);}
bool navigationIntersects(SkRect local,SkCanvas& canvas){
 const auto matrix=canvas.getTotalMatrix();if(!local.isFinite()||!matrix.isFinite()||matrix.hasPerspective())return true;
 auto mapped=matrix.mapRect(local);
 const double magnitude=std::max({std::abs(double(local.left())),std::abs(double(local.top())),std::abs(double(local.right())),std::abs(double(local.bottom()))});
 const double error=64*std::numeric_limits<float>::epsilon()*((std::abs(double(matrix.getScaleX()))+std::abs(double(matrix.getSkewX()))+std::abs(double(matrix.getSkewY()))+std::abs(double(matrix.getScaleY())))*magnitude+std::abs(double(matrix.getTranslateX()))+std::abs(double(matrix.getTranslateY())));
 mapped.outset(float(3+error),float(3+error));return !mapped.isFinite()||SkRect::Intersects(mapped,SkRect::Make(canvas.getDeviceClipBounds()));
}
}
std::shared_ptr<NavigationMaskPlan> EraseMaskPathCache::requestNavigationHistory(
 const scene::SharedVector<semantic::EraseMaskRecord>& records,std::shared_ptr<resources::ResourceBudgetManager> budget,std::size_t capacity){
 MPS_ADD(Request);
 MPS_CALL(mpsRequest,RequestPhase);
 if(records.empty()||!budget||capacity<sizeof(NavigationMaskPage)+512)return {};auto& state=*impl_;const auto owner=records.storage();
 if(state.drawingCapacity){if(state.budget!=budget)clear();state.bindCapacity(budget,std::min(capacity,state.residentUpperBound));}
 const auto cap=std::min(state.drawingCapacity?state.drawingCapacity:std::size_t(8U*1024U*1024U),std::min(state.residentUpperBound,capacity));
 if(state.navigationUsage->owner!=state.usage){state.navigationHistories.clear();state.navigationReaper=state.navigationHistories.end();state.navigationUsage=std::make_shared<NavigationMaskUsage>();state.navigationUsage->owner=state.usage;}
 NavigationMaskIdentity key{{owner,sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),0},budget};
 if(auto it=state.navigationHistories.find(key);it!=state.navigationHistories.end()){it->second->tick=++state.tick;return it->second;}
 constexpr auto metadata=sizeof(NavigationMaskPlan)+sizeof(NavigationMaskIdentity)+512;
 state.navigationUsage->capacity=cap;
 // Reclaim at most64 idle identity rows per request. A denied request
 // retains all active pins and is resumed by the same navigation retry.
 std::size_t examined=0;
 while(!state.navigationHistories.empty()&&examined<64&&
     (state.navigationHistories.size()>=512||metadata>cap||state.navigationUsage->used()>cap-metadata)){
  if(state.navigationReaper==state.navigationHistories.end())state.navigationReaper=state.navigationHistories.begin();
  const auto it=state.navigationReaper++;
  ++examined;NM_ADD(indexUnits,1);
  if(it->second.use_count()!=1)continue;
  // Releasing one child is one reclamation unit. Advance past pinned and
  // partly retired rows fairly, rather than restarting at the map prefix.
  auto& old=*it->second;
  if(!old.paths.empty()){auto child=old.paths.begin();old.pathBytes-=child->second.primitive->bytes;old.paths.erase(child);continue;}
  if(!old.pages.empty()){old.indexBytes-=sizeof(NavigationMaskPage)+256;old.pages.erase(old.pages.begin());continue;}
  if(!old.accounting.empty()){old.accounting.pop_front();continue;}
  state.navigationHistories.erase(it);
 }

 if(state.navigationHistories.size()>=512)return {};
 if(metadata>cap)return {};
 if(state.navigationUsage->used()>cap-metadata){(void)state.evict(true);return {}; }
 auto token=budget->reserve(resources::ResourceClass::kRecorded,metadata,true);if(!token){NM_ADD(waits,1);return {};}
 auto p=std::make_shared<NavigationMaskPlan>();p->metadata=resources::ResourceLease(budget,*token);p->records=records;p->budget=std::move(budget);p->pool=state.sources;p->capacity=cap;p->tick=++state.tick;
 p->usage=state.navigationUsage;p->charge=std::make_shared<NavigationMaskCharge>();p->charge->usage=p->usage;p->charge->bytes=metadata;MPS_IF(mps::charge(p->charge.get(),p->usage.get(),0,metadata));p->usage->bytes+=metadata;p->usage->owner->bytes+=metadata;
 p->source=p->pool->lookupMasks(records,p->budget);
 if(!p->source){const auto bytes=sizeof(std::vector<semantic::EraseMaskRecord>)+records.capacity()*sizeof(semantic::EraseMaskRecord);auto t=p->budget->reserve(resources::ResourceClass::kRecorded,bytes+128,true);if(!t)return {};p->accounting.emplace_back(p->budget,*t);p->footprint=bytes;}
 state.navigationHistories.emplace(std::move(key),p);return p;
}

EraseMaskPathCache::NavigationMaskStatus EraseMaskPathCache::advanceNavigationHistory(const std::shared_ptr<NavigationMaskPlan>& p){
 MPS_CALL(mpsPrepare,PreparePhase);
 using Status=NavigationMaskStatus;if(!p||p->invalid)return Status::kInvalid;if(p->ready)return Status::kReady;
 NM_SCOPE(maskPrepare,maskNs);std::size_t units=0,commands=0;const auto& records=std::as_const(p->records);
 const auto index=[&](NavigationMaskBound item)->bool{
  const auto pageIndex=p->units/64,slot=p->units%64;auto it=p->pages.find(pageIndex);
  if(it==p->pages.end()&&!p->streaming){constexpr auto bytes=sizeof(NavigationMaskPage)+256;
   if(bytes>p->capacity/2||p->indexBytes>p->capacity/2-bytes||bytes>p->usage->capacity/2||p->usage->used()>p->usage->capacity/2-bytes)p->streaming=true;
   else{auto t=p->budget->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!t){NM_ADD(waits,1);return false;}auto page=std::make_shared<NavigationMaskPage>();page->lease=resources::ResourceLease(p->budget,*t);page->charge=std::make_shared<NavigationMaskCharge>();page->charge->usage=p->usage;page->charge->bytes=bytes;MPS_IF(mps::charge(page->charge.get(),p->usage.get(),1,bytes));p->usage->bytes+=bytes;p->usage->owner->bytes+=bytes;it=p->pages.emplace(pageIndex,std::move(page)).first;p->indexBytes+=bytes;NM_ADD(pages,1);MPS_ADD(Page);}
  }
  if(it!=p->pages.end()){auto& page=*it->second;page.items[slot]=item;page.size=slot+1;if(slot)page.bounds.join(item.bounds);else page.bounds=item.bounds;}
  if(p->units)p->bounds.join(item.bounds);else p->bounds=item.bounds;
  ++p->units;NM_ADD(indexUnits,1);return true;
 };
 while(p->record<records.size()&&units<64&&commands<256){
  const auto& record=records[p->record];
  if(!p->header){
   NM_ADD(sourceUnits,1);MPS_ADD(SourceUnit);NM_ADD(accounted,1);++units;
   if(!p->source){const auto bytes=std::visit([](const auto& g)->std::size_t{if constexpr(requires{g.segments;})return g.segments.capacity()*sizeof(semantic::EraseCubicSegment);else return g.path.commands.capacity()*sizeof(semantic::PathCommand);},record.geometry);
    auto t=p->budget->reserve(resources::ResourceClass::kRecorded,bytes+sizeof(resources::ResourceLease)*2+128,true);if(!t){NM_ADD(waits,1);return Status::kPending;}p->accounting.emplace_back(p->budget,*t);p->footprint+=bytes;
   }p->header=true;p->filledBounds=SkRect::MakeEmpty();
  }
  if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&record.geometry)){
   if(sweep->segments.empty()){p->invalid=true;return Status::kInvalid;}
   while(p->primitive<sweep->segments.size()&&units<64){
    const auto& s=sweep->segments[p->primitive];if(!navigationSegment(s)){p->invalid=true;return Status::kInvalid;}
    if(!index({p->record,p->primitive,navigationBounds(s),false}))return Status::kPending;
    ++p->primitive;++units;NM_ADD(validated,1);NM_ADD(sourceUnits,1);MPS_ADD(SourceUnit);
   }
   if(p->primitive<sweep->segments.size())return Status::kPending;
  }else{
   const auto& path=std::get<semantic::FilledPathMask>(record.geometry).path;
   if(path.fill_rule!=semantic::FillRule::kNonZero&&path.fill_rule!=semantic::FillRule::kEvenOdd){p->invalid=true;return Status::kInvalid;}
   while(p->commands<path.commands.size()&&commands<256){
    if(!navigationCommand(path.commands[p->commands],&p->filledBounds)){p->invalid=true;return Status::kInvalid;}
    ++p->commands;++commands;NM_ADD(validated,1);NM_ADD(pathCommands,1);MPS_ADD(Command);
   }
   if(p->commands<path.commands.size())return Status::kPending;
   if(!index({p->record,0,p->filledBounds,true}))return Status::kPending;
  }
  ++p->record;p->primitive=p->commands=0;p->header=false;
 }
 if(p->record<records.size())return Status::kPending;
 if(!p->source){p->source=p->pool->adoptMasks(p->records,p->budget,p->footprint,p->accounting);if(!p->source){NM_ADD(waits,1);return Status::kPending;}}
 // A canonical consumer can win adoption while this job is preparing. The
 // source pin is authoritative; duplicate construction charges retire under
 // the same64-unit allowance, including prior validation/accounting work.
 while(!p->accounting.empty()&&units<64){p->accounting.pop_front();++units;NM_ADD(sourceUnits,1);MPS_ADD(SourceUnit);}
 if(!p->accounting.empty())return Status::kPending;
 p->ready=true;return Status::kReady;
}
EraseMaskPathCache::NavigationMaskStatus EraseMaskPathCache::replayNavigationHistory(
 const std::shared_ptr<NavigationMaskPlan>& p,SkCanvas& canvas,NavigationMaskReplay& cursor,std::size_t drawLimit){
 MPS_ADD(Lookup);
#if defined(AXIOM_MASK_PRIMITIVE_OBSERVATION)
 mpsGauge(p);
#endif
 MPS_CALL(mpsReplay,SelectPhase);
 using Status=NavigationMaskStatus;if(!p||!p->ready||p->invalid)return Status::kInvalid;cursor.lastDraws=0;NM_SCOPE(maskReplay,maskNs);if(cursor.initialized)NM_ADD(resumes,1);cursor.initialized=true;
#if !defined(AXIOM_MPS_NO_SHARE)
 MPS_PHASE(LookupPhase);
 std::shared_ptr<MaskHistoryPlan> shared;
 const auto storage=p->records.storage();
 NavigationMaskIdentity identity{{storage,sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),0},p->budget};
 if(auto it=impl_->drawIdentities->plans.find(identity);it!=impl_->drawIdentities->plans.end()){
  shared=it->second.lock();if(shared&&shared->usage!=p->usage->owner)shared.reset();
 }
#endif
 const auto discardConstruction=[&]{cursor.filled.reset();cursor.filledCommand=0;cursor.staging.reset();cursor.buildingPlan.reset();cursor.buildingOrdinal=0;};
 if(cursor.filled&&(cursor.buildingPlan!=p||cursor.buildingOrdinal!=cursor.ordinal))discardConstruction();
 std::size_t examined=0,drawn=0,commandsBuilt=0;const auto& records=std::as_const(p->records);
 while(cursor.record<records.size()&&examined<64){
  MPS_PHASE(SelectPhase);
  const auto& record=records[cursor.record];const auto* sweep=std::get_if<semantic::SweptCircleMask>(&record.geometry);
  const auto count=sweep?sweep->segments.size():std::size_t(1);
  if(cursor.primitive>=count){++cursor.record;cursor.primitive=0;continue;}
  const auto pageIt=p->pages.find(cursor.ordinal/64);const auto offset=cursor.ordinal%64;
  if(pageIt!=p->pages.end()&&offset==0&&pageIt->second->size==64&&!navigationIntersects(pageIt->second->bounds,canvas)){
   discardConstruction();const auto& last=pageIt->second->items[63];cursor.record=last.record;cursor.primitive=last.primitive+1;cursor.ordinal+=64;
   ++examined;NM_ADD(indexUnits,1);continue;
  }
  SkRect bounds;
  if(pageIt!=p->pages.end())bounds=pageIt->second->items[offset].bounds;
  else if(sweep)bounds=navigationBounds(sweep->segments[cursor.primitive]);
  else bounds=SkRect::MakeLTRB(-INFINITY,-INFINITY,INFINITY,INFINITY);
  ++examined;NM_ADD(candidates,1);MPS_ADD(Candidate);
  if(!navigationIntersects(bounds,canvas)){discardConstruction();++cursor.primitive;++cursor.ordinal;continue;}
  if(drawn>=drawLimit)return Status::kPending;
#if !defined(AXIOM_MPS_NO_SHARE)
  if(shared&&cursor.record<shared->primitiveEnds.size()){
   const auto begin=cursor.record?shared->primitiveEnds[cursor.record-1]:0;
   const auto end=shared->primitiveEnds[cursor.record];
   if(cursor.primitive<end-begin){MPS_ADD(Eligible);MPS_ADD(Reused);
    discardConstruction();const auto& primitive=shared->primitives[begin+cursor.primitive];
    MPS_PHASE(DrawPhase);(void)primitive->lease.markGpuUse();primitive->draw(canvas);
    ++drawn;++cursor.lastDraws;MPS_ADD(Draw);NM_ADD(drawn,1);++cursor.primitive;++cursor.ordinal;continue;
   }
  }
#endif
  MPS_ADD(NotResident);
  MPS_PHASE(LookupPhase);
  const auto key=std::pair{cursor.record,cursor.primitive};auto found=p->paths.find(key);
  if(found!=p->paths.end()){MPS_ADD(NavHit);discardConstruction();}
  if(found==p->paths.end()){MPS_PHASE(AllocatePhase);
   const auto* filled=sweep?nullptr:&std::get<semantic::FilledPathMask>(record.geometry);
   const auto units=filled?filled->path.commands.size():4;
   const auto estimate=sizeof(PreparedMaskPrimitive)+65536U+units*256U;
   // Oversized whole FilledPath still has one clear. It gets a temporary
   // admitted job reservation rather than a permanent allresident prerequisite.
   const bool transient=estimate>p->capacity;
   if(!cursor.filled){
    auto t=p->budget->reserve(resources::ResourceClass::kRecorded,estimate,true,0);if(!t){NM_ADD(waits,1);return Status::kPending;}cursor.staging=resources::ResourceLease(p->budget,*t);
    cursor.buildingPlan=p;cursor.buildingOrdinal=cursor.ordinal;
    cursor.filled=std::make_unique<SkPathBuilder>(filled&&filled->path.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
   }
   MPS_PHASE(BuildPhase);
   if(filled){const auto end=std::min(cursor.filledCommand+(256-commandsBuilt),filled->path.commands.size());for(;cursor.filledCommand<end;++cursor.filledCommand){navigationAppend(*cursor.filled,filled->path.commands[cursor.filledCommand]);++commandsBuilt;NM_ADD(pathCommands,1);MPS_ADD(Command);}if(cursor.filledCommand<filled->path.commands.size())return Status::kPending;}
   else{const auto& s=sweep->segments[cursor.primitive];cursor.filled->moveTo(float(s.p0.position.x),float(s.p0.position.y));cursor.filled->cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));}
   auto primitive=makePreparedMaskPrimitive();primitive->path=cursor.filled->detach();primitive->bounds=bounds;primitive->swept=bool(sweep);if(sweep)primitive->width=float(2*std::max(sweep->segments[cursor.primitive].p0.radius,sweep->segments[cursor.primitive].p1.radius));
   const auto bytes=sizeof(PreparedMaskPrimitive)+256+primitive->path.approximateBytesUsed();
   // Disposable geometry does not own a full-history LRU scan. Retire
   // at most one prepared primitive; otherwise use the admitted temporary.
   MPS_PHASE(RetirePhase);
   if(!p->paths.empty()&&(bytes>p->usage->capacity||p->usage->used()>p->usage->capacity-bytes)){
    auto victim=p->paths.begin();p->pathBytes-=victim->second.primitive->bytes;p->paths.erase(victim);
   }
   if(bytes>estimate||!cursor.staging.shrink(bytes))return Status::kInvalid;
   primitive->lease=std::move(cursor.staging);primitive->bytes=bytes;discardConstruction();
   NM_ADD(pathBuilds,1);MPS_ADD(NewBuild);
   if(transient||bytes>p->capacity-p->indexBytes||bytes>p->usage->capacity||p->usage->used()>p->usage->capacity-bytes){MPS_ADD(Transient);cursor.ready=std::move(primitive);}
   else{MPS_ADD(Resident);auto charge=std::make_shared<NavigationMaskCharge>();charge->usage=p->usage;charge->bytes=bytes;MPS_IF(mps::charge(charge.get(),p->usage.get(),2,bytes));p->usage->bytes+=bytes;p->usage->owner->bytes+=bytes;primitive->derivedCharge=charge;found=p->paths.emplace(key,NavigationMaskPlan::Cached{std::move(primitive),++p->tick,std::move(charge)}).first;p->pathBytes+=bytes;}
  }
  MPS_PHASE(DrawPhase);
  if(cursor.ready){(void)cursor.ready->lease.markGpuUse();cursor.ready->draw(canvas);cursor.ready.reset();}else{found->second.used=++p->tick;(void)found->second.primitive->lease.markGpuUse();found->second.primitive->draw(canvas);}++drawn;++cursor.lastDraws;MPS_ADD(Draw);NM_ADD(drawn,1);++cursor.primitive;++cursor.ordinal;
 }
 return cursor.record==records.size()?Status::kReady:Status::kPending;
}
