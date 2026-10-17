# Terraria DUO installer for Windows PowerShell 5.1+
# Uses official Valve SteamCMD, never asks for Steam password.
param(
  [ValidateSet('recommended','max','fables')][string]$Profile = 'recommended',
  [ValidateSet('auto','workshop')][string]$Method = 'auto',
  [switch]$NoDownload
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$profileNames = @{
  recommended = '01_DUO_RECOMMENDED_20'
  max         = '02_DUO_MAX_CONTENT_26'
  fables      = '03_DUO_FABLES_ONLY_20'
}
$documents = [Environment]::GetFolderPath('MyDocuments')
if ([string]::IsNullOrWhiteSpace($documents)) { throw 'Windows Documents folder is unavailable.' }
$tml = Join-Path $documents 'My Games\Terraria\tModLoader'
$modsDir = Join-Path $tml 'Mods'
$packsDir = Join-Path $tml 'ModPacks'
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Path $modsDir, $packsDir -Force | Out-Null
Write-Host 'Terraria Calamity x Fables - DUO Expanded' -ForegroundColor Cyan
Write-Host ('Profile: ' + $Profile)
Write-Host ('tModLoader data: ' + $tml)

# Install all three definitions safely; never touch Worlds or Players.
foreach ($key in @('recommended','max','fables')) {
  $name = $profileNames[$key]
  $source = Join-Path $root ('packs\' + $name)
  if (!(Test-Path (Join-Path $source 'Mods\enabled.json')) -or
      !(Test-Path (Join-Path $source 'Mods\install.txt'))) {
    throw ('Missing pack files: ' + $source + '. Extract the full ZIP first.')
  }
  $dest = Join-Path $packsDir $name
  if (Test-Path $dest) {
    $backup = $dest + '_backup_' + $timestamp
    Move-Item -LiteralPath $dest -Destination $backup
    Write-Host ('Saved old pack at: ' + $backup)
  }
  Copy-Item -LiteralPath $source -Destination $dest -Recurse
  Write-Host ('Modpack installed: ' + $name) -ForegroundColor Green
}
$selected = Join-Path $root ('packs\' + $profileNames[$Profile] + '\Mods')
$enabledPath = Join-Path $modsDir 'enabled.json'
if (Test-Path $enabledPath) {
  Copy-Item -LiteralPath $enabledPath -Destination (Join-Path $modsDir ('enabled_backup_' + $timestamp + '.json')) -Force
}
Copy-Item -LiteralPath (Join-Path $selected 'enabled.json') -Destination $enabledPath -Force
$manifest = Get-Content -LiteralPath (Join-Path $root 'manifest\mods.json') -Raw | ConvertFrom-Json
$wantedIDs = @(Get-Content (Join-Path $selected 'install.txt') | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^\d+$' })
Write-Host ("Chosen mod IDs: {0}" -f $wantedIDs.Count)
if ($NoDownload -or $Method -eq 'workshop') {
  Write-Host 'Next: open tModLoader -> Workshop -> Mod Packs -> select pack -> Download/Enable.' -ForegroundColor Yellow
  exit 0
}

# Download SteamCMD from Valve; all downloads stay on THIS computer.
$tools = Join-Path $env:LOCALAPPDATA 'TerrariaDuoPack\steamcmd'
$steamExe = Join-Path $tools 'steamcmd.exe'
if (!(Test-Path $steamExe)) {
  New-Item -ItemType Directory -Path $tools -Force | Out-Null
  $zipPath = Join-Path $tools 'steamcmd.zip'
  Write-Host 'Downloading official SteamCMD from Valve...'
  try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri 'https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip' -OutFile $zipPath -UseBasicParsing
    Expand-Archive -LiteralPath $zipPath -DestinationPath $tools -Force
    Remove-Item $zipPath -Force
  } catch {
    Write-Warning ('SteamCMD download failed: ' + $_.Exception.Message)
    Write-Host 'Profiles installed. Use tModLoader Workshop to download mods.' -ForegroundColor Yellow
    exit 2
  }
}
$arglist = [System.Collections.Generic.List[string]]::new()
$arglist.Add('+login')
$arglist.Add('anonymous')
foreach ($id in $wantedIDs) {
  $arglist.Add('+workshop_download_item')
  $arglist.Add('1281930')
  $arglist.Add($id)
}
$arglist.Add('+quit')
$outLog = Join-Path $tools ('download_' + $timestamp + '.out.log')
$errLog = Join-Path $tools ('download_' + $timestamp + '.err.log')
Write-Host 'Downloading mods via SteamCMD. Large packs may take a while...'
try {
  $p = Start-Process -FilePath $steamExe -WorkingDirectory $tools -ArgumentList $arglist.ToArray() -PassThru -Wait -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  Write-Host ('SteamCMD exit code: ' + $p.ExitCode)
} catch {
  Write-Warning ('SteamCMD could not start: ' + $_.Exception.Message)
}

# Copy only files that SteamCMD actually downloaded. Do not fabricate .tmod files.
$missing = @()
$copied = @()
foreach ($mod in @($manifest | Where-Object { $wantedIDs -contains $_.workshopId })) {
  $id = [string]$mod.workshopId
  $folder = Join-Path $tools ('steamapps\workshop\content\1281930\' + $id)
  $candidates = @()
  if (Test-Path $folder) {
    $candidates = @(Get-ChildItem -LiteralPath $folder -Recurse -File -Filter '*.tmod' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
  }
  if ($candidates.Count -eq 0) {
    $missing += ('{0} ({1})' -f $mod.name, $id)
    continue
  }
  $target = Join-Path $modsDir ($mod.internalName + '.tmod')
  Copy-Item -LiteralPath $candidates[0].FullName -Destination $target -Force
  if ((Get-Item $target).Length -lt 1024) {
    Remove-Item -LiteralPath $target -Force
    $missing += ('{0} ({1}) - invalid size' -f $mod.name, $id)
    continue
  }
  $copied += $mod.internalName
}
Write-Host ("Local .tmod files copied: {0}/{1}" -f $copied.Count, $wantedIDs.Count) -ForegroundColor Cyan
if ($missing.Count -gt 0) {
  Write-Warning ('SteamCMD did not deliver all mods (' + $missing.Count + '). Anonymous downloads can be restricted.')
  foreach ($line in $missing) { Write-Host ('  MISSING: ' + $line) -ForegroundColor Yellow }
  Write-Host 'Finish missing downloads in tModLoader -> Workshop -> Mod Packs -> Download All.' -ForegroundColor Yellow
  Write-Host ('Logs: ' + $outLog)
  exit 3
}
Write-Host 'All requested workshop items yielded local .tmod files.' -ForegroundColor Green
Write-Host 'Launch tModLoader, check dependency errors, Reload Mods, then start a NEW Large Expert world.'
