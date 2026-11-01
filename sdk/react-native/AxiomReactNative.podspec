require 'json'
package = JSON.parse(File.read(File.join(__dir__, 'package.json')))
Pod::Spec.new do |s|
 s.name = 'AxiomReactNative'
 s.version = package['version']
 s.summary = 'Shared Axiom Controller and native Skia CanvasView'
 s.homepage = 'https://github.com/Mostorm-Labs/axiom'
 s.author = 'Mostorm Labs'
 s.license = { :type => 'Proprietary', :text => 'See the matching SDK licenses directory.' }
 s.source = { :git => 'https://github.com/Mostorm-Labs/axiom.git', :tag => "v#{s.version}" }
 s.platforms = { :ios => '17.0', :osx => '15.0' }
 s.source_files = 'apple/*.{h,mm}'
 s.private_header_files = 'apple/*.h'
 s.requires_arc = true
 # Built by tools/release/build_native_sdk.py from accepted locked dependencies.
 # No internal Runtime/Skia headers or dependency archives are installed.
 s.ios.vendored_frameworks = 'native/apple/AxiomNative.xcframework'
 s.osx.vendored_frameworks = 'native/macos/AxiomNative.framework'
 s.resources = 'native/AxiomAssets.bundle'
 s.frameworks = 'Metal', 'QuartzCore'
 s.ios.frameworks = 'UIKit'
 s.osx.frameworks = 'AppKit'
 s.pod_target_xcconfig = { 'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20' }
 install_modules_dependencies(s)
end
