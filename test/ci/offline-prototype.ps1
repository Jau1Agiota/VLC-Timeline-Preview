# SPDX-License-Identifier: GPL-2.0-or-later
# Synthetic layout tests, never a playable VLC package or third-party DLL upload.
param([string]$Repository=(Join-Path $PSScriptRoot '..\..'), [string]$OriginalDll)
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
Add-Type -AssemblyName System.IO.Compression
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
    $badDll=Join-Path $root 'bad local fixture.dll';[IO.File]::WriteAllText($badDll,'synthetic fixture, not libmpv')
    $dll=if($OriginalDll){[IO.Path]::GetFullPath($OriginalDll)}else{$badDll}
    $output=Join-Path $root 'VLC stage248E private with spaces'
    $fixtureLauncher=Join-Path $root 'new launcher.exe'
    $bytes=New-Object byte[] 256;$bytes[0]=0x4d;$bytes[1]=0x5a;$bytes[60]=64;$bytes[64]=0x50;$bytes[65]=0x45;$bytes[68]=0x64;$bytes[69]=0x86;$bytes[88]=0x0b;$bytes[89]=0x02
    [IO.File]::WriteAllBytes($fixtureLauncher,$bytes)
    Reject {Assert-OriginalRuntime (Join-Path $root 'missing.dll')} 'missing'
    Reject {Assert-OriginalRuntime $badDll} 'SHA-256 mismatch'
    if($OriginalDll){Assert-OriginalRuntime $dll | Out-Null}
    foreach($bad in @('../outside','a/../outside','/absolute','C:/file','a\b','a//b','CON','a.')){Reject {Get-PackagePath $base $bad} 'Unsafe'}
    Reject {Invoke-OfflineAssembly $base $badDll $output (Split-Path $assembly) $fixtureLauncher} 'SHA-256 mismatch'
    if(Test-Path $output){throw 'Invalid runtime created output'}
    # Positive assembly path is exercised with a scoped validator fixture only.
    # There is NO production parameter or environment switch to bypass the DLL pin.
    if(-not $OriginalDll){
    function Assert-OriginalRuntime([string]$Path){
        if(-not [IO.File]::Exists($Path)){throw 'fixture missing'}
        if([IO.File]::ReadAllText($Path) -ne 'synthetic fixture, not libmpv'){throw 'unexpected fixture'}
        return Get-LocalHash $Path
    }
    }
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly) $fixtureLauncher} 'BUILD_MANIFEST.json is missing'
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
    Reject {Invoke-OfflineAssembly $base $dll (Join-Path $base 'unsafe output') (Split-Path $assembly) $fixtureLauncher} 'outside'
    $missing=Join-Path $base 'vlc.exe';$save=[IO.File]::ReadAllBytes($missing);[IO.File]::Delete($missing)
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly) $fixtureLauncher} 'Missing baseline file'
    [IO.File]::WriteAllBytes($missing,$save)
    [IO.File]::WriteAllText($missing,'corrupt')
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly) $fixtureLauncher} 'differs'
    [IO.File]::WriteAllBytes($missing,$save)
    Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly) $fixtureLauncher
    foreach($name in @('portable/Windows-Integration.ps1','portable/Register-Player.ps1','Register as Video Player.cmd','Remove Registration.cmd')){if(-not (Test-Path -LiteralPath (Join-Path $output $name))){throw 'Windows integration utility missing'}}
    $newManifest=[IO.File]::ReadAllText((Join-Path $output 'OFFLINE_PROTOTYPE_MANIFEST.json')) | ConvertFrom-Json
    if($newManifest.prototype -ne '2.4.8E' -or $newManifest.public_redistribution_cleared){throw 'Prototype/release gate changed'}
    if(Test-Path (Join-Path $output 'portable-data')){throw 'Used cache copied'}
    foreach($name in @('Get-Runtime.ps1','Setup-Runtime.ps1','Start-Preview.ps1')){if(Test-Path (Join-Path $output ('thumbnail-mpv/'+$name))){throw 'Old setup script shipped'}}
    if((Get-LocalHash (Join-Path $output 'thumbnail-mpv/mpv-thumbnail-accurate-worker.exe')) -ne $files['thumbnail-mpv/mpv-thumbnail-accurate-worker.exe']){throw 'Worker changed'}
    $oldHash=Get-LocalHash ($output+'.zip')
    Reject {Invoke-OfflineAssembly $base $dll $output (Split-Path $assembly) $fixtureLauncher} 'already exists'
    if((Get-LocalHash ($output+'.zip')) -ne $oldHash){throw 'Repeated assembly changed ZIP'}
    $check=[IO.Compression.ZipFile]::OpenRead($output+'.zip')
    try {foreach($entry in $check.Entries){if($entry.FullName.Contains('\')){throw 'Noncanonical ZIP separator'}}}finally{$check.Dispose()}
    $extracted=Join-Path $root 'Fresh extraction with spaces';[IO.Compression.ZipFile]::ExtractToDirectory(($output+'.zip'),$extracted)
    $app=Join-Path $extracted ([IO.Path]::GetFileName($output))
    if(-not (Test-Path (Join-Path $app 'Start VLC Timeline Preview.exe'))){throw 'Launcher missing after ZIP round trip'}
    if((Get-LocalHash (Join-Path $app 'thumbnail-mpv/libmpv-2.dll')) -ne (Get-LocalHash $dll)){throw 'ZIP runtime mismatch'}
    if([IO.File]::ReadAllText((Join-Path $base 'portable-data/runtime/do-not-copy.txt')) -ne 'private cache'){throw 'Original cache modified'}
    if($env:OS -eq 'Windows_NT'){
        # Run the REAL startup script in fresh PS5 children. Invalid/missing DLLs
        # must fail before Vulkan loading or any attempt to start the fake VLC.
        $pin='04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a'
        $dependencyPath=Join-Path $app 'thumbnail-mpv/dependencies.json'
        $dependency=[IO.File]::ReadAllText($dependencyPath) | ConvertFrom-Json
        $dependency.dll_sha256=$pin;[IO.File]::WriteAllText($dependencyPath,($dependency | ConvertTo-Json))
        $manifestPath=Join-Path $app 'OFFLINE_PROTOTYPE_MANIFEST.json'
        $manifest=[IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
        $manifest.dll_sha256=$pin;[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 8))
        $bundled=Join-Path $app 'thumbnail-mpv/libmpv-2.dll';[IO.File]::Delete($bundled)
        $ps=Join-Path $PSHOME 'powershell.exe'
        foreach($expected in @('Bundled libmpv-2.dll is missing','Bundled libmpv checksum mismatch')){
            $prior=$ErrorActionPreference
            try {
                $ErrorActionPreference='Continue'
                $details=(& $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $app 'portable/Start-Portable.ps1') -ValidateOnly 2>&1 | Out-String)
                $status=$LASTEXITCODE
            }finally{$ErrorActionPreference=$prior}
            if($status -eq 0 -or -not $details.Contains($expected)){throw "Offline startup failure diagnostic mismatch: $details"}
            [IO.File]::WriteAllText($bundled,'wrong hash fixture')
        }
        Write-Output 'PASS: real offline startup rejects missing/wrong-hash DLLs without setup or playback.'
    }
    Write-Output 'PASS: original-pin rejection, missing files, wrong hashes, path safety, spaced paths, repeat refusal, cache exclusion, ZIP readback, extraction and original preservation. Positive fixture is not real VLC/runtime acceptance.'
}finally{Remove-Item -LiteralPath $root -Recurse -Force}
