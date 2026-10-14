$ErrorActionPreference = 'Stop'
# Pin the official SDK; binary libraries and voice data stay outside the source tree.
$revision = '78a923d29a45e4e1433b909d3da12ab5b0f994fc'
$root = Join-Path (Split-Path $PSScriptRoot -Parent) '.local/esp_tts_sdk'
New-Item -ItemType Directory -Force -Path $root | Out-Null
$files = @('LICENSE', 'esp-tts/esp_tts_chinese/include/esp_tts.h',
    'esp-tts/esp_tts_chinese/include/esp_tts_voice.h',
    'esp-tts/esp_tts_chinese/include/esp_tts_voice_template.h',
    'esp-tts/esp_tts_chinese/esp32s3/libesp_tts_chinese.a',
    'esp-tts/esp_tts_chinese/esp32s3/libvoice_set_xiaole.a',
    'esp-tts/esp_tts_chinese/esp_tts_voice_data_xiaole.dat')
foreach ($file in $files) {
    $target = Join-Path $root $file
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/espressif/esp-sr/$revision/$file" -OutFile "$target.tmp" -TimeoutSec 120
    Move-Item -LiteralPath "$target.tmp" -Destination $target -Force
    Write-Output "Downloaded $file"
}
@'
idf_component_register(INCLUDE_DIRS "esp-tts/esp_tts_chinese/include")
add_prebuilt_library(tts_chinese "esp-tts/esp_tts_chinese/esp32s3/libesp_tts_chinese.a")
add_prebuilt_library(tts_voice "esp-tts/esp_tts_chinese/esp32s3/libvoice_set_xiaole.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE tts_chinese tts_voice)
'@ | Set-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Encoding utf8
$revision | Set-Content -LiteralPath (Join-Path $root 'revision.txt') -Encoding ascii
Write-Output 'ESP-TTS Xiaole installed locally. Automatic announcements and previews use this voice.'
