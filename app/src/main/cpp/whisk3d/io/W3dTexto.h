#ifndef W3D_TEXTO_H
#define W3D_TEXTO_H

// ============================================================================
//  W3dTexto - escaner de texto POR LINEAS y escritor de numeros del .w3dm
//
//  Es la base del formato de malla propio (W3dMalla.{h,cpp}). Dos razones para
//  que viva aparte y sin dependencias:
//
//   1) VELOCIDAD. El parseo es IN SITU: se camina el buffer del archivo con un
//      puntero y NO se hace ni una sola reserva de memoria por token (nada de
//      istringstream, ni substr, ni strtod sobre una copia). Medido en la PC de
//      desarrollo con 200.000 numeros: strtod 46,4 ns/token contra 7,4 ns/token
//      con este escaner. En un ARM11 de 332 MHz (N95) esa diferencia es la que
//      separa "abre al toque" de "se queda pensando". Mismo criterio que ya
//      usaba el importador de .obj (main/importers/import_obj.cpp).
//
//   2) EXACTITUD DE LOS FLOAT. Las costuras (seamEdges) y los bordes marcados
//      (sharpEdges) se indexan por los BYTES CRUDOS de la posicion, asi que si
//      un float no vuelve EXACTAMENTE igual del texto, esos datos se pierden en
//      SILENCIO al reabrir. Por eso W3dEscribirFloat busca la representacion
//      decimal MAS CORTA que, releida CON ESTE MISMO LECTOR, da el mismo
//      float32 bit a bit. Se valida contra W3dTextoAFloat y no contra atof a
//      proposito: lo que importa es que cierre el par escritor/lector real.
//
//  C++03 puro, sin STL pesada, sin C99 (nada de snprintf ni long long): compila
//  con RVCT de Symbian.
// ============================================================================

#include <string>
#include <cstddef>

// ---------------------------------------------------------------------------
//  ESCRITURA
// ---------------------------------------------------------------------------

// agrega a 's' el decimal MAS CORTO que relee igual (ver arriba). El cero
// negativo se escribe "-0" A PROPOSITO: sus bytes no son los de +0.0f y la
// clave de sharp/seam compara bytes. NaN -> "0", infinitos -> +-3.4e38
// (sanear es mejor que escribir un archivo que hace crashear un N95).
void W3dEscribirFloat(std::string& s, float v);

// entero decimal pelado (sin locale, sin sprintf).
void W3dEscribirInt(std::string& s, int v);

// ---------------------------------------------------------------------------
//  LECTURA IN SITU. Todas avanzan 'p' y nunca pasan de 'fin'.
// ---------------------------------------------------------------------------

// salta espacios y tabs.
void W3dSaltarBlancos(const char*& p, const char* fin);

// lee un entero decimal con signo. false = no habia token (no se toca 'out').
// Satura en +-2000000000 en vez de desbordar (un archivo corrupto no puede
// hacer aritmetica indefinida).
bool W3dLeerEntero(const char*& p, const char* fin, int& out);

// lee un float decimal (con signo, punto y exponente opcionales).
// false = no habia token. Un token que empieza pero no parsea da 0 y true.
bool W3dLeerFloat(const char*& p, const char* fin, float& out);

// lee una palabra (hasta el proximo blanco). false = no habia token.
bool W3dLeerPalabra(const char*& p, const char* fin, std::string& out);

// el RESTO de la linea desde 'p', recortando blancos de los dos bordes.
// NO SIRVE PARA NOMBRES (ver W3dNombreLinea): recortar cambia el nombre en silencio.
std::string W3dRestoLinea(const char* p, const char* fin);

// el RESTO de la linea desde 'p' TAL CUAL, salteando UN SOLO blanco (el separador
// que puso el escritor) y NADA MAS.
//
// LOS NOMBRES SON VINCULOS, NO TEXTO DECORATIVO. El proyecto.json guarda el nombre
// EXACTO y el .w3dm lo repite; si el lector le recorta un espacio, "Hueso " del
// archivo deja de ser "Hueso " del json y el binding (hueso 2D, material, mesh part)
// se corta SOLO al guardar, sin un solo aviso. Ademas dos nombres distintos que solo
// difieren en los bordes colapsarian en uno y romperian la regla de nombres unicos.
// Por eso todo lo que sea NOMBRE se lee con esta y no con W3dRestoLinea.
std::string W3dNombreLinea(const char* p, const char* fin);

// convierte un texto entero a float con el MISMO codigo que W3dLeerFloat (lo
// usa W3dEscribirFloat para validar el round-trip).
float W3dTextoAFloat(const char* s, size_t n);

// ---------------------------------------------------------------------------
//  RECORRIDO POR LINEAS. Tolera \n y \r\n. No copia nada.
// ---------------------------------------------------------------------------
class W3dLineas {
    public:
        W3dLineas(const char* datos, size_t n)
            : p(datos), f(datos + n), nro(0) {}
        // devuelve la proxima linea SIN el salto (ni el \r). false = fin.
        bool Siguiente(const char*& ini, const char*& finLinea);
        int  Nro() const { return nro; }              // numero de la ultima linea leida (base 1)
        size_t Restante() const { return (size_t)(f - p); } // bytes que faltan (cota de reserva)
    private:
        const char* p;
        const char* f;
        int nro;
};

#endif
