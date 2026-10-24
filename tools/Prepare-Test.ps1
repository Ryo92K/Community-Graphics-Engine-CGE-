param()
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$dist=Join-Path $project 'dist-skse'
$plugin=Join-Path $dist 'SKSE\Plugins\CGE.dll'
if(-not (Test-Path -LiteralPath $plugin)){throw 'CGE SKSE build missing'}
$docs=@('README.md','LICENSE','THIRD-PARTY-NOTICES.txt')
foreach($name in $docs) {
    Copy-Item -LiteralPath (Join-Path $project $name) -Destination (Join-Path $dist $name) -Force
}
Copy-Item -LiteralPath (Join-Path $project 'docs\INSTALLATION.txt') -Destination (Join-Path $dist 'INSTALLATION.txt') -Force
$hash=Get-FileHash -LiteralPath $plugin -Algorithm SHA256
($hash.Hash.ToLowerInvariant()+'  SKSE/Plugins/CGE.dll') | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii
$archive=Join-Path $project 'CGE-0.1alpha-SKSE-MO2-Vortex.zip'
$items=@((Join-Path $dist 'SKSE'),(Join-Path $dist 'licenses'))
foreach($name in @('README.md','LICENSE','THIRD-PARTY-NOTICES.txt','INSTALLATION.txt','SHA256SUMS.txt')){$items+=Join-Path $dist $name}
Compress-Archive -LiteralPath $items -DestinationPath $archive -Force
Write-Output "Archive installable avec MO2/Vortex : $archive"
