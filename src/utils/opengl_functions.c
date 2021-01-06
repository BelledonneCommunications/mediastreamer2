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
#include "opengl_functions.h"
#include "mediastreamer2/mscommon.h"
// =============================================================================

#ifndef MS2_WINDOWS_UWP
//[Desktop app only]
#include <wingdi.h>

#pragma comment(lib,"Opengl32.lib")	//Opengl32 symbols for wglGetProcAddress

void *GetAnyGLFuncAddress(HMODULE library, HMODULE firstFallback, const char *name)
{
	void *p = (void *)wglGetProcAddress(name);
	if(p == 0 ||
		(p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) ||
		(p == (void*)-1) ) {
		if(firstFallback)
			p = (void *)GetProcAddress(firstFallback, name);
		if(!p)
			p = (void *)GetProcAddress(library, name);	
	}
	
	return p;
}
#endif
#if defined( __ANDROID__ )
#define CAST(type, fn) (type)fn
#elif defined(WIN32)
#include <windows.h>
	#define CAST_EGL(type, fn) (type)f->eglGetProcAddress(#fn)
	#ifdef MS2_WINDOWS_UWP
		#define CAST(type, fn) (type)GetProcAddress( openglLibrary, #fn)
	#else
		#define CAST(type, fn) (type)GetAnyGLFuncAddress(openglLibrary,firstFallbackLibrary, #fn)	
	#endif
#else
#define CAST(type, fn) (type)fn
#endif

#ifdef __cplusplus
extern "C"{
#endif

void opengl_functions_default_init (OpenGlFunctions *f) {
	
	//----------------------    GL   
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libGLESv2.dll" with the app
	HMODULE openglLibrary, firstFallbackLibrary = NULL;
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#ifdef UNICODE
	openglLibrary = LoadLibraryW(L"libGLESv2.dll");
	firstFallbackLibrary = LoadLibraryW(L"opengl32.dll");
#else
	openglLibrary = LoadLibraryA("libGLESv2.dll");
	firstFallbackLibrary = LoadLibraryA("opengl32.dll");
#endif
#else
	openglLibrary = LoadPackagedLibrary(L"libGLESv2.dll", 0);// UWP compatibility
#endif
	if( openglLibrary == NULL ){
		ms_error("MSOpenGL Function : Fail to load plugin libGLESv2.dll: error %i", (int)GetLastError());
	}else{
#endif// _WIN32
		
		f->glActiveTexture = CAST(resolveGlActiveTexture, glActiveTexture);
		f->glAttachShader = CAST(resolveGlAttachShader, glAttachShader);
		f->glBindAttribLocation = CAST(resolveGlBindAttribLocation, glBindAttribLocation);
		f->glBindTexture = CAST(resolveGlBindTexture, glBindTexture);
		f->glClear = CAST(resolveGlClear, glClear);
		f->glClearColor = CAST(resolveGlClearColor, glClearColor);
		f->glCompileShader = CAST(resolveGlCompileShader, glCompileShader);
		f->glCreateProgram = CAST(resolveGlCreateProgram, glCreateProgram);
		f->glCreateShader = CAST(resolveGlCreateShader, glCreateShader);
		f->glDeleteProgram = CAST(resolveGlDeleteProgram, glDeleteProgram);
		f->glDeleteShader = CAST(resolveGlDeleteShader, glDeleteShader);
		f->glDeleteTextures = CAST(resolveGlDeleteTextures, glDeleteTextures);
		f->glDisable = CAST(resolveGlDisable,glDisable);
		f->glDrawArrays = CAST(resolveGlDrawArrays, glDrawArrays);
		f->glEnableVertexAttribArray = CAST(resolveGlEnableVertexAttribArray, glEnableVertexAttribArray);
		f->glGenTextures = CAST(resolveGlGenTextures,glGenTextures);
		f->glGetError = CAST(resolveGlGetError, glGetError);
		f->glGetProgramInfoLog = CAST(resolveGlGetProgramInfoLog, glGetProgramInfoLog);
		f->glGetProgramiv = CAST(resolveGlGetProgramiv, glGetProgramiv);
		f->glGetShaderInfoLog = CAST(resolveGlGetShaderInfoLog, glGetShaderInfoLog);
		f->glGetShaderiv = CAST(resolveGlGetShaderiv, glGetShaderiv);
		f->glGetString = CAST(resolveGlGetString, glGetString);
		f->glGetUniformLocation = CAST(resolveGlGetUniformLocation, glGetUniformLocation);
		f->glLinkProgram = CAST(resolveGlLinkProgram, glLinkProgram);
		f->glPixelStorei = CAST(resolveGlPixelStorei, glPixelStorei);
		f->glShaderSource = CAST(resolveGlShaderSource, glShaderSource);
		f->glTexImage2D = CAST(resolveGlTexImage2D, glTexImage2D);
		f->glTexParameteri = CAST(resolveGlTexParameteri, glTexParameteri);
		f->glTexSubImage2D = CAST(resolveGlTexSubImage2D, glTexSubImage2D);
		f->glUniform1f = CAST(resolveGlUniform1f, glUniform1f);
		f->glUniform1i = CAST(resolveGlUniform1i, glUniform1i);
		f->glUniformMatrix4fv = CAST(resolveGlUniformMatrix4fv, glUniformMatrix4fv);
		f->glUseProgram = CAST(resolveGlUseProgram, glUseProgram);
		f->glValidateProgram = CAST(resolveGlValidateProgram, glValidateProgram);
		f->glVertexAttribPointer = CAST(resolveGlVertexAttribPointer, glVertexAttribPointer);
		f->glViewport = CAST(resolveGlViewport, glViewport);
#ifdef _WIN32
	}
#endif
	//----------------------    EGL        
#if _WIN32	// Just to be sure to use egl only on Windows (for the moment)
	
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libEGL.dll" with the app
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#ifdef UNICODE
	openglLibrary = LoadLibraryExW(L"libEGL.dll", NULL, 0);
#else
	openglLibrary = LoadLibraryExA("libEGL.dll", NULL, 0);
#endif
#else
	openglLibrary = LoadPackagedLibrary(L"libEGL.dll", 0);// UWP compatibility
#endif
	if( openglLibrary == NULL ){
		ms_error("MSOpenGL Function : Fail to load plugin libEGL.dll: error %i", (int)GetLastError());
	}else{
#endif// _WIN32
		
		f->eglGetProcAddress = CAST(resolveEGLGetProcAddress, eglGetProcAddress);

		f->eglQueryAPI = CAST_EGL(resolveEGLQueryAPI, eglQueryAPI);
		f->eglBindAPI = CAST_EGL(resolveEGLBindAPI, eglBindAPI);
		f->eglQueryString = CAST_EGL(resolveEGLQueryString, eglQueryString);
		f->eglGetPlatformDisplayEXT = CAST_EGL(resolveEGLGetPlatformDisplayEXT, eglGetPlatformDisplayEXT);
		f->eglInitialize = CAST_EGL(resolveEGLInitialize, eglInitialize);
		f->eglChooseConfig = CAST_EGL(resolveEGLChooseConfig, eglChooseConfig);
		f->eglCreateContext = CAST_EGL(resolveEGLCreateContext, eglCreateContext);
		f->eglCreateWindowSurface = CAST_EGL(resolveEGLCreateWindowSurface, eglCreateWindowSurface);
		f->eglMakeCurrent = CAST_EGL(resolveEGLMakeCurrent, eglMakeCurrent);
		f->eglGetError = CAST_EGL(resolveEGLGetError, eglGetError);
		f->eglSwapBuffers = CAST_EGL(resolveEGLSwapBuffers, eglSwapBuffers);
		f->eglQuerySurface = CAST_EGL(resolveEGLQuerySurface, eglQuerySurface);
		f->eglDestroySurface = CAST_EGL( resolveEGLDestroySurface, eglDestroySurface);
		f->eglDestroyContext = CAST_EGL( resolveEGLDestroyContext, eglDestroyContext);
		f->eglTerminate = CAST_EGL( resolveEGLTerminate, eglTerminate);
#ifdef _WIN32
	}
#endif
#endif	// WIN32
}

#ifdef __cplusplus
}
#endif
