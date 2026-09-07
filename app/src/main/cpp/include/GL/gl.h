#pragma once
#ifndef W3D_ANDROID_GL_SHIM_H
#define W3D_ANDROID_GL_SHIM_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif

// Fixed-pipeline light definitions for compatibility
#ifndef GL_LIGHT0
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007

#define GL_SPOT_EXPONENT 0x1205
#define GL_SPOT_CUTOFF 0x1206
#define GL_CONSTANT_ATTENUATION 0x1207
#define GL_LINEAR_ATTENUATION 0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_POSITION 0x1203
#define GL_SPOT_DIRECTION 0x1204

static inline void glLightfv(GLenum, GLenum, const GLfloat*) {}
static inline void glLightf(GLenum, GLenum, GLfloat) {}
#endif

#endif // W3D_ANDROID_GL_SHIM_H
