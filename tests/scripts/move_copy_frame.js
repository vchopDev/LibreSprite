// Headless test for the P1 DocumentApi bindings added 2026-09-18 (L4):
// sprite.moveFrame() and sprite.copyFrame(). Deliberately kept out of the
// original P1 batch (document_api_p1.js): the native DocumentApi::moveFrame()
// silently no-ops on out-of-range indices instead of throwing (the same trap
// as the old sprite.crop()), and DocumentApi::copyFrame() has no bounds
// checking at all on its frame_t arguments. Both bindings validate here and
// throw a script error instead of letting the native call misbehave.
//
// The fixture (data/splash.ase) has no background layer, so newly added
// empty frames don't get an auto-populated cel on any layer (only a
// background layer gets that from AddFrame). The test therefore tracks
// layer 0's single pre-existing cel by pixel content as it's moved and
// copied across frame slots, rather than painting distinct per-frame cels.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/move_copy_frame.js

const input = "data/splash.ase";
const output = "build-codex-ucrt/move-copy-frame-test.ase";

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

const layer = sprite.layer(0);
assert(!!layer.cel(0), "fixture must have a cel at layer 0, frame 0");
const markColor = layer.cel(0).image.getPixel(0, 0);

// Grow to 3 frames. Only layer 0's original cel exists anywhere (see header).
sprite.addEmptyFrame(1);
sprite.addEmptyFrame(2);
assert(sprite.frameCount === 3, "setup: expected 3 frames");
assert(!layer.cel(1), "setup: new frame 1 must not have an auto-created cel");
assert(!layer.cel(2), "setup: new frame 2 must not have an auto-created cel");

// --- moveFrame() bounds ---
assertThrows(() => sprite.moveFrame(-1, 1), "moveFrame() with negative frame must throw");
assertThrows(() => sprite.moveFrame(3, 1), "moveFrame() with out-of-range frame must throw");
assertThrows(() => sprite.moveFrame(0, -1), "moveFrame() with negative beforeFrame must throw");
assertThrows(() => sprite.moveFrame(0, 4), "moveFrame() with out-of-range beforeFrame must throw (frameCount + 1)");

// --- moveFrame() actually relocates cel content, not just a bounds-checked no-op ---
sprite.moveFrame(0, 3); // move frame 0 to the end: [cel, -, -] -> [-, -, cel]
assert(!layer.cel(0), "moveFrame() did not vacate the source frame");
assert(!layer.cel(1), "moveFrame() must not disturb an untouched frame");
assert(!!layer.cel(2), "moveFrame() did not relocate the cel to the destination frame");
assert(layer.cel(2).image.getPixel(0, 0) === markColor, "moveFrame() changed the moved cel's pixel data");
assert(layer.celCount === 1, "moveFrame() must not change the layer's cel count");

// --- copyFrame() bounds ---
assertThrows(() => sprite.copyFrame(-1), "copyFrame() with negative fromFrame must throw");
assertThrows(() => sprite.copyFrame(3), "copyFrame() with out-of-range fromFrame must throw");
assertThrows(() => sprite.copyFrame(2, -1), "copyFrame() with negative newFrame must throw");
assertThrows(() => sprite.copyFrame(2, 4), "copyFrame() with out-of-range newFrame must throw (frameCount + 1)");

// --- copyFrame() with no newFrame defaults to appending at the end, and duplicates content ---
const inserted = sprite.copyFrame(2);
assert(inserted === 3, "copyFrame() without newFrame should default to the end index");
assert(sprite.frameCount === 4, "copyFrame() did not grow frameCount");
assert(!!layer.cel(3), "copyFrame() did not create a cel at the new frame");
assert(layer.cel(3).image.getPixel(0, 0) === markColor, "copyFrame() did not duplicate the source cel's pixel data");
assert(!!layer.cel(2), "copyFrame() must not remove the source cel");
assert(layer.celCount === 2, "copyFrame() should have grown the layer's cel count by one");

// --- copyFrame() with an explicit newFrame inserts at that position ---
const insertedAt1 = sprite.copyFrame(3, 1);
assert(insertedAt1 === 1, "copyFrame() did not return the requested insertion index");
assert(sprite.frameCount === 5, "copyFrame() with explicit newFrame did not grow frameCount");
assert(!!layer.cel(1), "copyFrame() with explicit newFrame did not insert a cel there");
assert(layer.cel(1).image.getPixel(0, 0) === markColor, "copyFrame() with explicit newFrame duplicated the wrong content");
assert(layer.celCount === 3, "copyFrame() with explicit newFrame should have grown the cel count again");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.frameCount === 5, "frame count was not persisted");
assert(sprite.layer(0).celCount === 3, "cel count from moveFrame/copyFrame was not persisted");
assert(sprite.layer(0).cel(1).image.getPixel(0, 0) === markColor, "copied cel pixel data was not persisted");

console.log("DocumentApi moveFrame/copyFrame bindings: PASS");
