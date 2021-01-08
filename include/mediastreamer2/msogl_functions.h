/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef msogl_functions_h
#define msogl_functions_h

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#if TARGET_OS_IPHONE
	#include <EGL/egl.h>
	#include <OpenGLES/ES2/gl.h>
	#include <OpenGLES/ES2/glext.h>
#elif TARGET_OS_MAC
	#include <EGL/egl.h>
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl.h>
#elif __ANDROID__
	#include <EGL/egl.h>
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
#elif _WIN32
    #define GL_GLEXT_PROTOTYPES
    #include <EGL/egl.h>
    #include <EGL/eglext.h>
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
#ifndef QOPENGLFUNCTIONS_H // glew is already included by QT.
	#include <GL/glew.h>
#endif
	#include <EGL/egl.h>
#endif

// =============================================================================

typedef void (GL_APIENTRYP resolveGlActiveTexture)(GLenum texture);
typedef void (GL_APIENTRYP resolveGlAttachShader)(GLuint program, GLuint shader);
typedef void (GL_APIENTRYP resolveGlBindAttribLocation)(GLuint program, GLuint index, const char *name);
typedef void (GL_APIENTRYP resolveGlBindBuffer)(GLenum target, GLuint buffer);
typedef void (GL_APIENTRYP resolveGlBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (GL_APIENTRYP resolveGlBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void (GL_APIENTRYP resolveGlBindTexture)(GLenum target, GLuint texture);
typedef void (GL_APIENTRYP resolveGlBlendColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (GL_APIENTRYP resolveGlBlendEquation)(GLenum mode);
typedef void (GL_APIENTRYP resolveGlBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
typedef void (GL_APIENTRYP resolveGlBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void (GL_APIENTRYP resolveGlBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GL_APIENTRYP resolveGlBufferData)(GLenum target, GLsizei *size, const void *data, GLenum usage);
typedef void (GL_APIENTRYP resolveGlBufferSubData)(GLenum target, GLint *offset, GLsizei *size, const void *data);
typedef GLenum (GL_APIENTRYP resolveGlCheckFramebufferStatus)(GLenum target);
typedef void (GL_APIENTRYP resolveGlClear)(GLbitfield mask);
typedef void (GL_APIENTRYP resolveGlClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (GL_APIENTRYP resolveGlClearDepthf)(GLclampf depth);
typedef void (GL_APIENTRYP resolveGlClearStencil)(GLint s);
typedef void (GL_APIENTRYP resolveGlColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (GL_APIENTRYP resolveGlCompileShader)(GLuint shader);
typedef void (GL_APIENTRYP resolveGlCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP resolveGlCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP resolveGlCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (GL_APIENTRYP resolveGlCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLuint (GL_APIENTRYP resolveGlCreateProgram)(void);
typedef GLuint (GL_APIENTRYP resolveGlCreateShader)(GLenum type);
typedef void (GL_APIENTRYP resolveGlCullFace)(GLenum mode);
typedef void (GL_APIENTRYP resolveGlDeleteBuffers)(GLsizei n, const GLuint *buffers);
typedef void (GL_APIENTRYP resolveGlDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
typedef void (GL_APIENTRYP resolveGlDeleteProgram)(GLuint program);
typedef void (GL_APIENTRYP resolveGlDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
typedef void (GL_APIENTRYP resolveGlDeleteShader)(GLuint shader);
typedef void (GL_APIENTRYP resolveGlDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void (GL_APIENTRYP resolveGlDepthFunc)(GLenum func);
typedef void (GL_APIENTRYP resolveGlDepthMask)(GLboolean flag);
typedef void (GL_APIENTRYP resolveGlDepthRangef)(GLclampf zNear, GLclampf zFar);
typedef void (GL_APIENTRYP resolveGlDetachShader)(GLuint program, GLuint shader);
typedef void (GL_APIENTRYP resolveGlDisable)(GLenum cap);
typedef void (GL_APIENTRYP resolveGlDisableVertexAttribArray)(GLuint index);
typedef void (GL_APIENTRYP resolveGlDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (GL_APIENTRYP resolveGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (GL_APIENTRYP resolveGlEnable)(GLenum cap);
typedef void (GL_APIENTRYP resolveGlEnableVertexAttribArray)(GLuint index);
typedef void (GL_APIENTRYP resolveGlFinish)(void);
typedef void (GL_APIENTRYP resolveGlFlush)(void);
typedef void (GL_APIENTRYP resolveGlFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GL_APIENTRYP resolveGlFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GL_APIENTRYP resolveGlFrontFace)(GLenum mode);
typedef void (GL_APIENTRYP resolveGlGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (GL_APIENTRYP resolveGlGenFramebuffers)(GLsizei n, GLuint *framebuffers);
typedef void (GL_APIENTRYP resolveGlGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
typedef void (GL_APIENTRYP resolveGlGenTextures)(GLsizei n, GLuint *textures);
typedef void (GL_APIENTRYP resolveGlGenerateMipmap)(GLenum target);
typedef void (GL_APIENTRYP resolveGlGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name);
typedef void (GL_APIENTRYP resolveGlGetActiveUniform)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name);
typedef void (GL_APIENTRYP resolveGlGetAttachedShaders)(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
typedef GLint (GL_APIENTRYP resolveGlGetAttribLocation)(GLuint program, const char *name);
typedef void (GL_APIENTRYP resolveGlGetBooleanv)(GLenum pname, GLboolean *params);
typedef void (GL_APIENTRYP resolveGlGetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLenum (GL_APIENTRYP resolveGlGetError)(void);
typedef void (GL_APIENTRYP resolveGlGetFloatv)(GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP resolveGlGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP resolveGlGetIntegerv)(GLenum pname, GLint *params);
typedef void (GL_APIENTRYP resolveGlGetProgramInfoLog)(GLuint program, GLsizei bufsize, GLsizei *length, char *infolog);
typedef void (GL_APIENTRYP resolveGlGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP resolveGlGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP resolveGlGetShaderInfoLog)(GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog);
typedef void (GL_APIENTRYP resolveGlGetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (GL_APIENTRYP resolveGlGetShaderSource)(GLuint shader, GLsizei bufsize, GLsizei *length, char *source);
typedef void (GL_APIENTRYP resolveGlGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef const GLubyte *(GL_APIENTRYP resolveGlGetString)(GLenum name);
typedef void (GL_APIENTRYP resolveGlGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP resolveGlGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLint (GL_APIENTRYP resolveGlGetUniformLocation)(GLuint program, const char *name);
typedef void (GL_APIENTRYP resolveGlGetUniformfv)(GLuint program, GLint location, GLfloat *params);
typedef void (GL_APIENTRYP resolveGlGetUniformiv)(GLuint program, GLint location, GLint *params);
typedef void (GL_APIENTRYP resolveGlGetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
typedef void (GL_APIENTRYP resolveGlGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP resolveGlGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP resolveGlHint)(GLenum target, GLenum mode);
typedef GLboolean (GL_APIENTRYP resolveGlIsBuffer)(GLuint buffer);
typedef GLboolean (GL_APIENTRYP resolveGlIsEnabled)(GLenum cap);
typedef GLboolean (GL_APIENTRYP resolveGlIsFramebuffer)(GLuint framebuffer);
typedef GLboolean (GL_APIENTRYP resolveGlIsProgram)(GLuint program);
typedef GLboolean (GL_APIENTRYP resolveGlIsRenderbuffer)(GLuint renderbuffer);
typedef GLboolean (GL_APIENTRYP resolveGlIsShader)(GLuint shader);
typedef GLboolean (GL_APIENTRYP resolveGlIsTexture)(GLuint texture);
typedef void (GL_APIENTRYP resolveGlLineWidth)(GLfloat width);
typedef void (GL_APIENTRYP resolveGlLinkProgram)(GLuint program);
typedef void (GL_APIENTRYP resolveGlPixelStorei)(GLenum pname, GLint param);
typedef void (GL_APIENTRYP resolveGlPolygonOffset)(GLfloat factor, GLfloat units);
typedef void (GL_APIENTRYP resolveGlReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef void (GL_APIENTRYP resolveGlReleaseShaderCompiler)(void);
typedef void (GL_APIENTRYP resolveGlRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP resolveGlSampleCoverage)(GLclampf value, GLboolean invert);
typedef void (GL_APIENTRYP resolveGlScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP resolveGlShaderBinary)(GLint n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLint length);

#ifdef MS2_USE_OLD_OPENGL_PROTOTYPE
typedef void (GL_APIENTRYP resolveGlShaderSource)(GLuint shader, GLsizei count, const char **string, const GLint *length);
#else
typedef void (GL_APIENTRYP resolveGlShaderSource)(GLuint shader, GLsizei count, const char *const*string, const GLint *length);
#endif

typedef void (GL_APIENTRYP resolveGlStencilFunc)(GLenum func, GLint ref, GLuint mask);
typedef void (GL_APIENTRYP resolveGlStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (GL_APIENTRYP resolveGlStencilMask)(GLuint mask);
typedef void (GL_APIENTRYP resolveGlStencilMaskSeparate)(GLenum face, GLuint mask);
typedef void (GL_APIENTRYP resolveGlStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (GL_APIENTRYP resolveGlStencilOpSeparate)(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
typedef void (GL_APIENTRYP resolveGlTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GL_APIENTRYP resolveGlTexParameterf)(GLenum target, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP resolveGlTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
typedef void (GL_APIENTRYP resolveGlTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (GL_APIENTRYP resolveGlTexParameteriv)(GLenum target, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP resolveGlTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GL_APIENTRYP resolveGlUniform1f)(GLint location, GLfloat x);
typedef void (GL_APIENTRYP resolveGlUniform1fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (GL_APIENTRYP resolveGlUniform1i)(GLint location, GLint x);
typedef void (GL_APIENTRYP resolveGlUniform1iv)(GLint location, GLsizei count, const GLint *v);
typedef void (GL_APIENTRYP resolveGlUniform2f)(GLint location, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP resolveGlUniform2fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (GL_APIENTRYP resolveGlUniform2i)(GLint location, GLint x, GLint y);
typedef void (GL_APIENTRYP resolveGlUniform2iv)(GLint location, GLsizei count, const GLint *v);
typedef void (GL_APIENTRYP resolveGlUniform3f)(GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void (GL_APIENTRYP resolveGlUniform3fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (GL_APIENTRYP resolveGlUniform3i)(GLint location, GLint x, GLint y, GLint z);
typedef void (GL_APIENTRYP resolveGlUniform3iv)(GLint location, GLsizei count, const GLint *v);
typedef void (GL_APIENTRYP resolveGlUniform4f)(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GL_APIENTRYP resolveGlUniform4fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (GL_APIENTRYP resolveGlUniform4i)(GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void (GL_APIENTRYP resolveGlUniform4iv)(GLint location, GLsizei count, const GLint *v);
typedef void (GL_APIENTRYP resolveGlUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP resolveGlUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP resolveGlUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP resolveGlUseProgram)(GLuint program);
typedef void (GL_APIENTRYP resolveGlValidateProgram)(GLuint program);
typedef void (GL_APIENTRYP resolveGlVertexAttrib1f)(GLuint indx, GLfloat x);
typedef void (GL_APIENTRYP resolveGlVertexAttrib1fv)(GLuint indx, const GLfloat *values);
typedef void (GL_APIENTRYP resolveGlVertexAttrib2f)(GLuint indx, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP resolveGlVertexAttrib2fv)(GLuint indx, const GLfloat *values);
typedef void (GL_APIENTRYP resolveGlVertexAttrib3f)(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
typedef void (GL_APIENTRYP resolveGlVertexAttrib3fv)(GLuint indx, const GLfloat *values);
typedef void (GL_APIENTRYP resolveGlVertexAttrib4f)(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GL_APIENTRYP resolveGlVertexAttrib4fv)(GLuint indx, const GLfloat *values);
typedef void (GL_APIENTRYP resolveGlVertexAttribPointer)(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *ptr);
typedef void (GL_APIENTRYP resolveGlViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

// -----------------------------------------------------------------------------

typedef void *(GL_APIENTRYP resolveEGLGetProcAddress)(char const * procname);
typedef EGLenum (GL_APIENTRYP resolveEGLQueryAPI)(void);
typedef EGLBoolean (GL_APIENTRYP resolveEGLBindAPI)(EGLenum api);
typedef char const *(GL_APIENTRYP resolveEGLQueryString)(EGLDisplay display, EGLint name);
typedef EGLDisplay (GL_APIENTRYP resolveEGLGetPlatformDisplayEXT)(EGLenum platform, void *native_display, const EGLint *attrib_list);
typedef EGLDisplay (GL_APIENTRYP resolveEGLGetCurrentDisplay)(void);
typedef EGLContext (GL_APIENTRYP resolveEGLGetCurrentContext)(void);
typedef EGLSurface (GL_APIENTRYP resolveEGLGetCurrentSurface)(void);
typedef EGLBoolean (GL_APIENTRYP resolveEGLInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
typedef EGLBoolean (GL_APIENTRYP resolveEGLChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
typedef EGLContext (GL_APIENTRYP resolveEGLCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
typedef EGLBoolean (GL_APIENTRYP resolveEGLMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef EGLint (GL_APIENTRYP resolveEGLGetError)(void);
typedef EGLBoolean (GL_APIENTRYP resolveEGLSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (GL_APIENTRYP resolveEGLQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
typedef EGLBoolean (GL_APIENTRYP resolveEGLDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (GL_APIENTRYP resolveEGLDestroyContext)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean (GL_APIENTRYP resolveEGLTerminate)(EGLDisplay dpy);



typedef EGLSurface (GL_APIENTRYP resolveEGLCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);



// -----------------------------------------------------------------------------
struct OpenGlFunctions {
	resolveGlActiveTexture glActiveTexture;
	resolveGlAttachShader glAttachShader;
	resolveGlBindAttribLocation glBindAttribLocation;
	// resolveGlBindBuffer glBindBuffer;
	// resolveGlBindFramebuffer glBindFramebuffer;
	// resolveGlBindRenderbuffer glBindRenderbuffer;
	resolveGlBindTexture glBindTexture;
	// resolveGlBlendColor glBlendColor;
	// resolveGlBlendEquation glBlendEquation;
	// resolveGlBlendEquationSeparate glBlendEquationSeparate;
	// resolveGlBlendFunc glBlendFunc;
	// resolveGlBlendFuncSeparate glBlendFuncSeparate;
	// resolveGlBufferData glBufferData;
	// resolveGlBufferSubData glBufferSubData;
	// resolveGlCheckFramebufferStatus glCheckFramebufferStatus;
	resolveGlClear glClear;
	resolveGlClearColor glClearColor;
	// resolveGlClearDepthf glClearDepthf;
	// resolveGlClearStencil glClearStencil;
	// resolveGlColorMask glColorMask;
	resolveGlCompileShader glCompileShader;
	// resolveGlCompressedTexImage2D glCompressedTexImage2D;
	// resolveGlCompressedTexSubImage2D glCompressedTexSubImage2D;
	// resolveGlCopyTexImage2D glCopyTexImage2D;
	// resolveGlCopyTexSubImage2D glCopyTexSubImage2D;
	resolveGlCreateProgram glCreateProgram;
	resolveGlCreateShader glCreateShader;
	// resolveGlCullFace glCullFace;
	// resolveGlDeleteBuffers glDeleteBuffers;
	// resolveGlDeleteFramebuffers glDeleteFramebuffers;
	resolveGlDeleteProgram glDeleteProgram;
	//resolveGlDeleteRenderbuffers glDeleteRenderbuffers;
	resolveGlDeleteShader glDeleteShader;
	resolveGlDeleteTextures glDeleteTextures;
	// resolveGlDepthFunc glDepthFunc;
	// resolveGlDepthMask glDepthMask;
	// resolveGlDepthRangef glDepthRangef;
	// resolveGlDetachShader glDetachShader;
	resolveGlDisable glDisable;
	// resolveGlDisableVertexAttribArray glDisableVertexAttribArray;
	resolveGlDrawArrays glDrawArrays;
	// resolveGlDrawElements glDrawElements;
	// resolveGlEnable glEnable;
	resolveGlEnableVertexAttribArray glEnableVertexAttribArray;
	// resolveGlFinish glFinish;
	// resolveGlFlush glFlush;
	// resolveGlFramebufferRenderbuffer glFramebufferRenderbuffer;
	// resolveGlFramebufferTexture2D glFramebufferTexture2D;
	// resolveGlFrontFace glFrontFace;
	// resolveGlGenBuffers glGenBuffers;
	// resolveGlGenFramebuffers glGenFramebuffers;
	// resolveGlGenRenderbuffers glGenRenderbuffers;
	resolveGlGenTextures glGenTextures;
	// resolveGlGenerateMipmap glGenerateMipmap;
	// resolveGlGetActiveAttrib glGetActiveAttrib;
	// resolveGlGetActiveUniform glGetActiveUniform;
	// resolveGlGetAttachedShaders glGetAttachedShaders;
	// resolveGlGetAttribLocation glGetAttribLocation;
	// resolveGlGetBooleanv glGetBooleanv;
	// resolveGlGetBufferParameteriv glGetBufferParameteriv;
	resolveGlGetError glGetError;
	// resolveGlGetFloatv glGetFloatv;
	//resolveGlGetFramebufferAttachmentParameteriv glGetFramebufferAttachmentParameteriv;
	//resolveGlGetIntegerv glGetIntegerv;
	resolveGlGetProgramInfoLog glGetProgramInfoLog;
	resolveGlGetProgramiv glGetProgramiv;
	//resolveGlGetRenderbufferParameteriv glGetRenderbufferParameteriv;
	resolveGlGetShaderInfoLog glGetShaderInfoLog;
	// resolveGlGetShaderPrecisionFormat glGetShaderPrecisionFormat;
	// resolveGlGetShaderSource glGetShaderSource;
	resolveGlGetShaderiv glGetShaderiv;
	resolveGlGetString glGetString;
	// resolveGlGetTexParameterfv glGetTexParameterfv;
	// resolveGlGetTexParameteriv glGetTexParameteriv;
	resolveGlGetUniformLocation glGetUniformLocation;
	// resolveGlGetUniformfv glGetUniformfv;
	// resolveGlGetUniformiv glGetUniformiv;
	// resolveGlGetVertexAttribPointerv glGetVertexAttribPointerv;
	// resolveGlGetVertexAttribfv glGetVertexAttribfv;
	// resolveGlGetVertexAttribiv glGetVertexAttribiv;
	// resolveGlHint glHint;
	// resolveGlIsBuffer glIsBuffer;
	// resolveGlIsEnabled glIsEnabled;
	// resolveGlIsFramebuffer glIsFramebuffer;
	// resolveGlIsProgram glIsProgram;
	// resolveGlIsRenderbuffer glIsRenderbuffer;
	// resolveGlIsShader glIsShader;
	// resolveGlIsTexture glIsTexture;
	// resolveGlLineWidth glLineWidth;
	resolveGlLinkProgram glLinkProgram;
	resolveGlPixelStorei glPixelStorei;
	// resolveGlPolygonOffset glPolygonOffset;
	// resolveGlReadPixels glReadPixels;
	// resolveGlReleaseShaderCompiler glReleaseShaderCompiler;
	// resolveGlRenderbufferStorage glRenderbufferStorage;
	// resolveGlSampleCoverage glSampleCoverage;
	// resolveGlScissor glScissor;
	// resolveGlShaderBinary glShaderBinary;
	resolveGlShaderSource glShaderSource;
	// resolveGlStencilFunc glStencilFunc;
	// resolveGlStencilFuncSeparate glStencilFuncSeparate;
	// resolveGlStencilMask glStencilMask;
	// resolveGlStencilMaskSeparate glStencilMaskSeparate;
	// resolveGlStencilOp glStencilOp;
	// resolveGlStencilOpSeparate glStencilOpSeparate;
	resolveGlTexImage2D glTexImage2D;
	// resolveGlTexParameterf glTexParameterf;
	// resolveGlTexParameterfv glTexParameterfv;
	resolveGlTexParameteri glTexParameteri;
	// resolveGlTexParameteriv glTexParameteriv;
	resolveGlTexSubImage2D glTexSubImage2D;
	resolveGlUniform1f glUniform1f;
	// resolveGlUniform1fv glUniform1fv;
	resolveGlUniform1i glUniform1i;
	// resolveGlUniform1iv glUniform1iv;
	// resolveGlUniform2f glUniform2f;
	// resolveGlUniform2fv glUniform2fv;
	// resolveGlUniform2i glUniform2i;
	// resolveGlUniform2iv glUniform2iv;
	// resolveGlUniform3f glUniform3f;
	// resolveGlUniform3fv glUniform3fv;
	// resolveGlUniform3i glUniform3i;
	// resolveGlUniform3iv glUniform3iv;
	// resolveGlUniform4f glUniform4f;
	// resolveGlUniform4fv glUniform4fv;
	// resolveGlUniform4i glUniform4i;
	// resolveGlUniform4iv glUniform4iv;
	// resolveGlUniformMatrix2fv glUniformMatrix2fv;
	// resolveGlUniformMatrix3fv glUniformMatrix3fv;
	resolveGlUniformMatrix4fv glUniformMatrix4fv;
	resolveGlUseProgram glUseProgram;
	resolveGlValidateProgram glValidateProgram;
	// resolveGlVertexAttrib1f glVertexAttrib1f;
	// resolveGlVertexAttrib1fv glVertexAttrib1fv;
	// resolveGlVertexAttrib2f glVertexAttrib2f;
	// resolveGlVertexAttrib2fv glVertexAttrib2fv;
	// resolveGlVertexAttrib3f glVertexAttrib3f;
	// resolveGlVertexAttrib3fv glVertexAttrib3fv;
	// resolveGlVertexAttrib4f glVertexAttrib4f;
	// resolveGlVertexAttrib4fv glVertexAttrib4fv;
	resolveGlVertexAttribPointer glVertexAttribPointer;
	resolveGlViewport glViewport;

	resolveEGLGetProcAddress eglGetProcAddress;
	
	resolveEGLQueryAPI eglQueryAPI;
	resolveEGLBindAPI eglBindAPI;
	resolveEGLQueryString eglQueryString;
	resolveEGLGetPlatformDisplayEXT eglGetPlatformDisplayEXT;
	resolveEGLGetCurrentDisplay eglGetCurrentDisplay;
	resolveEGLGetCurrentContext eglGetCurrentContext;
	resolveEGLGetCurrentSurface eglGetCurrentSurface;
	resolveEGLInitialize eglInitialize;
	resolveEGLChooseConfig eglChooseConfig;
	resolveEGLCreateContext eglCreateContext;
	resolveEGLCreateWindowSurface eglCreateWindowSurface;
	resolveEGLMakeCurrent eglMakeCurrent;
	resolveEGLGetError eglGetError;
	resolveEGLSwapBuffers eglSwapBuffers;
	resolveEGLQuerySurface eglQuerySurface;
	resolveEGLDestroySurface eglDestroySurface;
	resolveEGLDestroyContext eglDestroyContext;
	resolveEGLTerminate eglTerminate;
	
	void * (*getProcAddress)(const char * name);// Set it to let MS2 initialize all functions
};

typedef struct OpenGlFunctions OpenGlFunctions;

#endif
