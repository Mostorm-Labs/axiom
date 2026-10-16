#include "canvas/semantic/resource_manifest.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/apply_source.hpp"
#include <gtest/gtest.h>
using namespace canvas::semantic;
namespace { ResourceManifestEntry validEntry(){ResourceManifestEntry e; e.resource_id=ResourceId{canvas::foundation::ObjectId::fromUint64(1)}; e.resource_revision=1; e.content_hash="sha256:"+std::string(64,'a'); e.byte_length=1; e.decode_version=1; e.metadata={0x08,0x01,0x10,0x01,0x18,0x01,0x20,0x01}; return e;} }
TEST(ResourceManifest, ValidEntryAndCanonicalOrdering){ResourceManifest m; auto a=validEntry(); auto b=validEntry(); b.resource_id=ResourceId{canvas::foundation::ObjectId::fromUint64(2)}; m.entries={b,a}; EXPECT_TRUE(normalizeResourceManifest(m)); EXPECT_EQ(m.entries.front().resource_id.value, a.resource_id.value);}
TEST(ResourceManifest, RejectsUppercaseHash){auto e=validEntry();e.content_hash[7]='A';EXPECT_FALSE(validateResourceManifestEntry(e));}
TEST(ResourceManifest, RejectsDuplicateIds){auto e=validEntry();ResourceManifest m;m.entries={e,e};EXPECT_FALSE(validateResourceManifest(m));}

TEST(ResourceManifest, OperationEngineAppliesCreateAndEmitsResourceChange) {
 ResourceManifest manifest; ReferenceObjectStore objects; AppliedOperationLedger ledger; SemanticGenerationState generation; CanonicalCommitClock clock(RuntimeEpoch(1)); OperationEngine engine;
 ResourceManifestEntry e=validEntry(); UpdateResourceManifestOp payload; payload.expected_manifest_revision=0; payload.upserts={e}; payload.expectations={{e.resource_id,ResourceExpectationState::kNeverUsed,0}}; Operation op{}; op.id=OperationId{canvas::foundation::ObjectId::fromUint64(50)}; op.document_id=DocumentId{canvas::foundation::ObjectId::fromUint64(1000)}; op.schema_version=1; op.payload_version=1; op.payload=payload;
 auto result=engine.apply(op,ApplySource::kLocalInteraction,objects,manifest,ledger,generation,clock); ASSERT_EQ(result.disposition,ApplyDisposition::kApplied); ASSERT_EQ(manifest.entries.size(),1U); ASSERT_TRUE(result.commit_record.has_value()); ASSERT_EQ(result.commit_record->change_set.resourceChanges().size(),1U); EXPECT_EQ(result.commit_record->change_set.resourceChanges()[0].flags,ResourceManifestChangeFlags::kCreated);
}


