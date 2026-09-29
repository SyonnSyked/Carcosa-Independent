$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & "$projectRoot\build\dev\YELLOWTEAMGAME.exe"
}
finally {
    Pop-Location
}
