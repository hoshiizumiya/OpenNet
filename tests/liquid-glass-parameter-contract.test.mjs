import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

test('optical editor bounds agree with the shipped Hlsl native setter contracts', () => {
    const source = readFileSync(new URL('../OpenNet/UI/Xaml/Control/Theme/LiquidGlassOptics.h', import.meta.url), 'utf8');
    const ranges = new Map([...source.matchAll(/\{ L"(\w+)", L"[^"]+", ([\d.-]+), ([\d.-]+), ([\d.-]+),/g)]
        .map(([, key, min, max, step]) => [key, [Number(min), Number(max), Number(step)]]));
    // WinUI.Composition.Hlsl 1.3.0 rejects the old 0.1 minima before setting the DP.
    assert.deepEqual(ranges.get('HighlightSharpness'), [0.25, 64, 0.05]);
    assert.deepEqual(ranges.get('SpecularWidth'), [0.25, 32, 0.05]);
    assert.deepEqual(ranges.get('HighlightStrength'), [0, 4, 0.01]);
    assert.equal(ranges.size, 30);
    for (const [key, [min, max, step]] of ranges) {
        assert.ok(Number.isFinite(min) && Number.isFinite(max) && min <= max, key);
        assert.ok(step > 0 && Number.isFinite(step), key);
    }
});
