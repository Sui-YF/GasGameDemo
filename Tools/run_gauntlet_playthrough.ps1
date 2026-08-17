param(
    [string]$EngineRoot = "D:\UE_5.4",
    [string]$Configuration = "Development"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "CultivationVsAliensDemo.uproject"
$ReportDirectory = Join-Path $ProjectRoot "Saved\Gauntlet\FullPlaythrough"
$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

New-Item -ItemType Directory -Force $ReportDirectory | Out-Null

$UatArguments = @(
    "RunUnreal",
    "-project=$ProjectFile",
    "-platform=Win64",
    "-configuration=$Configuration",
    "-build=Editor",
    "-test=UE.EditorAutomation",
    "-RunTest=CVAD.Gauntlet.FullPlaythrough",
    "-ReportExportPath=$ReportDirectory",
    "-unattended",
    "-NoP4"
)

& $RunUAT @UatArguments

if ($LASTEXITCODE -ne 0) {
    throw "CVAD Gauntlet playthrough failed with exit code $LASTEXITCODE. See $ReportDirectory"
}

Write-Host "CVAD Gauntlet playthrough passed. Report: $ReportDirectory"
