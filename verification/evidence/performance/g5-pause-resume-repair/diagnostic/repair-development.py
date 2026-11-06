from pathlib import Path

def edit(name,f):
 p=Path(name);s=p.read_bytes().decode();t=f(s);assert t!=s,name;p.write_bytes(t.encode())
edit('runtime/render/include/canvas/render/skia_renderer.hpp',lambda s:s.replace('    std::shared_ptr<NavigationCache> navigationCache_;','    std::shared_ptr<NavigationCache> navigationCache_;\n    std::shared_ptr<NavigationCache> navigationFallback_;'))
edit('runtime/render/src/skia_navigation_tiles.hpp',lambda s:s.replace('    std::function<void(SkCanvas&,const foundation::WorldRect&)> draw;','    std::function<void(SkCanvas&,const foundation::WorldRect&)> draw;\n    const RetainedNavigationSource* fallback=nullptr;').replace('double density;bool retained=false;','double density;const RetainedNavigationSource* retained=nullptr;').replace('const CanonicalViewportTransform&,const FrameState&,\n        const RetainedNavigationSource*);','const CanonicalViewportTransform&,const FrameState&);'))
def nav(s):
 s=s.replace('double density;bool retained;','double density;const RetainedNavigationSource* retained;').replace('std::ldexp(1.,key.scale),false','std::ldexp(1.,key.scale),nullptr')
 old='''    if(retained&&retained->draw&&retained->density>0&&retained->bounds.isFiniteAndOrdered())
        candidates.push_back({{},retained->bounds,retained->density,true});'''
 new='''    // At most two qualified canonical planes. Each keeps its own density and
    // world bounds; the normal disjoint partition still picks sharper pixels.
    auto* source=retained;
    for(unsigned i=0;source&&i<2;++i,source=source->fallback)
        if(source->draw&&source->density>0&&source->bounds.isFiniteAndOrdered())
            candidates.push_back({{},source->bounds,source->density,source});'''
 assert old in s;s=s.replace(old,new)
 s=s.replace('const CanonicalViewportTransform& camera,const FrameState& frame,const RetainedNavigationSource* retained){','const CanonicalViewportTransform& camera,const FrameState& frame){')
 s=s.replace('if(patch.retained){retained->draw(canvas,patch.area);continue;}','if(patch.retained){patch.retained->draw(canvas,patch.area);continue;}')
 s=s.replace('compose(*target,patches,camera,frame,retained)','compose(*target,patches,camera,frame)')
 return s
edit('runtime/render/src/skia_navigation_tiles.cpp',nav)
def renderer(s):
 old='''    bool backgroundProofRejected=false;
};'''
 new='''    bool backgroundProofRejected=false;
    bool matches(const void* target,const FrameState& current)const noexcept {
        return image&&frame&&provider==target&&frame->viewId==current.viewId&&
            frame->sceneGeneration==current.sceneGeneration&&frame->sceneReadToken==current.sceneReadToken&&
            frame->metrics==current.metrics&&frame->metricsGeneration==current.metricsGeneration&&
            frame->surfaceGeneration==current.surfaceGeneration&&frame->viewportClip==current.viewportClip&&
            transform.a>0&&std::isfinite(transform.a)&&transform.d==transform.a&&transform.b==0&&transform.c==0&&
            std::isfinite(transform.tx)&&std::isfinite(transform.ty);
    }
    foundation::WorldRect worldBounds()const noexcept {
        return {float(-transform.tx/transform.a),float(-transform.ty/transform.a),
            float((frame->metrics.logicalWidth-transform.tx)/transform.a),
            float((frame->metrics.logicalHeight-transform.ty)/transform.a)};
    }
    double worldArea()const noexcept {
        return frame->metrics.logicalWidth*frame->metrics.logicalHeight/(transform.a*transform.a);
    }
};'''
 assert old in s;s=s.replace(old,new,1)
 s=s.replace('''                navigationCache_.reset();
                // Retiring the optional whole frame''','''                navigationFallback_.reset();
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                navigationCache_.reset();
                // Retiring the optional whole frame''',1)
 s=s.replace('''                navigationCache_.reset();
                const auto shared=''','''                const auto previous=navigationCache_;
                navigationCache_.reset();
                const auto shared=''',1)
 s=s.replace('''                    navigationCache_=std::move(candidate);''','''                    // Keep one previous qualified plane for a resumed gesture.
                    // A narrow sharp zoom-in must not erase all wider coverage.
                    // These are existing admitted images, never another copy or pool.
                    std::shared_ptr<NavigationCache> fallback;
                    const auto consider=[&](const std::shared_ptr<NavigationCache>& old){
                        if(old&&old->matches(&provider,plan.frame)&&old->transform!=candidate->transform&&
                           old->worldBounds().intersects(candidate->worldBounds())&&
                           (!fallback||old->worldArea()>fallback->worldArea()))fallback=old;
                    };
                    consider(previous);consider(navigationFallback_);
                    navigationFallback_=std::move(fallback);
                    navigationCache_=std::move(candidate);''',1)
 s=s.replace('''            }else navigationCache_.reset();
        }catch(const std::bad_alloc&){navigationCache_.reset();}''','''            }else{navigationCache_.reset();navigationFallback_.reset();}
        }catch(const std::bad_alloc&){navigationCache_.reset();navigationFallback_.reset();}''',1)
 old='''    internal::RetainedNavigationSource retained;
    const auto cache=navigationCache_;
    if(cache&&cache->image&&cache->frame&&cache->provider==&provider&&
       cache->frame->viewId==current.viewId&&cache->frame->sceneGeneration==current.sceneGeneration&&
       cache->frame->sceneReadToken==current.sceneReadToken&&cache->frame->metrics==current.metrics&&
       cache->frame->metricsGeneration==current.metricsGeneration&&cache->frame->surfaceGeneration==current.surfaceGeneration&&
       cache->frame->viewportClip==current.viewportClip&&cache->transform.a>0&&std::isfinite(cache->transform.a)&&
       cache->transform.d==cache->transform.a&&cache->transform.b==0&&cache->transform.c==0){'''
 new='''    internal::RetainedNavigationSource retained,fallback;
    const auto bindRetained=[&](const std::shared_ptr<NavigationCache>& cache,internal::RetainedNavigationSource& retained){
      if(cache&&cache->matches(&provider,current)){'''
 assert old in s;s=s.replace(old,new,1)
 old='''        };
    }
    const auto drawOverlay=[&](SkCanvas& canvas){'''
 new='''        };
      }
    };
    bindRetained(navigationCache_,retained);bindRetained(navigationFallback_,fallback);
    if(!retained.draw)retained=std::move(fallback);
    else if(fallback.draw)retained.fallback=&fallback;
    const auto drawOverlay=[&](SkCanvas& canvas){'''
 assert old in s;s=s.replace(old,new,1)
 s=s.replace('navigationTiles_.reset();navigationCache_.reset();if(renderCache_)','navigationTiles_.reset();navigationCache_.reset();navigationFallback_.reset();if(renderCache_)')
 return s
edit('runtime/render/src/skia_renderer.cpp',renderer)
