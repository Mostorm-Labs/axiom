// Validate consumed matrix coverage; no component-to-SDK promotion.
import fs from 'node:fs/promises';import {assertTierCoverage} from './complex_scale_metrics.mjs';
const matrix=JSON.parse(await fs.readFile(process.argv[2],'utf8'));
for(const distribution of matrix.profile.object_distributions)
 assertTierCoverage(matrix.rows.filter(r=>r.scope==='root-sdk'&&r.distribution===distribution&&r.dpr===2),'objects',matrix.profile.object_tiers);
assertTierCoverage(matrix.rows.filter(r=>r.scope==='root-sdk'&&r.dpr===2),'points',matrix.profile.point_tiers);
const summary={schema:matrix.schema,engineering_only:true,sourceStable:matrix.sourceStable,
 rows:matrix.rows.length,measured:matrix.rows.filter(r=>r.classification==='MEASURED').length,
 blocked:matrix.rows.filter(r=>r.classification==='BLOCKED_EVIDENCE').map(r=>({id:r.id,reason:r.reason,stage:r.worker?.last_stage??null})),
 largestMeasuredRootTier:{}};
for(const axis of ['objects','points'])for(const distribution of axis==='objects'?matrix.profile.object_distributions:['sparse'])
 summary.largestMeasuredRootTier[axis+'-'+distribution]=Math.max(0,...matrix.rows.filter(r=>r.scope==='root-sdk'&&r.axis===axis&&r.distribution===distribution&&r.dpr===2&&r.classification==='MEASURED').map(r=>r.count));
if(!matrix.sourceStable)throw Error('source drift');
await fs.writeFile(process.argv[3],JSON.stringify(summary,null,2)+'\n');console.log(JSON.stringify(summary));
