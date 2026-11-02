const path = require('path');
const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');

const projectRoot = __dirname;
const packageRoot = path.resolve(projectRoot, '../rn-macos-navigation-f4d1/package');
const config = {
  watchFolders: [packageRoot],
  resolver: {
    nodeModulesPaths: [path.resolve(projectRoot, 'node_modules')],
    disableHierarchicalLookup: true,
  },
};
module.exports = mergeConfig(getDefaultConfig(projectRoot), config);
