#pragma once
#ifdef _WIN32
#include <windows.h>
static std::string windowsOfficePath(){
    if(auto p=g_getenv("RAMD_LIBREOFFICE_PROGRAM"))return p;
    return "C:/Program Files/LibreOffice/program";
}
static void prepareWindowsRuntime(){
    wchar_t raw[MAX_PATH];GetModuleFileNameW(nullptr,raw,MAX_PATH);
    auto base=fs::path(raw).parent_path();
    auto modules=base/"lib/gdk-pixbuf-2.0/2.10.0/loaders";
    if(fs::exists(modules)){
        g_setenv("GDK_PIXBUF_MODULEDIR",modules.u8string().c_str(),true);
        auto query=(base/"gdk-pixbuf-query-loaders.exe").u8string();
        gchar *args[]={const_cast<char*>(query.c_str()),nullptr},*output=nullptr;
        if(g_spawn_sync(nullptr,args,nullptr,G_SPAWN_DEFAULT,nullptr,nullptr,&output,nullptr,nullptr,nullptr)&&output){
            auto cache=fs::path(g_get_user_cache_dir())/"ramd-docs";fs::create_directories(cache);auto file=cache/"loaders.cache";std::ofstream(file)<<output;g_setenv("GDK_PIXBUF_MODULE_FILE",file.u8string().c_str(),true);g_free(output);
        }
        g_setenv("GSETTINGS_SCHEMA_DIR",(base/"share/glib-2.0/schemas").u8string().c_str(),true);
        g_setenv("XDG_DATA_DIRS",(base/"share").u8string().c_str(),true);
    }
    auto office=fs::u8path(windowsOfficePath());
    SetDllDirectoryW(office.c_str());
}
#else
static void prepareWindowsRuntime(){}
#endif
