param(
    [Parameter(Mandatory=$true)]
    [string]$EditorDir
)

# Forbidden include patterns (editor must not touch D3D12 or engine-internal libraries)
# Permitted editor-side dependencies (NOT forbidden):
#   Win32 headers (Windows.h, commdlg.h, etc.) — editor is a native Win32 app
#   ImGuizmo.h  — editor-only UI gizmo lib, no engine abstraction needed
#   imgui.h / imgui_internal.h / imgui_impl_win32.h — editor UI framework
$ForbiddenPatterns = @(
    # D3D12 (editor must not touch rendering internals)
    'd3d12\.h'
    'dxgi\S*\.h'
    'd3dcompiler\.h'
    'wrl/.*'
    'D3D12MemAlloc\.h'
    # ImGui DX12 backend (exception: ImGuiLayer.cpp)
    'imgui_impl_dx12\.h'
    # External libraries (use engine API wrappers)
    'spdlog/.*'
    'SimpleMath\.h'
    'DirectXCollision\.h'
    'tiny_gltf\.h'
    'nlohmann/.*'
)

# Exception files (allowed to use platform headers)
$ExceptionFiles = @(
    'ImGuiLayer.cpp'
)

$combinedPattern = $ForbiddenPatterns -join '|'
$regex = "^\s*#\s*include\s*[<`"]($combinedPattern)[>`"]"

$violations = @()

$files = @()
$files += Get-ChildItem -Path "$EditorDir\src" -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue
$files += Get-ChildItem -Path "$EditorDir\include" -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue

foreach ($file in $files) {
    $fileName = $file.Name
    if ($ExceptionFiles -contains $fileName) { continue }

    $lineNum = 0
    foreach ($line in (Get-Content $file.FullName)) {
        $lineNum++
        if ($line -match $regex) {
            $violations += [PSCustomObject]@{
                File = $file.FullName
                Line = $lineNum
                Text = $line.Trim()
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host ""
    Write-Host "ERROR: Editor include policy violation(s) detected!" -ForegroundColor Red
    Write-Host "Editor files must not directly include D3D12 or engine-internal library headers." -ForegroundColor Red
    Write-Host "Use engine abstractions instead. D3D12 exception: ImGuiLayer.cpp" -ForegroundColor Red
    Write-Host ""
    foreach ($v in $violations) {
        Write-Host "  $($v.File):$($v.Line)" -ForegroundColor Yellow -NoNewline
        Write-Host "  $($v.Text)" -ForegroundColor White
    }
    Write-Host ""
    exit 1
}

Write-Host "Editor include policy: OK"
exit 0
