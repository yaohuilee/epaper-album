# ESP-TTS Xiaole: build and voice data

English | [简体中文](zh-CN/esp-tts.md) · [Announcements](announcement.md)

Since 0.16.0, automatic and test speech use official `esp_tts_voice_data_xiaole.dat` (2,938,039 bytes), speed 2. Output is 16 kHz mono signed 16-bit PCM, resampled to 24 kHz for the complete beep/silence/speech sequence. This voice synthesizes Mandarin, independently of the web locale.

```powershell
./tools/Install-EspTts.ps1
./tools/Build.ps1
# After loading ESP-IDF; substitute the actual port
idf.py -p COM5 flash
```

The installer downloads S3 libraries, headers, Xiaole data and LICENSE from official fixed source tree `78a923d29a45e4e1433b909d3da12ab5b0f994fc` into `.local/esp_tts_sdk/`. Local dependencies are not committed. `CONFIG_EPAPER_ESP_TTS` defaults on; missing files require installation first. Disabling the option leaves photo playback usable but speech unavailable.

Flash includes `voice_data` at `0x600000`, 4 MiB. Only Xiaole's data header is accepted; old trial voices and `tts-selected.dat` are not build inputs. Voice data is not updated through SD or HTTP.

## Selection and license

At 0.15.3 the user compared Xiaoxin Lite, Xiaole and full Xiaoxin at 40% volume/speed 2 and selected Xiaole. At 0.16.0 complete hourly/half-hourly phrases prepared in 4213/4342 ms and were confirmed audible and complete. See [validation](validation.md) and [trial history](history/esp-tts-trial.md).

Official [ESP-TTS documentation](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_synthesis/readme.html) and [SDK/voice source](https://github.com/espressif/esp-sr/tree/master/esp-tts). Upstream resources retain their license, including the Espressif-product usage condition; API header Apache notices remain. Project MIT does not relicense these components. Binaries, voice/model data and personal recordings are not stored in this repository.
