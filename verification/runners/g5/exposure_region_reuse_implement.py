from pathlib import Path
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');S=R/'out/G5-exposure-region-reuse-r1/source-C'
# Region value stays a private source/developer value, never consumer ABI.
p=S/'runtime/render/include/canvas/render/tile/tile_grid.hpp';s=p.read_text();s=s.replace('std::int32_t scale=0;auto operator<=>(const TileKey&)', 'std::int32_t scale=0;std::uint8_t region=0;auto operator<=>(const TileKey&)');old='''foundation::WorldRect bounds(TileKey key)const{const double unit=std::ldexp(size_,-key.scale);return {float(key.x*unit),float(key.y*unit),float((key.x+1)*unit),float((key.y+1)*unit)};}''';new='''foundation::WorldRect bounds(TileKey key)const{const double unit=std::ldexp(size_,-key.scale);if(key.region){const auto index=unsigned(key.region-1);const auto sub=unit/4;return{float(key.x*unit+(index%4)*sub),float(key.y*unit+(index/4)*sub),float(key.x*unit+(index%4+1)*sub),float(key.y*unit+(index/4+1)*sub)};}return {float(key.x*unit),float(key.y*unit),float((key.x+1)*unit),float((key.y+1)*unit)};}''';assert old in s;s=s.replace(old,new);p.write_text(s)
p=S/'runtime/render/src/skia_navigation_tiles.hpp';s=p.read_text().replace('#include "gpu_resource_retirement.hpp"','#include "gpu_resource_retirement.hpp"\n#include "skia_ink_layers.hpp"');s=s.replace('std::optional<NavigationSceneEmptyProof> emptyProof;', 'std::optional<NavigationSceneEmptyProof> emptyProof;\n        foundation::WorldRect queried{};');s=s.replace('bool cpuCoverage=false,gpuCoarse=false;','bool cpuCoverage=false,gpuCoarse=false;\n        std::size_t pixelBytes=0;\n        sk_sp<SkImage> sharedBody;');s=s.replace('std::shared_ptr<PreparedGeometryCache> geometry_;','std::shared_ptr<PreparedGeometryCache> geometry_;\n    SkiaInkLayers bodyLayers_;');p.write_text(s)
p=S/'runtime/render/src/skia_navigation_tiles.cpp';s=p.read_text()
s=s.replace('cancel();tiles_.clear();sources_.clear();','cancel();bodyLayers_.clear();tiles_.clear();sources_.clear();',1)
s=s.replace('if(scene_!=scene){cancel();', 'if(scene_!=scene){cancel();',1)
s=s.replace('if(!work.coverageComplete&&job_&&job_->key.scale>parents.front().scale&&!fillsMissing(*job_)){', 'if(!work.coverageComplete&&job_&&!job_->coveragePriority){',1)
# Regionqueryfull68 exactunit; stillsafeactualholecomponentcull below.
s=s.replace('bytes,diagnostics.sceneStamped,std::move(emptyProof)}', 'bytes,diagnostics.sceneStamped,std::move(emptyProof),queriedRegion}')
s=s.replace('''            if(!next&&!work.coverageComplete)for(const auto key:parents)if(!ready(key)&&
                std::any_of(missing.begin(),missing.end(),[&](const auto& area){return intersects(grid_.bounds(key),area);})){next=key;break;}''', '''            if(!next&&!work.coverageComplete){
                for(const auto parent:parents){
                    if(retained&&retained->draw){for(unsigned n=1;n<=16;++n){auto key=parent;key.region=std::uint8_t(n);
                        if(!ready(key)&&std::any_of(missing.begin(),missing.end(),[&](const auto&area){return intersects(grid_.bounds(key),area);})){next=key;break;}}
                    }else if(!ready(parent)&&std::any_of(missing.begin(),missing.end(),[&](const auto&area){return intersects(grid_.bounds(parent),area);}))next=parent;
                    if(next)break;
                }
            }''',1)
# New frontier must precede unserved quality in incompleteviewport (fairness resumes oncecomplete).
s=s.replace('if(bestQualityAge>=11&&!usefulQualityThisCall&&bestQuality&&!ready(*bestQuality))next=bestQuality;', 'if(work.coverageComplete&&bestQualityAge>=11&&!usefulQualityThisCall&&bestQuality&&!ready(*bestQuality))next=bestQuality;')
s=s.replace('''            const auto peak=tileBytes*3+tileMetadataBytes;''', '''            const auto renderExtent=next->region?68:extent;
            const auto renderBytes=std::size_t(renderExtent)*renderExtent*4;
            const auto peak=renderBytes*3+tileMetadataBytes;''',1)
s=s.replace('''peak,true,tileBytes+tileMetadataBytes''','''peak,true,renderBytes+tileMetadataBytes''')
s=s.replace('auto job=std::make_unique<Job>();job->key=*next;', 'auto job=std::make_unique<Job>();job->pixelBytes=renderBytes;job->key=*next;',1)
s=s.replace('makeWH(extent,extent)','makeWH(renderExtent,renderExtent)',1).replace('MakeN32Premul(extent,extent)','MakeN32Premul(renderExtent,renderExtent)',1).replace('MakeA8(extent,extent)','MakeA8(renderExtent,renderExtent)',1)
# source-specificv4capqualification,nonv4oldpath unchanged.
a=s.index('            job->gpuCoarse=coarseV4&&');b=s.index('            // Unprepared, raster-only',a);s=s[:a]+'''            job->gpuCoarse=next->scale<level&&context&&options.gpuCoverageCandidate&&
                std::any_of(job->sources->geometry.begin(),job->sources->geometry.end(),[](const auto&g){return g&&g->isContour();});
'''+s[b:]
# Region completion boundedrect/actual missing hole, keepvalidoldregionsseparately.
a=s.index('            if(zoomExposure){\n                // Primitive selection');b=s.index('            if(reusable)',a);s=s[:a]+s[b:]
# Restrict regionpaint to oneactualhole+texelsnap instead of boundingfourholes.
needle='''            if(reusable){(void)old->second.lease.markGpuUse();job->valid=old->second.valid;}'''
s=s.replace(needle,'''            if(next->region){
                demand=b;
                for(const auto&hole:missing)if(intersects(hole,b)){
                    auto r=foundation::WorldRect{std::max(b.left,hole.left),std::max(b.top,hole.top),std::min(b.right,hole.right),std::min(b.bottom,hole.bottom)};
                    r={std::max(b.left,b.left+std::floor((r.left-b.left)*s)/s),std::max(b.top,b.top+std::floor((r.top-b.top)*s)/s),std::min(b.right,b.left+std::ceil((r.right-b.left)*s)/s),std::min(b.bottom,b.top+std::ceil((r.bottom-b.top)*s)/s)};
                    demand=r;break;
                }
                job->paint.reset(demand);
            }
'''+needle,1)
# Cache/sharedsource actualbounds filtering beforeeachsource; noauthority inference.
s=s.replace('    const auto& entry=job.sources->entries[job.source];','''    const auto& entry=job.sources->entries[job.source];
        if(!job.sourceInitialized&&job.key.region&&!intersects(entry.record.worldBounds,job.demand)){
            ++job.source;++work.sourceSteps;++steps;continue;
        }''',1)
# Needcompletedregionpublication68 rather thanupperboundwholeimage
s=s.replace('gpu.publish(job.lease,tileBytes+tileMetadataBytes)','gpu.publish(job.lease,job.pixelBytes+tileMetadataBytes)',1)
# skipbodycache until sourceinitialization then qualify onreadygeometry andlocalmatrix.
needle='''                job.sourceInitialized = true;
            }'''
s=s.replace(needle,'''                job.sourceInitialized = true;
                if(job.key.region&&job.geometry&&job.geometry->isContour()){
                    job.sharedBody=bodyLayers_.navigationBody(job.geometry,*draw,options,budget);
                    if(job.sharedBody){job.knot=range.second;job.primitive=job.geometry->primitiveCount();}
                }
            }''',1)
# source-specific cache bodyrender isfullblockcoverage. drawclip originallydemand limits pixelcache keyclip; includeclip inkey.
s=s.replace('''            if(stroke.config.algorithmVersion==4 && job.geometry&&job.geometry->isContour()){''', '''            if(job.sharedBody){
                draw->save();draw->resetMatrix();draw->drawImage(job.sharedBody,0,0);draw->restore();end=range.second;
            }else if(stroke.config.algorithmVersion==4 && job.geometry&&job.geometry->isContour()){''',1)
needle='''            if (!entry.record.eraseMasks.empty())
                drawEraseMasksToSkCanvas(*draw, entry);'''
s=s.replace(needle,'''            if(job.key.region&&job.geometry&&job.geometry->isContour()&&!job.sharedBody)
                bodyLayers_.rememberNavigationBody(job.geometry,*draw,job.coverage->makeImageSnapshot(),options,budget);
'''+needle,1)
s=s.replace('++job.source;job.knot=0;job.sourceInitialized=false;job.geometry.reset();', '++job.source;job.knot=0;job.sourceInitialized=false;job.geometry.reset();job.sharedBody.reset();',1)
p.write_text(s)
