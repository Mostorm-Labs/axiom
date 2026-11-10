#include "canvas/semantic/snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace canvas;
semantic::SemanticSnapshot read(const char* name){std::ifstream f(name,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto s=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!s.ok())throw 1;return *s.snapshot;}
int main(int argc,char**argv){if(argc!=3)return 2;auto a=read(argv[1]),b=read(argv[2]);bool eq=a.objects==b.objects&&a.document_id==b.document_id&&a.manifest==b.manifest&&a.manifest_present==b.manifest_present&&a.background==b.background&&a.capability_requirements==b.capability_requirements;unsigned masks=0;for(auto& o:a.objects)masks+=o.erase_masks.size();std::cout<<"{\"objects\":"<<a.objects.size()<<",\"total_masks\":"<<masks<<",\"objects_equal\":"<<(a.objects==b.objects?"true":"false")<<",\"manifest_background_equal\":"<<(eq?"true":"false")<<",\"document_digest_equal\":"<<(a.document_digest==b.document_digest?"true":"false")<<",\"before_revision\":"<<a.document_revision<<",\"after_revision\":"<<b.document_revision<<"}"<<std::endl;return eq?0:1;}
