# Блок для визначення параметрів скрипту
param (
    # Назва проекту (Gandalf, Conduit, Exporter, Isengard)
    [Parameter(Mandatory=$true)]
    [string]$ProjectName,

    # 'чисте' розгортання (видаляє стару папку)
    [switch]$Clean
)

# --- НАЛАШТУВАННЯ ---
$QT_PATH = "D:\Qt\6.7.3\mingw_64"
$SOLUTION_ROOT = "D:\Develop\WhiteTower"
$DEPLOY_ROOT = "D:\Develop\Deploys"
# --- КІНЕЦЬ НАЛАШТУVANНЯ ---

# --- ЛОГІКА ---

# --- ОСЬ ТУТ БУЛО ВИПРАВЛЕНО ---
# Налаштування конфігурації (Жорстко задано 'release' маленькими літерами)
$ARCH = "mingw_64"
$CONFIG = "release" # <-- Змінено "Release" на "release"
$CONFIG_DISPLAY = "Release" # <-- Змінна для назви папки
$SQL_PLUGIN_FILE = "qsqlibase.dll"
# --- КІНЕЦЬ ВИПРАВЛЕННЯ ---

# Формування шляхів
$BUILD_DIR_NAME = "Desktop_Qt_6_7_3_${ARCH}_bit-${CONFIG_DISPLAY}"
$BUILD_PATH = Join-Path $SOLUTION_ROOT "\build\$BUILD_DIR_NAME\$ProjectName"
$DEPLOY_PATH = Join-Path $DEPLOY_ROOT "$ProjectName ($CONFIG_DISPLAY)"
$EXECUTABLE_PATH = Join-Path $BUILD_PATH "$ProjectName.exe"

# --- ПЕРЕВІРКИ ---
Write-Host "Checking paths..."
$WINDPLOYQT_EXE = Join-Path $QT_PATH "bin\windeployqt.exe"

if (-not (Test-Path $WINDPLOYQT_EXE)) {
    Write-Host -ForegroundColor Red "Error: windeployqt.exe not found in $($WINDPLOYQT_EXE)"
    exit 1
}

if (-not (Test-Path $EXECUTABLE_PATH)) {
    Write-Host -ForegroundColor Red "Error: Executable not found: $($EXECUTABLE_PATH)"
    Write-Host -ForegroundColor Yellow "(Make sure you built the '$CONFIG_DISPLAY' configuration)"
    exit 1
}
Write-Host -ForegroundColor Green "Paths OK."

# --- РОЗГОРТАННЯ ---

# Очистка, якщо вказано -Clean
if ($Clean) {
    Write-Host "Cleaning deployment folder: $DEPLOY_PATH"
    if (Test-Path $DEPLOY_PATH) {
        Remove-Item -Path $DEPLOY_PATH -Recurse -Force
    }
}

# Створюємо папку
New-Item -Path $DEPLOY_PATH -ItemType Directory -Force | Out-Null

# Запуск windeployqt
Write-Host "Deploying $ProjectName ($CONFIG_DISPLAY) to $DEPLOY_PATH"
Write-Host "Running windeployqt..."
# Використовуємо $CONFIG ("release") для прапора
& $WINDPLOYQT_EXE --$CONFIG --dir $DEPLOY_PATH $EXECUTABLE_PATH

if ($LASTEXITCODE -ne 0) {
    Write-Host -ForegroundColor Red "Error: windeployqt failed with code $LASTEXITCODE."
    exit $LASTEXITCODE
}

# Копіювання SQL-плагінів (Firebird)
Write-Host "Copying SQL plugins (Firebird)..."
$SQL_PLUGIN_PATH = Join-Path $QT_PATH "plugins\sqldrivers"
$DEPLOY_PLUGIN_PATH = Join-Path $DEPLOY_PATH "sqldrivers"

New-Item -Path $DEPLOY_PLUGIN_PATH -ItemType Directory -Force | Out-Null

Copy-Item -Path (Join-Path $SQL_PLUGIN_PATH $SQL_PLUGIN_FILE) -Destination $DEPLOY_PLUGIN_PATH -Force

if ($LASTEXITCODE -ne 0) {
    Write-Host -ForegroundColor Red "Error: Failed to copy SQL plugin: $SQL_PLUGIN_FILE"
    exit $LASTEXITCODE
}

Write-Host "-------------------------------------------------"
Write-Host -ForegroundColor Green "Deployment of $ProjectName ($CONFIG_DISPLAY) completed."
Write-Host -ForegroundColor Green "Path: $DEPLOY_PATH"
Write-Host "-------------------------------------------------"