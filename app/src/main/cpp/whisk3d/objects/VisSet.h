#ifndef W3D_VISSET_H
#define W3D_VISSET_H
// ============================================================================
//  VisSet — conjuntos de visibilidad POR TRIANGULO por celda, precalculados
//  offline. Es el DATO del sistema de visibilidad del motor: "desde la celda
//  c se ven estos triangulos, EN ESTE ORDEN". Quien decide que celda esta
//  activa es otro (VisZona / setSector); quien dibuja la lista es otro (el
//  render). VisSet solo carga, decodifica y guarda.
//
//  Formato en disco: `.w3dvis` (binario, spec CONGELADO en formato/w3dvis.md).
//  Dos modos:
//    * bitset  : un bit por triangulo por celda (RLE de palabras de 32 bits).
//                Orden de salida: ascendente. Opcional: stream de orden de
//                transparentes por celda.
//    * playlist: celdas-clave (lista completa, cada K celdas) + registros
//                DELTA (ops ADD/REMOVE/MOVE reversibles) entre celdas
//                vecinas. La lista conserva el ORDEN del archivo (si
//                ordenLejosCerca, es el orden del pintor: transparencia sin
//                sort en runtime).
//
//  El `.pvs.json` legacy (listas por sector, ver formato/pvs-json.md) se
//  convierte con DesdeListas() a un VisSet bitset en memoria: entrada valida
//  para siempre, nunca se elimina.
//
//  C++03 / Symbian-safe. Sin nada de ningun juego: esto es un contenedor de
//  datos generico del motor.
// ============================================================================
#include <vector>
#include <string>

class VisSet {
public:
    enum Modo { ModoBitset = 0, ModoPlaylist = 1 };

    // --- header (ver formato/w3dvis.md) ---
    int      nTris;          // triangulos totales de la malla descripta
    int      nCeldas;        // cantidad de celdas
    int      modo;           // ModoBitset | ModoPlaylist
    bool     ordenLejosCerca;// flags bit0: las listas vienen lejos->cerca (orden del pintor)
    bool     hayOrden;       // flags bit1 (solo bitset): stream de orden de transparentes
    int      K;              // playlist: paso de celdas-clave ((celda % K)==0 es clave)
    unsigned hashTopo;       // hash FNV-1a de la topologia (0 = sin verificar)

    VisSet() { Limpiar(); }

    void Limpiar();
    bool Valido() const { return nCeldas > 0 && nTris > 0; }

    // ---- CARGA ----
    // parsea un `.w3dvis` completo desde memoria. false + err = archivo invalido.
    bool CargarW3dvis(const unsigned char* bytes, size_t n, std::string* err = 0);
    // construye un set BITSET desde listas por celda (el camino del `.pvs.json`
    // legacy: sin orden, indices fuera de [0,nTris) se ignoran).
    bool DesdeListas(const std::vector< std::vector<int> >& listas, int nTrisMalla);
    // construye un set PLAYLIST desde listas por celda CON ORDEN (lo usan el
    // horneador del editor y las pruebas; un conversor externo escribe el
    // binario directo contra el spec). Calcula los registros delta con un
    // diff posicional simple (correcto siempre; minimal no hace falta).
    bool DesdeListasPlaylist(const std::vector< std::vector<int> >& listas,
                             int nTrisMalla, int pasoK, bool ordenado);

    // ---- GUARDA ----
    // serializa al formato `.w3dvis` (spec congelado). false = set invalido.
    bool Guardar(std::vector<unsigned char>& out) const;

    // ---- CONSUMO ----
    // acceso aleatorio: deja en 'lista' los triangulos visibles desde 'celda',
    // EN EL ORDEN del archivo (bitset: ascendente). false = celda invalida.
    bool Decodificar(int celda, std::vector<unsigned>& lista) const;
    // paso incremental: transforma 'lista' (que DEBE ser el resultado de
    // 'desde') en la lista de 'hacia'. En playlist con |hacia-desde|==1 aplica
    // o invierte UN registro delta; cualquier otro salto re-decodifica. El
    // resultado es SIEMPRE identico a Decodificar(hacia).
    bool Delta(int desde, int hacia, std::vector<unsigned>& lista) const;
    // orden de transparentes de una celda BITSET con hayOrden (lejos->cerca).
    // false = el set no trae ese stream.
    bool OrdenTransparentes(int celda, std::vector<unsigned>& orden) const;

    // hash de topologia del contrato (formato/w3dvis.md): FNV-1a de nTris + los
    // indices de faces (3 por triangulo), cada uno mezclado como u32 LE.
    static unsigned HashTopologia(int nTris, const unsigned int* faces, int nIndices);
    static unsigned HashTopologia16(int nTris, const unsigned short* faces, int nIndices);

private:
    std::vector<unsigned char> blob;   // los registros de celda, tal como viajan
    std::vector<unsigned>      tabla;  // offset de cada registro DENTRO de blob

    bool DecodificarRegistro(int celda, std::vector<unsigned>& lista, bool aplicarSobreLista) const;
    bool AplicarDelta(int celda, std::vector<unsigned>& lista, bool invertir) const;
};

#endif
