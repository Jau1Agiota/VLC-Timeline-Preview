# SPDX-License-Identifier: GPL-2.0-or-later
# Shared launcher/registration helpers. No network or default-association changes.
function Initialize-ExplorerInterop {
    if('PortableExplorer' -as [type]){return}
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PortableExplorer {
 [DllImport("shell32.dll", CharSet=CharSet.Unicode)] static extern IntPtr CommandLineToArgvW(string line, out int count);
 [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int mode);
 [DllImport("shell32.dll")] public static extern void SHChangeNotify(uint e, uint flags, IntPtr a, IntPtr b);
 public static string[] Parse(string line) {
   int count; IntPtr p=CommandLineToArgvW("launcher.exe "+line,out count);
   if(p==IntPtr.Zero) throw new InvalidOperationException("Cannot parse Explorer arguments.");
   try { string[] result=new string[count-1];
     for(int i=1;i<count;i++) result[i-1]=Marshal.PtrToStringUni(Marshal.ReadIntPtr(p,i*IntPtr.Size));
     return result;
   } finally { LocalFree(p); }
 }
}
'@
}
function ConvertFrom-ExplorerArguments([string]$Encoded) {
    if(-not $Encoded){return}
    Initialize-ExplorerInterop
    $bytes=[Convert]::FromBase64String($Encoded)
    if($bytes.Length -gt 16000 -or $bytes.Length%2){throw 'Explorer arguments are too long or invalid.'}
    $line=[Text.Encoding]::Unicode.GetString($bytes)
    if($line.Contains([string][char]0)){throw 'Invalid Explorer arguments.'}
    foreach($path in [PortableExplorer]::Parse($line)){
        # Explorer supplies local absolute paths, not VLC options or script code.
        if($path -notmatch '^[a-zA-Z]:[\\/]' -or $path.StartsWith('\\') -or -not [IO.File]::Exists($path)){throw "Media file is missing or not a local absolute path: $path"}
        [IO.Path]::GetFullPath($path)
    }
}
function ConvertTo-WindowsArgument([string]$Value) {
    # CommandLineToArgvW/CRT escaping, including backslashes preceding quotes/end.
    return '"'+([regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1'))+'"'
}
function Show-PortableInstance([string]$Application,[string]$Data) {
    # Never use VLC's global version-named IPC, which can target official VLC.
    try {
        $record=[IO.File]::ReadAllText((Join-Path $Data 'last-instance.json')) | ConvertFrom-Json
        $process=[Diagnostics.Process]::GetProcessById([int]$record.pid)
        if($process.StartTime.ToUniversalTime().Ticks.ToString() -ne $record.started -or -not $process.MainModule.FileName.Equals((Join-Path $Application 'vlc.exe'),[StringComparison]::OrdinalIgnoreCase)){return $false}
        $deadline=[DateTime]::UtcNow.AddSeconds(5)
        while($process.MainWindowHandle -eq [IntPtr]::Zero -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100;$process.Refresh()}
        if($process.MainWindowHandle -eq [IntPtr]::Zero){return (-not $process.HasExited)}
        Initialize-ExplorerInterop
        if([PortableExplorer]::IsIconic($process.MainWindowHandle)){[PortableExplorer]::ShowWindowAsync($process.MainWindowHandle,9) | Out-Null}
        [PortableExplorer]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
        return $true
    }catch{return $false}
}
function Save-PortableInstance([Diagnostics.Process]$Process,[string]$Data) {
    try {
        $record=@{pid=$Process.Id;started=$Process.StartTime.ToUniversalTime().Ticks.ToString()} | ConvertTo-Json
        [IO.File]::WriteAllText((Join-Path $Data 'last-instance.json'),$record)
    }catch{} # A rapidly closed VLC is not a startup error.
}
function Get-PortableRegistration([string]$Application) {
    $root=[IO.Path]::GetFullPath($Application).TrimEnd('\')
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$hash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToUpperInvariant())))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
    $id='VLCTimelinePreview.'+$hash.Substring(0,24)
    return @{Root=$root;Id=$id;ProgId=$id+'.Video';Capabilities='Software\VLCTimelinePreview\Apps\'+$id;Extensions=@('.mp4','.mkv','.avi','.mov','.webm','.wmv','.flv','.m4v','.mpg','.mpeg','.ts')}
}
function Invoke-PortableRegistration([string]$Application,[ValidateSet('Register','Remove')][string]$Action) {
    $r=Get-PortableRegistration $Application
    $user=[Microsoft.Win32.Registry]::CurrentUser
    $paths=@(('Software\Classes\'+$r.ProgId),$r.Capabilities)
    # Ownership is checked BEFORE any write. Never adopt another application's key.
    foreach($path in $paths){
        $key=$user.OpenSubKey($path)
        if($null -ne $key){try{if($key.GetValue('PortableOwner') -ne $r.Root){throw "Registration ownership conflict: $path"}}finally{$key.Dispose()}}
    }
    $registered=$user.OpenSubKey('Software\RegisteredApplications')
    if($registered){try{$existing=$registered.GetValue($r.Id);if($null -ne $existing -and $existing -ne ($r.Capabilities+'\Capabilities')){throw 'RegisteredApplications ownership conflict'}}finally{$registered.Dispose()}}
    if($Action -eq 'Register'){
        foreach($name in @('vlc.exe','Start VLC Timeline Preview.exe','OFFLINE_PROTOTYPE_MANIFEST.json')){if(-not [IO.File]::Exists((Join-Path $r.Root $name))){throw "Cannot register an incomplete or moved portable folder: $name"}}
        $key=$user.CreateSubKey($paths[0])
        try{
            $key.SetValue('PortableOwner',$r.Root);$key.SetValue('','VLC media player')
            $applicationKey=$key.CreateSubKey('Application')
            try{$applicationKey.SetValue('ApplicationName','VLC media player');$applicationKey.SetValue('ApplicationDescription','Unofficial portable VLC with timeline-hover thumbnails')}finally{$applicationKey.Dispose()}
            $icon=$key.CreateSubKey('DefaultIcon');try{$icon.SetValue('',('"'+(Join-Path $r.Root 'vlc.exe')+'",0'))}finally{$icon.Dispose()}
            $cmd=$key.CreateSubKey('shell\open\command');try{$cmd.SetValue('',('"'+(Join-Path $r.Root 'Start VLC Timeline Preview.exe')+'" "%1"'))}finally{$cmd.Dispose()}
        }finally{$key.Dispose()}
        $key=$user.CreateSubKey($r.Capabilities)
        try{
            $key.SetValue('PortableOwner',$r.Root)
            $cap=$key.CreateSubKey('Capabilities')
            try{
                $cap.SetValue('ApplicationName','VLC media player')
                $cap.SetValue('ApplicationDescription','Unofficial portable VLC with timeline-hover thumbnails')
                $cap.SetValue('ApplicationIcon',('"'+(Join-Path $r.Root 'vlc.exe')+'",0'))
                $assoc=$cap.CreateSubKey('FileAssociations');try{foreach($ext in $r.Extensions){$assoc.SetValue($ext,$r.ProgId)}}finally{$assoc.Dispose()}
            }finally{$cap.Dispose()}
        }finally{$key.Dispose()}
        foreach($ext in $r.Extensions){
            $key=$user.CreateSubKey('Software\Classes\'+$ext+'\OpenWithProgids')
            try{$key.SetValue($r.ProgId,[byte[]]@(),[Microsoft.Win32.RegistryValueKind]::None)}finally{$key.Dispose()}
        }
        $key=$user.CreateSubKey('Software\RegisteredApplications')
        try{$key.SetValue($r.Id,($r.Capabilities+'\Capabilities'))}finally{$key.Dispose()}
    }else{
        foreach($ext in $r.Extensions){
            $key=$user.OpenSubKey(('Software\Classes\'+$ext+'\OpenWithProgids'),$true)
            if($key){try{$key.DeleteValue($r.ProgId,$false)}finally{$key.Dispose()}}
        }
        $key=$user.OpenSubKey('Software\RegisteredApplications',$true)
        if($key){try{$key.DeleteValue($r.Id,$false)}finally{$key.Dispose()}}
        foreach($path in $paths){$user.DeleteSubKeyTree($path,$false)}
    }
    Initialize-ExplorerInterop
    [PortableExplorer]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
    return $r.Id
}
