#pragma once
#include <set>
struct Extension {std::string name,description,text,kind="text",path;bool enabled=true,builtin=false;};
static std::vector<Extension> builtinExtensions(){return {
 {"Meeting notes","Insert a structured meeting agenda and notes.","MEETING NOTES\nDate: \nParticipants: \n\nAgenda\n• \n\nDecisions\n• \n\nAction items\nOwner — Task — Due date\n", "text","meeting",true,true},
 {"Project brief","Start a project brief with goals and milestones.","PROJECT BRIEF\n\nProject name: \nOwner: \n\nOverview\n\nGoals\n\nScope\n\nMilestones\n\nNext steps\n","text","brief",true,true},
 {"Business letter","Insert a letter structure.","Your name\nYour address\n\nRecipient\nAddress\n\nSubject: \n\nDear ,\n\n\nKind regards,\n","text","letter",true,true},
 {"Today's date","Insert the current local date.","","date","date",true,true}
};}
static std::string jsonString(JsonObject *o,const char *key){if(!json_object_has_member(o,key))throw std::runtime_error(std::string("Missing extension field: ")+key);auto n=json_object_get_member(o,key);if(!JSON_NODE_HOLDS_VALUE(n)||json_node_get_value_type(n)!=G_TYPE_STRING)throw std::runtime_error(std::string("Invalid field: ")+key);return json_node_get_string(n);}
static Extension readExtension(const fs::path &path){
 if(fs::file_size(path)>65536)throw std::runtime_error("Extension exceeds 64 KB.");
 auto parser=json_parser_new();GError *err=nullptr;
 if(!json_parser_load_from_file(parser,path.string().c_str(),&err)){std::string msg=err?err->message:"Invalid extension JSON";g_clear_error(&err);g_object_unref(parser);throw std::runtime_error(msg);}
 Extension e;
 try{auto root=json_parser_get_root(parser);if(!JSON_NODE_HOLDS_OBJECT(root))throw std::runtime_error("Extension must be a JSON object.");auto o=json_node_get_object(root);
 if(jsonString(o,"format")!="ramd-docs-extension-1")throw std::runtime_error("Unsupported extension format.");
 e.name=jsonString(o,"name");e.description=jsonString(o,"description");e.text=jsonString(o,"text");e.path=path.string();
 if(e.name.empty()||e.name.size()>80||e.description.size()>500||e.text.size()>60000)throw std::runtime_error("Extension fields are too long or the name is empty.");
 // Only declarative text extensions are supported. No executable code or remote resources.
 }catch(...){g_object_unref(parser);throw;}g_object_unref(parser);return e;
}
