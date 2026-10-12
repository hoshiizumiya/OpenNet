import assert from 'node:assert/strict';
import fs from 'node:fs';
import { test } from 'node:test';
import { descendants, parse } from '../scripts/collapse-material-presenters.mjs';

const xaml = fs.readFileSync(new URL('../OpenNet/UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.xaml', import.meta.url), 'utf8');
const nodes = descendants(parse(xaml));
const bar = nodes.find(node => node.name === 'BreadcrumbBar' && node.tag.includes('x:Name="MainSettingsPageBar"'));

test('breadcrumb labels use their natural text height', () => {
    assert.ok(bar);
    const label = descendants(bar.children).find(node => node.name === 'TextBlock');
    assert.ok(label);
    assert.doesNotMatch(label.tag, /\b(?:Height|MinHeight|MaxHeight)="/);
    assert.match(label.tag, /VerticalAlignment="Center"/);
    assert.match(label.tag, /TextWrapping="NoWrap"/);
    assert.match(label.tag, /Text="\{x:Bind\}"/);
});

test('header spacing belongs to the settings NavigationView, not a negative offset', () => {
    assert.doesNotMatch(bar.tag, /Margin="[^\"]*-/);
    assert.match(bar.tag, /VerticalAlignment="Center"/);
    const nav = nodes.find(node => node.name === 'NavigationView' && node.tag.includes('x:Name="SettingsNavView"'));
    const resources = nav.children.find(node => node.name === 'NavigationView.Resources');
    assert.ok(resources);
    const margin = resources.children.find(node => node.tag.includes('x:Key="NavigationViewHeaderMargin"'));
    assert.ok(margin);
    assert.match(xaml.slice(margin.openEnd, margin.end), /^24,32,24,0<\/Thickness>$/);
});

test('breadcrumb collection and navigation event remain connected', () => {
    assert.match(bar.tag, /ItemsSource="\{x:Bind SettingsBarItems\}"/);
    assert.match(bar.tag, /ItemClicked="SettingsBar_ItemClicked"/);
});
