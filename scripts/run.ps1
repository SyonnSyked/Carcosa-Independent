$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & "$projectRoot\build\dev\Carcosa.exe"
}
finally {
    Pop-Location
}
