// macOS host-harness shim: the core is compiled with IOS=1, so ygl.h includes
// <OpenGLES/ES3/gl.h> for the GL typedefs used in its declarations. Only the
// types are provided; no GL entry point is ever called by this build.
#ifndef PA_SATURN_HOSTSHIM_GL_H
#define PA_SATURN_HOSTSHIM_GL_H
#include <stddef.h>
#include <stdint.h>
typedef void GLvoid;
typedef char GLchar;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int8_t GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef uint8_t GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef int32_t GLfixed;
typedef intptr_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef struct __GLsync *GLsync;
typedef unsigned short GLhalf;
typedef void (*PFNGLPATCHPARAMETERIPROC)(GLenum pname, GLint value);
typedef void (*PFNGLMEMORYBARRIERPROC)(GLbitfield barriers);
#endif
