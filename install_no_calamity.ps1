param(
 [ValidateSet('recommended','epic','thorium')][string]$Profile='recommended',
 [ValidateSet('auto','workshop')][string]$Method='auto'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$names=@{recommended='04_NO_CALAMITY_DUO_40';epic='05_NO_CALAMITY_DUO_EPIC_52';thorium='06_THORIUM_ONLY_DUO_39'}
$docs=[Environment]::GetFolderPath('MyDocuments')
if([string]::IsNullOrWhiteSpace($docs)){throw 'Windows Documents not found'}
$tml=Join-Path $docs 'My Games\Terraria\tModLoader'
$mods=Join-Path $tml 'Mods'
$packs=Join-Path $tml 'ModPacks'
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $mods,$packs|Out-Null
foreach($key in @('recommended','epic','thorium')){
 $name=$names[$key]
 $src=Join-Path $root ('packs\'+$name)
 if(!(Test-Path (Join-Path $src 'Mods\enabled.json'))){throw "Missing pack: $src. Extract ZIP fully."}
 $dst=Join-Path $packs $name
 if(Test-Path $dst){Move-Item -LiteralPath $dst -Destination ($dst+'_backup_'+$stamp)}
 Copy-Item -LiteralPath $src -Destination $dst -Recurse
 Write-Host "Imported $name"
}
$selected=Join-Path $root ('packs\'+$names[$Profile]+'\Mods')
$enabled=Join-Path $mods 'enabled.json'
if(Test-Path $enabled){Copy-Item -LiteralPath $enabled -Destination (Join-Path $mods ('enabled_backup_'+$stamp+'.json'))}
Copy-Item -LiteralPath (Join-Path $selected 'enabled.json') -Destination $enabled -Force
$ids=@(Get-Content (Join-Path $selected 'install.txt')|ForEach-Object{$_.Trim()}|Where-Object{$_ -match '^\d+$'})
Write-Host ("Selected $Profile; mods: "+$ids.Count) -ForegroundColor Cyan
if($Method -eq 'workshop'){
 Write-Host 'tModLoader -> Workshop -> Mod Packs -> Download All. The other player installs the same pack.' -ForegroundColor Yellow
 exit 0
}
$work=Join-Path $env:LOCALAPPDATA 'TerrariaDuoModpack\steamcmd'
$exe=Join-Path $work 'steamcmd.exe'
New-Item -ItemType Directory -Path $work -Force|Out-Null
if(!(Test-Path $exe)){
 try{
  [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
  $zip=Join-Path $work 'steamcmd.zip'
  Invoke-WebRequest -Uri 'https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip' -OutFile $zip -UseBasicParsing
  Expand-Archive -LiteralPath $zip -DestinationPath $work -Force
  Remove-Item $zip -Force
 }catch{Write-Warning "SteamCMD unavailable. Use Workshop in tModLoader.";exit 2}
}
$argsList=[System.Collections.Generic.List[string]]::new()
$argsList.Add('+login');$argsList.Add('anonymous')
foreach($id in $ids){$argsList.Add('+workshop_download_item');$argsList.Add('1281930');$argsList.Add($id)}
$argsList.Add('+quit')
$out=Join-Path $work ('download_'+$stamp+'.out.log')
$err=Join-Path $work ('download_'+$stamp+'.err.log')
try{$proc=Start-Process -FilePath $exe -WorkingDirectory $work -ArgumentList $argsList.ToArray() -PassThru -Wait -RedirectStandardOutput $out -RedirectStandardError $err;Write-Host ('SteamCMD: '+$proc.ExitCode)}
catch{Write-Warning 'SteamCMD failed. Use in-game Workshop.';exit 3}
$missing=@();$good=0
foreach($id in $ids){
 $src=Join-Path $work ('steamapps\workshop\content\1281930\'+$id)
 $found=@()
 if(Test-Path $src){$found=@(Get-ChildItem -LiteralPath $src -Recurse -Filter '*.tmod' -File|Sort-Object LastWriteTime -Descending)}
 if($found.Count -eq 0 -or $found[0].Length -lt 1024){$missing+= $id;continue}
 Copy-Item -LiteralPath $found[0].FullName -Destination (Join-Path $mods $found[0].Name) -Force
 $good++
}
Write-Host ("Downloaded .tmod: $good / "+$ids.Count)
if($missing.Count -gt 0){
 Write-Warning ('Unavailable via anonymous SteamCMD: '+($missing -join ', '))
 Write-Host 'Finish downloads via tModLoader -> Workshop -> Mod Packs -> Download All.' -ForegroundColor Yellow
 exit 4
}
Write-Host 'Now launch tModLoader and reload mods. Use a NEW Large Expert world.' -ForegroundColor Green
