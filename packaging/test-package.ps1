param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
$expected = @('BUILD-INFO.txt','FIRST-TEST.txt','SHA256SUMS.txt','START-HERE.cmd','kvmswitch.exe') | Sort-Object
$actual = @(Get-ChildItem -LiteralPath $Directory -File | ForEach-Object Name | Sort-Object)
if (Compare-Object $expected $actual) { throw 'Unexpected package contents.' }
foreach ($line in Get-Content -LiteralPath (Join-Path $Directory 'SHA256SUMS.txt')) {
    if ($line -notmatch '^([0-9a-f]{64})  ([A-Za-z0-9_.-]+)$') { throw 'Malformed checksum manifest.' }
    $hash = $Matches[1]; $name = $Matches[2]
    if ((Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash.ToLowerInvariant() -ne $hash) {
        throw "Checksum mismatch: $name"
    }
}
# Exercise the shipped launcher in a path with spaces; --check-package cannot write.
$launcher = Join-Path (Resolve-Path $Directory) 'START-HERE.cmd'
& $env:ComSpec /d /c "`"$launcher`" --check-package"
if ($LASTEXITCODE -ne 0) { throw 'Packaged launcher/executable smoke test failed.' }
Write-Host 'Package contents, checksums and launcher smoke test passed.'
