/* Source-only migration. Uses real library controls, never shader-only imitations.
 * Leaves generated sources and the native settings editors untouched.
 * Run without --write for inventory, with --write for the mechanical migration.
 */
import fs from 'node:fs';
import path from 'node:path';
import { parse, descendants } from './collapse-material-presenters.mjs';
import { removeIncompatibleGlassHelpers } from './material-helper-targets.mjs';
const root = path.resolve(import.meta.dirname, '../OpenNet');
const types = { Button:'Button', ToggleButton:'ToggleButton', HyperlinkButton:'HyperlinkButton',
    CheckBox:'CheckBox', RadioButton:'RadioButton', Slider:'Slider', TextBox:'TextBox',
    ToggleSwitch:'ToggleSwitch', PasswordBox:'PasswordBox', ComboBox:'ComboBox', AutoSuggestBox:'SearchBox', ContentControl:'Card' };
const attributes = tag => Object.fromEntries([...tag.matchAll(/([\w.:]+)\s*=\s*"([^"]*)"/g)].map(m=>[m[1],m[2]]));
const remove = (tag, key) => tag.replace(new RegExp('\\s+'+key.replaceAll('.', '\\.')+'="[^"]*"'), '');
const set = (tag, key, value) => remove(tag,key).replace(/(\s*\/?>)$/, ' '+key+'="'+value+'"$1');
let serial = 0, total = 0;
function* files(dir) {
    for (const item of fs.readdirSync(dir,{withFileTypes:true})) {
        if (['Generated Files','x64','ARM64','obj','bin'].includes(item.name)) continue;
        const file=path.join(dir,item.name);
        if(item.isDirectory()) yield* files(file); else if(file.endsWith('.xaml')) yield file;
    }
}

if (process.argv.includes('--repair-boxed-values')) {
    for(const file of files(root)) {
        let text=fs.readFileSync(file,'utf8');
        const edits=[];
        for(const host of descendants(parse(text)).filter(n=>attributes(n.tag)['ouxct:ControlAppearance.IsMaterialHost']==='True')) {
            const before=host.tag;
            const after=before.replace(/Value="\{x:Bind (ouxct:MaterialControlState.Current.\w+Material), Mode=OneWay\}"/,
                'Value="{x:Bind ouxct:MaterialControlState.BoxInt32($1), Mode=OneWay}"');
            if(before!==after) edits.push({start:host.start,end:host.openEnd,value:after});
        }
        if(!edits.length) continue;
        for(const e of edits.sort((a,b)=>b.start-a.start))text=text.slice(0,e.start)+e.value+text.slice(e.end);
        fs.writeFileSync(file,text);
    }
}

if (process.argv.includes('--repair-shared-flyouts')) {
    for(const file of files(root)) {
        let text=fs.readFileSync(file,'utf8');
        for(;;) {
            const nodes=descendants(parse(text)), edits=[];
            for(const host of nodes.filter(n=>attributes(n.tag)['ouxct:ControlAppearance.IsMaterialHost']==='True')) {
                const cases=host.children.filter(n=>n.name==='essential:Case');
                const native=cases[0]?.children.find(n=>!n.name.includes('.Value')), glass=cases[1]?.children.find(n=>!n.name.includes('.Value'));
                if(!native||!glass) continue;
                const name=attributes(native.tag)['x:Name'];
                for(const property of glass.children.filter(n=>/\.(ContextFlyout|Flyout)$/.test(n.name))) {
                    // A FlyoutBase is shareable; duplicating it duplicates every nested
                    // named binding/control and creates two independent settings trees.
                    const key=property.name.split('.').at(-1);
                    edits.push({start:property.start,end:property.end,value:''});
                    edits.push({start:glass.start,end:glass.openEnd,value:set(glass.tag,key,'{x:Bind '+name+'.'+key+', Mode=OneWay}')});
                }
            }
            if(!edits.length) break;
            // Remove outer duplicate flyouts first; discard edits inside those removals.
            const removes=edits.filter(e=>e.value==='');
            const filtered=edits.filter(e=>!removes.some(r=>r!==e&&r.start<=e.start&&r.end>=e.end));
            for(const e of filtered.sort((a,b)=>b.start-a.start)) text=text.slice(0,e.start)+e.value+text.slice(e.end);
        }
        fs.writeFileSync(file,text);
    }
}

if (process.argv.includes('--write') || process.argv.includes('--repair-material-layout')) {
    for (const file of files(root)) {
        let text=fs.readFileSync(file,'utf8');
        const edits=[];
        for (const host of descendants(parse(text)).filter(n=>attributes(n.tag)['ouxct:ControlAppearance.IsMaterialHost']==='True')) {
            const cases=host.children.filter(n=>n.name==='essential:Case');
            const native=cases[0]?.children.find(n=>!n.name.includes('.Value')), glass=cases[1]?.children.find(n=>!n.name.includes('.Value'));
            if(!native||!glass) continue;
            let hostTag=host.tag, nativeTag=native.tag, glassTag=remove(glass.tag,'Style');
            for (const key of ['Background','CornerRadius']) glassTag=remove(glassTag,key);
            if (glass.name==='wl:LiquidGlassSlider') glassTag=remove(glassTag,'Foreground');
            for (const property of glass.children.filter(n=>n.name.endsWith('.Template')))
                edits.push({start:property.start,end:property.end,value:''});
            // Height must reach the library control itself so its authored Viewbox
            // can scale; constraining only the host would crop the Kube geometry.
            for (const [key,value] of Object.entries(attributes(hostTag))) {
                if (/^(Width|Height|MinWidth|MinHeight|MaxWidth|MaxHeight)$/.test(key)) {
                    hostTag=remove(hostTag,key); nativeTag=set(nativeTag,key,value); glassTag=set(glassTag,key,value);
                }
            }
            glassTag=set(glassTag,'HorizontalAlignment','Stretch');
            glassTag=set(glassTag,'VerticalAlignment','Stretch');
            for(const [node,value] of [[host,hostTag],[native,nativeTag],[glass,glassTag]])
                if(node.tag!==value) edits.push({start:node.start,end:node.openEnd,value});
        }
        if(!edits.length) continue;
        for(const e of edits.sort((a,b)=>b.start-a.start)) text=text.slice(0,e.start)+e.value+text.slice(e.end);
        fs.writeFileSync(file,text);
    }
}
for(const file of files(root)) {
    let text=fs.readFileSync(file,'utf8'), count=0;
    const proxies=[];
    const className=text.match(/x:Class="([^"]+)"/)?.[1];
    for (;;) {
        const nodes=descendants(parse(text));
        const candidates=nodes.filter(n=>types[n.name]&&attributes(n.tag)['ouxct:ControlAppearance.IsEnabled']==='True');
        const leaves=candidates.filter(n=>!descendants(n.children).some(c=>candidates.includes(c)));
        if(!leaves.length) break;
        const edits=[];
        for(const n of leaves) {
            const attr=attributes(n.tag), kind=types[n.name], id=++serial;
            // Transparent headers/inline buttons remain intentionally native.
            if(/SubtleButtonStyle|DataTableColumnHeaderButtonStyle/.test(attr.Style??'')) {
                edits.push({start:n.start,end:n.openEnd,value:remove(n.tag,'ouxct:ControlAppearance.IsEnabled')});
                continue;
            }
            let native=text.slice(n.start,n.end), nativeTag=remove(n.tag,'ouxct:ControlAppearance.IsEnabled');
            const name=attr['x:Name'] || 'MaterialNative_'+id;
            nativeTag=set(nativeTag,'x:Name',name);
            let outer='';
            for(const [key,value] of Object.entries(attr)) {
                if(/^(Grid\.|Canvas\.)/.test(key)||/^(Margin|Width|Height|MinWidth|MinHeight|MaxWidth|MaxHeight|HorizontalAlignment|VerticalAlignment|Visibility|x:Load|x:DeferLoadStrategy)$/.test(key)) {
                    outer+=' '+key+'="'+value+'"';
                    nativeTag=remove(nativeTag,key);
                }
            }
            native=nativeTag+native.slice(n.tag.length);
            let glass=native.replace(new RegExp('(?<=<\\/?|<)'+n.name+'(?=[\\s.>])','g'),'wl:LiquidGlass'+kind);
            // All named descendants need distinct identifiers; original x:Bind sources
            // intentionally keep referencing the original contract/named elements.
            glass=glass.replace(/x:Name="([^"]+)"/g,(_,v)=>'x:Name="'+v+'Glass_'+id+'"');
            let glassTag=glass.match(/^<[^>]+>/)[0];
            glassTag=removeIncompatibleGlassHelpers(glassTag,kind);
            const events={ToggleSwitch:['Toggled'],CheckBox:['Checked','Unchecked','Indeterminate'],RadioButton:['Checked','Unchecked'],
                ToggleButton:['Checked','Unchecked','Indeterminate'],Slider:['ValueChanged'],TextBox:['TextChanged','TextChanging'],ComboBox:['SelectionChanged'],PasswordBox:['PasswordChanged']};
            for(const event of events[kind]??[]) glassTag=remove(glassTag,event);
            if(kind==='PasswordBox') {
                for(const key of ['x:Uid','Header','HeaderTemplate','PasswordRevealMode','MaxLength','PasswordChar']) glassTag=remove(glassTag,key);
            }
            if(['ToggleSwitch','PasswordBox','SearchBox'].includes(kind)&&attr.Loaded&&className&&!attr.Loaded.includes('{')) {
                const proxy='MaterialLoaded_'+id;
                glassTag=set(glassTag,'Loaded',proxy);
                proxies.push({name:proxy,native:name,method:attr.Loaded});
            }
            glassTag=set(glassTag,'ouxct:MaterialControlState.IsManaged','True');
            glass=glassTag+glass.slice(glass.match(/^<[^>]+>/)[0].length);
            const indent=text.slice(text.lastIndexOf('\n',n.start)+1,n.start);
            const hostName=name+'MaterialHost';
            const value='<essential:SwitchPresenter x:Name="'+hostName+'" HorizontalContentAlignment="Stretch" VerticalContentAlignment="Stretch" ouxct:ControlAppearance.IsMaterialHost="True" Value="{x:Bind ouxct:MaterialControlState.Current.'+kind+'Material, Mode=OneWay}"'+outer+'>\n'+
                indent+'    <essential:Case><essential:Case.Value><x:Int32>0</x:Int32></essential:Case.Value>\n'+indent+'        '+native+'\n'+indent+'    </essential:Case>\n'+
                indent+'    <essential:Case><essential:Case.Value><x:Int32>1</x:Int32></essential:Case.Value>\n'+indent+'        '+glass+'\n'+indent+'    </essential:Case>\n'+indent+'</essential:SwitchPresenter>';
            edits.push({start:n.start,end:n.end,value}); count++;
        }
        for(const e of edits.sort((a,b)=>b.start-a.start)) text=text.slice(0,e.start)+e.value+text.slice(e.end);
    }
    if(!count) continue;
    const first=parse(text)[0];
    let tag=first.tag;
    for(const [prefix,namespace] of [['essential','WinUI3Package'],['wl','WinUI.LiquidGlass']])
        if(!tag.includes('xmlns:'+prefix+'=')) tag=set(tag,'xmlns:'+prefix,'using:'+namespace);
    text=text.slice(0,first.start)+tag+text.slice(first.openEnd);
    console.log(path.relative(root,file)+': '+count); total+=count;
    if(!process.argv.includes('--write')) continue;
    fs.writeFileSync(file,text);
    if(proxies.length) {
        const header=file+'.h', cpp=file+'.cpp', simple=className.split('.').at(-1);
        let h=fs.readFileSync(header,'utf8');
        const declarations=proxies.map(p=>'        void '+p.name+'(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);').join('\n');
        h=h.replace(new RegExp('('+simple+'\\(\\);)'), '$1\n'+declarations);
        fs.writeFileSync(header,h);
        let c=fs.readFileSync(cpp,'utf8');
        c=c.replace('#include "XamlWorkaround.h"', '#include "XamlWorkaround.h"\n#include "UI/Xaml/Control/Theme/MaterialControlBridge.h"');
        c+='\nnamespace winrt::'+className.split('.').slice(0,-1).join('::')+'::implementation\n{\n'+
            proxies.map(p=>'    void '+simple+'::'+p.name+'(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)\n    {\n        ::OpenNet::UI::EnsureMaterialHostBridge(sender.as<winrt::Microsoft::UI::Xaml::DependencyObject>());\n        '+p.method+'('+p.native+'(), args);\n    }').join('\n')+'\n}\n';
        fs.writeFileSync(cpp,c);
    }
}
console.log('Restored actual library pairs: '+total);

// Keep native/STL headers before module imports, and keep UTF-8 BOM at EOF's
// opposite end: at the beginning, never between C++ preprocessing directives.
if (process.argv.includes('--write') || process.argv.includes('--repair-preambles')) {
    for (const xaml of files(root)) {
        const cpp=xaml+'.cpp';
        if(!fs.existsSync(cpp)) continue;
        const before=fs.readFileSync(cpp,'utf8');
        const header='#include "UI/Xaml/Control/Theme/MaterialControlBridge.h"';
        if(!before.startsWith(header)) continue;
        const bom=before.includes('\uFEFF')?'\uFEFF':'';
        let after=before.replaceAll('\uFEFF','').slice(header.length).replace(/^\r?\n/,'');
        after=after.replace('#include "XamlWorkaround.h"','#include "XamlWorkaround.h"\n'+header);
        fs.writeFileSync(cpp,bom+after);
    }
}
