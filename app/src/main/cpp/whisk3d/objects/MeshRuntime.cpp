// ============================================================================
//  MeshRuntime.cpp - Runtime implementation of editor stubs and bounding
//  methods for Mesh in game runtime builds.
// ============================================================================
#include "objects/Mesh.h"
#include "math/Matrix4.h"

void Mesh::LiberarCapas(bool incluirGrupos) {
    for (size_t i = 0; i < uvMaps.size(); i++) delete uvMaps[i];
    uvMaps.clear();
    uvMapActivo = -1;

    for (size_t i = 0; i < colorLayers.size(); i++) delete colorLayers[i];
    colorLayers.clear();
    colorActivo = -1;

    if (incluirGrupos) {
        for (size_t i = 0; i < vertexGroups.size(); i++) delete vertexGroups[i];
        vertexGroups.clear();
        grupoActivo = -1;
    }

    for (size_t i = 0; i < uvGroups.size(); i++) delete uvGroups[i];
    uvGroups.clear();
    uvGrupoActivo = -1;
}

void Mesh::InvalidarEdit() {}
void Mesh::LiberarModificadores() {}
void Mesh::LiberarMallaModificada() {}
void Mesh::RenderEditOverlay() {}
void Mesh::EnsureEdit() {}

void Mesh::EditSeleccionarTodo(bool /*sel*/) {}
void Mesh::EditInvertir() {}

Vector3 Mesh::PuntoFoco() const {
    Matrix4 W;
    GetWorldMatrix(W);
    return W * centroGeom;
}

float Mesh::EscalarRadioLocal(const Vector3& /*cLocal*/, float rLocal) const {
    Matrix4 W;
    GetWorldMatrix(W);
    float sx = Vector3(W.m[0], W.m[1], W.m[2]).Length();
    float sy = Vector3(W.m[4], W.m[5], W.m[6]).Length();
    float sz = Vector3(W.m[8], W.m[9], W.m[10]).Length();
    float maxS = sx > sy ? sx : sy;
    if (sz > maxS) maxS = sz;
    return rLocal * maxS;
}

float Mesh::RadioFoco() const {
    return EscalarRadioLocal(centroGeom, radioGeom);
}

// Stub for editor repaint event
void W3dScriptRedibujar(void) {}
