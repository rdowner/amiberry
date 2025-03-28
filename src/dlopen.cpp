#include "sysconfig.h"
#include "sysdeps.h"
#include "uae/api.h"
#include "uae/dlopen.h"
#include "uae/log.h"

#ifdef _WIN32
#include "windows.h"
#else
#include <dlfcn.h>
#endif
#ifdef WINUAE
#include "od-win32/win32.h"
#endif

#ifdef AMIBERRY
#include "uae.h"
#endif
#ifdef __MACH__
#include <mach-o/dyld.h>
#endif

UAE_DLHANDLE uae_dlopen(const TCHAR *path)
{
	UAE_DLHANDLE result;
	if (path == NULL || path[0] == _T('\0')) {
		write_log(_T("DLOPEN: No path given\n"));
		return NULL;
	}
#ifdef _WIN32
	result = LoadLibrary(path);
#else
	result = dlopen(path, RTLD_NOW);
	const char *error = dlerror();
	if (error != NULL)  {
		write_log("DLOPEN: %s\n", error);
	}
#endif
	if (result == NULL) {
		write_log("DLOPEN: Failed to open %s\n", path);
	}
	return result;
}

void *uae_dlsym(UAE_DLHANDLE handle, const char *name)
{
#ifdef _WIN32
	return (void *) GetProcAddress(handle, name);
#else
	return dlsym(handle, name);
#endif
}

void uae_dlclose(UAE_DLHANDLE handle)
{
#ifdef _WIN32
	FreeLibrary (handle);
#else
	dlclose(handle);
#endif
}

UAE_DLHANDLE uae_dlopen_plugin(const TCHAR *name)
{
#if defined(FSUAE)
	const TCHAR *path = NULL;
	if (plugin_lookup) {
		path = plugin_lookup(name);
	}
	if (path == NULL or path[0] == _T('\0')) {
		write_log(_T("DLOPEN: Could not find plugin \"%s\"\n"), name);
		return NULL;
	}
	UAE_DLHANDLE handle = uae_dlopen(path);
#elif defined(WINUAE)
	TCHAR path[MAX_DPATH];
	_tcscpy(path, name);
	_tcscat(path, LT_MODULE_EXT);
	UAE_DLHANDLE handle = WIN32_LoadLibrary(path);
#elif defined (__MACH__)
	TCHAR path[MAX_DPATH];
	char exepath[MAX_DPATH];
	uint32_t size = sizeof exepath;
	std::string directory;
	if (_NSGetExecutablePath(exepath, &size) == 0)
	{
		size_t last_slash_idx = string(exepath).rfind('/');
		if (std::string::npos != last_slash_idx)
		{
			directory = string(exepath).substr(0, last_slash_idx);
		}
		last_slash_idx = directory.rfind('/');
		if (std::string::npos != last_slash_idx)
		{
			directory = directory.substr(0, last_slash_idx);
		}
	}
	_tcscpy(path, directory.append("/Resources/").append(name).c_str());
	_tcscat(path, LT_MODULE_EXT);
	UAE_DLHANDLE handle = uae_dlopen(path);
#else
	TCHAR path[MAX_DPATH];
	std::string directory = string(start_path_data);
	_tcscpy(path, directory.append("/").append(name).c_str());
#ifdef _WIN64
	_tcscat(path, _T("_x64"));
#endif
	_tcscat(path, LT_MODULE_EXT);
	UAE_DLHANDLE handle = uae_dlopen(path);
#endif
	if (handle) {
		write_log(_T("DLOPEN: Loaded plugin %s\n"), path);
		uae_dlopen_patch_common(handle);
	}
	return handle;
}

void uae_dlopen_patch_common(UAE_DLHANDLE handle)
{
	void *ptr;
	ptr = uae_dlsym(handle, "uae_log");
	if (ptr) {
		write_log(_T("DLOPEN: Patching common functions\n"));
		*((uae_log_function *)ptr) = &write_log;
	}
}
