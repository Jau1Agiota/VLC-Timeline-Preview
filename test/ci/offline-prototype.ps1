# SPDX-License-Identifier: GPL-2.0-or-later
# Synthetic layout tests, never a playable VLC package or third-party DLL upload.
param([string]$Repository=(Join-Path $PSScriptRoot '..\..'))
$ErrorActionPreference='Stop'
$repo=(Resolve-Path $Repository).Path
$assembly=Join-Path $repo 'extras/package/portable/Assemble-Offline.ps1'
$startup=Join-Path $repo 'extras/package/portable/Start-Portable.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($assembly,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
foreach($function in $ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){. ([scriptblock]::Create($function.Extent.Text))}
$startAst=[Management.Automation.Language.Parser]::ParseFile($startup,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
$code=[IO.File]::ReadAllText($startup)
foreach($forbidden in @('Net.WebClient','DownloadFile','ServicePointManager','Setup-Runtime.ps1','Get-Runtime.ps1','tar.exe')){if($code.Contains($forbidden)){throw "Offline startup retained a network/setup operation: $forbidden"}}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Join-Path ([IO.Path]::GetTempPath()) ('offline-prototype-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
function Reject([scriptblock]$Action,[string]$Expected){
    $caught=$null
    try {& $Action | Out-Null}catch{$caught=$_.Exception.Message}
    if(-not $caught -or $caught -notlike "*$Expected*"){throw "Expected rejection '$Expected', observed '$caught'"}
}
try {
    $base=Join-Path $root 'Original Stage F with spaces';[IO.Directory]::CreateDirectory($base) | Out-Null
    $dll=Join-Path $root 'local original fixture.dll';[IO.File]::WriteAllText($dll,'synthetic fixture, not libmpv')
    $output=Join-Path $root 'VLC stage248D private with spaces'
    Reject {Assert-OriginalRuntime (Join-Path $root 'missing.dll')} 'missing'
    Reject {Assert-OriginalRuntime $dll} 'SHA-256 mismatch'
    foreach($bad in @('../outside','a/../outside','/absolute','C:/file','a\b','a//b','CON','a.')){Reject {Get-PackagePath $base $bad} 'Unsafe'}
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly)} 'SHA-256 mismatch'
    if(Test-Path $output){throw 'Invalid runtime created output'}
    # Positive assembly path is exercised with a scoped validator fixture only.
    # There is NO production parameter or environment switch to bypass the DLL pin.
    function Assert-OriginalRuntime([string]$Path){
        if(-not [IO.File]::Exists($Path)){throw 'fixture missing'}
        if([IO.File]::ReadAllText($Path) -ne 'synthetic fixture, not libmpv'){throw 'unexpected fixture'}
        return Get-LocalHash $Path
    }
    $files=[ordered]@{}
    $names=@('vlc.exe','libvlc.dll','libvlccore.dll','Start VLC Timeline Preview.exe','plugins/gui/libqt_plugin.dll','thumbnail-preview/thumbnail-helper.exe','thumbnail-preview/libvlc.dll','thumbnail-preview/libvlccore.dll','thumbnail-mpv/mpv-thumbnail-worker.exe','thumbnail-mpv/mpv-thumbnail-accurate-worker.exe','thumbnail-mpv/dependencies.json','COPYING.txt','AUTHORS.txt','THANKS.txt','thumbnail-preview/COPYING.txt','portable/Start-Portable.ps1','CI_1_README.txt','thumbnail-mpv/Get-Runtime.ps1','thumbnail-mpv/Setup-Runtime.ps1','thumbnail-mpv/Start-Preview.ps1')
    foreach($name in $names){
        $path=Get-PackagePath $base $name;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
        if($name.EndsWith('.exe') -or $name.EndsWith('.dll')){
            $bytes=New-Object byte[] 256;$bytes[0]=0x4d;$bytes[1]=0x5a;$bytes[60]=64;$bytes[64]=0x50;$bytes[65]=0x45;$bytes[68]=0x64;$bytes[69]=0x86;$bytes[88]=0x0b;$bytes[89]=0x02
            [IO.File]::WriteAllBytes($path,$bytes)
        }elseif($name -eq 'thumbnail-mpv/dependencies.json'){
            [IO.File]::WriteAllText($path,('{"dll_sha256":"'+(Get-LocalHash $dll)+'"}'))
        }else{[IO.File]::WriteAllText($path,'original fixture')}
        $files[$name]=Get-LocalHash $path
    }
    [IO.File]::WriteAllText((Join-Path $base 'BUILD_MANIFEST.json'),(@{source_commit='e29d1dc98c01e64de60de0fe2f299acd2af57aca';files_sha256=$files} | ConvertTo-Json -Depth 8))
    [IO.Directory]::CreateDirectory((Join-Path $base 'portable-data/runtime')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $base 'portable-data/runtime/do-not-copy.txt'),'private cache')
    Reject {Invoke-OfflineAssembly $base $dll (Join-Path $base 'unsafe output') (Split-Path $assembly)} 'outside'
    $missing=Join-Path $base 'vlc.exe';$save=[IO.File]::ReadAllBytes($missing);[IO.File]::Delete($missing)
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly)} 'Missing baseline file'
    [IO.File]::WriteAllBytes($missing,$save)
    [IO.File]::WriteAllText($missing,'corrupt')
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly)} 'differs'
    [IO.File]::WriteAllBytes($missing,$save)
    Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly)
    if(Test-Path (Join-Path $output 'portable-data')){throw 'Used cache copied'}
    foreach($name in @('Get-Runtime.ps1','Setup-Runtime.ps1','Start-Preview.ps1')){if(Test-Path (Join-Path $output ('thumbnail-mpv/'+$name))){throw 'Old setup script shipped'}}
    if((Get-LocalHash (Join-Path $output 'thumbnail-mpv/mpv-thumbnail-accurate-worker.exe')) -ne $files['thumbnail-mpv/mpv-thumbnail-accurate-worker.exe']){throw 'Worker changed'}
    $oldHash=Get-LocalHash ($output+'.zip')
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly)} 'already exists'
    if((Get-LocalHash ($output+'.zip')) -ne $oldHash){throw 'Repeated assembly changed ZIP'}
    $extracted=Join-Path $root 'Fresh extraction with spaces';[IO.Compression.ZipFile]::ExtractToDirectory(($output+'.zip'),$extracted)
    $app=Join-Path $extracted ([IO.Path]::GetFileName($output))
    if(-not (Test-Path (Join-Path $app 'Start VLC Timeline Preview.exe'))){throw 'Launcher missing after ZIP round trip'}
    if((Get-LocalHash (Join-Path $app 'thumbnail-mpv/libmpv-2.dll')) -ne (Get-LocalHash $dll)){throw 'ZIP runtime mismatch'}
    if([IO.File]::ReadAllText((Join-Path $base 'portable-data/runtime/do-not-copy.txt')) -ne 'private cache'){throw 'Original cache modified'}
    Write-Output 'PASS: original-pin rejection, missing files, wrong hashes, path safety, spaced paths, repeat refusal, cache exclusion, ZIP readback, extraction and original preservation. Positive fixture is not real VLC/runtime acceptance.'
}finally{Remove-Item -LiteralPath $root -Recurse -Force}
