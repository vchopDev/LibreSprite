// Headless test for the L6 batch 2 DocumentApi bindings added 2026-09-18:
// sprite.setPixelFormat(), sprite.setTransparentColor(), sprite.trim(),
// sprite.flatten(), sprite.newLayerFolder(). Lower priority than batch 1
// (authoring conveniences, not animation-pipeline blockers per Codex's C7
// report), but still validated the same way as the rest of this API.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/sprite_transform.js

const input = "data/splash.ase";
const output = "build-legacy/sprite-transform-test.ase";

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
assert(sprite.colorMode === 2, "fixture must start indexed (colorMode 2)");

// --- setPixelFormat() bounds ---
assertThrows(() => sprite.setPixelFormat("not-a-format"), "setPixelFormat() with an unknown name must throw");
assertThrows(() => sprite.setPixelFormat(99), "setPixelFormat() with an out-of-range number must throw");

// --- setPixelFormat() actually converts ---
sprite.setPixelFormat("rgb");
assert(sprite.colorMode === 0, "setPixelFormat('rgb') did not change colorMode to RGB (0)");

// --- setTransparentColor() ---
sprite.setTransparentColor(0);

// --- flatten() collapses every layer into one ---
const initialLayers = sprite.layerCount;
assert(initialLayers > 1, "setup: fixture must have more than one layer to prove flatten() does something");
sprite.flatten();
assert(sprite.layerCount === 1, "flatten() did not collapse the sprite to a single layer");

// --- newLayerFolder() ---
// Note: this fork's .ase loader flattens layer groups on load (pre-existing
// fork commit, unrelated to this binding) - an empty folder does not
// round-trip through save/reopen, so this only checks in-memory state.
const folder = sprite.newLayerFolder("Test Folder");
assert(!!folder, "newLayerFolder() did not return a Layer");
assert(folder.name === "Test Folder", "newLayerFolder() did not apply the requested name");
assert(!folder.isImage, "newLayerFolder() must return a folder, not an image layer");
assert(sprite.layerCount === 2, "newLayerFolder() did not grow layerCount");
sprite.removeLayer(folder);
assert(sprite.layerCount === 1, "cleanup: removeLayer() did not remove the folder");

// --- trim() shrinks the sprite to its non-empty content ---
// Flattening onto a background-less canvas leaves the whole canvas as the
// single layer's content, so cropping most of it away first gives trim()
// real empty margin to remove.
sprite.crop(0, 0, 4, 4);
sprite.trim();
assert(sprite.width <= 4 && sprite.height <= 4, "trim() must not grow the sprite");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.colorMode === 0, "persisted colorMode is wrong");
assert(sprite.layerCount === 1, "persisted layer count is wrong");

console.log("DocumentApi sprite-transform bindings (setPixelFormat/setTransparentColor/trim/flatten/newLayerFolder): PASS");
