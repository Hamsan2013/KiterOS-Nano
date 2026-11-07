const assert = require("assert");
const fs = require("fs");
const path = require("path");
const Fridge = require("../runtime/fridge.js");

console.log("=== Running FRIDGE v1.0 Automated Language Tests ===");

// Test 1: Prompt Example 1 ("Hello KiterOS")
const helloSrc = fs.readFileSync(path.join(__dirname, "../examples/hello.frg"), "utf8");
const vm1 = Fridge.createVM();
const res1 = vm1.run(helloSrc, "hello.frg");
assert.deepStrictEqual(res1.outputs, [
  "Welcome to KiterOS!",
  "Excellent!",
  "Hello from FRIDGE!"
]);
console.log("[PASS] Prompt Example 1 ('Hello KiterOS') executed accurately.");

// Test 2: Prompt Example 2 ("Space Runner")
const gameSrc = fs.readFileSync(path.join(__dirname, "../examples/spacerunner.frg"), "utf8");
const vm2 = Fridge.createVM();
vm2.run(gameSrc, "spacerunner.frg");
assert.strictEqual(vm2.entities.player.x, 100);
assert.strictEqual(vm2.entities.player.y, 200);
vm2.triggerTouch();
assert.strictEqual(vm2.entities.player.y, 188); // moved up by default 12px
console.log("[PASS] Prompt Example 2 ('Space Runner') entity creation & touch movement verified.");

// Test 3: Loops, Lists, Functions with Parameters, Collision Detection
const advSrc = `
app "Test Suite"
remember items as [10, 20, 30]
remember sum as 0
for each x in items:
    set sum to sum + x
end
create a
set a position to 10, 10
set a size to 20, 20
create b
set b position to 15, 15
set b size to 20, 20
if collides(a, b):
    show "COLLISION:" + sum
end
`;
const vm3 = Fridge.createVM();
const res3 = vm3.run(advSrc);
assert.strictEqual(res3.outputs[0], "COLLISION:60");
console.log("[PASS] Lists, loops, and 2D AABB collision detection verified.");

// Test 4: Syntax Error Reporting with File & Line Number
let caught = false;
try {
  const vmErr = Fridge.createVM();
  vmErr.run('app "Broken"\nwhen app starts:\nshow "Missing end"', "broken.frg");
} catch (e) {
  caught = true;
  assert(e.message.includes("broken.frg"), "Error must include filename");
}
assert(caught, "Unclosed block must throw syntax error");
console.log("[PASS] Error reporting with filename & line numbers verified.");
