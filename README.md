# Open RealtimeAPI Embedded SDK

# Table of Contents

- [Docs](#docs)
- [Installation](#installation)
- [Usage](#usage)

## Platform/Device Support

This SDK has been developed tested on a `esp32s3` and `linux`. You don't need any physical hardware
to run this SDK. You can use it from Linux directly.

To use it on hardware purchase either of these microcontrollers. Others may work, but this is what
has been developed against.

* [Freenove ESP32-S3-WROOM](https://www.amazon.com/gp/product/B0BMQ8F7FN)
* [Sonatino - ESP32-S3 Audio Development Board](https://www.amazon.com/gp/product/B0BVY8RJNP)

You can get a ESP32S3 for much less money on eBay/AliExpress.

## Installation

`protoc` must be in your path with `protobufc` installed.

Select the target board.

* CoreS3 (SE) - `export SDK_CONFIG="sdkconfig.defaults;sdkconfig.defaults.cores3`
* AtomS3    (with Atomic Speaker and PDM Microphone Unit) - `export SDK_CONFIG="sdkconfig.defaults;sdkconfig.defaults.atoms3`
* Atom Lite (with Atomic Speaker and PDM Microphone Unit) - `export SDK_CONFIG="sdkconfig.defaults;sdkconfig.defaults.atom_lite`

Call `set-target` with the platform you are targetting. Today only `linux`, `esp32` and `esp32s3` are supported.

* ESP32 targets   : Atom Lite
    * `idf.py set-target esp32`

* ESP32-S3 targets: CoreS3 (SE), AtomS3
    * `idf.py set-target esp32s3`

Note that to change the target, you have to remove the build directory. 

Configure device specific settings. None needed at this time
* `idf.py menuconfig`

Set your Wifi SSID + Password as env variables
* `export WIFI_SSID=foo`
* `export WIFI_PASSWORD=bar`
* `export OPENAI_API_KEY=bing`

Build
* `idf.py build`

If you built for `esp32` or `esp32s3` run the following to flash to the device
* `idf.py flash`

If you built for `linux` you can run the binary directly
* `./build/src.elf`

See [build.yaml](.github/workflows/build.yaml) for a Docker command to do this all in one step.

## Debugging

You can enable the debug audio stream output from menuconfig.
The settings are in `Embedded SDK Configuration` menu.

To enable the debug audio UDP stream output, enable `Enable Debug Audio UDP Client` and configure the host IP address to send the audio data for debugging.

```
[*] Enable Debug Audio UDP Client
(192.168.100.1) Debug Audio Host (NEW)
(10000) UDP port to send microphone input audio data to (NEW)
(10001) UDP port to send speaker output audio data to (NEW)
```

At the host, you can receive the raw PCM audio data by UDP server software like netcat.

To receive the microphone input data

```
nc -ul 10000 > audio_input.pcm
```

To receive the speaker output data

```
nc -ul 10001 > audio_output.pcm
```

You can convert the received audio data by using `ffmpeg` like this.

```
ffmpeg -y -f s16le -ar 8k -ac 1 -i audio_input.pcm audio_input.wav
ffmpeg -y -f s16le -ar 8k -ac 1 -i audio_output.pcm audio_output.wav
```

## Usage
