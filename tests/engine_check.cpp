#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKit.hxx>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include "../src/process_exit.h"
int runCheck(int argc,char **argv){
    if(argc!=4){std::cerr<<"engine-check PROGRAM_DIR PROFILE_URL OUTPUT_DIR\n";return 2;}
    std::filesystem::create_directories(argv[3]);
    auto source=std::filesystem::absolute(std::filesystem::path(argv[3])/"fixture.fodt");
    std::ofstream(source)<<R"(<?xml version="1.0"?><office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" office:version="1.2" office:mimetype="application/vnd.oasis.opendocument.text"><office:body><office:text><text:h text:outline-level="1">RAMD Docs test</text:h><text:p>RAMD_ROUNDTRIP_2026</text:p></office:text></office:body></office:document>)";
    std::unique_ptr<lok::Office> office(lok::lok_cpp_init(argv[1],argv[2]));
    if(!office){std::cerr<<"Engine initialization failed\n";return 1;}
    std::unique_ptr<lok::Document> document(office->documentLoad(source.c_str()));
    if(!document){std::cerr<<"Document load failed\n";return 1;}
    document->initializeForRendering();
    long width=0,height=0;document->getDocumentSize(&width,&height);
    if(width<=0||height<=0){std::cerr<<"Invalid document size\n";return 1;}
    std::vector<unsigned char> pixels(512*512*4);
    document->paintTile(pixels.data(),512,512,0,0,width,height);
    auto docx=std::filesystem::absolute(std::filesystem::path(argv[3])/"roundtrip.docx");
    auto pdf=std::filesystem::absolute(std::filesystem::path(argv[3])/"roundtrip.pdf");
    if(!document->saveAs(docx.c_str(),"docx")||!document->saveAs(pdf.c_str(),"pdf"))return 1;
    document.reset();document.reset(office->documentLoad(docx.c_str()));
    if(!document){std::cerr<<"DOCX reopen failed\n";return 1;}
    document->initializeForRendering();document->getDocumentSize(&width,&height);
    if(width<=0||height<=0){std::cerr<<"Invalid document size\n";return 1;}
    std::cout<<"PASS: engine load, paginated render, DOCX export/reopen, PDF export"<<std::endl;
    return 0;
}

int main(int argc,char **argv){int status=runCheck(argc,argv);finishProcess(status);}
