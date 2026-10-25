# SPDX-License-Identifier: GPL-2.0-or-later
param([Parameter(Mandatory=$true)][string]$LauncherExecutable)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repo 'extras/package/portable/Windows-Integration.ps1')
function Assert([bool]$Result,[string]$Message){if(-not $Result){throw $Message}}
function Reject([scriptblock]$Action,[string]$Pattern){
    $errorMessage='';try{& $Action | Out-Null}catch{$errorMessage=$_.Exception.Message}
    Assert ($errorMessage -like ('*'+$Pattern+'*')) "Expected '$Pattern', observed '$errorMessage'"
}
$root=Join-Path ([IO.Path]::GetTempPath()) ('VLC integration '+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$app=Join-Path $root ('portable '+[char]0x00e7+[char]0x4e2d)
[IO.Directory]::CreateDirectory((Join-Path $app 'portable')) | Out-Null
$r=Get-PortableRegistration $app
$user=[Microsoft.Win32.Registry]::CurrentUser
$sentinelId='VLC.official-isolation-fixture-'+[guid]::NewGuid().ToString('N')
$sentinel='Software\Classes\'+$sentinelId
$key=$user.CreateSubKey($sentinel);$key.SetValue('','untouched');$key.Dispose()
try {
    foreach($name in @('vlc.exe','OFFLINE_PROTOTYPE_MANIFEST.json')){[IO.File]::WriteAllText((Join-Path $app $name),'fixture')}
    $exe=Join-Path $app 'Start VLC Timeline Preview.exe'
    [IO.File]::Copy((Resolve-Path $LauncherExecutable).Path,$exe)
    Initialize-ExplorerInterop
    # Parse+CRT quote roundtrips are independent of PowerShell string interpolation.
    foreach($value in @('C:\a b\video.mp4','C:\a\end\','a"b','x\\"y')){
        $parsed=[PortableExplorer]::Parse((ConvertTo-WindowsArgument $value))
        Assert ($parsed.Count -eq 1 -and $parsed[0] -ceq $value) 'Argument quote roundtrip'
    }
    $video=Join-Path $root ('movie & $() [1] '+[char]0x00e9+[char]0x4e2d+'.mkv')
    [IO.File]::WriteAllText($video,'fixture')
    $second=Join-Path $root 'another video.mp4';[IO.File]::WriteAllText($second,'fixture')
    $line=(ConvertTo-WindowsArgument $video)+' '+(ConvertTo-WindowsArgument $second)
    $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($line))
    $decoded=@(ConvertFrom-ExplorerArguments $encoded)
    Assert ($decoded.Count -eq 2 -and $decoded[0] -ceq $video -and $decoded[1] -ceq $second) 'Unicode/path forwarding'
    Reject {ConvertFrom-ExplorerArguments ([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes('--extraintf=evil')))} 'not a local absolute path'
    Reject {ConvertFrom-ExplorerArguments ([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes('"C:\missing video.mp4"')))} 'missing'
    # Native EXE -> real inbox PS5 -File -> lossless payload, no shell evaluation.
    $capture=Join-Path $app 'portable/Start-Portable.ps1'
    [IO.File]::WriteAllText($capture,'param([string]$ExplorerArgumentsBase64) [IO.File]::WriteAllText((Join-Path $PSScriptRoot "captured.txt"),$ExplorerArgumentsBase64)')
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$exe;$info.Arguments=$line;$info.UseShellExecute=$false
    $child=[Diagnostics.Process]::Start($info)
    Assert ($child.WaitForExit(5000) -and $child.ExitCode -eq 0) 'Native bootstrap status'
    $output=Join-Path $app 'portable/captured.txt'
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    while(-not [IO.File]::Exists($output) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
    Assert ([IO.File]::Exists($output)) 'Bootstrap did not invoke PS5'
    $decoded=@(ConvertFrom-ExplorerArguments ([IO.File]::ReadAllText($output)))
    Assert ($decoded.Count -eq 2 -and $decoded[0] -ceq $video -and $decoded[1] -ceq $second) 'Native Unicode payload changed'
    # No-file behavior still starts normally and does not inherit previous payload.
    [IO.File]::Delete($output);$info.Arguments='';$child=[Diagnostics.Process]::Start($info);$child.WaitForExit(5000) | Out-Null
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    while(-not [IO.File]::Exists($output) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
    Assert ([IO.File]::Exists($output) -and [IO.File]::ReadAllText($output) -eq '') 'No-argument bootstrap'
    $fakeVlc=Join-Path $app 'vlc.exe'
    [IO.File]::Delete($fakeVlc)
    Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing -OutputAssembly $fakeVlc -OutputType WindowsApplication -TypeDefinition @'
using System;
using System.Windows.Forms;
public static class PortableInstanceFixture {
 [STAThread] public static void Main() {
   Form f=new Form(); f.Text="Portable instance fixture (not VLC)";
   Timer t=new Timer(); t.Interval=20000; t.Tick+=(s,e)=>f.Close();
   t.Start(); Application.Run(f);
 }
}
'@
    $fixtureProcess=[Diagnostics.Process]::Start($fakeVlc)
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(10)
        while($fixtureProcess.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100;$fixtureProcess.Refresh()}
        Assert ($fixtureProcess.MainWindowHandle -ne [IntPtr]::Zero) 'Fixture window did not start'
        $data=Join-Path $app 'portable-data';[IO.Directory]::CreateDirectory($data) | Out-Null
        Save-PortableInstance $fixtureProcess $data
        Assert (Show-PortableInstance $app $data) 'No-file reuse failed'
        Assert (-not (Show-PortableInstance ($app+' wrong folder') $data)) 'Cross-folder process focused'
        $state=Join-Path $data 'last-instance.json'
        $record=[IO.File]::ReadAllText($state) | ConvertFrom-Json
        $record.started='0';[IO.File]::WriteAllText($state,($record | ConvertTo-Json))
        Assert (-not (Show-PortableInstance $app $data)) 'Stale PID identity accepted'
    }finally{if(-not $fixtureProcess.HasExited){$fixtureProcess.Kill();$fixtureProcess.WaitForExit()};$fixtureProcess.Dispose()}
    # HKCU only. No UserChoice, extension default or official VLC key is written.
    $defaults=@{}
    foreach($ext in $r.Extensions){
        foreach($path in @(('Software\Classes\'+$ext),('Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\'+$ext+'\UserChoice'))){
            $key=$user.OpenSubKey($path);try{$defaults[$path]=if($key){@($key.GetValueNames() | Sort-Object | ForEach-Object {$_+'='+$key.GetValue($_)}) -join "`n"}else{''}}finally{if($key){$key.Dispose()}}
        }
    }
    $id=Invoke-PortableRegistration $app Register
    Assert ($id -eq $r.Id) 'Registration ID'
    Invoke-PortableRegistration $app Register | Out-Null
    $key=$user.OpenSubKey('Software\Classes\'+$r.ProgId+'\shell\open\command')
    try{$command=$key.GetValue('')}finally{$key.Dispose()}
    Assert ($command -ceq ('"'+$exe+'" "%1"')) 'Association bypassed launcher'
    $selected=$command.Replace('%1',$video)
    $parsed=[PortableExplorer]::Parse($selected)
    Assert ($parsed[0] -ceq $exe -and $parsed[1] -ceq $video) 'Registered command quoting'
    foreach($path in $defaults.Keys){$key=$user.OpenSubKey($path);try{$actual=if($key){@($key.GetValueNames() | Sort-Object | ForEach-Object {$_+'='+$key.GetValue($_)}) -join "`n"}else{''};Assert ($actual -eq $defaults[$path]) ("Existing default changed: "+$path)}finally{if($key){$key.Dispose()}}}
    $key=$user.OpenSubKey($sentinel);try{Assert ($key.GetValue('') -eq 'untouched') 'Official sentinel changed'}finally{$key.Dispose()}
    # Reject a conflicting owner before any update or removal.
    $key=$user.OpenSubKey($r.Capabilities,$true);$key.SetValue('PortableOwner','another installation');$key.Dispose()
    Reject {Invoke-PortableRegistration $app Register} 'ownership conflict'
    Reject {Invoke-PortableRegistration $app Remove} 'ownership conflict'
    $key=$user.OpenSubKey($r.Capabilities,$true);$key.SetValue('PortableOwner',$r.Root);$key.Dispose()
    # Removal can clean a missing/moved folder; does not require running its EXE.
    [IO.File]::Delete((Join-Path $app 'vlc.exe'))
    Reject {Invoke-PortableRegistration $app Register} 'incomplete or moved'
    Invoke-PortableRegistration $app Remove | Out-Null
    Invoke-PortableRegistration $app Remove | Out-Null
    Assert ($null -eq $user.OpenSubKey('Software\Classes\'+$r.ProgId)) 'ProgID remained'
    $other=Get-PortableRegistration ($app+' moved');Assert ($other.Id -ne $r.Id) 'Moved path reused identity'
    Write-Output 'PASS: native launcher/PS5 Unicode+spaces forwarding, quoting, no-args, HKCU registration/removal, repeat, conflict rejection, defaults and official-key isolation, moved-folder cleanup. Not physical Windows acceptance.'
}finally{
    Invoke-PortableRegistration $app Remove | Out-Null
    $user.DeleteSubKeyTree($sentinel,$false)
    Remove-Item -LiteralPath $root -Recurse -Force
}
