# SPDX-License-Identifier: GPL-2.0-or-later
# Invoked by the double-click launcher. No machine configuration or elevation.
param([ValidateSet('exact','fast')][string]$SeekMode='exact',
      [switch]$Diagnostics, [switch]$PlaybackDiagnostics, [switch]$ValidateOnly, [switch]$SetupOnly, [switch]$StageWorkerLocally, [string]$ExplorerArgumentsBase64)
$ErrorActionPreference='Stop'
# Explorer, cmd, and PowerShell 7 supply different PSModulePath values.
# Get-FileHash is a script function in Windows PowerShell's built-in utility
# module; import the OS copy explicitly rather than discover a caller's module.
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
Add-Type -AssemblyName System.Windows.Forms
$app=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# Startup-only loader probe: no GPU enumeration, Vulkan calls, installation or network.
function Initialize-PortableLibrary {
    if('PortableLibrary' -as [type]){return}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PortableLibrary {
 [DllImport("kernel32.dll", EntryPoint="LoadLibraryExW", CharSet=CharSet.Unicode, SetLastError=true)]
 private static extern IntPtr NativeLoadLibraryEx(string path, IntPtr file, uint flags);
 [ThreadStatic] private static int loadError;
 public static IntPtr LoadLibraryEx(string path, IntPtr file, uint flags) {
   IntPtr result = NativeLoadLibraryEx(path, file, flags);
   loadError = Marshal.GetLastWin32Error();
   return result;
 }
 public static int LastError() { return loadError; }
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern uint GetDriveType(string root);
}
'@
}
function Assert-PortableVulkanLoader([string]$RuntimeDirectory) {
    Initialize-PortableLibrary
    # Match the existing libmpv LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | DEFAULT_DIRS
    # policy, including an already present runtime-local loader. Never search CWD/PATH.
    $localLoader=Join-Path $RuntimeDirectory 'vulkan-1.dll'
    $path='vulkan-1.dll'
    $flags=0x00001000
    if([IO.File]::Exists($localLoader)){$path=[IO.Path]::GetFullPath($localLoader);$flags=0x00001100}
    $loader=[PortableLibrary]::LoadLibraryEx($path,[IntPtr]::Zero,$flags)
    $loadError=[PortableLibrary]::LastError()
    if($loader -eq [IntPtr]::Zero){
        $detail=if($loadError -eq 193){'The loader must be compatible with a Windows x64 process.'}else{'The loader is unavailable or one of its own dependencies could not be loaded.'}
        throw "The original preview runtime requires the Windows x64 Vulkan loader (vulkan-1.dll). $detail Windows error $loadError. Compatible, up-to-date graphics drivers commonly provide it, but availability is not guaranteed. Obtain drivers only from your PC or graphics vendor; do not download loose DLLs. This check does not require a Vulkan-capable GPU and does not install or download anything."
    }
    [PortableLibrary]::FreeLibrary($loader) | Out-Null
}

$gate=$null
$localWorker=$null
try {
    $integration=Join-Path $PSScriptRoot 'Windows-Integration.ps1'
    if(-not [IO.File]::Exists($integration)){throw 'Missing portable Windows integration support file. Extract the complete new ZIP.'}
    . $integration
    $media=@(ConvertFrom-ExplorerArguments $ExplorerArgumentsBase64)
    $data=Join-Path $app 'portable-data'
    # Fail when the private extracted folder is not writable.
    [IO.Directory]::CreateDirectory($data) | Out-Null
    $gate=[IO.File]::Open((Join-Path $data 'launch.lock'),'OpenOrCreate','ReadWrite','None')
    $mpv=Join-Path $app 'thumbnail-mpv'
    foreach($file in @('vlc.exe','libvlc.dll','libvlccore.dll','thumbnail-mpv\mpv-thumbnail-worker.exe','thumbnail-mpv\mpv-thumbnail-accurate-worker.exe','thumbnail-mpv\dependencies.json')) {
        if(-not [IO.File]::Exists((Join-Path $app $file))){throw "Missing package dependency: $file. Extract the complete ZIP again into a new folder."}
    }
    $originalHash='04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a'
    $lock=[IO.File]::ReadAllText((Join-Path $mpv 'dependencies.json')) | ConvertFrom-Json
    if($lock.dll_sha256 -ne $originalHash){throw 'Runtime metadata does not identify the approved original libmpv DLL.'}
    $manifestPath=Join-Path $app 'OFFLINE_PROTOTYPE_MANIFEST.json'
    if(-not [IO.File]::Exists($manifestPath)){throw 'Offline package manifest is missing. Use Assemble-Offline.ps1 with a verified local Stage F copy, then extract the complete new ZIP.'}
    $manifest=[IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
    if($manifest.prototype -ne '2.4.8E' -or $manifest.dll_sha256 -ne $originalHash -or -not $manifest.files_sha256){throw 'Unexpected offline package manifest. Use a fresh local assembly.'}
    $runtime=$mpv
    $dll=Join-Path $runtime 'libmpv-2.dll'
    if(-not [IO.File]::Exists($dll)){throw 'Bundled libmpv-2.dll is missing. Reassemble this private package using your verified original DLL. This launcher never downloads runtimes.'}
    if((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant() -ne $originalHash){throw 'Bundled libmpv checksum mismatch. Use the approved original DLL and a fresh local assembly.'}
    foreach($file in @('vlc.exe','libvlc.dll','libvlccore.dll','Start VLC Timeline Preview.exe','plugins/gui/libqt_plugin.dll','thumbnail-mpv/mpv-thumbnail-worker.exe','thumbnail-mpv/mpv-thumbnail-accurate-worker.exe','portable/Windows-Integration.ps1')) {
        $entry=$manifest.files_sha256.PSObject.Properties[$file]
        $path=Join-Path $app ($file.Replace('/',[IO.Path]::DirectorySeparatorChar))
        if($null -eq $entry -or -not [IO.File]::Exists($path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value){throw "Missing or changed offline component: $file. Extract a fresh private assembly."}
    }
    # Probe only after bundled runtime and worker integrity checks.
    Assert-PortableVulkanLoader $runtime
    $module=[PortableLibrary]::LoadLibraryEx($dll,[IntPtr]::Zero,0x00001100)
    if($module -eq [IntPtr]::Zero){throw "Cannot load verified libmpv (Windows error $([PortableLibrary]::LastError())). The Vulkan prerequisite check passed, but libmpv or another runtime dependency failed to load. Use a complete, checksum-verified package and report this error; do not download loose DLLs."}
    [PortableLibrary]::FreeLibrary($module) | Out-Null
    $workerRoot=$runtime
    if($StageWorkerLocally -or [PortableLibrary]::GetDriveType([IO.Path]::GetPathRoot($runtime)) -ne 3){
        # Existing controller permits helpers only on DRIVE_FIXED. Do not bypass it.
        $localTemp=[IO.Path]::GetTempPath()
        if([PortableLibrary]::GetDriveType([IO.Path]::GetPathRoot($localTemp)) -ne 3){throw 'Preview helper requires a writable local fixed-drive TEMP folder when running from removable storage.'}
        $localWorker=Join-Path $localTemp ('vlc-preview-portable-'+[guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($localWorker) | Out-Null
        foreach($file in @('libmpv-2.dll','mpv-thumbnail-worker.exe','mpv-thumbnail-accurate-worker.exe')){
            [IO.File]::Copy((Join-Path $runtime $file),(Join-Path $localWorker $file),$false)
            if((Get-FileHash -LiteralPath (Join-Path $localWorker $file) -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $runtime $file) -Algorithm SHA256).Hash){throw 'Temporary worker copy checksum mismatch.'}
        }
        $workerRoot=$localWorker
    }
    if($ValidateOnly -or $SetupOnly){return}
    if($media.Count -eq 0 -and (Show-PortableInstance $app $data)){return}
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path $app 'vlc.exe'
    $info.WorkingDirectory=$app
    $info.UseShellExecute=$false
    $info.EnvironmentVariables['VLC_THUMBNAIL_BACKEND']='libmpv'
    $info.EnvironmentVariables['VLC_MPV_SEEK']=$SeekMode
    $worker=if($SeekMode -eq 'exact'){'mpv-thumbnail-accurate-worker.exe'}else{'mpv-thumbnail-worker.exe'}
    $info.EnvironmentVariables['VLC_MPV_HELPER']=Join-Path $workerRoot $worker
    $info.EnvironmentVariables['VLC_TIMELINE_HOVER_DEBUG']=$(if($Diagnostics){'1'}else{'0'})
    $info.EnvironmentVariables['VLC_PLAYBACK_STABILITY_DEBUG']=$(if($PlaybackDiagnostics){'1'}else{'0'})
    # Child-only profile/cache isolation; never alter the user's normal VLC profile.
    $info.EnvironmentVariables['APPDATA']=Join-Path $data 'config'
    $info.EnvironmentVariables['LOCALAPPDATA']=Join-Path $data 'cache'
    $info.EnvironmentVariables['TEMP']=$data
    $info.EnvironmentVariables['TMP']=$data
    foreach($folder in @('config','cache')){[IO.Directory]::CreateDirectory((Join-Path $data $folder)) | Out-Null}
    $info.Arguments='--no-one-instance --no-qt-privacy-ask --no-qt-updates-notif --verbose=0'
    if($Diagnostics -or $PlaybackDiagnostics){$info.Arguments+=' --verbose=2 --file-logging --logfile="'+(Join-Path $data ('diagnostic-'+[guid]::NewGuid().ToString('N')+'.log'))+'"'}
    foreach($path in $media){$info.Arguments+=' '+(ConvertTo-WindowsArgument $path)}
    $child=[Diagnostics.Process]::Start($info)
    Save-PortableInstance $child $data
    # For removable storage only, retain the temporary worker while this VLC lives.
    if($gate){$gate.Dispose();$gate=$null}
    if($localWorker){$child.WaitForExit()}
} catch {
    $problem=$_.Exception.GetBaseException()
    try {
        [IO.File]::WriteAllText((Join-Path $data ('startup-error-'+[guid]::NewGuid().ToString('N')+'.log')),$problem.ToString())
    } catch {} # Read-only/missing storage still gets a dialog.
    if($ValidateOnly -or $SetupOnly){throw $problem}
    [Windows.Forms.MessageBox]::Show($problem.Message,'VLC Timeline Preview could not start','OK','Error') | Out-Null
    exit 1
} finally {
    if($localWorker -and [IO.Directory]::Exists($localWorker)){
        # The controller may still be reaping a worker at VLC shutdown; bounded retry.
        for($attempt=0;$attempt -lt 10;$attempt++){
            try {Remove-Item -LiteralPath $localWorker -Recurse -Force; break} catch {Start-Sleep -Milliseconds 200}
        }
    }
    if($gate){$gate.Dispose()}
}
