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
namespace fs = std::filesystem;
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
static std::string uri(const std::string &path){gchar *p=g_filename_to_uri(fs::absolute(path).c_str(),nullptr,nullptr);std::string out=p?p:"";g_free(p);return out;}
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
struct App {
    GtkWidget *window{},*view{},*status{},*heading{},*editorControls{},*search{};
    bool ready=false,dirty=false,busy=false;
    std::string filename,pendingPath,pendingCommand,session;
    GtkAccelGroup *accels{};
    void message(const std::string &text){gtk_label_set_text(GTK_LABEL(status),text.c_str());}
    void error(const std::string &text){
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
            a->message("Ready · Document tools work offline");gtk_widget_grab_focus(a->view);
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
        pendingCommand=pdf?".uno:SaveTo":".uno:SaveAs";pendingPath=target;
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
            bool pdf=a->pendingCommand==".uno:SaveTo";
            if(ok&&!pdf){a->filename=a->pendingPath;a->dirty=false;a->title();}
            a->pendingCommand.clear();a->pendingPath.clear();a->setBusy(false);
            if(ok)a->message(pdf?"PDF exported":"Document saved");
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
    ~App(){std::error_code ec;fs::remove_all(session,ec);}
    App(){
        gchar *tmp=g_dir_make_tmp("ramd-docs-XXXXXX",nullptr);session=tmp?tmp:"";g_free(tmp);
        if(session.empty())throw std::runtime_error("Could not create working directory");
        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_default_size(GTK_WINDOW(window),1200,850);
        gtk_window_set_title(GTK_WINDOW(window),"RAMD Docs");
        accels=gtk_accel_group_new();gtk_window_add_accel_group(GTK_WINDOW(window),accels);g_object_unref(accels);
        heading=gtk_header_bar_new();gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(heading),true);
        gtk_header_bar_set_title(GTK_HEADER_BAR(heading),"RAMD Docs");
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR(heading),"RAMD STUDIOS  /  Development build 0.1");
        gtk_window_set_titlebar(GTK_WINDOW(window),heading);
        auto root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(window),root);
        auto filebar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);gtk_widget_set_name(filebar,"filebar");
        gtk_box_pack_start(GTK_BOX(root),filebar,false,false,0);
        button(filebar,"New",[](GtkButton*,gpointer p){static_cast<App*>(p)->newDocument();},"<Control>n");
        button(filebar,"Open…",[](GtkButton*,gpointer p){auto a=static_cast<App*>(p);if(!a->canLeave())return;auto f=a->choose(false,"Open document","","Word / OpenDocument");if(!f.empty()){a->dirty=false;a->load(f);}},"<Control>o");
        button(filebar,"Save",[](GtkButton*,gpointer p){static_cast<App*>(p)->save(false,false);},"<Control>s");
        button(filebar,"Save As…",[](GtkButton*,gpointer p){static_cast<App*>(p)->save(true,false);},"<Control><Shift>s");
        button(filebar,"Export PDF",[](GtkButton*,gpointer p){static_cast<App*>(p)->save(true,true);});
        auto spacer=gtk_label_new("");gtk_box_pack_start(GTK_BOX(filebar),spacer,true,true,0);
        button(filebar,"About",[](GtkButton*,gpointer p){auto a=static_cast<App*>(p);gtk_show_about_dialog(GTK_WINDOW(a->window),"program-name","RAMD Docs","version","0.1 Development","comments","Word-processing workspace by RAMD Studios. Powered by LibreOfficeKit. This development build is not a finished Microsoft Word replacement.",nullptr);});
        editorControls=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_widget_set_name(editorControls,"ribbon");
        gtk_box_pack_start(GTK_BOX(root),editorControls,false,false,0);
        auto row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);gtk_box_pack_start(GTK_BOX(editorControls),row,false,false,0);
        unoButton(row,"Undo",".uno:Undo");unoButton(row,"Redo",".uno:Redo");
        auto styles=gtk_combo_box_text_new();
        for(auto s:{"Body text","Title","Heading 1","Heading 2","Heading 3"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(styles),s);
        gtk_combo_box_set_active(GTK_COMBO_BOX(styles),0);gtk_box_pack_start(GTK_BOX(row),styles,false,false,8);
        g_signal_connect(styles,"changed",G_CALLBACK((+[](GtkComboBox *b,gpointer p){const char *names[]={"Text body","Title","Heading 1","Heading 2","Heading 3"};int n=gtk_combo_box_get_active(b);if(n>=0&&n<5)static_cast<App*>(p)->command(".uno:StyleApply",{{"Style",{"string",names[n]}},{"FamilyName",{"string","ParagraphStyles"}}});})),this);
        unoButton(row,"Bold",".uno:Bold");unoButton(row,"Italic",".uno:Italic");unoButton(row,"Underline",".uno:Underline");
        unoButton(row,"Align left",".uno:LeftPara");unoButton(row,"Center",".uno:CenterPara");unoButton(row,"Align right",".uno:RightPara");
        unoButton(row,"Bullets",".uno:DefaultBullet");unoButton(row,"Numbers",".uno:DefaultNumbering");
        auto row2=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);gtk_box_pack_start(GTK_BOX(editorControls),row2,false,false,0);
        auto fonts=gtk_combo_box_text_new();
        for(auto s:{"Liberation Serif","Liberation Sans","DejaVu Sans","DejaVu Serif","Carlito"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fonts),s);
        gtk_combo_box_set_active(GTK_COMBO_BOX(fonts),0);gtk_box_pack_start(GTK_BOX(row2),fonts,false,false,0);
        g_signal_connect(fonts,"changed",G_CALLBACK((+[](GtkComboBox *b,gpointer p){auto text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));if(text){static_cast<App*>(p)->command(".uno:CharFontName",{{"CharFontName",{"string",text}}});g_free(text);}})),this);
        auto size=gtk_spin_button_new_with_range(6,96,1);gtk_spin_button_set_value(GTK_SPIN_BUTTON(size),11);gtk_box_pack_start(GTK_BOX(row2),size,false,false,0);
        g_signal_connect(size,"value-changed",G_CALLBACK((+[](GtkSpinButton *b,gpointer p){static_cast<App*>(p)->command(".uno:FontHeight",{{"FontHeight",{"float",std::to_string(gtk_spin_button_get_value(b))}}});})),this);
        unoButton(row2,"Page break",".uno:InsertPagebreak");
        button(row2,"Table 3 × 3",[](GtkButton*,gpointer p){static_cast<App*>(p)->command(".uno:InsertTable",{{"Rows",{"long","3"}},{"Columns",{"long","3"}}});});
        search=gtk_search_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(search),"Find in document");gtk_box_pack_end(GTK_BOX(row2),search,false,false,0);
        g_signal_connect(search,"activate",G_CALLBACK((+[](GtkEntry *e,gpointer p){auto a=static_cast<App*>(p);if(a->ready&&!a->busy)lok_doc_view_find_next(LOK_DOC_VIEW(a->view),gtk_entry_get_text(e),false);})),this);
        GError *err=nullptr;auto profile=uri((fs::path(session)/"profile").string());
        view=lok_doc_view_new_from_user_profile(nullptr,profile.c_str(),nullptr,&err);
        if(!view){std::string msg=err?err->message:"LibreOfficeKit could not start";g_clear_error(&err);throw std::runtime_error(msg);}
        auto scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_container_add(GTK_CONTAINER(scroll),view);
        gtk_box_pack_start(GTK_BOX(root),scroll,true,true,0);
        auto bottom=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);gtk_widget_set_name(bottom,"statusbar");gtk_box_pack_end(GTK_BOX(root),bottom,false,false,0);
        status=gtk_label_new("Start with a blank document or open a Word file.");gtk_box_pack_start(GTK_BOX(bottom),status,true,true,12);gtk_label_set_xalign(GTK_LABEL(status),0);
        auto zoom=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,50,200,10);gtk_range_set_value(GTK_RANGE(zoom),100);gtk_widget_set_size_request(zoom,180,-1);gtk_scale_set_value_pos(GTK_SCALE(zoom),GTK_POS_LEFT);gtk_box_pack_end(GTK_BOX(bottom),zoom,false,false,8);
        g_signal_connect(zoom,"value-changed",G_CALLBACK((+[](GtkRange *r,gpointer p){auto a=static_cast<App*>(p);if(a->ready)lok_doc_view_set_zoom(LOK_DOC_VIEW(a->view),gtk_range_get_value(r)/100);})),this);
        g_signal_connect(view,"command-result",G_CALLBACK(result),this);
        g_signal_connect(view,"command-changed",G_CALLBACK((+[](LOKDocView*,gchar *s,gpointer p){auto a=static_cast<App*>(p);std::string text=s?s:"";if(text.rfind(".uno:ModifiedStatus=",0)==0){a->dirty=text.substr(std::string(".uno:ModifiedStatus=").size())=="true";a->title();}})),this);
        g_signal_connect(view,"search-not-found",G_CALLBACK((+[](LOKDocView*,gchar*,gpointer p){static_cast<App*>(p)->message("No matching text found.");})),this);
        g_signal_connect(window,"delete-event",G_CALLBACK((+[](GtkWidget*,GdkEvent*,gpointer p)->gboolean{return !static_cast<App*>(p)->canLeave();})),this);
        g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),nullptr);
        gtk_widget_set_sensitive(editorControls,false);
        auto css=gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,R"(
window{background:#e9e9e6;color:#253343}
headerbar{background:#183b38;color:#fff;border:0;padding:12px}
headerbar .subtitle{color:#b8d3cc}
#filebar{background:#f8f8f5;padding:10px 16px;border-bottom:1px solid #d5dcd7}
#ribbon{background:#fff;padding:14px 16px;border-bottom:1px solid #ced5d1}
button{border-radius:4px;padding:6px 10px;box-shadow:none}
#ribbon button{background:#f5f7f5;border:1px solid #d9e0da}
#ribbon button:hover{background:#dfede5}
#statusbar{background:#f8f8f5;padding:4px;border-top:1px solid #ced5d1}
)",-1,nullptr);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);g_object_unref(css);
        gtk_widget_show_all(window);
    }
};
int main(int argc,char **argv){
    gtk_init(&argc,&argv);g_set_application_name("RAMD Docs");
    try{App a;if(argc>1)a.load(fs::absolute(argv[1]).string());else a.newDocument();gtk_main();}
    catch(const std::exception &e){g_printerr("RAMD Docs: %s\n",e.what());return 1;}
}
