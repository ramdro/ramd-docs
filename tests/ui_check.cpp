#define RAMD_DOCS_NO_MAIN
#include "../src/main.cpp"
#include <iostream>
struct Check {
    App *app;
    std::string dir;
    int step=0,ticks=0;
    static gboolean tick(gpointer data){
        auto c=static_cast<Check*>(data);auto a=c->app;
        if(++c->ticks>110){g_printerr("UI test timeout at step %d\n",c->step);std::exit(1);}
        if(!a->ready||a->busy)return G_SOURCE_CONTINUE;
        switch(c->step){
        case 0:{
            const std::string html="<h1>RAMD Docs</h1><p>A real document workspace.</p><p>RAMD_EDITOR_TYPED — café, español.</p><table><tr><td>Project</td><td>Status</td></tr><tr><td>RAMD Docs</td><td>In development</td></tr></table>";
            if(!lok_doc_view_paste(LOK_DOC_VIEW(a->view),"text/html",html.c_str(),html.size())){g_printerr("HTML paste failed\n");std::exit(1);}
            gtk_widget_grab_focus(a->view);
            GError *err=nullptr;
            if(!g_spawn_command_line_sync("xdotool search --name Untitled windowfocus --sync type --clearmodifiers --delay 40 keyboard_input_ok",nullptr,nullptr,nullptr,&err)){g_printerr("Keyboard driver failed: %s\n",err->message);std::exit(1);}
            break;
        }
        case 1:{
            std::ofstream(c->dir+"/test.ramdext.json")<<R"({"format":"ramd-docs-extension-1","name":"Integration extension","description":"Test insertion","text":" EXTENSION_INSERT_OK "})";
            a->installExtension(c->dir+"/test.ramdext.json");
            size_t index=0;for(size_t i=0;i<a->extensions.size();++i)if(a->extensions[i].name=="Integration extension")index=i;
            a->insertExtension(index);
            a->command(".uno:ToggleOrientation");
            auto icon=gtk_window_get_icon(GTK_WINDOW(a->window));gdk_pixbuf_save(icon,(c->dir+"/insert.png").c_str(),"png",nullptr,nullptr);
            a->command(".uno:InsertGraphic",{{"FileName",{"string",uri(c->dir+"/insert.png")}},{"AsLink",{"boolean","false"}}});
            break;
        }
        case 2:a->recoveryCopy();break;
        case 3:
            if(!a->filename.empty()||!fs::exists(a->recoveryFile)){g_printerr("Recovery changed path or did not save\n");std::exit(1);}
            fs::copy_file(a->recoveryFile,c->dir+"/recovery.odt",fs::copy_options::overwrite_existing);
            if(!a->dirty){g_printerr("Editing did not mark document modified\n");std::exit(1);}
            a->savePath(c->dir+"/edited.docx",false);break;
        case 4:
            if(a->dirty||!fs::exists(c->dir+"/edited.docx")){g_printerr("DOCX save not confirmed\n");std::exit(1);}
            a->savePath(c->dir+"/edited.pdf",true);break;
        case 5:
            if(a->filename!=c->dir+"/edited.docx"){g_printerr("PDF export changed editable document path\n");std::exit(1);}
            a->load(c->dir+"/edited.docx");break;
        case 6:
            a->action(".uno:SelectAll");break;
        case 7:{
            gchar *used=nullptr;gchar *text=lok_doc_view_copy_selection(LOK_DOC_VIEW(a->view),"text/plain;charset=utf-8",&used);
            bool ok=text&&std::string(text).find("RAMD_EDITOR_TYPED")!=std::string::npos;
            g_free(used);g_free(text);
            if(!ok){g_printerr("Saved document text missing after reopening\n");std::exit(1);}
            a->command(".uno:GoToStartOfDoc");
            break;
        }
        case 8:{
            auto win=gtk_widget_get_window(a->window);int w=gtk_widget_get_allocated_width(a->window),h=gtk_widget_get_allocated_height(a->window);
            auto image=gdk_pixbuf_get_from_window(win,0,0,w,h);
            if(image){gdk_pixbuf_save(image,(c->dir+"/editor.png").c_str(),"png",nullptr,nullptr);g_object_unref(image);}
            std::cout<<"PASS: embedded editor insertion, dirty state, asynchronous DOCX save, PDF export, reopen and selection\n";
            g_timeout_add(500,[](gpointer data)->gboolean{
                auto c=static_cast<Check*>(data);auto windows=gtk_window_list_toplevels();
                for(auto it=windows;it;it=it->next){auto w=GTK_WIDGET(it->data);const char *title=gtk_window_get_title(GTK_WINDOW(w));
                    if(GTK_IS_DIALOG(w)&&title&&std::string(title)=="Extensions"){
                        auto pic=gdk_pixbuf_get_from_window(gtk_widget_get_window(w),0,0,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w));
                        if(pic){gdk_pixbuf_save(pic,(c->dir+"/extensions.png").c_str(),"png",nullptr,nullptr);g_object_unref(pic);}gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_CLOSE);
                    }
                }g_list_free(windows);return G_SOURCE_REMOVE;
            },c);
            a->extensionManager();break;
        }
        case 9:a->load(c->dir+"/recovery.odt",true);break;
        case 10:a->command(".uno:SelectAll");break;
        case 11:{
            gchar *used=nullptr;auto text=lok_doc_view_copy_selection(LOK_DOC_VIEW(a->view),"text/plain;charset=utf-8",&used);
            bool ok=text&&std::string(text).find("EXTENSION_INSERT_OK")!=std::string::npos;g_free(text);g_free(used);
            if(!ok){g_printerr("Recovery copy could not be reopened\n");std::exit(1);}
            std::cout<<"PASS: extension manager and recovery reopening\n";gtk_widget_destroy(a->window);return G_SOURCE_REMOVE;
        }
        }
        ++c->step;return G_SOURCE_CONTINUE;
    }
};
int runCheck(int argc,char **argv){
    gtk_init(&argc,&argv);if(argc!=2)return 2;
    fs::create_directories(argv[1]);App a;a.testing=true;
    Check c{&a,fs::absolute(argv[1]).string()};a.newDocument();
    g_timeout_add(500,Check::tick,&c);gtk_main();return 0;
}

int main(int argc,char **argv){int status=runCheck(argc,argv);finishProcess(status);}
