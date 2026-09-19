// Headless test for the cel content-transfer DocumentApi bindings added
// 2026-09-18 (L6, first batch): layer.addCel(), layer.clearCel(), and
// layer.copyCel(). These exist so a pipeline that renders each animation
// frame as a separate image (e.g. a PNG per frame) can place that pixel
// data into an independent cel on a specific layer/frame without going
// through app.command.* (unsafe headlessly, see Design-Notes) or relying on
// the background-layer auto-cel behavior of addFrame/addEmptyFrame (only
// background layers get that, see move_copy_frame.js).
//
// The native DocumentApi::addCel() only ASSERTs that no cel already exists
// at the target frame - compiled out in a release build, so calling it twice
// on the same frame would silently corrupt the layer's cel list instead of
// erroring. This binding checks first and throws.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/cel_transfer.js

const input = "data/splash.ase";
const output = "build-legacy/cel-transfer-test.ase";

function assert(condition, message) {
  if (!condition)
    throw new Error(message);
}

function assertThrows(fn, message) {
  try {
    fn();
  } catch (e) {
    return;
  }
  throw new Error("expected an error: " + message);
}

const opened = app.open(input);
assert(!!opened, "app.open() did not return a document");

sprite.addEmptyFrame(1);
sprite.addEmptyFrame(2);
assert(sprite.frameCount === 3, "setup: expected 3 frames");

const layerAIndex = sprite.layerCount;
const layerA = sprite.newLayer("Cel Transfer A");
const layerBIndex = sprite.layerCount;
const layerB = sprite.newLayer("Cel Transfer B");

// --- addCel() bounds ---
assertThrows(() => layerA.addCel(-1), "addCel() with a negative frame must throw");
assertThrows(() => layerA.addCel(3), "addCel() out of range must throw");

// --- addCel() creates a blank cel sized to the sprite ---
assert(!layerA.cel(0), "setup: new layer must start without a cel at frame 0");
const celA0 = layerA.addCel(0);
assert(!!celA0, "addCel() did not return a Cel");
assert(layerA.celCount === 1, "addCel() did not grow the layer's cel count");
assert(celA0.image.width === sprite.width, "addCel() image width does not match the sprite");
assert(celA0.image.height === sprite.height, "addCel() image height does not match the sprite");

// --- addCel() refuses to double-add on an occupied frame ---
assertThrows(() => layerA.addCel(0), "addCel() must refuse a frame that already has a cel");

// --- paint into the new cel via the existing Image API ---
// data/splash.ase is indexed color mode, so pixel values are palette
// indices (0-255), not packed RGBA - use a plain small integer marker.
const markColor = 42;
celA0.image.putPixel(0, 0, markColor);
assert(layerA.cel(0).image.getPixel(0, 0) === markColor, "painted pixel did not stick");

// --- clearCel() bounds ---
assertThrows(() => layerA.clearCel(-1), "clearCel() with a negative frame must throw");
assertThrows(() => layerA.clearCel(3), "clearCel() out of range must throw");

// --- clearCel() on an already-empty frame is a harmless no-op ---
layerA.clearCel(1);
assert(!layerA.cel(1), "clearCel() must not create a cel on an empty frame");

// --- copyCel() bounds ---
assertThrows(() => layerA.copyCel(0, {}, 0), "copyCel() with a non-Layer destination must throw");
assertThrows(() => layerA.copyCel(-1, layerB, 0), "copyCel() with a negative source frame must throw");
assertThrows(() => layerA.copyCel(0, layerB, 3), "copyCel() with an out-of-range destination frame must throw");
assertThrows(() => layerA.copyCel(0, layerA, 0), "copyCel() to the same layer/frame must throw");

// --- copyCel() duplicates content onto another layer without disturbing the source ---
assert(!layerB.cel(0), "setup: destination layer must start without a cel at frame 0");
layerA.copyCel(0, layerB, 0);
assert(!!layerB.cel(0), "copyCel() did not create a cel on the destination layer");
assert(layerB.cel(0).image.getPixel(0, 0) === markColor, "copyCel() did not duplicate pixel data");
assert(!!layerA.cel(0), "copyCel() must not remove the source cel");

// --- clearCel() actually removes a populated cel ---
layerA.clearCel(0);
assert(!layerA.cel(0), "clearCel() did not remove a populated cel");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.layer(layerAIndex).name === "Cel Transfer A", "layer A identity was not persisted");
assert(!sprite.layer(layerAIndex).cel(0), "cleared source cel must not reappear after reopen");
assert(!!sprite.layer(layerBIndex).cel(0), "copied destination cel was not persisted");
assert(sprite.layer(layerBIndex).cel(0).image.getPixel(0, 0) === markColor, "persisted cel pixel data is wrong");

console.log("DocumentApi cel-transfer bindings (addCel/clearCel/copyCel): PASS");
