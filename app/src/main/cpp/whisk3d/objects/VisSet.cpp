// ============================================================================
//  VisSet.cpp — ver VisSet.h y el spec CONGELADO en formato/w3dvis.md.
//
//  Decision de representacion: el blob interno es CANONICO (los registros de
//  celda concatenados en orden de celda, tabla[] = offset de cada uno DENTRO
//  del blob). CargarW3dvis PARSEA cada registro al cargar (eso valida el
//  archivo entero una sola vez y mide el largo real de cada registro, que el
//  formato no declara: los registros son auto-terminados); Guardar escribe
//  header + blob + tabla, asi que un archivo escrito por el motor re-carga
//  byte a byte identico.
// ============================================================================
#include "VisSet.h"
#include <string.h> // memcpy

// ---------------------------------------------------------------------------
//  primitivas de encoding (spec: varint LEB128 + zigzag + palabra u32 LE)
// ---------------------------------------------------------------------------
namespace {

struct Cur {                     // cursor de LECTURA con chequeo de rango
    const unsigned char* p;
    size_t n;                    // tope
    size_t i;                    // posicion
    bool ok;
    Cur(const unsigned char* p_, size_t n_, size_t i_) : p(p_), n(n_), i(i_), ok(true) {}
};

unsigned LeerVarint(Cur& c) {
    unsigned v = 0; int shift = 0;
    while (c.ok) {
        if (c.i >= c.n || shift > 28) { c.ok = false; break; }
        unsigned char b = c.p[c.i++];
        v |= (unsigned)(b & 0x7F) << shift;
        if (!(b & 0x80)) return v;
        shift += 7;
    }
    return 0;
}

unsigned LeerU32(Cur& c) {       // palabra cruda u32 LE
    if (!c.ok || c.i + 4 > c.n) { c.ok = false; return 0; }
    unsigned v = (unsigned)c.p[c.i] | ((unsigned)c.p[c.i+1] << 8)
               | ((unsigned)c.p[c.i+2] << 16) | ((unsigned)c.p[c.i+3] << 24);
    c.i += 4;
    return v;
}

int DeZigzag(unsigned v) { return (int)(v >> 1) ^ -(int)(v & 1); }
unsigned Zigzag(int n)   { return ((unsigned)n << 1) ^ (unsigned)(n >> 31); }

void PonerVarint(std::vector<unsigned char>& out, unsigned v) {
    while (v >= 0x80) { out.push_back((unsigned char)(v | 0x80)); v >>= 7; }
    out.push_back((unsigned char)v);
}
void PonerU32(std::vector<unsigned char>& out, unsigned v) {
    out.push_back((unsigned char)(v & 0xFF));
    out.push_back((unsigned char)((v >> 8) & 0xFF));
    out.push_back((unsigned char)((v >> 16) & 0xFF));
    out.push_back((unsigned char)((v >> 24) & 0xFF));
}

// opcodes del registro DELTA (spec)
enum { OpFin = 0, OpAdd = 1, OpRemove = 2, OpMove = 3 };

// una op parseada (para poder aplicarlas al reves al invertir)
struct OpDelta { unsigned char op; unsigned a, b; };

// parsea el registro de una celda y AVANZA el cursor hasta su fin.
// esClave/nWords/hayOrden describen que tipo de registro se espera.
// Si 'lista' != NULL, ademas lo decodifica (bitset -> ascendente; clave ->
// la lista; delta -> deja las ops en 'ops' para que el caller las aplique).
bool ParsearRegistro(Cur& c, bool esBitset, bool esClave, int nTris, bool hayOrden,
                     std::vector<unsigned>* lista, std::vector<OpDelta>* ops,
                     std::vector<unsigned>* orden) {
    if (esBitset) {
        const unsigned nWords = (unsigned)((nTris + 31) / 32);
        unsigned hechas = 0;
        while (hechas < nWords && c.ok) {
            unsigned ctrl = LeerVarint(c);
            unsigned len = ctrl >> 1;
            if (!c.ok || len == 0 || hechas + len > nWords) { c.ok = false; break; }
            if (ctrl & 1) {                       // REPETIDA: 1 palabra, len veces
                unsigned w = LeerU32(c);
                if (lista && w) for (unsigned k = 0; k < len; k++) {
                    unsigned base = (hechas + k) * 32;
                    for (int b = 0; b < 32; b++)
                        if ((w >> b) & 1u) { unsigned t = base + (unsigned)b; if ((int)t < nTris) lista->push_back(t); }
                }
                hechas += len;
            } else {                              // LITERAL: len palabras crudas
                for (unsigned k = 0; k < len && c.ok; k++) {
                    unsigned w = LeerU32(c);
                    if (lista && w) {
                        unsigned base = (hechas + k) * 32;
                        for (int b = 0; b < 32; b++)
                            if ((w >> b) & 1u) { unsigned t = base + (unsigned)b; if ((int)t < nTris) lista->push_back(t); }
                    }
                }
                hechas += len;
            }
        }
        if (c.ok && hayOrden) {                   // stream opcional de orden de transparentes
            unsigned nOrd = LeerVarint(c);
            if (nOrd > (unsigned)nTris) { c.ok = false; }
            for (unsigned k = 0; k < nOrd && c.ok; k++) {
                unsigned id = LeerVarint(c);
                if (orden) orden->push_back(id);
            }
        }
        return c.ok;
    }
    if (esClave) {                                // lista completa, zigzag delta-encoded
        unsigned n = LeerVarint(c);
        if (!c.ok || n > (unsigned)nTris) { c.ok = false; return false; }
        int prev = 0;
        for (unsigned k = 0; k < n && c.ok; k++) {
            int d = DeZigzag(LeerVarint(c));
            int id = (k == 0) ? d : prev + d;
            prev = id;
            if (id < 0 || id >= nTris) { c.ok = false; break; }
            if (lista) lista->push_back((unsigned)id);
        }
        return c.ok;
    }
    // registro DELTA: ops hasta FIN
    unsigned tope = 0;
    while (c.ok) {
        if (c.i >= c.n || ++tope > (unsigned)nTris * 4u + 16u) { c.ok = false; break; }
        unsigned char op = c.p[c.i++];
        if (op == OpFin) break;
        if (op > OpMove) { c.ok = false; break; }
        OpDelta o; o.op = op;
        o.a = LeerVarint(c);
        o.b = LeerVarint(c);
        if (ops && c.ok) ops->push_back(o);
    }
    return c.ok;
}

// aplica UNA op (o su inversa) sobre la lista. false = stream corrupto
// (posicion fuera de rango o REMOVE cuyo id no coincide).
bool AplicarOp(std::vector<unsigned>& lista, const OpDelta& o, bool invertir) {
    unsigned char op = o.op;
    unsigned a = o.a, b = o.b;
    if (invertir) {
        if (op == OpAdd) op = OpRemove;           // ADD(pos,id) <-> REMOVE(pos,id)
        else if (op == OpRemove) op = OpAdd;
        else { unsigned t = a; a = b; b = t; }    // MOVE(desde,hacia) <-> MOVE(hacia,desde)
    }
    if (op == OpAdd) {
        if (a > lista.size()) return false;
        lista.insert(lista.begin() + a, b);
        return true;
    }
    if (op == OpRemove) {
        if (a >= lista.size() || lista[a] != b) return false;
        lista.erase(lista.begin() + a);
        return true;
    }
    // MOVE: sacar de 'a', insertar en 'b' (b medido en la lista YA sin el)
    if (a >= lista.size()) return false;
    unsigned id = lista[a];
    lista.erase(lista.begin() + a);
    if (b > lista.size()) return false;
    lista.insert(lista.begin() + b, id);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
void VisSet::Limpiar() {
    nTris = 0; nCeldas = 0; modo = ModoBitset;
    ordenLejosCerca = false; hayOrden = false;
    K = 0; hashTopo = 0;
    blob.clear(); tabla.clear();
}

// ---------------------------------------------------------------------------
//  CARGA del binario (valida TODO el archivo una vez y canonicaliza el blob)
// ---------------------------------------------------------------------------
bool VisSet::CargarW3dvis(const unsigned char* bytes, size_t n, std::string* err) {
    Limpiar();
    if (!bytes || n < 32) { if (err) *err = "archivo demasiado corto"; return false; }
    if (bytes[0] != 'W' || bytes[1] != '3' || bytes[2] != 'V' || bytes[3] != 'S') {
        if (err) *err = "magic invalido (no es un .w3dvis)"; return false;
    }
    Cur h(bytes, n, 4);
    unsigned version = (unsigned)bytes[4] | ((unsigned)bytes[5] << 8);
    unsigned modoF   = (unsigned)bytes[6] | ((unsigned)bytes[7] << 8);
    h.i = 8;
    unsigned flags    = LeerU32(h);
    unsigned nTrisF   = LeerU32(h);
    unsigned nCeldasF = LeerU32(h);
    unsigned KF       = LeerU32(h);
    unsigned offTabla = LeerU32(h);
    unsigned hashF    = LeerU32(h);
    if (version != 1) { if (err) *err = "version desconocida"; return false; }
    if (modoF > 1)    { if (err) *err = "modo desconocido"; return false; }
    if (nTrisF == 0 || nCeldasF == 0 || nTrisF > 0x7FFFFFFFu) { if (err) *err = "header vacio"; return false; }
    if (nCeldasF > 1000000u) { if (err) *err = "nCeldas absurdo"; return false; }
    if (modoF == 1 && KF < 1) { if (err) *err = "playlist con K < 1"; return false; }
    if ((size_t)offTabla + (size_t)nCeldasF * 4 > n) { if (err) *err = "tabla fuera del archivo"; return false; }

    nTris = (int)nTrisF; nCeldas = (int)nCeldasF; modo = (int)modoF;
    ordenLejosCerca = (flags & 1u) != 0;
    hayOrden        = (flags & 2u) != 0 && modo == ModoBitset;
    K = (modo == ModoPlaylist) ? (int)KF : 0;
    hashTopo = hashF;

    tabla.resize((size_t)nCeldas);
    blob.clear();
    blob.reserve(n > 32 ? n - 32 : 0);
    for (int c = 0; c < nCeldas; c++) {
        Cur t(bytes, n, (size_t)offTabla + (size_t)c * 4);
        unsigned off = LeerU32(t);
        if (!t.ok || off < 32 || off >= n) { Limpiar(); if (err) *err = "offset de celda invalido"; return false; }
        Cur r(bytes, n, off);
        bool esClave = (modo == ModoPlaylist) && (c % K == 0);
        if (!ParsearRegistro(r, modo == ModoBitset, esClave, nTris, hayOrden, 0, 0, 0) || !r.ok) {
            Limpiar(); if (err) *err = "registro de celda corrupto"; return false;
        }
        tabla[c] = (unsigned)blob.size();
        blob.insert(blob.end(), bytes + off, bytes + r.i);   // canonicaliza
    }
    return true;
}

// ---------------------------------------------------------------------------
//  construccion desde listas (legacy .pvs.json -> bitset; horneador/pruebas
//  -> playlist)
// ---------------------------------------------------------------------------
bool VisSet::DesdeListas(const std::vector< std::vector<int> >& listas, int nTrisMalla) {
    Limpiar();
    if (listas.empty() || nTrisMalla <= 0) return false;
    nTris = nTrisMalla; nCeldas = (int)listas.size(); modo = ModoBitset;
    const unsigned nWords = (unsigned)((nTris + 31) / 32);
    std::vector<unsigned> words(nWords);
    tabla.resize((size_t)nCeldas);
    for (int c = 0; c < nCeldas; c++) {
        for (unsigned w = 0; w < nWords; w++) words[w] = 0;
        const std::vector<int>& l = listas[c];
        for (size_t k = 0; k < l.size(); k++) {
            int t = l[k];
            if (t >= 0 && t < nTris) words[t / 32] |= 1u << (t % 32);
        }
        tabla[c] = (unsigned)blob.size();
        // RLE: corridas de palabras repetidas -> (len<<1)|1 + palabra; el resto
        // literales agrupadas -> (len<<1) + palabras crudas
        unsigned i = 0;
        while (i < nWords) {
            unsigned j = i + 1;
            while (j < nWords && words[j] == words[i]) j++;
            unsigned run = j - i;
            if (run >= 2) {
                PonerVarint(blob, (run << 1) | 1u);
                PonerU32(blob, words[i]);
                i = j;
            } else {
                unsigned ini = i;                  // juntar literales hasta la proxima corrida
                while (j < nWords) {
                    unsigned j2 = j + 1;
                    while (j2 < nWords && words[j2] == words[j]) j2++;
                    if (j2 - j >= 2) break;
                    j = j2;
                }
                PonerVarint(blob, (j - ini) << 1);
                for (unsigned k = ini; k < j; k++) PonerU32(blob, words[k]);
                i = j;
            }
        }
    }
    return true;
}

bool VisSet::DesdeListasPlaylist(const std::vector< std::vector<int> >& listas,
                                 int nTrisMalla, int pasoK, bool ordenado) {
    Limpiar();
    if (listas.empty() || nTrisMalla <= 0 || pasoK < 1) return false;
    nTris = nTrisMalla; nCeldas = (int)listas.size(); modo = ModoPlaylist;
    K = pasoK; ordenLejosCerca = ordenado;
    tabla.resize((size_t)nCeldas);
    std::vector<unsigned> prev, cur, work;
    for (int c = 0; c < nCeldas; c++) {
        cur.clear();
        for (size_t k = 0; k < listas[c].size(); k++) {
            int t = listas[c][k];
            if (t >= 0 && t < nTris) cur.push_back((unsigned)t);
        }
        tabla[c] = (unsigned)blob.size();
        if (c % K == 0) {
            // celda CLAVE: lista completa zigzag delta-encoded
            PonerVarint(blob, (unsigned)cur.size());
            int prevId = 0;
            for (size_t k = 0; k < cur.size(); k++) {
                int id = (int)cur[k];
                PonerVarint(blob, Zigzag(k == 0 ? id : id - prevId));
                prevId = id;
            }
        } else {
            // registro DELTA: diff posicional prev -> cur (correcto; minimal no
            // hace falta: esto lo usan el horneador y las pruebas)
            work = prev;
            // 1) REMOVE de atras hacia adelante de todo lo que no sigue
            std::vector<bool> enCur((size_t)nTris, false);
            for (size_t k = 0; k < cur.size(); k++) enCur[cur[k]] = true;
            for (int p = (int)work.size() - 1; p >= 0; p--) {
                if (!enCur[work[p]]) {
                    blob.push_back((unsigned char)OpRemove);
                    PonerVarint(blob, (unsigned)p);
                    PonerVarint(blob, work[p]);
                    work.erase(work.begin() + p);
                }
            }
            // 2) MOVE/ADD hasta que work == cur
            for (size_t i = 0; i < cur.size(); i++) {
                if (i < work.size() && work[i] == cur[i]) continue;
                size_t j = i + 1; bool esta = false;
                for (; j < work.size(); j++) if (work[j] == cur[i]) { esta = true; break; }
                if (esta) {
                    unsigned id = work[j];
                    work.erase(work.begin() + j);
                    blob.push_back((unsigned char)OpMove);
                    PonerVarint(blob, (unsigned)j);
                    PonerVarint(blob, (unsigned)i);
                    work.insert(work.begin() + i, id);
                } else {
                    blob.push_back((unsigned char)OpAdd);
                    PonerVarint(blob, (unsigned)i);
                    PonerVarint(blob, cur[i]);
                    work.insert(work.begin() + i, cur[i]);
                }
            }
            blob.push_back((unsigned char)OpFin);
        }
        prev.swap(cur);
    }
    return true;
}

// ---------------------------------------------------------------------------
bool VisSet::Guardar(std::vector<unsigned char>& out) const {
    out.clear();
    if (!Valido()) return false;
    out.push_back('W'); out.push_back('3'); out.push_back('V'); out.push_back('S');
    out.push_back(1); out.push_back(0);                       // version u16
    out.push_back((unsigned char)modo); out.push_back(0);     // modo u16
    unsigned flags = (ordenLejosCerca ? 1u : 0u) | (hayOrden ? 2u : 0u);
    PonerU32(out, flags);
    PonerU32(out, (unsigned)nTris);
    PonerU32(out, (unsigned)nCeldas);
    PonerU32(out, (unsigned)((modo == ModoPlaylist) ? K : 0));
    PonerU32(out, 32u + (unsigned)blob.size());               // offTabla: tabla tras el blob
    PonerU32(out, hashTopo);
    out.insert(out.end(), blob.begin(), blob.end());
    for (int c = 0; c < nCeldas; c++) PonerU32(out, 32u + tabla[c]);
    return true;
}

// ---------------------------------------------------------------------------
//  consumo
// ---------------------------------------------------------------------------
bool VisSet::Decodificar(int celda, std::vector<unsigned>& lista) const {
    lista.clear();
    if (!Valido() || celda < 0 || celda >= nCeldas) return false;
    if (modo == ModoBitset) {
        Cur r(&blob[0], blob.size(), tabla[celda]);
        return ParsearRegistro(r, true, false, nTris, hayOrden, &lista, 0, 0);
    }
    // playlist: desde la celda-clave anterior, aplicando los deltas intermedios
    int kc = celda - (celda % K);
    {
        Cur r(&blob[0], blob.size(), tabla[kc]);
        if (!ParsearRegistro(r, false, true, nTris, false, &lista, 0, 0)) return false;
    }
    for (int c = kc + 1; c <= celda; c++)
        if (!AplicarDelta(c, lista, false)) return false;
    return true;
}

bool VisSet::AplicarDelta(int celda, std::vector<unsigned>& lista, bool invertir) const {
    if (celda <= 0 || celda >= nCeldas || (celda % K) == 0) return false;
    std::vector<OpDelta> ops;
    Cur r(&blob[0], blob.size(), tabla[celda]);
    if (!ParsearRegistro(r, false, false, nTris, false, 0, &ops, 0)) return false;
    if (!invertir) {
        for (size_t k = 0; k < ops.size(); k++)
            if (!AplicarOp(lista, ops[k], false)) return false;
    } else {
        for (size_t k = ops.size(); k > 0; k--)
            if (!AplicarOp(lista, ops[k - 1], true)) return false;
    }
    return true;
}

bool VisSet::Delta(int desde, int hacia, std::vector<unsigned>& lista) const {
    if (!Valido() || desde < 0 || desde >= nCeldas || hacia < 0 || hacia >= nCeldas)
        return false;
    if (desde == hacia) return true;
    if (modo == ModoPlaylist && hacia == desde + 1) {
        if (hacia % K == 0) return Decodificar(hacia, lista);   // clave: lista completa
        return AplicarDelta(hacia, lista, false);
    }
    if (modo == ModoPlaylist && hacia == desde - 1) {
        if (desde % K == 0) return Decodificar(hacia, lista);   // una clave no se invierte
        return AplicarDelta(desde, lista, true);                // registro de 'desde' al reves
    }
    return Decodificar(hacia, lista);                           // salto largo / bitset
}

bool VisSet::OrdenTransparentes(int celda, std::vector<unsigned>& orden) const {
    orden.clear();
    if (!Valido() || modo != ModoBitset || !hayOrden) return false;
    if (celda < 0 || celda >= nCeldas) return false;
    Cur r(&blob[0], blob.size(), tabla[celda]);
    return ParsearRegistro(r, true, false, nTris, true, 0, 0, &orden);
}

// ---------------------------------------------------------------------------
//  hash de topologia (contrato del spec: FNV-1a de nTris + faces como u32 LE)
// ---------------------------------------------------------------------------
static unsigned FnvU32(unsigned h, unsigned x) {
    for (int b = 0; b < 4; b++) { h = (h ^ ((x >> (b * 8)) & 0xFF)) * 16777619u; }
    return h;
}
unsigned VisSet::HashTopologia(int nTris, const unsigned int* faces, int nIndices) {
    unsigned h = 2166136261u;
    h = FnvU32(h, (unsigned)nTris);
    for (int i = 0; i < nIndices; i++) h = FnvU32(h, faces[i]);
    return h;
}
unsigned VisSet::HashTopologia16(int nTris, const unsigned short* faces, int nIndices) {
    unsigned h = 2166136261u;
    h = FnvU32(h, (unsigned)nTris);
    for (int i = 0; i < nIndices; i++) h = FnvU32(h, (unsigned)faces[i]);
    return h;
}
