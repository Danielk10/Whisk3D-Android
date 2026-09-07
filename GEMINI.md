# Instrucciones de Compilación y SDK - Whisk3D Android

> **Lema:** *Potencia 3D retro y liviana para Android: simplicidad, portabilidad y rendimiento nativo en C++.*

Este documento describe cómo instalar el SDK de Android y compilar el proyecto **Whisk3D-Android**.

## 1. Instalación del SDK

El SDK de Android (junto con NDK y CMake) necesario para compilar este proyecto se instala automáticamente ejecutando el script proporcionado:

```bash
bash setup-sdk.sh
```

- **Ubicación del SDK:** Todas las descargas y herramientas del SDK se instalan en el directorio `/tmp/android-sdk` (o la ruta especificada por `ANDROID_SDK_ROOT`).
- **Configuración automática:** El script genera el archivo `local.properties` con `sdk.dir` apuntando al SDK configurado y da permisos de ejecución a `gradlew`.

## 2. Compilación (Solo Debug)

Este proyecto está configurado para compilación de depuración (**debug** sin firmas de producción):

```bash
./gradlew assembleDebug
```

Para compilar e instalar directamente en un dispositivo o emulador conectado vía ADB:

```bash
./gradlew installDebug
```

## 3. Ubicación del APK Generado

Después de una compilación exitosa, el archivo APK generado se encontrará en:

```
app/build/outputs/apk/debug/app-debug.apk
```

## 4. Arquitectura y Tecnologías
- **GameActivity (C++):** Ciclo de vida y gestión de eventos de juego nativos de Android.
- **Whisk3D Core:** Motor 3D retro en C++ abstraído con OpenGL ES 2.0 / 3.0.
- **Gradle & AGP:** Gradle 9.6.0 + Android Gradle Plugin 9.2.1.
- **Compile SDK:** Android 37 / Min SDK 23 / Target SDK 37.
