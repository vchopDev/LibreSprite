// Headless test for the frame-timing DocumentApi bindings added 2026-09-18
// (L6, first batch): sprite.frameDuration(), sprite.setFrameDuration(), and
// sprite.setFrameRangeDuration(). The native Sprite::setFrameDuration()
// silently no-ops on an out-of-range frame index (same trap shape as the old
// crop()/moveFrame()); setFrameRangeDuration()'s native bounds are only
// ASSERTs, which are compiled out in a release build, so an invalid range
// would otherwise be a silent no-op too. Both bindings validate first.
//
// Run from the LibreSprite source directory with:
//   libresprite -b --script tests/scripts/frame_duration.js

const input = "data/splash.ase";
const output = "build-legacy/frame-duration-test.ase";

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

// --- frameDuration() read, and its own bounds check ---
assertThrows(() => sprite.frameDuration(-1), "frameDuration() with negative index must throw");
assertThrows(() => sprite.frameDuration(3), "frameDuration() out of range must throw");
const defaultDuration = sprite.frameDuration(0);
assert(defaultDuration > 0, "fixture frame must start with a positive default duration");

// --- setFrameDuration() bounds ---
assertThrows(() => sprite.setFrameDuration(-1, 100), "setFrameDuration() with negative frame must throw");
assertThrows(() => sprite.setFrameDuration(3, 100), "setFrameDuration() out of range frame must throw");
assertThrows(() => sprite.setFrameDuration(0, 0), "setFrameDuration() with a zero duration must throw");
assertThrows(() => sprite.setFrameDuration(0, -5), "setFrameDuration() with a negative duration must throw");
assertThrows(() => sprite.setFrameDuration(0, 70000), "setFrameDuration() over 65535ms must throw");

// --- setFrameDuration() actually sticks ---
sprite.setFrameDuration(1, 250);
assert(sprite.frameDuration(1) === 250, "setFrameDuration() did not change the frame's duration");
assert(sprite.frameDuration(0) === defaultDuration, "setFrameDuration() must not disturb other frames");

// --- setFrameRangeDuration() bounds ---
assertThrows(() => sprite.setFrameRangeDuration(1, 0, 100), "setFrameRangeDuration() with fromFrame >= toFrame must throw");
assertThrows(() => sprite.setFrameRangeDuration(0, 0, 100), "setFrameRangeDuration() with an empty range must throw");
assertThrows(() => sprite.setFrameRangeDuration(-1, 2, 100), "setFrameRangeDuration() with a negative fromFrame must throw");
assertThrows(() => sprite.setFrameRangeDuration(0, 3, 100), "setFrameRangeDuration() with toFrame out of range must throw");
assertThrows(() => sprite.setFrameRangeDuration(0, 2, 0), "setFrameRangeDuration() with a zero duration must throw");

// --- setFrameRangeDuration() actually sticks across the whole range ---
sprite.setFrameRangeDuration(0, 2, 80);
assert(sprite.frameDuration(0) === 80, "setFrameRangeDuration() did not set frame 0");
assert(sprite.frameDuration(1) === 80, "setFrameRangeDuration() did not set frame 1");
assert(sprite.frameDuration(2) === 80, "setFrameRangeDuration() did not set frame 2");

// --- persistence: save, reopen, re-check the surviving state ---
sprite.saveAs(output, true);
const reopened = app.open(output);
assert(!!reopened, "saved sprite could not be reopened");
assert(sprite.frameDuration(0) === 80, "persisted frame 0 duration is wrong");
assert(sprite.frameDuration(1) === 80, "persisted frame 1 duration is wrong");
assert(sprite.frameDuration(2) === 80, "persisted frame 2 duration is wrong");

console.log("DocumentApi frame-timing bindings (frameDuration/setFrameDuration/setFrameRangeDuration): PASS");
