// Remove imperative writes to the deleted visual mirrors, preserving native events/settings.
// Reads HEAD for the old XAML name pairs; does not modify Git or generated files.
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { parse, descendants } from './collapse-material-presenters.mjs';

const repository = path.resolve(import.meta.dirname, '..');
const sourceRoot = path.join(repository, 'OpenNet');
function* sources(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
        if (['Generated Files', 'x64', 'ARM64', 'obj', 'bin'].includes(entry.name)) continue;
        const file = path.join(directory, entry.name);
        if (entry.isDirectory()) yield* sources(file);
        else if (entry.name.endsWith('.xaml.cpp')) yield file;
    }
}
for (const file of sources(sourceRoot)) {
    const before = fs.readFileSync(file, 'utf8');
    if (!/\w*Glass\w*\(/.test(before)) continue;
    const xamlPath = path.relative(repository, file.slice(0, -4)).replaceAll('\\', '/');
    let original;
    try { original = execFileSync('git', ['-C', repository, 'show', `HEAD:${xamlPath}`], { encoding: 'utf8', maxBuffer: 4 * 1024 * 1024 }); }
    catch { continue; }
    const aliases = new Map();
    for (const presenter of descendants(parse(original)).filter(n => n.name.endsWith(':SwitchPresenter') && /MaterialControlState/.test(n.tag))) {
        const cases = presenter.children.filter(n => n.name.endsWith(':Case'));
        const branches = cases.map(n => n.children.filter(c => !c.name.endsWith(':Case.Value'))).filter(n => n.length === 1);
        if (branches.length !== 2) continue;
        const native = branches[0][0].tag.match(/\bx:Name="([^"]+)"/)?.[1];
        const glass = branches[1][0].tag.match(/\bx:Name="([^"]+)"/)?.[1];
        if (native && glass && native !== glass) aliases.set(glass, native);
    }
    let after = before;
    for (const [glass, native] of aliases) {
        // These operations duplicated the native write immediately beside them.
        after = after.replace(new RegExp(`^[ \\t]*Set(?:ToggleIsOn|MaterialSwitchValue)\\(\\s*${glass}\\(\\),[\\s\\S]*?\\);[ \\t]*\\r?\\n`, 'gm'), '');
        after = after.replace(new RegExp(`^[ \\t]*if \\(auto glass = ${glass}\\(\\)\\)[^\\r\\n]*\\r?\\n`, 'gm'), '');
        after = after.replace(new RegExp(`\\b${glass}\\b`, 'g'), native);
    }
    // Native controls now own all input and event semantics; no derived toggle dispatch.
    after = after.replace(/^[ \t]*if \(auto glass = (?:sender|target)\.try_as<(?:winrt::)?WinUI::LiquidGlass::LiquidGlassToggleSwitch>\(\)\)\s*(?:return glass\.IsOn\(\);|glass\.IsOn\(value\);)[ \t]*\r?\n/gm, '');
    if (after !== before) {
        console.log(`${path.relative(repository, file)}: ${aliases.size} mirror aliases`);
        if (process.argv.includes('--write')) fs.writeFileSync(file, after);
    }
}
