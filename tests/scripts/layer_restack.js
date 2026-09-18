// Headless test for the L6 batch 2 DocumentApi bindings added 2026-09-18:
// layer.restackAfter(), layer.restackBefore(), layer.duplicate(),
// layer.moveCel(), layer.swapCel(), and image.flip().
//
// layer.restackBefore() deliberately does NOT delegate to
// DocumentApi::restackLayerBefore(): that helper resolves its target via
// Sprite::layerToIndex()/indexToLayer(), a flat index across the whole
// layer tree (recursing into folders), then calls restackLayerAfter() with
// whatever layer sits at index-1 - which is not guaranteed to be a sibling
// of the layer being moved. LayerFolder::stackLayer() then does
// `std::find(...)` for that "after" layer within the *moving* layer's own
// parent and increments the resulting iterator with only an ASSERT
// guarding it (compiled out in a release build) - so a "before" layer that
// isn't in the same folder is undefined behavior, not a clean error. This
// binding instead validates same-parent locally and walks the sibling
// chain directly (`beforeLayer.getPrevious()`), which stays within a single
// folder's list by construction.
//
// image.flip() bounds-checks its region before calling
// doc::algorithm::flip_image(): that function indexes pixels through the
// unchecked get_pixel()/put_pixel() free functions (unlike Image::putPixel,
// which the putPixel() binding bounds-checks itself), so an out-of-range
// rect there would be a real out-of-bounds memory access.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/layer_restack.js

const input = "data/splash.ase";
const output = "build-codex-ucrt/layer-restack-test.ase";

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

const layerA = sprite.newLayer("A");
const layerB = sprite.newLayer("B");
const layerC = sprite.newLayer("C");
// Stack order after three newLayer() calls (each appends at the end): A, B, C.
assert(sprite.layer(sprite.layerCount - 3).name === "A", "setup: expected A before B before C");
assert(sprite.layer(sprite.layerCount - 2).name === "B", "setup: expected A before B before C");
assert(sprite.layer(sprite.layerCount - 1).name === "C", "setup: expected A before B before C");

// --- restackAfter() bounds ---
// Note: there's no script-level way to create a layer nested inside a
// folder yet (newLayer()/newLayerFolder() only append to the sprite's root
// folder), so the same-parent guard against the native stackLayer() UB
// (see header comment) can't be exercised here - it's a defensive check for
// when nested-layer creation is added, not dead code.
assertThrows(() => layerA.restackAfter({}), "restackAfter() with a non-Layer must throw");

// --- restackAfter() actually reorders ---
layerA.restackAfter(layerC);
// A, B, C -> B, C, A
const baseIndex = sprite.layerCount - 3;
assert(sprite.layer(baseIndex).name === "B", "restackAfter() did not move A after C");
assert(sprite.layer(baseIndex + 1).name === "C", "restackAfter() did not move A after C");
assert(sprite.layer(baseIndex + 2).name === "A", "restackAfter() did not move A after C");

// --- restackAfter(undefined) moves to the front of the parent ---
layerA.restackAfter(undefined);
assert(sprite.layer(0).name === "A", "restackAfter(undefined) did not move A to the front");

// --- restackBefore() bounds and behavior ---
assertThrows(() => layerA.restackBefore({}), "restackBefore() with a non-Layer must throw");
layerA.restackBefore(layerC);
// current order: B, C, A(front-removed) -> after restackBefore(C): B, A, C
const idx = sprite.layerCount - 3;
assert(sprite.layer(idx).name === "B", "restackBefore() produced the wrong order");
assert(sprite.layer(idx + 1).name === "A", "restackBefore() did not place A directly before C");
assert(sprite.layer(idx + 2).name === "C", "restackBefore() did not place A directly before C");

// --- duplicate() ---
assertThrows(() => sprite.newLayerFolder("F").duplicate(), "duplicate() on a folder must throw (native duplicateLayerAfter() assumes an image layer)");
// clean up the stray folder created for that assertion
sprite.removeLayer(sprite.layer(sprite.layerCount - 1));

const layersBeforeDuplicate = sprite.layerCount;
const dup = layerA.duplicate();
assert(!!dup, "duplicate() did not return a Layer");
assert(dup.name === "A Copy", "duplicate() did not name the copy '<name> Copy'");
assert(sprite.layerCount === layersBeforeDuplicate + 1, "duplicate() did not grow the layer list");

// --- moveCel() / swapCel() ---
sprite.addEmptyFrame(1);
const celA0 = layerA.addCel(0);
celA0.image.putPixel(0, 0, 11);

assertThrows(() => layerA.moveCel(-1, layerB, 0), "moveCel() with a negative source frame must throw");
assertThrows(() => layerA.moveCel(0, layerB, 99), "moveCel() with an out-of-range destination frame must throw");
assertThrows(() => layerA.moveCel(1, layerB, 0), "moveCel() must refuse a source frame with no cel");
assertThrows(() => layerA.moveCel(0, {}, 0), "moveCel() with a non-Layer destination must throw");

layerA.moveCel(0, layerB, 1);
assert(!layerA.cel(0), "moveCel() did not remove the source cel");
assert(!!layerB.cel(1), "moveCel() did not create the destination cel");
assert(layerB.cel(1).image.getPixel(0, 0) === 11, "moveCel() did not carry the pixel data");

assertThrows(() => layerB.swapCel(0, 0), "swapCel() with identical frames must throw");
assertThrows(() => layerB.swapCel(-1, 1), "swapCel() with a negative frame must throw");
layerB.addCel(0).image.putPixel(0, 0, 22);
layerB.swapCel(0, 1);
assert(layerB.cel(0).image.getPixel(0, 0) === 11, "swapCel() did not swap frame 1's cel into frame 0");
assert(layerB.cel(1).image.getPixel(0, 0) === 22, "swapCel() did not swap frame 0's cel into frame 1");

// --- image.flip() ---
const img = layerB.cel(0).image;
img.putPixel(0, 0, 1);
img.putPixel(1, 0, 2);
assertThrows(() => img.flip("diagonal", 0, 0, 2, 1), "flip() with an unknown direction must throw");
assertThrows(() => img.flip("horizontal", 0, 0, img.width + 1, 1), "flip() with a region past the image width must throw");
assertThrows(() => img.flip("horizontal", -1, 0, 2, 1), "flip() with a negative origin must throw");
img.flip("horizontal", 0, 0, 2, 1);
assert(img.getPixel(0, 0) === 2 && img.getPixel(1, 0) === 1, "flip() did not swap the two pixels");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.frameCount === 2, "persisted frame count is wrong");

console.log("DocumentApi layer-restack bindings (restackAfter/restackBefore/duplicate/moveCel/swapCel/flip): PASS");
