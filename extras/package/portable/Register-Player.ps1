# SPDX-License-Identifier: GPL-2.0-or-later
param([ValidateSet('Register','Remove')][string]$Action='Register', [string]$RegisteredFolder)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
. (Join-Path $PSScriptRoot 'Windows-Integration.ps1')
try {
    $root=if($RegisteredFolder){$RegisteredFolder}else{Join-Path $PSScriptRoot '..'}
    if($Action -eq 'Register' -and $RegisteredFolder){throw 'Registration must use this application folder.'}
    $id=Invoke-PortableRegistration $root $Action
    $message=if($Action -eq 'Register'){'Registered VLC media player for this user. Defaults were not changed. Choose VLC media player in Open with or Windows Settings > Apps > Default apps. This entry describes the unofficial portable edition.'}else{'Removed only this portable folder registration. Windows may ask you to select another default app.'}
    [Windows.Forms.MessageBox]::Show($message,'VLC media player','OK','Information') | Out-Null
}catch{
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'VLC media player registration','OK','Error') | Out-Null
    exit 1
}
