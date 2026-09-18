// Headless test for the P1 DocumentApi bindings added 2026-09-18:
// sprite.crop() (fixed from a dead no-op), sprite.removeFrame(),
// sprite.removeLayer(), and cel.opacity.
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/document_api_p1.js

const input = "data/splash.ase";
const output = "build-codex-ucrt/document-api-p1-test.ase";

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

const initialLayers = sprite.layerCount;
const initialFrames = sprite.frameCount;
const initialWidth = sprite.width;
const initialHeight = sprite.height;

// --- crop() ---
assertThrows(() => sprite.crop(0, 0, 0, 10), "crop() with zero width must throw");
assertThrows(() => sprite.crop(0, 0, 10, -1), "crop() with negative height must throw");

const cropW = Math.max(1, Math.floor(initialWidth / 2));
const cropH = Math.max(1, Math.floor(initialHeight / 2));
sprite.crop(0, 0, cropW, cropH);
assert(sprite.width === cropW, "crop() did not change sprite width");
assert(sprite.height === cropH, "crop() did not change sprite height");

// --- removeFrame() ---
assertThrows(() => sprite.removeFrame(-1), "removeFrame() with negative index must throw");
assertThrows(() => sprite.removeFrame(sprite.frameCount), "removeFrame() out of range must throw");

const addedFrame = sprite.addEmptyFrame(sprite.frameCount);
assert(sprite.frameCount === initialFrames + 1, "setup: addEmptyFrame() did not grow frameCount");
sprite.removeFrame(addedFrame);
assert(sprite.frameCount === initialFrames, "removeFrame() did not shrink frameCount back");

// Removing the sprite down to its last frame must be refused, not silently no-op.
while (sprite.frameCount > 1)
  sprite.removeFrame(sprite.frameCount - 1);
assertThrows(() => sprite.removeFrame(0), "removeFrame() must refuse to remove the last frame");

// --- removeLayer() ---
assertThrows(() => sprite.removeLayer({}), "removeLayer() with a non-Layer must throw");

const newLayer = sprite.newLayer("P1 Test Layer");
assert(sprite.layerCount === initialLayers + 1, "setup: newLayer() did not grow layerCount");
sprite.removeLayer(newLayer);
assert(sprite.layerCount === initialLayers, "removeLayer() did not shrink layerCount back");

while (sprite.layerCount > 1)
  sprite.removeLayer(sprite.layer(sprite.layerCount - 1));
assertThrows(() => sprite.removeLayer(sprite.layer(0)), "removeLayer() must refuse to remove the last layer");

// --- cel.opacity ---
const cel = sprite.layer(0).cel(0);
assert(!!cel, "setup: expected layer 0 to have a cel at index 0");
const initialOpacity = cel.opacity;
cel.opacity = 128;
assert(sprite.layer(0).cel(0).opacity === 128, "cel.opacity setter did not stick");
assertThrows(() => { cel.opacity = 300; }, "cel.opacity out of [0,255] must throw");
assertThrows(() => { cel.opacity = -1; }, "cel.opacity out of [0,255] must throw");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.width === cropW, "cropped width was not persisted");
assert(sprite.height === cropH, "cropped height was not persisted");
assert(sprite.frameCount === 1, "trimmed frame count was not persisted");
assert(sprite.layerCount === 1, "trimmed layer count was not persisted");
assert(sprite.layer(0).cel(0).opacity === 128, "cel opacity was not persisted");

console.log("DocumentApi P1 bindings (crop/removeFrame/removeLayer/cel.opacity): PASS");
