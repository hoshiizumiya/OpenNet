import assert from 'node:assert/strict';
import fs from 'node:fs';
import { test } from 'node:test';
import { collapse, descendants, parse } from '../scripts/collapse-material-presenters.mjs';

const read = name => fs.readFileSync(new URL(`../${name}`, import.meta.url), 'utf8');
const bridge = read('OpenNet/UI/Xaml/Control/Theme/MaterialControlBridge.cpp');
const templates = descendants(parse(read('OpenNet/UI/Xaml/Control/Theme/MaterialLibraryTemplates.xaml')))
    .filter(node => node.name === 'ControlTemplate');

test('material templates instantiate real library controls, not brush approximations', () => {
    assert.equal(templates.length, 11);
    for (const template of templates) {
        assert.equal(template.children.length, 1);
        assert.match(template.children[0].name, /^wl:LiquidGlass/);
        assert.match(template.children[0].tag, /x:Name="MaterialVisual"/);
        assert.match(template.children[0].tag, /MaterialControlState.IsManaged="True"/);
        assert.doesNotMatch(template.children[0].tag, /\b(?:Background|CornerRadius|Template)="/);
    }
    const slider = templates.find(node => node.tag.includes('MaterialSliderTemplate'));
    assert.doesNotMatch(slider.children[0].tag, /Foreground="/);
});

test('first transfer is business to visual, and a transient empty selection cannot write back', () => {
    assert.doesNotMatch(bridge, /if \(glassActive && twoWay\)/);
    assert.match(bridge, /copy\(native, nativeProperty, glass, glassProperty\);/);
    assert.match(bridge, /SelectedIndexProperty\(\)[\s\S]*?unbox_value<std::int32_t>\(from.GetValue\(property\)\) < 0\) return;/);
    assert.match(bridge, /UnregisterPropertyChangedCallback\(subscription.Property, subscription.Token\)/);
    assert.match(bridge, /native.GotFocus\(link.FocusToken\)/);
    assert.match(bridge, /\*link.Active = false;/);
    assert.match(bridge, /if \(!\*active \|\| \*updating\) return;/);
});

test('effect type names use compiled item bindings', () => {
    const xaml = read('OpenNet/UI/Xaml/View/Windows/LiquidGlassEffectsWindow.xaml');
    assert.doesNotMatch(xaml, /DisplayMemberPath="Name"/);
    assert.match(xaml, /<DataTemplate x:DataType="ouxct:LiquidGlassProfile">\s*<TextBlock Text="\{x:Bind Name\}"/);
});

test('backdrop persistence rejects empty selections before writing settings', () => {
    const cpp = read('OpenNet/UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.xaml.cpp');
    for (const name of ['BackgroundComboBox', 'MicaTypeComboBox', 'AcrylicTypeComboBox']) {
        assert.match(cpp, new RegExp(`selectedIndex = static_cast<int>\\(${name}\\(\\).SelectedIndex\\(\\)\\);\\s*if \\(selectedIndex < 0 \\|\\| selectedIndex >= static_cast<int>\\(${name}\\(\\).Items\\(\\).Size\\(\\)\\)\\) return;`));
    }
});

test('migration keeps business x:Bind, initialization, layout and original names', () => {
    const xaml = `<Grid xmlns:ouxct="using:OpenNet.UI.Xaml.Control.Theme"><essential:SwitchPresenter x:Name="VolumeHost" Grid.Column="2" Value="{x:Bind ouxct:MaterialControlState.Current.SliderMaterial, Mode=OneWay}"><essential:Case><essential:Case.Value><x:Int32>0</x:Int32></essential:Case.Value><Slider x:Name="Volume" Value="{x:Bind ViewModel.Volume, Mode=TwoWay}" Loaded="VolumeLoaded" /></essential:Case><essential:Case><essential:Case.Value><x:Int32>1</x:Int32></essential:Case.Value><wl:LiquidGlassSlider /></essential:Case></essential:SwitchPresenter></Grid>`;
    const result = collapse(xaml, true);
    assert.equal(result.count, 1);
    assert.doesNotMatch(result.text, /SwitchPresenter|LiquidGlassSlider/);
    for (const value of ['x:Name="Volume"', 'Grid.Column="2"', 'Loaded="VolumeLoaded"', 'Value="{x:Bind ViewModel.Volume, Mode=TwoWay}"', 'ouxct:ControlAppearance.IsEnabled="True"']) assert.ok(result.text.includes(value));
});

test('button template has one command owner and forwards native invocation', () => {
    const button = templates.find(node => node.tag.includes('MaterialButtonTemplate'));
    assert.doesNotMatch(button.children[0].tag, /\b(?:Command|Flyout)="/);
    assert.match(bridge, /PatternInterface::Invoke/);
    assert.match(bridge, /invoke.Invoke\(\)/);
});
