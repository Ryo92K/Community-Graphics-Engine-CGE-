param()
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$root=Join-Path $project 'PUBLICATION'
$github=Join-Path $root '01 - GitHub'
$nexus=Join-Path $root '02 - Nexus Mods'
if(Test-Path -LiteralPath $github){throw 'GitHub export already exists; refusing to merge with an old export.'}
if(Test-Path -LiteralPath $nexus){throw 'Nexus export already exists; refusing to merge with an old export.'}
New-Item -ItemType Directory -Path $github,$nexus -Force | Out-Null
$files=@('.gitignore','CMakeLists.txt','CGE.ini','LICENSE','README.md','CONTRIBUTING.md','CHANGELOG.md','THIRD-PARTY-NOTICES.txt')
$folders=@('.github','src','shaders','tests','tools','third_party','licenses','docs')
foreach($name in $files){Copy-Item -LiteralPath (Join-Path $project $name) -Destination (Join-Path $github $name)}
foreach($name in $folders){Copy-Item -LiteralPath (Join-Path $project $name) -Destination $github -Recurse}
$dist=Join-Path $project 'dist-skse'
foreach($name in @('SKSE','licenses')){Copy-Item -LiteralPath (Join-Path $dist $name) -Destination $nexus -Recurse}
foreach($name in @('README.md','LICENSE','THIRD-PARTY-NOTICES.txt','SHA256SUMS.txt')) {
    $source=if($name -eq 'SHA256SUMS.txt'){Join-Path $dist $name}else{Join-Path $project $name}
    Copy-Item -LiteralPath $source -Destination (Join-Path $nexus $name)
}
Copy-Item -LiteralPath (Join-Path $project 'docs\INSTALLATION.txt') -Destination (Join-Path $nexus 'INSTALLATION.txt')
Copy-Item -LiteralPath (Join-Path $project 'docs\PUBLICATION-DOSSIERS.txt') -Destination (Join-Path $root 'LIRE AVANT PUBLICATION.txt')
Copy-Item -LiteralPath (Join-Path $project 'CGE-0.1alpha-SKSE-MO2-Vortex.zip') -Destination $root
Write-Output $root
