const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const app = fs.readFileSync(path.join(root, 'prototype/app.js'), 'utf8');
const native = fs.readFileSync(path.join(root, 'native/Source/MainComponent.cpp'), 'utf8');
const cmake = fs.readFileSync(path.join(root, 'CMakeLists.txt'), 'utf8');
const mapping = app.match(/const kongInstrumentArtwork = \{([\s\S]*?)\n\};/)[1];
for (const name of ['xun', 'liuqin', 'sanxian']) {
  const file = `instrument_kong_${name}.png`;
  assert(mapping.includes(`'kong-${name}':'${file}'`));
  assert(fs.existsSync(path.join(root, 'assets/instruments', file)));
  assert(cmake.includes(`assets/instruments/${file}`));
  assert(native.includes(`"assets/instruments/${file}"`));
  assert(native.includes(`BinaryData::instrument_kong_${name}_png`));
}
assert(!app.includes('/中阮|阮|柳琴|三弦/'));
assert(!app.includes('/笛|巴乌|管子|埙/'));
console.log('Independent Qin artwork mappings and embedded resources: PASS');
