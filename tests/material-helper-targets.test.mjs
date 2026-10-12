import assert from 'node:assert/strict';
import { test } from 'node:test';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { removeIncompatibleGlassHelpers } from '../scripts/material-helper-targets.mjs';

test('search wrapper drops native-only helper but preserves bindings', () => {
    const tag = '<wl:LiquidGlassSearchBox essential:AutoSuggestBoxHelper.AcrylicWorkaround="True" Text="{x:Bind SearchText, Mode=TwoWay}" />';
    assert.equal(removeIncompatibleGlassHelpers(tag, 'SearchBox'),
        '<wl:LiquidGlassSearchBox Text="{x:Bind SearchText, Mode=TwoWay}" />');
});

test('native helper filtering works with aliases and multiline attributes', () => {
    const tag = '<wl:LiquidGlassSearchBox\n    helpers:AutoSuggestBoxHelper.AcrylicWorkaround = "True"\n    x:Name="Search" />';
    assert.equal(removeIncompatibleGlassHelpers(tag, 'SearchBox'), '<wl:LiquidGlassSearchBox\n    x:Name="Search" />');
});

test('helpers on derived native types are retained', () => {
    for (const [kind, tag] of [
        ['ComboBox', '<wl:LiquidGlassComboBox essential:ComboBoxHelper.AcrylicWorkaround="True" />'],
        ['Slider', '<wl:LiquidGlassSlider essential:SliderHelper.AcrylicWorkaround="True" />'],
        ['AutoSuggestBox', '<AutoSuggestBox essential:AutoSuggestBoxHelper.AcrylicWorkaround="True" />'],
    ]) assert.equal(removeIncompatibleGlassHelpers(tag, kind), tag);
});

test('application search wrappers contain no native-only helpers', () => {
    function* files(dir) {
        for (const item of fs.readdirSync(dir, { withFileTypes: true })) {
            const file = path.join(dir, item.name);
            if (item.isDirectory()) yield* files(file);
            else if (file.endsWith('.xaml')) yield file;
        }
    }
    for (const file of files(fileURLToPath(new URL('../OpenNet/UI', import.meta.url)))) {
        const text = fs.readFileSync(file, 'utf8');
        for (const [tag] of text.matchAll(/<[\w]+:LiquidGlassSearchBox\b[^>]*>/g))
            assert.doesNotMatch(tag, /AutoSuggestBoxHelper\./, file);
    }
});
