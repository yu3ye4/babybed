@echo off
REM Fix CM55 build files after RT-Thread Studio regeneration
REM Adds pahomqtt include paths and removes paho_mqtt_udp.c

setlocal enabledelayedexpansion

set "PAHO_IPATH1=-I\"D:\\RT-ThreadStudio\\workspace\\Edgi_Talk_M55_WIFI\\packages\\pahomqtt-latest\\MQTTPacket\\src\""
set "PAHO_IPATH2=-I\"D:\\RT-ThreadStudio\\workspace\\Edgi_Talk_M55_WIFI\\packages\\pahomqtt-latest\\MQTTClient-RT\""
set "SEARCH=-I\"D:\\RT-ThreadStudio\\workspace\\Edgi_Talk_M55_WIFI\\rt-thread\\libcpu\\arm\\cortex-m7\""
set "REPLACE=%SEARCH% %PAHO_IPATH1% %PAHO_IPATH2%"

REM 1. Fix applications/subdir.mk - add pahomqtt include paths
set "FILE=Debug\applications\subdir.mk"
if exist "%FILE%" (
    echo Patching %FILE%...
    powershell -Command "(gc '%FILE%' -Raw) -replace [regex]::Escape('%SEARCH%'), '%REPLACE%' | Out-File -Encoding ascii '%FILE%'"
)

REM 2. Fix MQTTClient-RT/subdir.mk - add pahomqtt include paths + remove udp
set "FILE=Debug\packages\pahomqtt-latest\MQTTClient-RT\subdir.mk"
if exist "%FILE%" (
    echo Patching %FILE%...
    powershell -Command "$c = gc '%FILE%' -Raw; $c = $c -replace [regex]::Escape('%SEARCH%'), '%REPLACE%'; $c = $c -replace '[^\n]*paho_mqtt_udp\.[cod][^\n]*\n?', ''; $c = $c -replace '\\\s*\n\s*\n', '\' + \"`n\"; Out-File -Input $c -Encoding ascii '%FILE%'"
)

REM 3. Fix MQTTPacket/src/subdir.mk - add pahomqtt include paths
set "FILE=Debug\packages\pahomqtt-latest\MQTTPacket\src\subdir.mk"
if exist "%FILE%" (
    echo Patching %FILE%...
    powershell -Command "(gc '%FILE%' -Raw) -replace [regex]::Escape('%SEARCH%'), '%REPLACE%' | Out-File -Encoding ascii '%FILE%'"
)

echo Done.
endlocal
