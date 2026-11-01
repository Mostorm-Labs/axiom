module.exports={dependency:{platforms:{
 android:{sourceDir:'./android',packageImportPath:'import dev.mostorm.axiom.AxiomPackage;',packageInstance:'new AxiomPackage()'},
 ios:{podspecPath:'./AxiomReactNative.podspec'},macos:{podspecPath:'./AxiomReactNative.podspec'},
 windows:{sourceDir:'./windows',projects:[{projectFile:'AxiomReactNative.vcxproj',directDependency:true}],cppHeaders:['winrt/MostormAxiom.h'],cppPackageProviders:['MostormAxiom::ReactPackageProvider']}
}}};
