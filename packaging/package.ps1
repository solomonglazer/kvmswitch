param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Commit,
    [Parameter(Mandatory=$true)][string]$RunUrl,
    [string]$OutputDirectory = 'build/package'
)
$ErrorActionPreference = 'Stop'
if ($Commit -notmatch '^[0-9a-f]{40}$') { throw 'Expected a full source commit SHA.' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh package output directory.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $OutputDirectory 'kvmswitch.exe')
foreach ($name in @('START-HERE.cmd', 'FIRST-TEST.txt')) {
    $text = Get-Content -LiteralPath (Join-Path $PSScriptRoot $name) -Raw
    # CMD must have Windows newlines regardless of checkout configuration.
    $text = $text -replace '\r?\n', "`r`n"
    [IO.File]::WriteAllText((Join-Path (Resolve-Path $OutputDirectory) $name), $text, [Text.Encoding]::ASCII)
}
@("kvmswitch 0.1.0 - experimental Windows x64 test", "Source commit: $Commit",
  "Build run: $RunUrl", 'Hardware switch: not validated by CI') |
    Set-Content -LiteralPath (Join-Path $OutputDirectory 'BUILD-INFO.txt') -Encoding ascii
Get-ChildItem -LiteralPath $OutputDirectory -File | Sort-Object Name | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($_.Name)"
} | Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Portable package prepared in $OutputDirectory"
