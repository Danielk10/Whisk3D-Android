# ✈️ Whisk3D: Sky Strike

<p align="center">
  <img src="app/src/main/ic_launcher-playstore.png" width="160" height="160" alt="Whisk3D: Sky Strike Logo" />
</p>

[![Android](https://img.shields.io/badge/Android-6.0%20(API%2023)%20a%20Android%2017%20(API%2037)-3DDC84?logo=android&logoColor=white)](https://developer.android.com/)
[![ABI](https://img.shields.io/badge/ABI-arm64--v8a%20%7C%20armeabi--v7a%20%7C%20x86__64-0091EA?logo=arm&logoColor=white)](https://developer.android.com/ndk/guides/abis)
[![NDK](https://img.shields.io/badge/NDK-r30--rc1-4CAF50?logo=android&logoColor=white)](https://developer.android.com/ndk)
[![AGP](https://img.shields.io/badge/AGP-9.2.1-blue?logo=android)](https://developer.android.com/studio/releases/gradle-plugin)
[![Google Play](https://img.shields.io/badge/Google%20Play-Target%20Ready-34A853?logo=googleplay&logoColor=white)](https://play.google.com/store)
[![Licencia](https://img.shields.io/badge/Licencia-Apache%202.0-blue)](./LICENSE)

> Nombre visible de la app: **Whisk3D: Sky Strike**.  
> Lema: *Potencia 3D retro y liviana para Android: simplicidad, portabilidad y rendimiento nativo en C++.*  
> Género: **Retro 3D Aerial Combat / Flight Arcade** (Avión de combate 3D sobre mar y tierra disparando a objetivos).  
> Rango de soporte Android: **API 23 a API 37** (Android 6.0 a Android 17+).

---

## 🎮 Descripción del Juego

**Whisk3D: Sky Strike** es un juego arcade de combate aéreo en 3D para Android donde pilotas un caza militar sobre archipiélagos, océano abierto y bases terrestres enemigas. Diseñado con una estética retro 3D ultraliviana utilizando **Whisk3D Core** en C++ puro y **Google GameActivity** (AGDK) para máxima fluidez y tasa de refresco nativa.

- 🛩️ **Vuelo 3D y Combate:** Navega escenarios sobre mar y tierra esquivando fuego antiaéreo y destruyendo blancos estratégicos.
- 🎯 **Armamento:** Proyectiles balísticos, misiles y láseres con efectos de partículas e impacto en tiempo real.
- ⚡ **Rendimiento Nativo:** Cero sobrecarga de motores pesados, renderizado directo en OpenGL ES 2.0 / 3.0.

---

## 👥 Créditos y Origen

Este proyecto está basado en el motor **[Whisk3D Core](https://github.com/Dante-Leoncini/Whisk3D-Core)** desarrollado por **[Dante Leoncini](https://github.com/Dante-Leoncini)** ([@soykhaler](https://github.com/soykhaler)).

- **Repositorio original del Core:** [https://github.com/Dante-Leoncini/Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core)
- **Comunidad en Telegram:** [https://t.me/Whisk3D](https://t.me/Whisk3D)
- **Ejemplos oficiales del motor:** [https://github.com/Dante-Leoncini/Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples)

Agradecimientos especiales a **Dante Leoncini** por concebir y liderar la arquitectura de este motor gráfico 3D retro y ultraligero.

---

## 🌟 Características Técnicas

- ⚡ **Alto rendimiento y bajo consumo:** Motor en C++ optimizado para correr con una fracción de los recursos habituales.
- 🕹️ **GameActivity (AGDK):** Manejo robusto del ciclo de vida de la ventana, loop de renderizado a pantalla completa y eventos táctiles/gamepad nativos.
- 🎨 **Renderizado Retro:** Abstracción gráfica unificada sobre OpenGL ES 2.0 / 3.0 con soporte de texturas, iluminación, niebla, alpha blending y estética retro.
- 📦 **I/O y Assets APK:** Integración directa con el `AAssetManager` de Android para cargar mallas `.w3dm`, texturas PNG/JPG y paquetes `.w3dpack`.
- 🛠️ **Configuración moderna de Gradle:** Compatible con Gradle 9.6.0 y Android Gradle Plugin (AGP) 9.2.1.
- 💾 **Caché y Builds en `/tmp`:** Redirección de Gradle cache y build outputs hacia `/tmp`, protegiendo el almacenamiento local y acelerando la compilación en RAM/tmpfs.
- 🔊 **Audio Nativo OpenSL ES:** Mezclador por software estéreo a 44.1 kHz con efectos de turbina, cañón, misiles e impactos navales, con botón de sonido (MUTE/ON) en el HUD.

---

## 🚀 Requisitos y Configuración

### 1. Preparar el SDK / NDK

El proyecto incluye el script automatizado `setup-sdk.sh` para descargar y configurar el SDK/NDK en entornos Linux:

```bash
bash setup-sdk.sh
```

### 2. Compilar el Proyecto (Debug)

```bash
./gradlew assembleDebug
```

El APK resultante se genera en:
`/tmp/whisk3d/outputs/apk/debug/app-debug.apk`

O para compilar el Android App Bundle (AAB):
```bash
./gradlew bundleDebug
```
Ruta del AAB:
`/tmp/whisk3d/outputs/bundle/debug/app-debug.aab`

---

## 📁 Estructura del Proyecto

```
Whisk3D-Android/
├── app/
│   ├── build.gradle              # Configuración de app (compileSdk 37, minSdk 23, NDK)
│   └── src/main/
│       ├── ic_launcher-playstore.png # Icono 512x512 para Google Play Store
│       ├── cpp/
│       │   ├── CMakeLists.txt    # Configuración de CMake para compilar Whisk3D Core
│       │   ├── main.cpp          # Punto de entrada android_main con GameActivity
│       │   ├── Renderer.cpp      # Render loop inicializando y dibujando con Whisk3D
│       │   ├── include/GL/gl.h   # Shim portable de OpenGL para Android NDK
│       │   └── whisk3d/          # Código fuente de Whisk3D Core (por Dante Leoncini)
│       ├── java/                 # Actividad Android Java vinculada con GameActivity
│       └── res/                  # Recursos de Android (iconos mipmap, temas, strings)
├── gradle/
│   ├── libs.versions.toml        # Versiones de dependencias (AGP 9.2.1, GamesActivity 4.4.2)
│   └── wrapper/                  # Gradle Wrapper 9.6.0
├── GEMINI.md                     # Guía rápida de compilación para asistentes y desarrolladores
├── setup-sdk.sh                  # Script de instalación automática del SDK/NDK
└── LICENSE                       # Licencia Apache 2.0
```

---

## 📜 Licencia

Distribuido bajo la Licencia **Apache 2.0**. Consulta el archivo [`LICENSE`](./LICENSE) para más detalles.  
El código de **Whisk3D Core** en `app/src/main/cpp/whisk3d/` pertenece a **Dante Leoncini** bajo licencia **MIT**.
