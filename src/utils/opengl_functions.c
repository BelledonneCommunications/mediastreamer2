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

// =============================================================================

#if defined( __ANDROID__ )
#define CAST(type, fn) (type)fn
#elif defined(WIN32)
#include <windows.h>
#include "mediastreamer2/mscommon.h"
#define CAST(type, fn) (type)GetProcAddress( openglLibrary, #fn)
#else
#define CAST(type, fn) fn
#endif

void opengl_functions_default_init (OpenGlFunctions *f) {
	
	//----------------------    GL   
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libGLESv2.dll" with the app
	HINSTANCE openglLibrary;
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#ifdef UNICODE
	openglLibrary = LoadLibraryExW(L"libGLESv2.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	openglLibrary = LoadLibraryExA("libGLESv2.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#endif
	if (openglLibrary==NULL)
	{
		ms_message("MSOpenGL Function : Fail to load plugin %s with altered search path: error %i",L"libGLESv2.dll",(int)GetLastError());
#ifdef UNICODE
		openglLibrary = LoadLibraryExW(L"libGLESv2.dll", NULL, 0);
#else
		openglLibrary = LoadLibraryExA("libGLESv2.dll", NULL, 0);
#endif
	}
#else
	openglLibrary = LoadPackagedLibrary(L"libGLESv2.dll", 0);// UWP compatibility
#endif
	if( openglLibrary == NULL ){
		ms_error("MSOpenGL Function : Fail to load plugin %s: error %i", L"libGLESv2.dll", (int)GetLastError());
	}else{
#endif// WIN32
		
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
#ifdef WIN32
	}
#endif
	//----------------------    EGL        
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libEGL.dll" with the app
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#ifdef UNICODE
	openglLibrary = LoadLibraryExW(L"libEGL.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	openglLibrary = LoadLibraryExA("libEGL.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#endif
	if (openglLibrary==NULL)
	{
		ms_message("MSOpenGL Function : Fail to load plugin %s with altered search path: error %i",L"libEGL.dll",(int)GetLastError());
#ifdef UNICODE
		openglLibrary = LoadLibraryExW(L"libEGL.dll", NULL, 0);
#else
		openglLibrary = LoadLibraryExA("libEGL.dll", NULL, 0);
#endif
	}
#else
	openglLibrary = LoadPackagedLibrary(L"libEGL.dll", 0);// UWP compatibility
#endif
	if( openglLibrary == NULL ){
		ms_error("MSOpenGL Function : Fail to load plugin %s: error %i", L"libEGL.dll", (int)GetLastError());
	}else{
#endif// WIN32
		
		f->eglGetPlatformDisplayEXT = CAST(resolveEGLGetPlatformDisplayEXT, eglGetPlatformDisplayEXT);
		f->eglInitialize = CAST(resolveEGLInitialize, eglInitialize);
		f->eglChooseConfig = CAST(resolveEGLChooseConfig, eglChooseConfig);
		f->eglCreateContext = CAST(resolveEGLCreateContext, eglCreateContext);
		f->eglCreateWindowSurface = CAST(resolveEGLCreateWindowSurface, eglCreateWindowSurface);
		f->eglMakeCurrent = CAST(resolveEGLMakeCurrent, eglMakeCurrent);
		f->eglGetError = CAST(resolveEGLGetError, eglGetError);
		f->eglSwapBuffers = CAST(resolveEGLSwapBuffers, eglSwapBuffers);
		f->eglQuerySurface = CAST(resolveEGLQuerySurface, eglQuerySurface);
		f->eglDestroySurface = CAST( resolveEGLDestroySurface, eglDestroySurface);
		f->eglDestroyContext = CAST( resolveEGLDestroyContext, eglDestroyContext);
		f->eglTerminate = CAST( resolveEGLTerminate, eglTerminate);
		
#ifdef WIN32
	}
#endif
}
