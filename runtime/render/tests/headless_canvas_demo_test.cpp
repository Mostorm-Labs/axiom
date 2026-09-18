#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "canvas/semantic/snapshot.hpp"
namespace fs = std::filesystem;
static std::vector<std::uint8_t> bytes(const fs::path& p){std::ifstream i(p,std::ios::binary);return {std::istreambuf_iterator<char>(i),{}};}
static std::uint64_t fnv(const std::vector<std::uint8_t>& b){std::uint64_t v=1469598103934665603ULL;for(auto x:b){v^=x;v*=1099511628211ULL;}return v;}
int main(){const fs::path root=G3_07_SOURCE_ROOT,fixture=root/"runtime/render/fixtures/g3-07/nine-kind-v1.axsnap";assert(fs::exists(fixture));const auto fb=bytes(fixture);assert(fb.size()==1791U&&fnv(fb)==0xc74f5632133e9de2ULL);const auto decoded=canvas::semantic::SnapshotCodec::decode(fb);assert(decoded.ok()&&decoded.snapshot->objects.size()==9U);for(std::uint64_t i=0;i<9U;++i){assert(decoded.snapshot->objects[i].id==canvas::foundation::ObjectId::fromUint64(501U+i));assert(static_cast<unsigned>(decoded.snapshot->objects[i].kind)==i+1U);}const fs::path scratch=fs::temp_directory_path()/"axiom-g3-07-demo-test";std::error_code ec;fs::remove_all(scratch,ec);fs::create_directories(scratch,ec);const std::string b=G3_07_DEMO_BINARY,f=fixture.string();const fs::path a=scratch/"a",c=scratch/"c";assert(std::system((b+" --fixture '"+f+"' --output-dir '"+a.string()+"'").c_str())==0);assert(std::system((b+" --fixture '"+f+"' --output-dir '"+c.string()+"'").c_str())==0);for(const char*n:{"render.rgba","render-digest.txt","render-evidence.json"})assert(bytes(a/n)==bytes(c/n));const auto raster=bytes(a/"render.rgba");assert(raster.size()==256U*256U*4U);const auto digest=bytes(a/"render-digest.txt");const std::string digestText(digest.begin(),digest.end());assert(digestText=="fnv1a64:7b8eb8c70c65afe5\n");assert(std::system((b+" --fixture '"+f+"' --output-dir '"+a.string()+"'").c_str())!=0);assert(bytes(a/"render.rgba")==raster);assert(std::system((b+" --fixture '"+(scratch/"missing.axsnap").string()+"' --output-dir '"+(scratch/"missing-out").string()+"'").c_str())!=0);assert(!fs::exists(scratch/"missing-out"));fs::remove_all(scratch,ec);return 0;}
