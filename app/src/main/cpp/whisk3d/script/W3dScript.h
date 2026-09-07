#ifndef W3DSCRIPT_H
#define W3DSCRIPT_H
#include <string>
#include <vector>
#include <utility>

class Object;

// ============================================================================
//  W3dScript — scripts LUA estilo Unity: cualquier objeto puede tener un .lua
//  colgado. El interprete (Lua 5.4, C plano, bien liviano) vive en el Core:
//  el juego final y el editor usan EXACTAMENTE el mismo runtime.
//
//  El .lua declara una tabla global `propiedades` con lo que quiere que el
//  editor exponga (estilo Unity): referencias a objetos, desplegables de
//  opciones y VALORES editables por instancia (numero / bool / texto):
//
//      propiedades = {
//        pelota     = "objeto",                          -- referencia
//        dificultad = { "facil", "normal", "dificil" },  -- desplegable
//        frame      = 0,                                 -- valor numerico
//        agarrada   = false,                             -- valor bool
//        etiqueta   = "x",                               -- valor de texto
//      }
//      function inicio() ... end            -- una vez, al dar PLAY
//      function actualizar(dt) ... end      -- cada frame (dt en segundos)
//
//  El .lua lee lo configurado con objeto("pelota"), opcion("dificultad") y
//  propiedad("frame") (si la instancia no configuro nada, propiedad() devuelve
//  el default declarado en la tabla).
//
//  Cada instancia corre en su PROPIO lua_State (aislada). El orden de
//  ejecucion es el del arbol de la escena (como se dibuja / outliner).
//  Para pasarse datos entre scripts (fruta.lua -> el HUD del principal) esta el
//  ESTADO COMPARTIDO: setCompartido("suma", 3) escribe en un mapa C++ unico del
//  juego y compartido("suma") lo lee desde CUALQUIER script (nil si no existe).
//  Acepta numero/bool/string; setCompartido(nombre, nil) borra la clave. El
//  mapa entero se limpia al parar el juego (W3dScriptDescargarTodo).
// ============================================================================

// ---- PREFABS (bind lua `instanciar`): el Core no conoce el parser del .w3d,
// asi que instanciar un prefab de la Biblioteca pasa por este hook, que el
// importador del editor instala al cargar una Biblioteca. posMotor = NULL o
// {x,y,z} en coordenadas del MOTOR (como setPosicion). NULL = no hay prefab
// con ese nombre (o no hay Biblioteca cargada): el bind devuelve nil y el
// script reintenta el frame siguiente si quiere.
extern Object* (*W3dInstanciarPrefabHook)(const char* nombre, const float* posMotor);

// una propiedad expuesta por el .lua (lo que la tarjeta del editor muestra)
struct W3dScriptProp {
    std::string nombre;
    int tipo;                          // 0 = objeto, 1 = opciones (desplegable), 2 = valor
    std::vector<std::string> opciones; // solo tipo 1 (la primera es el default)
    int subtipo;                       // solo tipo 2: 0 = numero, 1 = bool, 2 = texto
    std::string defecto;               // solo tipo 2: el default declarado (como texto)
    W3dScriptProp() : tipo(0), subtipo(0) {}
};

// UN script asignado: su .lua + lo elegido en el editor para sus propiedades
// (el NOMBRE del objeto referenciado, o la opcion del desplegable)
struct W3dScriptEntrada {
    std::string ruta;
    std::vector<std::pair<std::string, std::string> > refs;
};

// datos de script GUARDADOS en el objeto (puntero opcional: casi nadie lo usa).
// Un objeto puede tener VARIOS scripts: corren en orden.
struct W3dScriptDatos {
    std::vector<W3dScriptEntrada> scripts;
    // cual esta SELECCIONADO en el panel (estado de UI, no se guarda en el .w3d):
    // la tarjeta de abajo muestra las propiedades de ESTE. Mismo rol que
    // Mesh::modificadorActivo para el stack de modificadores.
    int activo;
    W3dScriptDatos() : activo(0) {}
};

// ---- runtime (instancias vivas durante el PLAY) ----------------------------
// crea UNA instancia por script del objeto (lee propiedades; false si fallo todo)
bool W3dScriptCargar(Object* duenio);
// resuelve una referencia expuesta del script 'idx' (el editor busca por nombre)
void W3dScriptResolverRef(Object* duenio, int idx, const std::string& prop, Object* obj);
// asigna una OPCION elegida (los desplegables); el .lua la lee con opcion("x")
void W3dScriptResolverOpcion(Object* duenio, int idx, const std::string& prop, const std::string& valor);
// asigna un VALOR configurado por instancia (tipo 2); el .lua lo lee con
// propiedad("x") (numero/bool/string segun el default declarado en el script)
void W3dScriptResolverValor(Object* duenio, int idx, const std::string& prop, const std::string& valor);
bool W3dScriptInicio(Object* duenio);              // llama inicio() de cada script
bool W3dScriptActualizar(Object* duenio, float dt);// llama actualizar(dt) de cada uno
void W3dScriptDescargarTodo();                     // STOP: mata todas las instancias
void W3dScriptDescargarDe(Object* duenio);         // solo las de ESTE objeto (edicion en vivo)

// lee SOLO la tabla `propiedades` de un .lua (para la tarjeta del editor,
// sin arrancar el juego). false si el archivo no parsea.
bool W3dScriptLeerPropiedades(const std::string& ruta, std::vector<W3dScriptProp>* props);

// ---- estado COMPARTIDO entre scripts (ver el bloque de arriba) -------------
// Los lua_State son independientes a proposito (aislamiento); este mapa C++
// unico es el UNICO canal entre scripts: setCompartido()/compartido() en lua.
// Se limpia solo en W3dScriptDescargarTodo (Stop del editor / cierre del juego).
// Accesores para el harness/depuracion: cantidad de claves y el par i-esimo
// (el valor formateado como texto: "3", "true", "hola"). false = fuera de rango.
int  W3dScriptCompartidoCantidad();
bool W3dScriptCompartidoPar(int i, std::string* nombre, std::string* valor);

// teclado para los scripts: la plataforma lo alimenta (nombres en minuscula:
// "w", "s", "arriba", "abajo", ...). tecla("w") lo lee el .lua.
void W3dScriptTecla(const char* nombre, bool apretada);
void W3dScriptSoltarTeclas();
// GAMEPAD analogico: la plataforma alimenta los sticks (con la deadzone ya aplicada)
// y los botones; el .lua los lee con stick("izq") -> x,y y boton("a") -> bool
void W3dScriptStick(int cual, float x, float y);        // 0 = izq, 1 = der
void W3dScriptBotonPad(const char* nombre, bool v);     // "a" "b" "x" "y" "arriba"...
// LO QUE EL SCRIPT VE ahora mismo (harness de pruebas y overlays de debug): el
// mismo valor que devuelven stick("izq") y boton("a") adentro del .lua.
void W3dScriptStickLeer(int cual, float* x, float* y);
bool W3dScriptBotonPadLeer(const char* nombre);
// TOQUE/arrastre (dedo o mouse) en coordenadas del LIENZO del juego (px, centro
// en 0,0 como posPx). activo=false al soltar. Lua: toque() -> x, y, tocando
void W3dScriptToque(float x, float y, bool activo);      // dedo 0 (compat)
void W3dScriptDedo(int i, float x, float y, bool activo); // multi-touch: dedo i (0-based)
void W3dScriptRaton(float x, float y, bool dentro);       // mouse-over (posicion sin clic)

// SEMILLA del random: refuerzo OPCIONAL de la plataforma para math.random / azar(). El Core ya se siembra
// solo (self-contained: time() + reloj del motor + direccion del state + contador), asi que math.random ya
// varia entre ejecuciones sin llamar a esto. Si la plataforma quiere sumar mas entropia (ej time(0) del
// desktop), llama a W3dScriptSemilla(s) ANTES de cargar los scripts; se mezcla en la semilla de cada state.
void W3dScriptSemilla(unsigned s);

// el ultimo error de lua ("" si no hubo): para mostrarlo en el editor
const char* W3dScriptUltimoError();

// binding EXTRA por plataforma (el editor registra su API 2D: posPx, setTexto,
// pantalla...). Recibe el lua_State crudo como void* para no arrastrar lua.h.
typedef void (*W3dScriptBindFn)(void* L);
void W3dScriptSetBindExtra(W3dScriptBindFn fn);

// HOOK de REDIBUJO: el editor es EVENT-DRIVEN (solo repinta si g_redraw esta prendido), asi que un
// cambio hecho desde lua (setPosicion/mover/girar/setEscala/setVisible/setColor/...) no se veria hasta
// el proximo evento del mouse. El Core NO puede tocar g_redraw / g_objetosMovidos (viven en main/), asi
// que main/ registra aca una funcion que los prende y CADA setter 3D del Core la llama al final.
// En el runtime compilado no hace falta (el loop dibuja siempre): queda en NULL y no cuesta nada.
typedef void (*W3dScriptRedibujoFn)(void);
void W3dScriptSetRedibujo(W3dScriptRedibujoFn fn);
// dispara ese hook (no hace nada si nadie lo registro). Lo llaman los modulos del Core que
// mueven objetos por su cuenta, como la fisica (physics/W3dFisica.cpp).
void W3dScriptRedibujar(void);

// helpers para los bindings de plataforma (empujan/leen el Object* del stack)
Object* W3dScriptParamObjeto(void* L, int idx);

#endif // W3DSCRIPT_H
