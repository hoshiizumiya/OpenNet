import assert from 'node:assert/strict';
import fs from 'node:fs';
import { test } from 'node:test';
import { descendants, parse } from '../scripts/collapse-material-presenters.mjs';

const read = name => fs.readFileSync(new URL(`../${name}`, import.meta.url), 'utf8');
const bridge = read('OpenNet/UI/Xaml/Control/Theme/MaterialControlBridge.cpp');
const forward = bridge.slice(bridge.indexOf('void ForwardMaterialFocus('), bridge.indexOf('void ReleaseMaterialTemplateBridge('));

test('the material text editor inherits focusability instead of disabling text input', () => {
    const nodes = descendants(parse(read('OpenNet/UI/Xaml/Control/Theme/MaterialLibraryTemplates.xaml')));
    const template = nodes.find(node => node.tag.includes('x:Key="MaterialTextBoxTemplate"'));
    const editor = template.children.find(node => node.name === 'wl:LiquidGlassTextBox');
    // IsTabStop=False also makes Focus() fail, not just keyboard tab navigation.
    assert.match(editor.tag, /IsTabStop="\{TemplateBinding IsTabStop\}"/);
    assert.match(editor.tag, /IsReadOnly="\{TemplateBinding IsReadOnly\}"/);
    assert.match(editor.tag, /InputScope="\{TemplateBinding InputScope\}"/);
    assert.match(bridge, /bind\(C::TextBox::TextProperty\(\), C::TextBox::TextProperty\(\)\);/);
});

test('the nested editor is one tab stop and the original navigation contract is restored', () => {
    assert.match(bridge, /OriginalTabNavigation = native.ReadLocalValue\(property\)/);
    assert.match(bridge, /OriginalTabNavigationBinding = expression.ParentBinding\(\)/);
    assert.match(bridge, /native.TabFocusNavigation\(Input::KeyboardNavigationMode::Once\)/);
    const release = bridge.slice(bridge.indexOf('void ReleaseMaterialTemplateBridge('), bridge.indexOf('void EnsureMaterialTemplateBridge('));
    assert.match(release, /native.SetBinding\(property, link.OriginalTabNavigationBinding\)/);
    assert.match(release, /native.ClearValue\(property\)/);
    assert.match(release, /native.SetValue\(property, link.OriginalTabNavigation\)/);
    assert.match(bridge, /native.try_as<C::TextBox>\(\) && native.FocusState\(\) != FocusState::Unfocused/);
});

test('focus handoff rejects an inactive connection, unloaded targets and Unfocused', () => {
    assert.match(forward, /if \(!\*active\) return;/);
    assert.match(forward, /!control.IsLoaded\(\) \|\| !target.IsLoaded\(\)/);
    assert.match(forward, /target.XamlRoot\(\) != root/);
    assert.match(forward, /state == FocusState::Unfocused\) return;/);
    assert.match(forward, /FocusManager::GetFocusedElement\(root\) != control\) return;/);
    assert.ok(forward.indexOf('FocusState::Unfocused') < forward.indexOf('.Focus(state)'));
    assert.doesNotMatch(bridge, /\.Focus\(control.FocusState\(\)\)/);
});

test('deferred focus callback retains its connection generation and filters only disconnection failures', () => {
    assert.match(bridge, /owner = make_weak\(native\), active = link.Active/);
    assert.match(bridge, /TryEnqueue\(\[weak, owner, active\] \{ ForwardMaterialFocus\(owner, weak, active\); \}\)/);
    assert.match(forward, /0x80070490/);
    assert.match(forward, /0x80000013\)\) throw;/);
    assert.doesNotMatch(forward, /catch \(\.\.\.\)/);
});

test('task toolbar search has matching compact size and does not shrink text to eighty percent', () => {
    const nodes = descendants(parse(read('OpenNet/UI/Xaml/View/Pages/TasksPage.xaml')));
    const host = nodes.find(node => node.tag.includes('x:Name="SearchBoxMaterialHost"'));
    assert.match(host.tag, /VerticalAlignment="Center"/);
    const native = nodes.find(node => node.tag.includes('x:Name="SearchBox"'));
    const glass = nodes.find(node => node.tag.includes('x:Name="SearchBoxGlass"'));
    for (const node of [native, glass]) assert.match(node.tag, /Height="36"/);
    assert.match(glass.tag, /MinHeight="0"/);
    assert.match(glass.tag, /LiquidGlassInteraction.RestScale="1"/);
    assert.match(glass.tag, /LiquidGlassInteraction.FocusedScale="1"/);
});

test('sortable headers use natural text height and centered labels', () => {
    const styles = descendants(parse(read('OpenNet/Themes/Generic.xaml')));
    const style = styles.find(node => node.tag.includes('x:Key="DataTableColumnHeaderButtonStyle"'));
    assert.ok(style.children.some(node => /Property="MinHeight" Value="0"/.test(node.tag)));
    assert.ok(style.children.some(node => /Property="VerticalContentAlignment" Value="Center"/.test(node.tag)));
    const helper = read('OpenNet/UI/Xaml/Control/DataTableSortHelper.ixx');
    assert.match(helper, /label.VerticalAlignment\(VerticalAlignment::Center\)/);
    assert.match(helper, /label.TextWrapping\(TextWrapping::NoWrap\)/);
});
