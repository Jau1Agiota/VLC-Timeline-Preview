# SPDX-License-Identifier: GPL-2.0-or-later
# Startup-only tests: never execute the application's setup/download body.
param([string]$SourcePath=(Join-Path $PSScriptRoot '..\..\extras\package\portable\Start-Portable.ps1'), [switch]$NativeProbe)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $SourcePath).Path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
foreach($name in @('Initialize-PortableLibrary','Assert-PortableVulkanLoader')){
    $functions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false) | Where-Object {$_.Name -eq $name})
    if($functions.Count -ne 1){throw "Missing/duplicate startup function: $name"}
    . ([scriptblock]::Create($functions[0].Extent.Text))
}
if($NativeProbe){
    $nativeRoot=Join-Path ([IO.Path]::GetTempPath()) ('vlc-vulkan-native-'+[guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($nativeRoot) | Out-Null
    try {
    Initialize-PortableLibrary
    $module=[PortableLibrary]::LoadLibraryEx('vulkan-1.dll',[IntPtr]::Zero,0x1000)
    $code=[PortableLibrary]::LastError()
    if($module -ne [IntPtr]::Zero){
        [PortableLibrary]::FreeLibrary($module) | Out-Null
        Assert-PortableVulkanLoader $nativeRoot
        Write-Output 'PASS: real Windows loader available; startup probe accepted it without GPU enumeration.'
    }else{
        try {Assert-PortableVulkanLoader $nativeRoot;throw 'Unexpected probe success'}
        catch {if($_.Exception.Message -notmatch 'vulkan-1.dll' -or $_.Exception.Message -notmatch 'Windows error'){throw}}
        Write-Output "PASS: real Windows loader unavailable (error $code); actionable prerequisite diagnostic received."
    }
    } finally {Remove-Item -LiteralPath $nativeRoot -Recurse -Force}
    exit 0
}
Add-Type @'
using System;
public static class PortableLibrary {
 public static IntPtr Next = new IntPtr(1234);
 public static int Error = 0;
 public static int Frees = 0;
 public static uint Flags;
 public static string Path;
 public static IntPtr LoadLibraryEx(string path, IntPtr file, uint flags) { Path=path; Flags=flags; return Next; }
 public static int LastError() { return Error; }
 public static bool FreeLibrary(IntPtr module) { if(module == IntPtr.Zero) throw new Exception("Invalid release"); Frees++; return true; }
}
'@
$root=Join-Path ([IO.Path]::GetTempPath()) ('vlc-vulkan-guard-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    # Present loader succeeds even though fake class exposes no GPU/Vulkan API.
    Assert-PortableVulkanLoader $root
    if([PortableLibrary]::Frees -ne 1 -or [PortableLibrary]::Flags -ne 0x1000 -or [PortableLibrary]::Path -ne 'vulkan-1.dll'){throw 'Unsafe search or missing release'}
    Assert-PortableVulkanLoader $root
    if([PortableLibrary]::Frees -ne 2){throw 'Repeated probe leaked a loader reference'}
    foreach($code in @(126,193,1114)){
        [PortableLibrary]::Next=[IntPtr]::Zero;[PortableLibrary]::Error=$code
        try {Assert-PortableVulkanLoader $root;throw 'Missing loader accepted'}
        catch {
            $message=$_.Exception.Message
            foreach($required in @('vulkan-1.dll',"Windows error $code",'PC or graphics vendor','do not download loose DLLs','does not require a Vulkan-capable GPU')){
                if(-not $message.Contains($required)){throw "Incomplete dependency diagnostic: $message"}
            }
            if($code -eq 193 -and -not $message.Contains('Windows x64 process')){throw 'Missing architecture guidance'}
        }
        if([PortableLibrary]::Frees -ne 2){throw 'Invalid handle released'}
    }
    # Preserve existing DLL-directory search semantics; this file is never loaded.
    $local=Join-Path $root 'vulkan-1.dll';[IO.File]::WriteAllText($local,'mock fixture, not a DLL')
    [PortableLibrary]::Next=[IntPtr]::new(1234);[PortableLibrary]::Error=0
    Assert-PortableVulkanLoader $root
    if([PortableLibrary]::Path -ne $local -or [PortableLibrary]::Flags -ne 0x1100 -or [PortableLibrary]::Frees -ne 3){throw 'Runtime-local compatibility/search mismatch'}
    if([IO.File]::ReadAllText($local) -ne 'mock fixture, not a DLL'){throw 'Probe modified dependency'}
    Write-Output 'PASS: success without GPU query, repeated release, missing/transitive/architecture errors, safe search, local compatibility and no dependency modification.'
} finally {Remove-Item -LiteralPath $root -Recurse -Force}
