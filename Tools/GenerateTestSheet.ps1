# Game/Assets/TestSheet.png を生成する（M5 検証用の 4 コマテストシート）。
#
# 128x32 RGBA。32x32 のコマが横に 4 つ（赤・緑・青・黄）。各コマは 2px の黒枠を持ち、
# 8x8 の白マーカーがコマごとに異なる隅（TL / TR / BR / BL）に置かれる。
# マーカー位置でコマの識別・flip・回転が目視検証できる。
#
# 依存: PowerShell 7+（.NET の ZLibStream を使用）。外部ツール不要。
# 使い方: pwsh Tools/GenerateTestSheet.ps1

$ErrorActionPreference = 'Stop'

$frameSize = 32
$border    = 2
$mark      = 8
$black     = @(0, 0, 0, 255)
$white     = @(255, 255, 255, 255)

# 塗り色と白マーカーを置く隅
$frames = @(
    @{ Color = @(220,  60,  60, 255); Corner = 'TL' },  # 0: 赤
    @{ Color = @( 60, 180,  60, 255); Corner = 'TR' },  # 1: 緑
    @{ Color = @( 60,  90, 220, 255); Corner = 'BR' },  # 2: 青
    @{ Color = @(230, 200,  50, 255); Corner = 'BL' }   # 3: 黄
)
$width  = $frameSize * $frames.Count
$height = $frameSize

# ── 生スキャンライン生成（各行の先頭にフィルタタイプ 0 を付ける）───────────────
$stride = 1 + $width * 4
$raw = New-Object byte[] ($height * $stride)
for ($y = 0; $y -lt $height; $y++) {
    $rowBase = $y * $stride
    $raw[$rowBase] = 0  # filter: None
    for ($x = 0; $x -lt $width; $x++) {
        $f  = [int][math]::Floor($x / $frameSize)
        $fx = $x % $frameSize
        $fy = $y
        $px = $frames[$f].Color
        if ($fx -lt $border -or $fy -lt $border -or
            $fx -ge $frameSize - $border -or $fy -ge $frameSize - $border) {
            $px = $black
        } else {
            $inLeft   = $fx -lt $border + $mark
            $inRight  = $fx -ge $frameSize - $border - $mark
            $inTop    = $fy -lt $border + $mark
            $inBottom = $fy -ge $frameSize - $border - $mark
            $corner = $frames[$f].Corner
            if (($corner -eq 'TL' -and $inLeft  -and $inTop) -or
                ($corner -eq 'TR' -and $inRight -and $inTop) -or
                ($corner -eq 'BR' -and $inRight -and $inBottom) -or
                ($corner -eq 'BL' -and $inLeft  -and $inBottom)) {
                $px = $white
            }
        }
        $i = $rowBase + 1 + $x * 4
        $raw[$i]     = $px[0]
        $raw[$i + 1] = $px[1]
        $raw[$i + 2] = $px[2]
        $raw[$i + 3] = $px[3]
    }
}

# ── zlib 圧縮（PNG の IDAT は zlib フレーミング必須。ZLibStream が担う）────────
$ms = New-Object System.IO.MemoryStream
$zs = New-Object System.IO.Compression.ZLibStream(
    $ms, [System.IO.Compression.CompressionLevel]::SmallestSize, $true)
$zs.Write($raw, 0, $raw.Length)
$zs.Dispose()
$idatData = $ms.ToArray()

# ── CRC32（PNG チャンク用。.NET 標準に無いためテーブル法で実装）────────────────
$crcTable = New-Object uint32[] 256
for ($n = 0; $n -lt 256; $n++) {
    $c = [uint32]$n
    for ($k = 0; $k -lt 8; $k++) {
        # 0xEDB88320。16 進リテラルは int32 負数に解釈されるため 10 進で書く。
        if ($c -band 1) { $c = [uint32]3988292384 -bxor ($c -shr 1) }
        else            { $c = $c -shr 1 }
    }
    $crcTable[$n] = $c
}
function Get-Crc32([byte[]]$data) {
    $c = [uint32]4294967295
    foreach ($b in $data) {
        $c = $crcTable[($c -bxor $b) -band 0xFF] -bxor ($c -shr 8)
    }
    return $c -bxor [uint32]4294967295
}

function ConvertTo-BigEndian([uint32]$value) {
    $bytes = [BitConverter]::GetBytes($value)
    [Array]::Reverse($bytes)
    return $bytes
}

function New-Chunk([string]$tag, [byte[]]$data) {
    $tagBytes = [System.Text.Encoding]::ASCII.GetBytes($tag)
    $crc = Get-Crc32 ($tagBytes + $data)
    return (ConvertTo-BigEndian ([uint32]$data.Length)) + $tagBytes + $data +
           (ConvertTo-BigEndian $crc)
}

# ── PNG 組み立て ─────────────────────────────────────────────────────────────
$ihdr = (ConvertTo-BigEndian ([uint32]$width)) + (ConvertTo-BigEndian ([uint32]$height)) +
        [byte[]]@(8, 6, 0, 0, 0)  # bit depth 8, color type 6 (RGBA)
$png = [byte[]]@(0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A) +
       (New-Chunk 'IHDR' $ihdr) +
       (New-Chunk 'IDAT' $idatData) +
       (New-Chunk 'IEND' ([byte[]]@()))

$out = Join-Path $PSScriptRoot '..\Game\Assets\TestSheet.png'
$out = [System.IO.Path]::GetFullPath($out)
[System.IO.File]::WriteAllBytes($out, $png)
Write-Output "Wrote $out (${width}x${height})"
