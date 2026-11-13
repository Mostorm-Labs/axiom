# R2 single-variable zoom repair

R1 halved source passes but did not improve IAB and introduced Chrome70ms callback. Preserve it as UNMET. Read r1followup-static-hypothesis and originalperformance JSON; complex SkRegionclip likelycauses GPU cost but not established causalmeasurement.

Existing prepared nextVisible/drawContour culling sees boundinglocalclip, not disjointholes. ExistingjobSourcesquery covers entirecoarsecell/gutter. R2 may render the entirebbox aroundactualpaintregions with a simpledeviceclipRect, clear itwithSrc, replayallordered sources andallmask/material, publishbboxvalid onlyafterthejobiscomplete. This removes expensiveclipshape withoutavoidingunknowncontent ornewsemanticdisplaypolicy. Sharpoldimage still wins overlap in compose. Preservepan/no-retained path andGPUpolicy.

R1 nativecorrectness nowfullgrid+sharpresampleDPR1/2/4+Raster; rerunaffected withsameoracles. Needsourceattribution for known1-2channelblendquantization; no broadtolerance.

UseexactIABbaselinebeforeSnapshot percase fromr1 raw forALLbc40/r1/r2lanes, same1squiet/zoom. TwoChromeoppositelaneorders +oneIABpage maximally. Candidateonlyafterproductionstable. If stillunmetdonotiterateagainautomatically orpromote4335. ReturntoControlwithvalidworkretained.
