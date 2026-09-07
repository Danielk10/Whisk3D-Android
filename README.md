# 🎮 Whisk3D Android

> **Lema:** *Potencia 3D retro y liviana para Android: simplicidad, portabilidad y rendimiento nativo en C++.*  
> *(Bringing classic retro 3D game power to Android with lightweight C++ and modern GameActivity).*

Whisk3D Android es la implementación y plantilla oficial para ejecutar el motor de videojuegos 3D/2D **Whisk3D Core** en dispositivos Android modernos utilizando **Google GameActivity** del Android Game Development Kit (AGDK) y C++ nativo.

---

## 🌟 Características Principales

- ⚡ **Alto rendimiento y bajo consumo:** Motor en C++ optimizado para correr con una fracción de los recursos habituales.
- 🕹️ **GameActivity (AGDK):** Manejo robusto del ciclo de vida de la ventana, loop de renderizado a pantalla completa y eventos táctiles/gamepad nativos.
- 🎨 **Renderizado Retro:** Abstracción gráfica unificada sobre OpenGL ES 2.0 / 3.0 con soporte de texturas, iluminación, niebla, alpha blending y estética retro/pixelada.
- 📦 **I/O y Assets APK:** Integración directa con el `AAssetManager` de Android para cargar mallas `.w3dm`, texturas PNG/JPG y paquetes cifrados `.w3dpack` directamente desde los assets del APK.
- 🛠️ **Configuración moderna de Gradle:** Compatible con Gradle 9.6.0 y Android Gradle Plugin (AGP) 9.2.1, listo para Android Studio y Android Code Studio.
- 🧪 **Solo Debug:** Configurado sin dependencias de firmas de producción para facilitar el desarrollo, pruebas y clonado directo.

---

## 🚀 Requisitos y Configuración

### 1. Preparar el SDK / NDK

El proyecto incluye el script automatizado `setup-sdk.sh` para descargar y configurar el SDK/NDK en entornos Linux/CloudShell:

```bash
bash setup-sdk.sh
```

### 2. Compilar el Proyecto (Debug)

```bash
./gradlew assembleDebug
```

El APK resultante se genera en:
`app/build/outputs/apk/debug/app-debug.apk`

---

## 📁 Estructura del Proyecto

```
Whisk3D-Android/
├── app/
│   ├── build.gradle              # Configuración de app (compileSdk 37, minSdk 23, NDK)
│   └── src/main/
│       ├── cpp/
│       │   ├── CMakeLists.txt    # Configuración de CMake para compilar Whisk3D Core
│       │   ├── main.cpp          # Punto de entrada android_main con GameActivity
│       │   ├── Renderer.cpp      # Render loop inicializando y dibujando con Whisk3D
│       │   ├── include/GL/gl.h   # Shim portable de OpenGL para Android NDK
│       │   └── whisk3d/          # Código fuente completo de Whisk3D Core
│       ├── java/                 # Actividad Android Java vinculada con GameActivity
│       └── res/                  # Recursos de Android (iconos, temas)
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
