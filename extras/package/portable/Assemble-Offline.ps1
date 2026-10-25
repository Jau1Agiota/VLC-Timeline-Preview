# SPDX-License-Identifier: GPL-2.0-or-later
# Private local assembly only. No network, installers, original writes or cleanup.
param([Parameter(Mandatory=$true)][string]$BaseApplication,
      [Parameter(Mandatory=$true)][string]$RuntimeDll,
      [Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility -ErrorAction Stop
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Get-LocalHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Assert-NoReparse([string]$Path) {
    $item=Get-Item -LiteralPath $Path -Force
    while($null -ne $item){
        if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw "Reparse points/junctions are not accepted: $($item.FullName)"}
        $parent=if($item -is [IO.FileInfo]){$item.Directory}else{$item.Parent}
        $item=$parent
    }
}
function Get-PackagePath([string]$Root,[string]$Relative) {
    if(-not $Relative -or $Relative.Contains('\') -or $Relative.Contains(':') -or $Relative.StartsWith('/')){throw 'Unsafe package path'}
    foreach($part in $Relative.Split('/')){
        if(-not $part -or $part -eq '.' -or $part -eq '..' -or $part -match '[<>"|?*]' -or $part.TrimEnd(' ','.') -ne $part -or $part -match '^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)'){throw 'Unsafe package path component'}
    }
    return Join-Path $Root ($Relative.Replace('/',[IO.Path]::DirectorySeparatorChar))
}
function Assert-OriginalRuntime([string]$Path) {
    if(-not [IO.File]::Exists($Path)){throw 'Original DLL is missing. Supply your existing verified Stage F libmpv-2.dll; nothing will be downloaded.'}
    Assert-NoReparse $Path
    $hash=Get-LocalHash $Path
    if($hash -ne '04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a'){throw 'Original libmpv DLL SHA-256 mismatch. No replacement runtime is accepted.'}
    return $hash
}
function Assert-Pe64([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    try {
        $reader=New-Object IO.BinaryReader($stream)
        if($reader.ReadUInt16() -ne 0x5a4d -or $stream.Length -lt 90){throw 'Invalid PE file'}
        $stream.Position=60;$offset=$reader.ReadUInt32()
        if($offset -lt 64 -or $offset -gt 1048576 -or $offset+26 -gt $stream.Length){throw 'Invalid PE header offset'}
        $stream.Position=$offset
        if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664){throw 'Expected Windows x64 PE'}
        $stream.Position=$offset+24
        if($reader.ReadUInt16() -ne 0x20b){throw 'Expected PE32+ format'}
    }finally{$stream.Dispose()}
}
function Invoke-OfflineAssembly([string]$BaseApplication,[string]$RuntimeDll,[string]$Destination,[string]$OverlayDirectory) {
    $base=[IO.Path]::GetFullPath($BaseApplication).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $dll=[IO.Path]::GetFullPath($RuntimeDll)
    $dest=[IO.Path]::GetFullPath($Destination).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $zip=$dest+'.zip';$checksum=$zip+'.sha256'
    foreach($path in @($dest,$zip,$checksum)){if(Test-Path -LiteralPath $path){throw "Output already exists; choose a new destination: $path"}}
    Assert-NoReparse $base
    $parent=[IO.Path]::GetDirectoryName($dest)
    if(-not [IO.Directory]::Exists($parent)){throw 'Destination parent must already exist'}
    Assert-NoReparse $parent
    Get-PackagePath $parent ([IO.Path]::GetFileName($dest)) | Out-Null
    if($dest.Equals($base,[StringComparison]::OrdinalIgnoreCase) -or $dest.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Destination must be outside the Stage F installation'}
    $repository=[IO.Path]::GetFullPath((Join-Path $OverlayDirectory '../../..')).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if((Test-Path -LiteralPath (Join-Path $repository '.git')) -and ($dest.Equals($repository,[StringComparison]::OrdinalIgnoreCase) -or $dest.StartsWith($repository+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))){throw 'Private package destination must be outside the public repository checkout'}
    $originalHash=Assert-OriginalRuntime $dll
    $baseManifest=Join-Path $base 'BUILD_MANIFEST.json'
    if(-not [IO.File]::Exists($baseManifest)){throw 'BUILD_MANIFEST.json is missing. Select the extracted Stage F application folder, not an outer download folder.'}
    Assert-NoReparse $baseManifest
    $metadata=[IO.File]::ReadAllText($baseManifest) | ConvertFrom-Json
    if($metadata.source_commit -ne 'e29d1dc98c01e64de60de0fe2f299acd2af57aca' -or -not $metadata.files_sha256){throw 'Expected an original Stage F package with its producer BUILD_MANIFEST.json'}
    $required=@('vlc.exe','libvlc.dll','libvlccore.dll','Start VLC Timeline Preview.exe','plugins/gui/libqt_plugin.dll','thumbnail-preview/thumbnail-helper.exe','thumbnail-preview/libvlc.dll','thumbnail-preview/libvlccore.dll','thumbnail-mpv/mpv-thumbnail-worker.exe','thumbnail-mpv/mpv-thumbnail-accurate-worker.exe','thumbnail-mpv/dependencies.json','COPYING.txt','AUTHORS.txt','THANKS.txt','thumbnail-preview/COPYING.txt')
    foreach($name in $required){if($null -eq $metadata.files_sha256.PSObject.Properties[$name]){throw "Required Stage F component missing from manifest: $name"}}
    foreach($entry in $metadata.files_sha256.PSObject.Properties){
        if($entry.Name -match '^(portable-data/|OFFLINE_PROTOTYPE_MANIFEST.json$|BASE_STAGE_F_BUILD_MANIFEST.json$)' -or ($entry.Name.ToLowerInvariant().EndsWith('libmpv-2.dll') -or $entry.Name.ToLowerInvariant().EndsWith('vulkan-1.dll'))){throw 'Base manifest contains runtime cache or reserved prototype material'}
        if($entry.Value -notmatch '^[a-fA-F0-9]{64}$'){throw 'Invalid baseline file digest'}
        $source=Get-PackagePath $base $entry.Name
        if(-not [IO.File]::Exists($source)){throw "Missing baseline file: $($entry.Name)"}
        Assert-NoReparse $source
        if((Get-LocalHash $source) -ne $entry.Value){throw "Baseline file differs from its manifest: $($entry.Name). Use an unmodified Stage F extraction."}
    }
    foreach($name in $required | Where-Object {$_ -like '*.exe' -or $_ -like '*.dll'}){Assert-Pe64 (Get-PackagePath $base $name)}
    $dependency=[IO.File]::ReadAllText((Join-Path $base 'thumbnail-mpv/dependencies.json')) | ConvertFrom-Json
    if($dependency.dll_sha256 -ne $originalHash){throw 'Base runtime pin does not match the original DLL'}
    foreach($name in @('Start-Portable.ps1','OFFLINE_README.txt','DEPENDENCIES.md')){if(-not [IO.File]::Exists((Join-Path $OverlayDirectory $name))){throw "Assembly support file missing: $name"}}
    # All inputs verified before output creation. Never copy used caches/profiles.
    New-Item -ItemType Directory -Path $dest -ErrorAction Stop | Out-Null
    $omit=@('CI_1_README.txt','README.txt','portable/Start-Portable.ps1','portable/DEPENDENCIES.md','thumbnail-mpv/Get-Runtime.ps1','thumbnail-mpv/Setup-Runtime.ps1','thumbnail-mpv/Start-Preview.ps1')
    foreach($entry in $metadata.files_sha256.PSObject.Properties){
        if($entry.Name -in $omit){continue}
        $target=Get-PackagePath $dest $entry.Name
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
        [IO.File]::Copy((Get-PackagePath $base $entry.Name),$target,$false)
        if((Get-LocalHash $target) -ne $entry.Value){throw 'Copied component failed hash verification'}
    }
    [IO.Directory]::CreateDirectory((Join-Path $dest 'portable')) | Out-Null
    [IO.File]::Copy($baseManifest,(Join-Path $dest 'BASE_STAGE_F_BUILD_MANIFEST.json'),$false)
    [IO.File]::Copy((Join-Path $OverlayDirectory 'Start-Portable.ps1'),(Join-Path $dest 'portable/Start-Portable.ps1'),$false)
    [IO.File]::Copy((Join-Path $OverlayDirectory 'DEPENDENCIES.md'),(Join-Path $dest 'portable/DEPENDENCIES.md'),$false)
    [IO.File]::Copy((Join-Path $OverlayDirectory 'OFFLINE_README.txt'),(Join-Path $dest 'README.txt'),$false)
    [IO.File]::Copy($dll,(Join-Path $dest 'thumbnail-mpv/libmpv-2.dll'),$false)
    if((Assert-OriginalRuntime (Join-Path $dest 'thumbnail-mpv/libmpv-2.dll')) -ne $originalHash){throw 'Copied runtime verification failed'}
    $files=[ordered]@{}
    foreach($path in Get-ChildItem -LiteralPath $dest -Recurse -File | Sort-Object FullName){
        $relative=$path.FullName.Substring($dest.Length+1).Replace('\','/')
        $files[$relative]=Get-LocalHash $path.FullName
    }
    $manifest=[ordered]@{prototype='2.4.8D';private_local_prototype=$true;public_redistribution_cleared=$false;runtime_downloads=$false;dll_sha256=$originalHash;baseline_source=$metadata.source_commit;baseline_manifest_sha256=(Get-LocalHash $baseManifest);files_sha256=$files}
    $manifestPath=Join-Path $dest 'OFFLINE_PROTOTYPE_MANIFEST.json'
    [IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
    $files['OFFLINE_PROTOTYPE_MANIFEST.json']=Get-LocalHash $manifestPath
    # .NET Framework used by inbox PS5 can emit backslashes via CreateFromDirectory.
    # Write canonical forward-slash entries explicitly for consistent extraction.
    $archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {
        $prefix=[IO.Path]::GetFileName($dest)+'/'
        foreach($relative in $files.Keys){
            $entry=$archive.CreateEntry($prefix+$relative,[IO.Compression.CompressionLevel]::Optimal)
            $inputStream=[IO.File]::OpenRead((Get-PackagePath $dest $relative))
            try {
                $outputStream=$entry.Open()
                try{$inputStream.CopyTo($outputStream)}finally{$outputStream.Dispose()}
            }finally{$inputStream.Dispose()}
        }
    }finally{$archive.Dispose()}
    # Read/hash every archived file, rather than relying solely on ZIP creation.
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $seen=@{};$prefix=[IO.Path]::GetFileName($dest)+'/'
        foreach($entry in $archive.Entries){
            if($entry.FullName.EndsWith('/')){continue}
            if(-not $entry.FullName.StartsWith($prefix,[StringComparison]::Ordinal)){throw 'Unexpected ZIP root'}
            $relative=$entry.FullName.Substring($prefix.Length)
            if(-not $files.Contains($relative) -or $seen.ContainsKey($relative)){throw 'Unexpected or duplicate ZIP entry'}
            $stream=$entry.Open();$hasher=[Security.Cryptography.SHA256]::Create()
            try{$digest=([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$hasher.Dispose()}
            if($digest -ne $files[$relative]){throw 'ZIP readback checksum mismatch'}
            $seen[$relative]=$true
        }
        if($seen.Count -ne $files.Count){throw 'ZIP is incomplete'}
    }finally{$archive.Dispose()}
    $zipHash=Get-LocalHash $zip
    $sumStream=[IO.File]::Open($checksum,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$text=[Text.Encoding]::ASCII.GetBytes($zipHash+'  '+[IO.Path]::GetFileName($zip)+"`n");$sumStream.Write($text,0,$text.Length)}finally{$sumStream.Dispose()}
    Write-Output "Private local prototype: $zip"
    Write-Output "SHA-256: $zipHash"
}
Invoke-OfflineAssembly $BaseApplication $RuntimeDll $Destination $PSScriptRoot
