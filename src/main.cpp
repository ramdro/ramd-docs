#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <LibreOfficeKit/LibreOfficeKitGtk.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>
#include <stdexcept>
#include "process_exit.h"
#include "icon.h"
#include <cstring>
namespace fs = std::filesystem;
#include "windows_platform.h"
using Fields = std::vector<std::pair<std::string,std::pair<std::string,std::string>>>;
static std::string arguments(const Fields &fields) {
    JsonBuilder *b=json_builder_new();json_builder_begin_object(b);
    for(const auto &f:fields){
        json_builder_set_member_name(b,f.first.c_str());json_builder_begin_object(b);
        json_builder_set_member_name(b,"type");json_builder_add_string_value(b,f.second.first.c_str());
        json_builder_set_member_name(b,"value");
        if(f.second.first=="boolean")json_builder_add_boolean_value(b,f.second.second=="true");
        else if(f.second.first=="long"||f.second.first=="short")json_builder_add_int_value(b,std::stoll(f.second.second));
        else if(f.second.first=="float")json_builder_add_double_value(b,std::stod(f.second.second));
        else json_builder_add_string_value(b,f.second.second.c_str());
        json_builder_end_object(b);
    }
    json_builder_end_object(b);auto root=json_builder_get_root(b);auto gen=json_generator_new();
    json_generator_set_root(gen,root);gchar *data=json_generator_to_data(gen,nullptr);
    std::string out=data;g_free(data);json_node_free(root);g_object_unref(gen);g_object_unref(b);return out;
}
static std::string uri(const std::string &path){gchar *p=g_filename_to_uri(fs::absolute(path).string().c_str(),nullptr,nullptr);std::string out=p?p:"";g_free(p);return out;}
static const char *blank=R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
<office:styles><style:default-style style:family="paragraph"><style:paragraph-properties fo:line-height="115%"/><style:text-properties fo:font-size="11pt" fo:font-family="Liberation Serif"/></style:default-style></office:styles>
<office:automatic-styles><style:page-layout style:name="A4"><style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm" fo:margin="2.5cm"/></style:page-layout></office:automatic-styles>
<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="A4"/></office:master-styles>
<office:body><office:text><text:p/></office:text></office:body></office:document>)";
#include "extensions.h"
struct App {
    GtkWidget *window{},*view{},*status{},*heading{},*editorControls{},*search{};
    bool ready=false,dirty=false,busy=false,testing=false,syncing=false;
    std::string filename,pendingPath,pendingCommand,session;
    GtkAccelGroup *accels{};
    void message(const std::string &text){gtk_label_set_text(GTK_LABEL(status),text.c_str());}
    void error(const std::string &text){
        if(testing){g_printerr("UI TEST FAILURE: %s\n",text.c_str());std::exit(1);}
        auto d=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",text.c_str());
        gtk_dialog_run(GTK_DIALOG(d));gtk_widget_destroy(d);
    }
    void title(){std::string name=filename.empty()?"Untitled":fs::path(filename).filename().string();gtk_header_bar_set_title(GTK_HEADER_BAR(heading),(name+(dirty?" •":"")).c_str());}
    void setBusy(bool value){busy=value;gtk_widget_set_sensitive(editorControls,ready&&!busy);gtk_widget_set_sensitive(view,!busy);}
    void command(const std::string &name,const Fields &fields={}) {
        if(!ready||busy)return;
        std::string json=arguments(fields);
        lok_doc_view_post_command(LOK_DOC_VIEW(view),name.c_str(),json.c_str(),false);
        gtk_widget_grab_focus(view);
    }
    std::string choose(bool save,const char *title,const char *suggestion,const char *pattern) {
        auto d=gtk_file_chooser_dialog_new(title,GTK_WINDOW(window),save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,
          "_Cancel",GTK_RESPONSE_CANCEL,save?"_Save":"_Open",GTK_RESPONSE_ACCEPT,nullptr);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);
        if(save)gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),suggestion);
        auto filter=gtk_file_filter_new();gtk_file_filter_set_name(filter,pattern);
        if(save)gtk_file_filter_add_pattern(filter,pattern);
        else {gtk_file_filter_add_pattern(filter,"*.docx");gtk_file_filter_add_pattern(filter,"*.odt");gtk_file_filter_add_pattern(filter,"*.doc");gtk_file_filter_add_pattern(filter,"*.rtf");gtk_file_filter_add_pattern(filter,"*.fodt");}
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d),filter);
        std::string result;
        if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){
            char *p=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));result=p?p:"";g_free(p);
        }
        gtk_widget_destroy(d);return result;
    }
    bool canLeave(){
        if(busy){message("Please wait for the current document operation.");return false;}
        if(!dirty)return true;
        auto d=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_WARNING,GTK_BUTTONS_NONE,
          "Save your changes before leaving?");
        gtk_dialog_add_buttons(GTK_DIALOG(d),"_Cancel",GTK_RESPONSE_CANCEL,"_Discard",GTK_RESPONSE_REJECT,"_Save",GTK_RESPONSE_ACCEPT,nullptr);
        int answer=gtk_dialog_run(GTK_DIALOG(d));gtk_widget_destroy(d);
        if(answer==GTK_RESPONSE_ACCEPT){save(false,false);return false;}
        return answer==GTK_RESPONSE_REJECT;
    }
    void load(const std::string &path,bool fresh=false){
        if(!canLeave())return;
        setBusy(true);message("Opening document…");
        pendingPath=fresh?"":path;
        lok_doc_view_open_document(LOK_DOC_VIEW(view),path.c_str(),"{\".uno:Author\":{\"type\":\"string\",\"value\":\"RAMD Docs\"}}",nullptr,
          [](GObject *object,GAsyncResult *result,gpointer data){
            auto a=static_cast<App*>(data);GError *err=nullptr;
            bool ok=lok_doc_view_open_document_finish(LOK_DOC_VIEW(object),result,&err);
            a->ready=ok;a->setBusy(false);
            if(!ok){a->error(err?err->message:"Could not open document.");g_clear_error(&err);a->message("Open failed. Choose another document.");return;}
            a->filename=a->pendingPath;a->pendingPath.clear();a->dirty=false;a->title();
            lok_doc_view_set_edit(LOK_DOC_VIEW(a->view),true);
            a->message("Ready · Recovery copies every 2 minutes");gtk_widget_grab_focus(a->view);
          },this);
    }
    void newDocument(){
        if(!canLeave())return;
        auto p=fs::path(session)/"blank.fodt";std::ofstream(p)<<blank;
        // The guard already ran; discard choice is explicit.
        dirty=false;load(p.string(),true);
    }
    void save(bool as,bool pdf){
        if(!ready||busy)return;
        std::string target=filename;
        if(as||pdf||target.empty()){
            target=choose(true,pdf?"Export PDF":"Save document",pdf?"Document.pdf":"Document.docx",pdf?"*.pdf":"*.docx");
            if(target.empty())return;
            std::string suffix=pdf?".pdf":".docx";
            if(fs::path(target).extension()!=suffix){
                target+=suffix;
                if(fs::exists(target)){error("That filename already exists. Choose its full filename to confirm replacement.");return;}
            }
        }
        const std::string ext=fs::path(target).extension().string();
        // Legacy inputs are always saved as DOCX, never silently rewritten in a different format.
        if(!pdf&&ext!=".docx"&&ext!=".odt"){save(true,false);return;}
        savePath(target,pdf);
    }
    void savePath(const std::string &target,bool pdf){
        if(!ready||busy)return;
        const auto ext=fs::path(target).extension().string();
        pendingCommand=pdf?".uno:ExportDirectToPDF":".uno:SaveAs";pendingPath=target;
        const auto args=arguments({{"URL",{"string",uri(target)}},{"FilterName",{"string",pdf?"writer_pdf_Export":ext==".odt"?"writer8":"Office Open XML Text"}},{"Overwrite",{"boolean","true"}}});
        setBusy(true);message(pdf?"Exporting PDF…":"Saving document…");
        lok_doc_view_post_command(LOK_DOC_VIEW(view),pendingCommand.c_str(),args.c_str(),true);
    }
    static void result(LOKDocView*,gchar *payload,gpointer data){
        auto a=static_cast<App*>(data);auto parser=json_parser_new();
        if(!json_parser_load_from_data(parser,payload,-1,nullptr)){g_object_unref(parser);return;}
        auto node=json_parser_get_root(parser);
        if(!JSON_NODE_HOLDS_OBJECT(node)){g_object_unref(parser);return;}
        auto obj=json_node_get_object(node);
        const char *name=json_object_has_member(obj,"commandName")?json_object_get_string_member(obj,"commandName"):"";
        if(a->pendingCommand==name){
            bool ok=json_object_has_member(obj,"success")&&json_object_get_boolean_member(obj,"success");
            bool pdf=a->pendingCommand==".uno:ExportDirectToPDF";
            bool wasCopy=a->copying;
            if(wasCopy){a->filename=a->copySource;a->dirty=a->copyDirty;a->copying=false;a->title();}
            else if(ok&&!pdf){a->filename=a->pendingPath;a->dirty=false;a->title();std::error_code ec;fs::remove(a->recoveryFile,ec);}
            a->pendingCommand.clear();a->pendingPath.clear();a->setBusy(false);
            if(ok)a->message(wasCopy?"Recovery copy saved · original document remains unsaved":pdf?"PDF exported":"Document saved");
            else{a->message("Save failed — your document remains open");a->error("The document engine could not save this file. Try Save As in a writable folder.");}
        }
        g_object_unref(parser);
    }
    GtkWidget *button(GtkWidget *box,const char *label,void(*fn)(GtkButton*,gpointer),const char *shortcut=nullptr){
        auto b=gtk_button_new_with_label(label);gtk_box_pack_start(GTK_BOX(box),b,false,false,0);
        gtk_widget_set_can_focus(b,false);g_signal_connect(b,"clicked",G_CALLBACK(fn),this);
        if(shortcut){guint key;GdkModifierType mods;gtk_accelerator_parse(shortcut,&key,&mods);gtk_widget_add_accelerator(b,"clicked",accels,key,mods,GTK_ACCEL_VISIBLE);}
        return b;
    }
    void unoButton(GtkWidget *box,const char *label,const char *cmd){
        auto b=button(box,label,[](GtkButton *b,gpointer p){auto a=static_cast<App*>(p);a->command(static_cast<const char*>(g_object_get_data(G_OBJECT(b),"command")));});
        g_object_set_data_full(G_OBJECT(b),"command",g_strdup(cmd),g_free);
    }
    GtkWidget *tabs{},*zoomControl{};
    std::vector<std::pair<std::string,GtkWidget*>> toggles;
    void openDocument(){
        if(!canLeave())return;
        auto f=choose(false,"Open document","","Word / OpenDocument");
        if(!f.empty()){dirty=false;load(f);}
    }
    void tableDialog(){
        if(!ready||busy)return;
        auto d=gtk_dialog_new_with_buttons("Insert table",GTK_WINDOW(window),GTK_DIALOG_MODAL,"Cancel",GTK_RESPONSE_CANCEL,"Insert",GTK_RESPONSE_ACCEPT,nullptr);
        auto grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),12);gtk_grid_set_column_spacing(GTK_GRID(grid),24);gtk_container_set_border_width(GTK_CONTAINER(grid),24);
        auto rows=gtk_spin_button_new_with_range(1,100,1),cols=gtk_spin_button_new_with_range(1,20,1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(rows),3);gtk_spin_button_set_value(GTK_SPIN_BUTTON(cols),3);
        gtk_grid_attach(GTK_GRID(grid),gtk_label_new("Rows"),0,0,1,1);gtk_grid_attach(GTK_GRID(grid),rows,1,0,1,1);
        gtk_grid_attach(GTK_GRID(grid),gtk_label_new("Columns"),0,1,1,1);gtk_grid_attach(GTK_GRID(grid),cols,1,1,1,1);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))),grid);gtk_widget_show_all(d);
        if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT)command(".uno:InsertTable",{{"Rows",{"long",std::to_string(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rows)))}},{"Columns",{"long",std::to_string(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cols)))}}});
        gtk_widget_destroy(d);
    }
    void clipboard(bool cut){
        if(!ready||busy)return;
        gchar *used=nullptr;auto text=lok_doc_view_copy_selection(LOK_DOC_VIEW(view),"text/plain;charset=utf-8",&used);
        if(text&&*text){gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),text,-1);if(cut)command(".uno:Delete");}
        g_free(text);g_free(used);
    }
    void action(const std::string &id){
        if(syncing)return;
        if(id=="new")newDocument();
        else if(id=="open")openDocument();
        else if(id=="save")save(false,false);
        else if(id=="save-as")save(true,false);
        else if(id=="pdf")save(true,true);
        else if(id=="close")gtk_window_close(GTK_WINDOW(window));
        else if(id=="find"){gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs),0);gtk_widget_grab_focus(search);}
        else if(id=="table")tableDialog();
        else if(id=="image")insertImage();
        else if(id=="extensions")extensionManager();
        else if(id=="build-extension")extensionBuilder();
        else if(id=="recover")recoverDocument();
        else if(id=="recovery-copy")recoveryCopy();
        else if(id=="word-count")showWordCount();
        else if(id=="meeting-notes")insertExtension(0);
        else if(id=="project-brief")insertExtension(1);
        else if(id=="letter")insertExtension(2);
        else if(id=="date")insertExtension(3);
        else if(id=="copy")clipboard(false);
        else if(id=="cut")clipboard(true);
        else if(id=="paste"){
            if(!ready||busy)return;
            auto text=gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
            if(text){lok_doc_view_paste(LOK_DOC_VIEW(view),"text/plain;charset=utf-8",text,strlen(text));g_free(text);gtk_widget_grab_focus(view);}
        }
        else if(id=="zoom-in")gtk_range_set_value(GTK_RANGE(zoomControl),gtk_range_get_value(GTK_RANGE(zoomControl))+10);
        else if(id=="zoom-out")gtk_range_set_value(GTK_RANGE(zoomControl),gtk_range_get_value(GTK_RANGE(zoomControl))-10);
        else if(id=="zoom-reset")gtk_range_set_value(GTK_RANGE(zoomControl),100);
        else if(id=="about")gtk_show_about_dialog(GTK_WINDOW(window),"program-name","RAMD Docs","version","1.0","logo-icon-name","ramd-docs","comments","A document workspace by RAMD Studios.\nPowered by LibreOfficeKit.\nIncludes two-minute recovery copies and text extensions.",nullptr);
        else command(id);
    }
    void bindAction(GtkWidget *w,const char *signal,const char *id,const char *shortcut=nullptr){
        g_object_set_data_full(G_OBJECT(w),"action",g_strdup(id),g_free);
        g_signal_connect(w,signal,G_CALLBACK((+[](GtkWidget *w,gpointer p){auto a=static_cast<App*>(p);try{a->action(static_cast<const char*>(g_object_get_data(G_OBJECT(w),"action")));}catch(const std::exception &e){a->error(e.what());}})),this);
        if(shortcut){guint key;GdkModifierType mods;gtk_accelerator_parse(shortcut,&key,&mods);gtk_widget_add_accelerator(w,signal,accels,key,mods,GTK_ACCEL_VISIBLE);}
    }
    GtkWidget *actionButton(GtkWidget *box,const char *icon,const char *label,const char *id,bool compact=false){
        auto b=gtk_button_new();auto content=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
        gtk_container_add(GTK_CONTAINER(b),content);gtk_box_pack_start(GTK_BOX(content),gtk_image_new_from_icon_name(icon,GTK_ICON_SIZE_BUTTON),false,false,0);
        if(!compact)gtk_box_pack_start(GTK_BOX(content),gtk_label_new(label),false,false,0);
        gtk_widget_set_tooltip_text(b,label);gtk_widget_set_can_focus(b,false);bindAction(b,"clicked",id);
        gtk_box_pack_start(GTK_BOX(box),b,false,false,0);return b;
    }
    void toggle(GtkWidget *box,const char *icon,const char *label,const char *cmd){
        auto b=gtk_toggle_button_new();gtk_container_add(GTK_CONTAINER(b),gtk_image_new_from_icon_name(icon,GTK_ICON_SIZE_BUTTON));
        gtk_widget_set_tooltip_text(b,label);gtk_widget_set_can_focus(b,false);bindAction(b,"clicked",cmd);gtk_box_pack_start(GTK_BOX(box),b,false,false,0);toggles.emplace_back(cmd,b);
    }
    GtkWidget *menu(GtkWidget *bar,const char *label){auto item=gtk_menu_item_new_with_mnemonic(label),sub=gtk_menu_new();gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),sub);gtk_menu_shell_append(GTK_MENU_SHELL(bar),item);return sub;}
    void menuItem(GtkWidget *m,const char *label,const char *id,const char *shortcut=nullptr){auto w=gtk_menu_item_new_with_mnemonic(label);bindAction(w,"activate",id,shortcut);gtk_menu_shell_append(GTK_MENU_SHELL(m),w);}
    GtkWidget *group(GtkWidget *row,const char *label){
        auto col=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);gtk_widget_set_name(col,"toolgroup");gtk_box_pack_start(GTK_BOX(row),col,false,false,0);
        auto content=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_box_pack_start(GTK_BOX(col),content,true,true,0);
        auto caption=gtk_label_new(label);gtk_style_context_add_class(gtk_widget_get_style_context(caption),"group-caption");gtk_box_pack_end(GTK_BOX(col),caption,false,false,0);return content;
    }
    GtkWidget *toolRow(GtkWidget *box){auto row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);gtk_box_pack_start(GTK_BOX(box),row,false,false,0);return row;}
    GtkWidget *tab(const char *name){auto row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);auto scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll),true);gtk_container_add(GTK_CONTAINER(scroll),row);gtk_notebook_append_page(GTK_NOTEBOOK(tabs),scroll,gtk_label_new(name));return row;}
    std::vector<Extension> extensions;
    fs::path extensionDir,recoveryDir,recoveryFile;
    guint recoveryTimer=0;
    bool copying=false,copyDirty=false,closeAfterSave=false;
    std::string copySource,wordCount="Word count updates while you edit.";
    GtkWidget *countLabel{};
    void reloadExtensions(){
        extensions=builtinExtensions();
        for(auto &e:extensions)e.enabled=!fs::exists(extensionDir/(e.path+".disabled"));
        for(const auto &entry:fs::directory_iterator(extensionDir))if(entry.path().extension()==".json"){
            try{auto e=readExtension(entry.path());e.enabled=!fs::exists(e.path+".disabled");extensions.push_back(e);}catch(const std::exception &e){g_warning("Ignoring extension: %s",e.what());}
        }
    }
    void insertExtension(size_t index){
        if(index>=extensions.size()||!ready||busy)return;
        if(!extensions[index].enabled){message("This extension is disabled. Enable it in Tools → Extensions.");return;}
        auto &e=extensions[index];std::string text=e.text;
        if(e.kind=="date"){auto now=g_date_time_new_now_local();auto s=g_date_time_format(now,"%d %B %Y");text=s;g_free(s);g_date_time_unref(now);}
        if(!lok_doc_view_paste(LOK_DOC_VIEW(view),"text/plain;charset=utf-8",text.c_str(),text.size())){error("The extension text could not be inserted.");return;}
        gtk_widget_grab_focus(view);message("Inserted: "+e.name);
    }
    std::string pickFile(const char *title,const char *pattern,bool save=false,const char *folder=nullptr){
        auto d=gtk_file_chooser_dialog_new(title,GTK_WINDOW(window),save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,"Cancel",GTK_RESPONSE_CANCEL,save?"Save":"Open",GTK_RESPONSE_ACCEPT,nullptr);
        if(save){gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),"My-extension.ramdext.json");gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);}
        if(folder)gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d),folder);
        auto filter=gtk_file_filter_new();gtk_file_filter_set_name(filter,pattern);gtk_file_filter_add_pattern(filter,pattern);gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d),filter);
        std::string out;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto f=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));out=f?f:"";g_free(f);}gtk_widget_destroy(d);return out;
    }
    void installExtension(const std::string &path){
        auto e=readExtension(path);gchar *hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,(e.name+e.text).c_str(),-1);auto dest=extensionDir/(std::string(hash)+".json");g_free(hash);
        fs::copy_file(path,dest,fs::copy_options::overwrite_existing);reloadExtensions();
    }
    void extensionManager(){
        for(;;){
            reloadExtensions();auto d=gtk_dialog_new_with_buttons("Extensions",GTK_WINDOW(window),GTK_DIALOG_MODAL,"Close",GTK_RESPONSE_CLOSE,"Install…",1,"Enable / Disable",2,"Remove",3,"Insert",4,nullptr);
            gtk_window_set_default_size(GTK_WINDOW(d),570,370);auto box=gtk_dialog_get_content_area(GTK_DIALOG(d));gtk_container_set_border_width(GTK_CONTAINER(box),20);
            auto intro=gtk_label_new("Document tools and reusable text templates.\nInstalled extensions insert text only; they do not run code.");gtk_label_set_xalign(GTK_LABEL(intro),0);gtk_box_pack_start(GTK_BOX(box),intro,false,false,10);
            auto list=gtk_combo_box_text_new();for(const auto &e:extensions){auto name=e.name+(e.builtin?" · Built in":" · Installed")+(e.enabled?"":" · Disabled");gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(list),name.c_str());}gtk_combo_box_set_active(GTK_COMBO_BOX(list),0);gtk_box_pack_start(GTK_BOX(box),list,false,false,8);
            auto desc=gtk_label_new(extensions[0].description.c_str());gtk_label_set_line_wrap(GTK_LABEL(desc),true);gtk_label_set_xalign(GTK_LABEL(desc),0);gtk_box_pack_start(GTK_BOX(box),desc,true,true,12);
            struct Context{App *app;GtkWidget *description;};Context ctx{this,desc};
            g_signal_connect(list,"changed",G_CALLBACK((+[](GtkComboBox *combo,gpointer p){auto c=static_cast<Context*>(p);int n=gtk_combo_box_get_active(combo);if(n>=0)gtk_label_set_text(GTK_LABEL(c->description),c->app->extensions[n].description.c_str());})),&ctx);
            gtk_widget_show_all(d);int response=gtk_dialog_run(GTK_DIALOG(d));int n=gtk_combo_box_get_active(GTK_COMBO_BOX(list));gtk_widget_destroy(d);
            if(response==1){auto path=pickFile("Install text extension","*.json");if(!path.empty())try{installExtension(path);}catch(const std::exception &e){error(e.what());}}
            else if(response==2&&n>=0){auto &e=extensions[n];auto disabled=e.builtin?extensionDir/(e.path+".disabled"):fs::path(e.path+".disabled");if(e.enabled)std::ofstream(disabled)<<"disabled";else fs::remove(disabled);}
            else if(response==3&&n>=0){auto &e=extensions[n];if(e.builtin)message("Built-in extensions can be disabled, but not removed.");else{fs::remove(e.path);fs::remove(e.path+".disabled");}}
            else if(response==4&&n>=0){if(!extensions[n].enabled){message("Enable this extension before using it.");continue;}insertExtension(n);break;}
            else break;
        }
    }
    void extensionBuilder(){
        auto d=gtk_dialog_new_with_buttons("Build a text extension",GTK_WINDOW(window),GTK_DIALOG_MODAL,"Cancel",GTK_RESPONSE_CANCEL,"Save extension…",GTK_RESPONSE_ACCEPT,nullptr);gtk_window_set_default_size(GTK_WINDOW(d),600,480);
        auto box=gtk_dialog_get_content_area(GTK_DIALOG(d));gtk_container_set_border_width(GTK_CONTAINER(box),20);
        auto name=gtk_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(name),"Extension name");gtk_entry_set_max_length(GTK_ENTRY(name),80);gtk_box_pack_start(GTK_BOX(box),name,false,false,6);
        auto desc=gtk_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(desc),"Short description");gtk_entry_set_max_length(GTK_ENTRY(desc),500);gtk_box_pack_start(GTK_BOX(box),desc,false,false,6);
        auto note=gtk_label_new("Write the reusable text this extension will insert.");gtk_box_pack_start(GTK_BOX(box),note,false,false,8);
        auto scroll=gtk_scrolled_window_new(nullptr,nullptr),editor=gtk_text_view_new();gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editor),GTK_WRAP_WORD_CHAR);gtk_container_add(GTK_CONTAINER(scroll),editor);gtk_box_pack_start(GTK_BOX(box),scroll,true,true,6);
        gtk_widget_show_all(d);
        if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){
            auto buf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));GtkTextIter start,end;gtk_text_buffer_get_bounds(buf,&start,&end);auto text=gtk_text_buffer_get_text(buf,&start,&end,false);
            auto b=json_builder_new();json_builder_begin_object(b);for(auto item:std::vector<std::pair<const char*,const char*>>{{"format","ramd-docs-extension-1"},{"name",gtk_entry_get_text(GTK_ENTRY(name))},{"description",gtk_entry_get_text(GTK_ENTRY(desc))},{"text",text}}){json_builder_set_member_name(b,item.first);json_builder_add_string_value(b,item.second);}json_builder_end_object(b);auto node=json_builder_get_root(b);auto gen=json_generator_new();json_generator_set_root(gen,node);auto json=json_generator_to_data(gen,nullptr);
            auto path=pickFile("Save extension","*.json",true);if(!path.empty()){std::ofstream file(path);file<<json;file.close();try{installExtension(path);message("Extension created and installed.");}catch(const std::exception &e){error(e.what());}}
            g_free(text);g_free(json);json_node_free(node);g_object_unref(gen);g_object_unref(b);
        }gtk_widget_destroy(d);
    }
    void recoveryCopy(){
        if(!ready||busy||!dirty)return;
        copying=true;copyDirty=dirty;copySource=filename;pendingPath=recoveryFile.string();pendingCommand=".uno:SaveAs";
        auto args=arguments({{"URL",{"string",uri(pendingPath)}},{"FilterName",{"string","writer8"}},{"SaveTo",{"boolean","true"}},{"Overwrite",{"boolean","true"}}});
        setBusy(true);message("Writing recovery copy…");lok_doc_view_post_command(LOK_DOC_VIEW(view),pendingCommand.c_str(),args.c_str(),true);
    }
    void recoverDocument(){
        if(!canLeave())return;auto path=pickFile("Open a recovery copy","*.odt",false,recoveryDir.string().c_str());if(path.empty())return;dirty=false;load(path,true);
    }
    void insertImage(){
        if(!ready||busy)return;auto path=pickFile("Insert image (PNG or JPEG)","*");if(path.empty())return;
        GError *err=nullptr;auto image=gdk_pixbuf_new_from_file(path.c_str(),&err);if(!image){error(err?err->message:"Unsupported image.");g_clear_error(&err);return;}g_object_unref(image);
        command(".uno:InsertGraphic",{{"FileName",{"string",uri(path)}},{"AsLink",{"boolean","false"}}});
    }
    void showWordCount(){auto d=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE,"%s",wordCount.c_str());gtk_window_set_title(GTK_WINDOW(d),"Word count");gtk_dialog_run(GTK_DIALOG(d));gtk_widget_destroy(d);}
    ~App(){if(recoveryTimer)g_source_remove(recoveryTimer);std::error_code ec;fs::remove(recoveryFile,ec);fs::remove_all(session,ec);}
    App(){
        gchar *tmp=g_dir_make_tmp("ramd-docs-XXXXXX",nullptr);session=tmp?tmp:"";g_free(tmp);
        if(session.empty())throw std::runtime_error("Could not create working directory");
        extensionDir=fs::path(g_get_user_data_dir())/"ramd-docs"/"extensions";recoveryDir=fs::path(g_get_user_data_dir())/"ramd-docs"/"recovery";
        fs::create_directories(extensionDir);fs::create_directories(recoveryDir);recoveryFile=recoveryDir/(fs::path(session).filename().string()+".odt");reloadExtensions();
        recoveryTimer=g_timeout_add_seconds(120,[](gpointer p)->gboolean{static_cast<App*>(p)->recoveryCopy();return G_SOURCE_CONTINUE;},this);
        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_default_size(GTK_WINDOW(window),1200,850);
        gtk_window_set_title(GTK_WINDOW(window),"RAMD Docs");
        accels=gtk_accel_group_new();gtk_window_add_accel_group(GTK_WINDOW(window),accels);g_object_unref(accels);
        heading=gtk_header_bar_new();gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(heading),true);
        gtk_header_bar_set_title(GTK_HEADER_BAR(heading),"RAMD Docs");
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR(heading),"RAMD Docs 1.0");
        gtk_window_set_titlebar(GTK_WINDOW(window),heading);
        auto root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(window),root);
        auto loader=gdk_pixbuf_loader_new();gdk_pixbuf_loader_write(loader,reinterpret_cast<const guchar*>(appIcon),strlen(appIcon),nullptr);gdk_pixbuf_loader_close(loader,nullptr);
        auto logo=gdk_pixbuf_loader_get_pixbuf(loader);
        if(!logo)throw std::runtime_error("App icon could not load. Install librsvg2-common.");
        if(logo){gtk_window_set_icon(GTK_WINDOW(window),logo);auto small=gdk_pixbuf_scale_simple(logo,32,32,GDK_INTERP_BILINEAR);auto mark=gtk_image_new_from_pixbuf(small);gtk_header_bar_pack_start(GTK_HEADER_BAR(heading),mark);g_object_unref(small);}g_object_unref(loader);
        auto quick=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);gtk_header_bar_pack_start(GTK_HEADER_BAR(heading),quick);
        actionButton(quick,"document-open-symbolic","Open document (Ctrl+O)","open",true);
        actionButton(quick,"document-save-symbolic","Save (Ctrl+S)","save",true);
        actionButton(quick,"edit-undo-symbolic","Undo (Ctrl+Z)",".uno:Undo",true);
        auto exportBox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);gtk_header_bar_pack_end(GTK_HEADER_BAR(heading),exportBox);
        actionButton(exportBox,"document-send-symbolic","Export PDF","pdf");
        auto menubar=gtk_menu_bar_new();gtk_widget_set_name(menubar,"menubar");gtk_box_pack_start(GTK_BOX(root),menubar,false,false,0);
        auto file=menu(menubar,"_File");menuItem(file,"_New document","new","<Control>n");menuItem(file,"_Open…","open","<Control>o");menuItem(file,"_Save","save","<Control>s");menuItem(file,"Save _as…","save-as","<Control><Shift>s");menuItem(file,"Export _PDF…","pdf","<Control><Shift>e");menuItem(file,"Open _recovery copy…","recover");menuItem(file,"Write recovery copy now","recovery-copy");menuItem(file,"_Close","close","<Control>w");
        auto edit=menu(menubar,"_Edit");menuItem(edit,"_Undo",".uno:Undo","<Control>z");menuItem(edit,"_Redo",".uno:Redo","<Control>y");menuItem(edit,"Cu_t text","cut");menuItem(edit,"_Copy text","copy");menuItem(edit,"_Paste text","paste");menuItem(edit,"Select _all",".uno:SelectAll","<Control>a");menuItem(edit,"_Find…","find","<Control>f");
        auto insert=menu(menubar,"_Insert");menuItem(insert,"_Table…","table");menuItem(insert,"_Page break",".uno:InsertPagebreak","<Control>Return");
        menuItem(insert,"_Image…","image");menuItem(insert,"Page number field",".uno:InsertPageNumberField");
        auto format=menu(menubar,"F_ormat");menuItem(format,"_Bold",".uno:Bold","<Control>b");menuItem(format,"_Italic",".uno:Italic","<Control>i");menuItem(format,"_Underline",".uno:Underline","<Control>u");menuItem(format,"Clear direct formatting",".uno:ResetAttributes","<Control>m");
        auto vm=menu(menubar,"_View");menuItem(vm,"Zoom _in","zoom-in");menuItem(vm,"Zoom _out","zoom-out");menuItem(vm,"_Actual size","zoom-reset");
        auto layout=menu(menubar,"_Layout");menuItem(layout,"Toggle portrait / landscape",".uno:ToggleOrientation");
        auto toolsMenu=menu(menubar,"_Tools");menuItem(toolsMenu,"Word _count","word-count");menuItem(toolsMenu,"_Extensions…","extensions");menuItem(toolsMenu,"_Build a text extension…","build-extension");
        auto help=menu(menubar,"_Help");menuItem(help,"_About RAMD Docs","about");
        editorControls=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_box_pack_start(GTK_BOX(root),editorControls,false,false,0);
        tabs=gtk_notebook_new();gtk_widget_set_name(tabs,"ribbon");gtk_notebook_set_show_border(GTK_NOTEBOOK(tabs),false);gtk_box_pack_start(GTK_BOX(editorControls),tabs,false,false,0);
        auto home=tab("Home");
        auto clip=group(home,"Clipboard");auto cr=toolRow(clip);actionButton(cr,"edit-paste-symbolic","Paste text","paste");cr=toolRow(clip);actionButton(cr,"edit-cut-symbolic","Cut text","cut",true);actionButton(cr,"edit-copy-symbolic","Copy text","copy",true);
        auto fontGroup=group(home,"Font");auto row=toolRow(fontGroup);
        auto fonts=gtk_combo_box_text_new();for(auto s:{"Liberation Serif","Liberation Sans","DejaVu Sans","DejaVu Serif","Carlito"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fonts),s);
        gtk_combo_box_set_active(GTK_COMBO_BOX(fonts),0);gtk_widget_set_tooltip_text(fonts,"Apply font to selection");gtk_box_pack_start(GTK_BOX(row),fonts,false,false,0);
        g_signal_connect(fonts,"changed",G_CALLBACK((+[](GtkComboBox *b,gpointer p){auto t=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));if(t){static_cast<App*>(p)->command(".uno:CharFontName",{{"CharFontName",{"string",t}}});g_free(t);}})),this);
        auto size=gtk_combo_box_text_new();for(auto s:{"8","9","10","11","12","14","16","18","20","24","28","32","36","48","72"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(size),s);gtk_combo_box_set_active(GTK_COMBO_BOX(size),3);gtk_widget_set_tooltip_text(size,"Apply font size");gtk_box_pack_start(GTK_BOX(row),size,false,false,0);
        g_signal_connect(size,"changed",G_CALLBACK((+[](GtkComboBox *b,gpointer p){auto t=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));if(t){static_cast<App*>(p)->command(".uno:FontHeight",{{"FontHeight",{"float",t}}});g_free(t);}})),this);
        row=toolRow(fontGroup);toggle(row,"format-text-bold-symbolic","Bold (Ctrl+B)",".uno:Bold");toggle(row,"format-text-italic-symbolic","Italic (Ctrl+I)",".uno:Italic");toggle(row,"format-text-underline-symbolic","Underline (Ctrl+U)",".uno:Underline");toggle(row,"format-text-strikethrough-symbolic","Strikethrough",".uno:Strikeout");actionButton(row,"edit-clear-symbolic","Clear formatting",".uno:ResetAttributes",true);
        auto para=group(home,"Paragraph");row=toolRow(para);toggle(row,"format-justify-left-symbolic","Align left",".uno:LeftPara");toggle(row,"format-justify-center-symbolic","Center",".uno:CenterPara");toggle(row,"format-justify-right-symbolic","Align right",".uno:RightPara");toggle(row,"format-justify-fill-symbolic","Justify",".uno:JustifyPara");
        row=toolRow(para);actionButton(row,"view-list-bullet-symbolic","Bulleted list",".uno:DefaultBullet",true);actionButton(row,"view-list-ordered-symbolic","Numbered list",".uno:DefaultNumbering",true);actionButton(row,"format-indent-less-symbolic","Decrease indent",".uno:DecrementIndent",true);actionButton(row,"format-indent-more-symbolic","Increase indent",".uno:IncrementIndent",true);
        auto styleGroup=group(home,"Styles");auto styles=gtk_combo_box_text_new();for(auto s:{"Body text","Title","Heading 1","Heading 2","Heading 3"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(styles),s);gtk_combo_box_set_active(GTK_COMBO_BOX(styles),0);gtk_widget_set_tooltip_text(styles,"Apply paragraph style");gtk_box_pack_start(GTK_BOX(styleGroup),styles,false,false,0);
        g_signal_connect(styles,"changed",G_CALLBACK((+[](GtkComboBox *b,gpointer p){const char *names[]={"Text body","Title","Heading 1","Heading 2","Heading 3"};int n=gtk_combo_box_get_active(b);if(n>=0&&n<5)static_cast<App*>(p)->command(".uno:StyleApply",{{"Style",{"string",names[n]}},{"FamilyName",{"string","ParagraphStyles"}}});})),this);
        auto findGroup=group(home,"Editing");search=gtk_search_entry_new();gtk_entry_set_width_chars(GTK_ENTRY(search),16);gtk_entry_set_placeholder_text(GTK_ENTRY(search),"Find text…");gtk_box_pack_start(GTK_BOX(findGroup),search,false,false,0);
        g_signal_connect(search,"activate",G_CALLBACK((+[](GtkEntry *e,gpointer p){auto a=static_cast<App*>(p);if(a->ready&&!a->busy)lok_doc_view_find_next(LOK_DOC_VIEW(a->view),gtk_entry_get_text(e),false);})),this);
        row=toolRow(findGroup);actionButton(row,"edit-redo-symbolic","Redo",".uno:Redo",true);actionButton(row,"edit-select-all-symbolic","Select all",".uno:SelectAll",true);
        auto insertTab=tab("Insert");auto tables=group(insertTab,"Tables");actionButton(toolRow(tables),"view-grid-symbolic","Insert table…","table");
        auto pages=group(insertTab,"Pages");actionButton(toolRow(pages),"document-new-symbolic","Page break",".uno:InsertPagebreak");
        auto images=group(insertTab,"Images");actionButton(toolRow(images),"insert-image-symbolic","Insert image…","image");
        auto fields=group(insertTab,"Fields");actionButton(toolRow(fields),"document-properties-symbolic","Page number",".uno:InsertPageNumberField");
        auto layoutTab=tab("Layout");auto pg=group(layoutTab,"Page layout");actionButton(toolRow(pg),"object-rotate-right-symbolic","Portrait / landscape",".uno:ToggleOrientation");
        auto extTab=tab("Extensions");auto extGroup=group(extTab,"Workspace extensions");auto er=toolRow(extGroup);actionButton(er,"application-x-addon-symbolic","Manage extensions","extensions");actionButton(er,"document-new-symbolic","Build an extension","build-extension");
        auto writing=group(extTab,"Writing tools");auto wr=toolRow(writing);actionButton(wr,"view-list-symbolic","Meeting notes","meeting-notes");actionButton(wr,"document-properties-symbolic","Project brief","project-brief");wr=toolRow(writing);actionButton(wr,"mail-message-new-symbolic","Letter","letter");actionButton(wr,"x-office-calendar-symbolic","Today’s date","date");
        auto viewTab=tab("View");auto viewGroup=group(viewTab,"Zoom");row=toolRow(viewGroup);actionButton(row,"zoom-out-symbolic","Zoom out","zoom-out");actionButton(row,"zoom-original-symbolic","100%","zoom-reset");actionButton(row,"zoom-in-symbolic","Zoom in","zoom-in");
        GError *err=nullptr;auto profile=uri((fs::path(session)/"profile").string());
        #ifdef _WIN32
        auto officePath=windowsOfficePath();
        view=lok_doc_view_new_from_user_profile(officePath.c_str(),profile.c_str(),nullptr,&err);
#else
        view=lok_doc_view_new_from_user_profile(nullptr,profile.c_str(),nullptr,&err);
#endif
        if(!view){std::string msg=err?err->message:"LibreOfficeKit could not start";g_clear_error(&err);throw std::runtime_error(msg);}
        auto scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_container_add(GTK_CONTAINER(scroll),view);
        gtk_box_pack_start(GTK_BOX(root),scroll,true,true,0);
        auto bottom=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);gtk_widget_set_name(bottom,"statusbar");gtk_box_pack_end(GTK_BOX(root),bottom,false,false,0);
        status=gtk_label_new("Start with a blank document or open a Word file.");gtk_box_pack_start(GTK_BOX(bottom),status,true,true,12);gtk_label_set_xalign(GTK_LABEL(status),0);
        countLabel=gtk_label_new("Words: —");gtk_box_pack_end(GTK_BOX(bottom),countLabel,false,false,8);
        auto zoom=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,50,200,10);zoomControl=zoom;gtk_range_set_value(GTK_RANGE(zoom),100);gtk_widget_set_size_request(zoom,180,-1);gtk_scale_set_value_pos(GTK_SCALE(zoom),GTK_POS_LEFT);gtk_box_pack_end(GTK_BOX(bottom),zoom,false,false,8);
        g_signal_connect(zoom,"value-changed",G_CALLBACK((+[](GtkRange *r,gpointer p){auto a=static_cast<App*>(p);if(a->ready)lok_doc_view_set_zoom(LOK_DOC_VIEW(a->view),gtk_range_get_value(r)/100);})),this);
        g_signal_connect(view,"command-result",G_CALLBACK(result),this);
        g_signal_connect(view,"command-changed",G_CALLBACK((+[](LOKDocView*,gchar *s,gpointer p){auto a=static_cast<App*>(p);std::string text=s?s:"";if(text.rfind(".uno:StateWordCount=",0)==0){a->wordCount=text.substr(20);gtk_label_set_text(GTK_LABEL(a->countLabel),a->wordCount.c_str());}if(text.rfind(".uno:ModifiedStatus=",0)==0){a->dirty=text.substr(std::string(".uno:ModifiedStatus=").size())=="true";a->title();}for(const auto &item:a->toggles){const auto prefix=item.first+"=";if(text.rfind(prefix,0)==0){a->syncing=true;gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(item.second),text.substr(prefix.size())=="true");a->syncing=false;}}})),this);
        g_signal_connect(view,"search-not-found",G_CALLBACK((+[](LOKDocView*,gchar*,gpointer p){static_cast<App*>(p)->message("No matching text found.");})),this);
        g_signal_connect(window,"delete-event",G_CALLBACK((+[](GtkWidget*,GdkEvent*,gpointer p)->gboolean{return !static_cast<App*>(p)->canLeave();})),this);
        g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),nullptr);
        gtk_widget_set_sensitive(editorControls,false);
        auto css=gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,R"(
window{background:#e7e8ed;color:#242331;font-family:Sans;font-size:13px}
headerbar{background:#f9f9fc;color:#252336;border:0;border-bottom:1px solid #dddde6;padding:6px 12px;min-height:48px}
headerbar .title{font-weight:600}headerbar .subtitle{color:#777386;font-size:11px}
headerbar button{background:transparent;border-color:transparent}
#menubar{background:#f9f9fc;padding:0 12px;border-bottom:1px solid #e5e3ec}
#menubar>menuitem{padding:7px 13px}menu{background:#fff;padding:6px;border:1px solid #ddd9e8}menuitem:hover{background:#ece7fa;color:#483585}
#ribbon{background:#fff}#ribbon>header{background:#f9f9fc;border-bottom:1px solid #e5e3ec;padding:0 16px}
#ribbon>header tab{padding:8px 20px;border-bottom:3px solid transparent}#ribbon>header tab:checked{color:#5643a3;border-bottom-color:#5643a3;font-weight:600}
#toolgroup{padding:12px 14px 8px;border-right:1px solid #eeeaf3;min-height:88px}
.group-caption{color:#817a8e;font-size:10px;margin-top:4px}
button{border-radius:5px;padding:6px 8px;box-shadow:none;min-height:20px}
#ribbon button{background:transparent;border:1px solid transparent}
#ribbon button:hover,headerbar button:hover{background:#eee9fa;border-color:#dcd3f1}
#ribbon button:checked{background:#e5ddf7;color:#49318c;border-color:#c9b9e8}
#ribbon combobox button{background:#fff;border:1px solid #ddd9e6;padding:5px 6px}
entry{background:#fff;color:#242331;border:1px solid #ddd9e6;border-radius:5px;padding:5px 8px}
#statusbar{background:#f9f9fc;padding:5px;border-top:1px solid #dbd8e4;font-size:11px;color:#706a80}
scale highlight{background:#7160b5}scale slider{background:#fff;border:1px solid #b7adc9}

)",-1,nullptr);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);g_object_unref(css);
        gtk_widget_show_all(window);
    }
};
#ifndef RAMD_DOCS_NO_MAIN
int runApp(int argc,char **argv){
    prepareWindowsRuntime();gtk_init(&argc,&argv);g_set_application_name("RAMD Docs");
    try{App a;if(argc>1)a.load(fs::absolute(argv[1]).string());else a.newDocument();gtk_main();}
    catch(const std::exception &e){g_printerr("RAMD Docs: %s\n",e.what());return 1;}
    return 0;
}

int main(int argc,char **argv){int status=runApp(argc,argv);finishProcess(status);}
#endif
