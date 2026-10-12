import assert from 'node:assert/strict';
import fs from 'node:fs';
import { test } from 'node:test';

const helper = fs.readFileSync(new URL('../OpenNet/Helpers/WindowHelper.cpp', import.meta.url), 'utf8');
const reset = helper.slice(helper.indexOf('void ResetBackgroundSources('), helper.indexOf('bool IsSecondaryWindow('));
const closed = helper.slice(helper.indexOf('window.Closed('), helper.indexOf('void WindowHelper::TrackWindow(winrt::WinUI3Package::WindowEx'));

// Source-level guards; these do not exercise WinRT teardown at runtime.
test('background cleanup checks optional presenters independently', () => {
    assert.match(reset, /if \(auto image = state\.ImagePresenter\)\s*\{\s*try\s*\{\s*image\.Opacity\(0\);\s*image\.Source\(nullptr\);/);
    assert.match(reset, /if \(auto video = state\.VideoPresenter\)\s*\{\s*try\s*\{\s*if \(auto player = video\.MediaPlayer\(\)\)/);
    assert.match(reset, /video\.Source\(nullptr\);\s*video\.SetMediaPlayer\(nullptr\);/);
    assert.doesNotMatch(reset, /state\.(?:ImagePresenter|VideoPresenter)\./);
    assert.match(helper, /if \(!applyToSecondary\)\s*\{\s*ResetBackgroundSources\(state\);/);
});

test('closed window is detached from background registry before cleanup', () => {
    assert.match(closed, /auto visual = g_secondaryWindowVisualStates\.extract\(trackedHwnd\);/);
    assert.match(closed, /if \(!visual\.empty\(\)\)\s*\{\s*ResetBackgroundSources\(visual\.mapped\(\)\);/);
    assert.doesNotMatch(closed, /(?:ImagePresenter|VideoPresenter)\./);
    assert.ok(closed.indexOf('.extract(trackedHwnd)') < closed.indexOf('ResetBackgroundSources('));
});

test('restart button uses an MRT path backed by every shipped locale', () => {
    const page = fs.readFileSync(new URL('../OpenNet/UI/Xaml/View/Pages/SettingsPages/SettingsPage.xaml.cpp', import.meta.url), 'utf8');
    assert.match(page, /ResourceGetString\(L"RestartToApplyLanguageButton\/Content"\)/);
    assert.doesNotMatch(page, /ResourceGetString\(L"RestartToApplyLanguageButton\.Content"\)/);
    const strings = new URL('../OpenNet/Strings/', import.meta.url);
    for (const locale of fs.readdirSync(strings, { withFileTypes: true }).filter(entry => entry.isDirectory())) {
        const resw = fs.readFileSync(new URL(`${locale.name}/Resources.resw`, strings), 'utf8');
        assert.match(resw, /<data name="RestartToApplyLanguageButton\.Content"[^>]*>\s*<value>[^<]+<\/value>/, locale.name);
    }
});
