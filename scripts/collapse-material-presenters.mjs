// Mechanical source migration. No build, code generation, or generated-file edits.
// Default is an inventory; --write collapses known two-case material mirrors only.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const root = path.resolve(import.meta.dirname, '../OpenNet');
const types = { Button: 'Button', ToggleButton: 'ToggleButton', HyperlinkButton: 'HyperlinkButton',
    CheckBox: 'CheckBox', RadioButton: 'RadioButton', Slider: 'Slider', TextBox: 'TextBox',
    PasswordBox: 'PasswordBox', ComboBox: 'ComboBox', ToggleSwitch: 'ToggleSwitch', SearchBox: 'AutoSuggestBox', Card: 'ContentControl' };
const attrs = text => [...text.matchAll(/([\w.:]+)\s*=\s*"([^"]*)"/g)].map(m => [m[1], m[2]]);
export function parse(text) {
    const roots = [], stack = [];
    for (const m of text.matchAll(/<!--[\s\S]*?-->|<(?:(?:"[^"]*")|(?:'[^']*')|[^'">])*>/g)) {
        const tag = m[0];
        if (/^<\?|^<!/.test(tag)) continue;
        if (tag.startsWith('</')) { const node = stack.pop(); if (node) node.end = m.index + tag.length; continue; }
        const name = tag.match(/^<([^\s/>]+)/)?.[1];
        const node = { name, start: m.index, openEnd: m.index + tag.length, end: m.index + tag.length, tag, children: [] };
        (stack.length ? stack.at(-1).children : roots).push(node);
        if (!tag.endsWith('/>')) stack.push(node);
    }
    return roots;
}
export function descendants(nodes) { return nodes.flatMap(n => [n, ...descendants(n.children)]); }
export function collapse(text, libraryTemplates = true) {
    let count = 0;
    const aliases = new Map();
    for (;;) {
        const nodes = descendants(parse(text));
        const candidates = nodes.filter(n => n.name.endsWith(':SwitchPresenter') && /MaterialControlState/.test(n.tag));
        const edits = [];
        for (const n of candidates) {
            if (descendants(n.children).some(child => candidates.includes(child))) continue;
            const attributes = Object.fromEntries(attrs(n.tag));
            let kind = attributes.Value?.match(/\.([A-Za-z]+)Material\b/)?.[1];
            if (kind === 'Type' && libraryTemplates) kind = 'Card';
            if (!types[kind]) continue;
            // Search owns QuerySubmitted/SuggestionChosen, which cannot be raised
            // through the sealed outer AutoSuggestBox's public API. Keep its adapter.
            if (libraryTemplates && kind === 'SearchBox') continue;
            const cases = n.children.filter(child => child.name.endsWith(':Case'));
            if (cases.length !== 2) continue;
            const normal = cases.find(c => c.children.some(v => v.name.endsWith(':Case.Value') && />\s*0\s*</.test(text.slice(v.start, v.end))));
            if (!normal) continue;
            const content = normal.children.filter(child => !child.name.endsWith(':Case.Value'));
            if (content.length !== 1 || ![types[kind], `wl:LiquidGlass${kind}`].includes(content[0].name)) continue;
            const native = content[0];
            // Choice Click has different semantics from IsChecked. Preserve this
            // explicit event adapter until it has its own tested command contract.
            if (libraryTemplates && ['CheckBox', 'RadioButton', 'ToggleButton'].includes(kind) && /\bClick=/.test(native.tag)) continue;
            let replacement = text.slice(native.start, native.end);
            let tag = native.tag;
            const existing = Object.fromEntries(attrs(tag));
            const oldName = attributes['x:Name'];
            // These mirrors existed only because an inactive Case loses inheritance.
            for (const property of ['DataContext', 'RequestedTheme']) {
                if (oldName && existing[property]?.includes(oldName))
                    tag = tag.replace(new RegExp(`\\s+${property}="[^"]*"`), '');
            }
            const added = [['ouxct:ControlAppearance.IsEnabled', 'True']];
            const newName = existing['x:Name'] || oldName;
            if (!existing['x:Name'] && oldName) added.push(['x:Name', oldName]);
            if (oldName && newName && oldName !== newName) aliases.set(oldName, newName);
            for (const [key, value] of Object.entries(attributes)) {
                if (/^(Grid\.|Canvas\.|DockPanel\.)/.test(key) || /^(Margin|Width|Height|MinWidth|MinHeight|MaxWidth|MaxHeight|HorizontalAlignment|VerticalAlignment|x:Load|x:DeferLoadStrategy)$/.test(key)) {
                    if (!(key in existing)) added.push([key, value]);
                }
            }
            const indent = text.slice(text.lastIndexOf('\n', native.start) + 1, native.start);
            tag = tag.replace(/(\s*\/?>)$/, added.filter(([key]) => !(key in existing)).map(([key, value]) => `\n${indent}    ${key}="${value}"`).join('') + '$1');
            replacement = tag + replacement.slice(native.tag.length);
            if (native.name !== types[kind])
                replacement = replacement.replaceAll(`wl:LiquidGlass${kind}`, types[kind]);
            const outerIndent = text.slice(text.lastIndexOf('\n', n.start) + 1, n.start);
            replacement = replacement.split('\n').map((line, i) => i && line.startsWith(indent) ? outerIndent + line.slice(indent.length) : line).join('\n');
            edits.push({ start: n.start, end: n.end, replacement });
        }
        if (!edits.length) break;
        for (const edit of edits.sort((a, b) => b.start - a.start))
            text = text.slice(0, edit.start) + edit.replacement + text.slice(edit.end);
        count += edits.length;
    }
    for (const [before, after] of aliases) text = text.replace(new RegExp(`\\b${before}\\b`, 'g'), after);
    return { text, count };
}
function* files(directory) {
    for (const item of fs.readdirSync(directory, { withFileTypes: true })) {
        if (['Generated Files', 'x64', 'ARM64', 'obj', 'bin'].includes(item.name)) continue;
        const full = path.join(directory, item.name);
        if (item.isDirectory()) yield* files(full);
        else if (item.name.endsWith('.xaml')) yield full;
    }
}
if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
let total = 0;
for (const file of files(root)) {
    const original = fs.readFileSync(file, 'utf8');
    const result = collapse(original, true);
    if (!result.count) continue;
    total += result.count;
    console.log(`${path.relative(root, file)}: ${result.count}`);
    if (process.argv.includes('--write')) fs.writeFileSync(file, result.text);
}
console.log(`Total: ${total} material presenters ${process.argv.includes('--write') ? 'collapsed' : 'eligible'}`);

}
