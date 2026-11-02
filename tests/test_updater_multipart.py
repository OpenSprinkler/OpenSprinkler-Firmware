"""Execute both vendored multipart parsers with host TCP/String adapters."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
stub = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <memory>
#include <new>
#include "platform/update_http_reader.h"
#define F(x) x
#define FPSTR(x) x
#define DBGWS(...) ((void)0)
#define log_v(...) ((void)0)
#define log_e(...) ((void)0)
#define WEBSERVER_MAX_POST_ARGS 32
#define WEBSERVER_MAX_LINE_LEN 1024
#define WEBSERVER_MAX_MULTIPART_SKIP_LINES 32
#define HTTP_MAX_POST_WAIT 5000
#define HTTP_UPLOAD_BUFLEN 64
struct String:std::string {
 using std::string::string;using std::string::operator=;
 String(const std::string&s):std::string(s){}
 int indexOf(char c,size_t pos=0)const{auto n=find(c,pos);return n==npos?-1:int(n);}
 String substring(unsigned a,unsigned b=UINT32_MAX)const{
   a=std::min(a,unsigned(size()));b=std::min(b,unsigned(size()));if(a>b)std::swap(a,b);return substr(a,b-a);}
 bool equalsIgnoreCase(const String&s)const{
   if(size()!=s.size())return false;for(size_t i=0;i<size();++i)if(tolower((*this)[i])!=tolower(s[i]))return false;return true;}
};
namespace mime {enum {txt};struct Mime {const char*mimeType;};const Mime mimeTable[]={{"text/plain"}};}
static const char Content_Type[]="Content-Type",filename[]="filename";
struct Clock {static uint32_t time;static uint32_t now(){return time;}static void idle(){++time;}};
uint32_t Clock::time=0;uint32_t millis(){return Clock::time;}
using UpdateHttpReader=UpdateHttpReaderBase<Clock>;
struct Client {
 std::string data;size_t pos=0;bool alive=true;bool eof=false;
 int read(){return pos<data.size()?uint8_t(data[pos++]):-1;}
 bool connected(){return alive && !(eof && pos==data.size());}
 int available(){return data.size()-pos;}void stop(){alive=false;}
};
using NetworkClient=Client;
enum {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
enum class LineStatus {Ok,TooLong,TimedOut};
struct HTTPUpload {int status;String name,filename,type;size_t totalSize,currentSize,contentLength;uint8_t buf[64];};
struct RequestArgument {String key,value;};
struct Handler {
 int starts=0,ends=0,aborts=0;std::string payload;
 bool canUpload(const String&){return true;}
 template<class S>bool canUpload(S&,const String&){return true;}
 template<class S>void upload(S&,const String&,HTTPUpload&u){
   if(u.status==UPLOAD_FILE_START)++starts;
   if(u.status==UPLOAD_FILE_WRITE)payload.append((char*)u.buf,u.currentSize);
   if(u.status==UPLOAD_FILE_END)++ends;
   if(u.status==UPLOAD_FILE_ABORTED)++aborts;
 }
};
struct State {
 UpdateHttpReader _updateReader;std::unique_ptr<HTTPUpload>_currentUpload;
 RequestArgument *_postArgs=nullptr,*_currentArgs=nullptr;int _postArgsLen=0,_currentArgCount=0;
 String _currentUri="/update";Handler handler;Handler*_currentHandler=&handler;
 bool hasArg(const char*){return false;}String arg(const char*){return "";}
 ~State(){delete[]_postArgs;delete[]_currentArgs;}
};
struct Backend {using ClientType=Client;};
'''
declarations = r'''
 void _uploadWriteByte(uint8_t);
 int _uploadReadByte(Client&);
 bool _parseForm(Client&,const String&,uint32_t);
 bool _parseFormUploadAborted();
'''
runner = r'''
std::string part(const std::string&p,bool file=true){
 return "Content-Disposition: form-data; name=\"file\""+std::string(file?"; filename=\"fw.bin\"":"")+
   "\r\nContent-Type: application/octet-stream\r\n\r\n"+p+"\r\n--boundary";
}
bool parse(Server&s,Client&c,size_t declared=0){
 s._updateReader.begin();assert(s._updateReader.begin_body(declared?declared:c.data.size(),true,8));
 return s._parseForm(c,"boundary",declared?declared:c.data.size());
}
int main(){
 std::string payload(200,'x');payload+="\r\n--boundX\r\n";payload+=char(0);payload+="end";
 {Server s;Client c;c.data="--boundary\r\n"+part(payload)+"--\r\n";
   assert(parse(s,c) && s.handler.starts==1 && s.handler.ends==1 && s.handler.aborts==0);
   assert(s.handler.payload==payload);}
 {Server s;Client c;c.data="--boundary\r\n"+part("password",false)+"--\r\n";
   assert(!parse(s,c) && s.handler.starts==0);}
 {Server s;Client c;c.data="--boundary\r\n"+part(payload)+"\r\n"+part("second")+"--\r\n";
   assert(parse(s,c) && s.handler.starts==2); /* API admission rejects the second START. */}
 // Missing closing boundary, truncated suffix, and malformed part headers must never succeed.
 for(size_t cut:{size_t(60),size_t(200),size_t(6),size_t(1)}){
   Server s;Client c;auto all="--boundary\r\n"+part(payload)+"--\r\n";
   c.data=all.substr(0,all.size()-cut);auto start=Clock::time;
   assert(!parse(s,c,all.size()) && Clock::time-start<=10000);
   if(s.handler.starts)assert(s.handler.aborts==1);
 }
 {Server s;Client c;c.data="--boundary\r\n"+part(payload);c.eof=true;
   assert(!parse(s,c,c.data.size()+4));}
 {Server s;Client c;c.data="--boundary\r\n"+std::string(1025,'x')+"\r\n";
   assert(!parse(s,c));}
 puts("Actual multipart parser: binary payload, text fields, duplicate parts, truncation, EOF and line cap PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
    for platform, filename in [("esp8266", "Parsing-impl.h"), ("esp32", "Parsing.cpp")]:
        parser = (ROOT / "external" / ("updater-" + platform) / "src" / filename).read_text()
        methods = []
        for name in ["_updateReadLine", "_uploadReadByte", "_uploadWriteByte", "_parseForm", "_parseFormUploadAborted"]:
            pos = parser.index("::" + name + "(")
            start = parser.rfind("template <typename ServerType>", 0, pos) if platform == "esp8266" else parser.rfind("\n", 0, pos) + 1
            end = parser.index("\n}", pos) + 2
            methods.append(parser[start:end])
        if platform == "esp8266":
            cls = "template<class ServerType>struct ESP8266WebServerTemplate:State {using ClientType=Client;" + declarations + "String _updateReadLine(Client&,char);};\n"
            alias = "using Server=ESP8266WebServerTemplate<Backend>;\n"
        else:
            cls = "struct WebServer:State {" + declarations + "LineStatus _updateReadLine(Client&,String&,size_t,unsigned long=0,unsigned long=0);};\n"
            alias = "using Server=WebServer;\n"
        binary = Path(temp) / platform
        subprocess.run(["g++", "-std=gnu++14", "-I", str(ROOT), "-x", "c++", "-", "-o", str(binary)],
                       input=stub + cls + '\n'.join(methods) + alias + runner, text=True, check=True)
        print(platform, flush=True)
        subprocess.run([str(binary)], check=True, timeout=10)
