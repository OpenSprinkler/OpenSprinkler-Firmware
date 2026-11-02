"""Compile the production updater callbacks against observable HTTP/flash stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "api/server.cpp").read_text()
admission = source[source.index("static bool firmware_upload_authorized"):
                   source.index("void on_update_page()")]
upload = source[source.index("static void finish_firmware_upload"):
                source.index("static void register_update_routes()")]
stub = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <map>
#include <cstdio>
#include <cstdint>
#define F(x) x
#define DEBUG_PRINT(x) ((void)0)
#define DEBUG_PRINTLN(x) ((void)0)
struct String: std::string {
 using std::string::string; using std::string::operator+=;
 String&operator+=(int n){append(std::to_string(n));return *this;}
 long toInt()const{return strtol(c_str(),nullptr,10);}
};
enum {HTTP_POST,HTTP_GET,HTML_UNAUTHORIZED=2,HTML_NOT_PERMITTED=48,HTML_UPLOAD_FAILED=64,HTML_SUCCESS=1};
enum {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
enum class FirmwareUpdatePhase {Idle,Prepared,Uploading,Verifying,Success,Error};
struct HTTPUpload {int status=UPLOAD_FILE_START;String filename="fw.bin";uint8_t buf[32]={};size_t currentSize=32;};
struct Client {int stops=0;void stop(){++stops;}};
struct Server {
 String path="/update";int verb=HTTP_POST,status=0;std::map<std::string,String>args;
 HTTPUpload file;Client connection;
 const String&uri(){return path;}int method(){return verb;}
 String arg(const char*k){return args[k];}bool hasArg(const char*k){return args.count(k);}
 void sendHeader(const char*,const char*){}void send(int n,const char*,const String&){status=n;}
 HTTPUpload&upload(){return file;}Client&client(){return connection;}
} server,*update_server=&server;
struct Service {
 FirmwareUpdatePhase state=FirmwareUpdatePhase::Idle;int begins=0,writes=0,finishes=0,aborts=0;
 bool write_ok=true;
 bool authenticate(const char*p){return !strcmp(p,"password");}
 bool token_valid(const char*p){return state==FirmwareUpdatePhase::Prepared && !strcmp(p,"token");}
 bool busy(){return state==FirmwareUpdatePhase::Uploading || state==FirmwareUpdatePhase::Verifying;}
 FirmwareUpdatePhase phase(){return state;}const char*message(){return "test";}
 bool begin_manual(const char*,uint32_t,const char*){if(busy()||state==FirmwareUpdatePhase::Success)return false;
   ++begins;state=FirmwareUpdatePhase::Uploading;return true;}
 bool begin_legacy_manual(const char*f){return begin_manual(f,0,"");}
 bool begin_verified(const char*f,const char*t){return token_valid(t) && begin_manual(f,0,"");}
 bool write_manual(const uint8_t*,size_t){++writes;return write_ok;}
 bool write_verified(const uint8_t*p,size_t n){return write_manual(p,n);}
 bool finish_manual(){++finishes;state=FirmwareUpdatePhase::Success;return true;}
 bool finish_verified(){return finish_manual();}
 void abort_upload(){++aborts;state=FirmwareUpdatePhase::Error;}
} firmware_update;
int result=0;
void update_server_send_result(int n,const char* = nullptr){result=n;}
void delay(int){}
'''
runner = r'''
void reset(){server=Server{};firmware_update=Service{};result=0;
 firmware_upload_seen=firmware_upload_authorized=firmware_upload_failed=firmware_upload_auth_failed=firmware_upload_legacy=false;}
void event(int status,bool verified=false){server.file.status=status;receive_firmware_upload(verified);}
int main(){
 reset();on_update_check();assert(server.status==401 && firmware_update.begins==0);
 server.args["pw"]="password";on_update_check();assert(result==1 && firmware_update.begins==0);
 firmware_update.state=FirmwareUpdatePhase::Success;on_update_check();assert(server.status==409);
 reset();server.args["token"]="token";firmware_update.state=FirmwareUpdatePhase::Prepared;
 on_update_check();assert(result==1 && firmware_update.token_valid("token") && firmware_update.begins==0);
 reset();assert(!updater_body_authorized(true,2048) && server.status==401);
 server.args["pw"]="password";assert(updater_body_authorized(true,2048) && firmware_update.begins==0);
 server.path="/update/prepare";assert(updater_body_authorized(false,1024));
 assert(!updater_body_authorized(true,1024) && server.status==415);
 assert(!updater_body_authorized(false,1025));
 server.path="/unknown";assert(!updater_body_authorized(true,2048) && server.status==404);
 server.path="/update";server.verb=HTTP_GET;assert(!updater_body_authorized(true,2048));
 server.verb=HTTP_POST;
 for(auto state:{FirmwareUpdatePhase::Uploading,FirmwareUpdatePhase::Verifying,FirmwareUpdatePhase::Success}){
   firmware_update.state=state;assert(!updater_body_authorized(true,2048) && server.status==409);
   assert(firmware_update.state==state && firmware_update.aborts==0);
 }
 reset();server.path="/update/upload";firmware_update.state=FirmwareUpdatePhase::Prepared;
 server.args["token"]="bad";assert(!updater_body_authorized(true,2048) && server.status==401);
 server.args["token"]="token";assert(updater_body_authorized(true,2048));
 assert(firmware_update.token_valid("token") && firmware_update.begins==0);
 event(UPLOAD_FILE_START,true);assert(firmware_update.begins==1);
 event(UPLOAD_FILE_WRITE,true);finish_firmware_upload(true);assert(result==1 && firmware_update.finishes==1);

 reset();server.args["pw"]="password";server.args["size"]="100";server.args["sha256"]="hash";
 assert(updater_body_authorized(true,2048));event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE);
 finish_firmware_upload(false);assert(result==1 && firmware_update.finishes==1);
 // Unsized manual format remains possible, but only with pre-body query credentials.
 reset();server.args["pw"]="password";assert(updater_body_authorized(true,2048));
 event(UPLOAD_FILE_START);assert(firmware_upload_legacy);event(UPLOAD_FILE_ABORTED);
 assert(firmware_update.aborts==1 && !firmware_upload_authorized && firmware_update.finishes==0);
 reset();server.args["pw"]="password";assert(updater_body_authorized(true,2048));
 event(UPLOAD_FILE_START);event(UPLOAD_FILE_END);event(UPLOAD_FILE_START);
 finish_firmware_upload(false);assert(firmware_update.begins==1 && firmware_update.finishes==0 && server.connection.stops==1);
 reset();server.args["pw"]="password";assert(updater_body_authorized(true,2048));
 event(UPLOAD_FILE_START);server.args["pw"]="bad";finish_firmware_upload(false);
 assert(result==2 && firmware_update.aborts==1 && firmware_update.finishes==0);
 reset();server.args["pw"]="password";assert(updater_body_authorized(true,2048));
 event(UPLOAD_FILE_START);firmware_update.write_ok=false;event(UPLOAD_FILE_WRITE);finish_firmware_upload(false);
 assert(result==64 && firmware_update.finishes==0);
 puts("Updater callbacks: pre-body auth, shape, busy, tokens, abort, duplicate file and final recheck PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp) / "admission"
    subprocess.run(["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror", "-x", "c++", "-", "-o", str(binary)],
                   input=stub + admission + upload + runner, text=True, check=True)
    subprocess.run([str(binary)], check=True)

# Wiring checks complement the executable shared-reader and admission tests.
for platform, filename in [("esp8266", "Parsing-impl.h"), ("esp32", "Parsing.cpp")]:
    parser = (ROOT / "external" / ("updater-" + platform) / "src" / filename).read_text()
    request = parser[parser.index("::_parseRequest"):parser.index("::_collectHeader")]
    assert request.index("_updateAuthorization(isForm,") < request.index("_parseForm(client,")
    assert request.index("_parseArguments(searchStr)") < request.index("_updateAuthorization(isForm,")
    assert "client.readStringUntil" not in parser and "client.readBytes" not in parser
    assert "_updateReader.read_byte(client)" in parser
    assert "Transfer-Encoding" in request and "gotLength ||" in request
print("Both vendored parsers: bounded readers and admission wiring PASS")
