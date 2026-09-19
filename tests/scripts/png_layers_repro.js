// Minimal reproducer for a class of asset-pipeline blocker: open a PNG,
// rename layer 0, create N layers via sprite.newLayer(), saveAs .ase, reopen.
// Mirrors the shape of a generic "PNG -> layered .ase" scripting workflow
// without depending on any external project's asset tree.
//
// Uses data/splash.png, the PNG sibling of the data/splash.ase fixture
// already used by tests/scripts/document_api.js and frame_tags.js, so this
// test is self-contained and portable to any checkout of this repo.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/png_layers_repro.js
// Requires C:\msys64\ucrt64\bin on PATH for an MSYS2-built binary.

const input = "data/splash.png";
const output = "build-legacy/png-layers-repro.ase";

function assert(condition, message) {
  if (!condition)
    throw new Error(message);
}

const doc = app.open(input);
assert(!!doc, "open failed");

const spr = doc.sprite;
console.log("opened PNG: " + spr.width + "x" + spr.height + ", layers=" + spr.layerCount);

spr.layer(0).name = "base";
spr.newLayer("outline");
spr.newLayer("shade");
spr.newLayer("highlight");
spr.newLayer("accent");

assert(spr.layerCount === 5, "expected 5 layers, got " + spr.layerCount);

spr.saveAs(output, true);

const reopened = app.open(output);
assert(!!reopened, "saved .ase could not be reopened");
assert(spr.layerCount === 5, "layer count was not persisted, got " + spr.layerCount);

let names = [];
for (let i = 0; i < spr.layerCount; ++i)
  names.push(spr.layer(i).name);

console.log("persisted layers: " + names.join(", "));
console.log("PNG -> layered .ase reproducer: PASS");
