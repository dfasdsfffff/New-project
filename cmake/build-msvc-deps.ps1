# 使用现有源码构建 MSVC x64 依赖，所有产物均留在项目内。
param(
    [Parameter(Mandatory = $true)][string]$ZlibSource,
    [Parameter(Mandatory = $true)][string]$LibXml2Source,
    [string]$Generator = 'Visual Studio 18 2026'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$dependencyRoot = Join-Path $projectRoot 'build-msvc-deps'
$installRoot = Join-Path $dependencyRoot 'install'

function Invoke-CMake {
    & cmake @args
    if ($LASTEXITCODE -ne 0) { throw "CMake 执行失败，退出码：$LASTEXITCODE" }
}

foreach ($source in @($ZlibSource, $LibXml2Source)) {
    if (!(Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt'))) {
        throw "找不到依赖源码中的 CMakeLists.txt：$source"
    }
}

# zlib 配置会重命名源码中的 zconf.h，先复制，避免修改用户的源文件。
New-Item -ItemType Directory -Force -Path $dependencyRoot | Out-Null
$zlibCopy = Join-Path $dependencyRoot 'zlib-source'
if (!(Test-Path -LiteralPath $zlibCopy)) {
    Copy-Item -LiteralPath $ZlibSource -Destination $zlibCopy -Recurse
}
Invoke-CMake -S $zlibCopy -B "$dependencyRoot/zlib-build" -G $Generator -A x64 `
    "-DCMAKE_INSTALL_PREFIX=$installRoot" -DZLIB_BUILD_EXAMPLES=OFF
Invoke-CMake --build "$dependencyRoot/zlib-build" --config Release --target INSTALL --parallel 4

# DOCX 的 ZIP 由项目自身处理，libxml2 无需额外压缩库或 Python 绑定。
Invoke-CMake -S $LibXml2Source -B "$dependencyRoot/libxml2-build" -G $Generator -A x64 `
    "-DCMAKE_INSTALL_PREFIX=$installRoot" -DBUILD_SHARED_LIBS=ON `
    -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_ZLIB=OFF `
    -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_TESTS=OFF
Invoke-CMake --build "$dependencyRoot/libxml2-build" --config Release --target INSTALL --parallel 4
Write-Host "MSVC x64 依赖已安装到：$installRoot"
