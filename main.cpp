#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h> // for sockets
#include <ws2tcpip.h> // for getaddrinfo
#include <io.h>
#include <direct.h>
#include <stdio.h>
#pragma comment(lib,"winmm.lib") // for midi
#pragma comment(lib,"ws2_32.lib") // for sockets
#pragma comment(lib,"opengl32.lib") // for wglGetProcAddress
#pragma comment(lib,"portmidi_win/portmidi_s.lib") // for midi
#ifdef _WIN64
#pragma comment(lib,"imgui/examples/libs/glfw/lib-vc2010-64/glfw3.lib") // for imgui rendering
#else
#pragma comment(lib,"imgui/examples/libs/glfw/lib-vc2010-32/glfw3.lib") // for imgui rendering
#endif
#pragma comment(lib,"legacy_stdio_definitions.lib") // for vsnprintf
#define close closesocket
#define chdir _chdir
#define strdup _strdup
void usleep(int usec) { Sleep(usec/1000); }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif
#endif
#define WINDOW_HEIGHT 500
#include "imgui.h"
#include "imgui/examples/imgui_impl_glfw.h"
#include "imgui/examples/imgui_impl_opengl3.h"
#include <stdio.h>
#include <GL/gl3w.h>    // This example is using gl3w to access OpenGL functions (because it is small). You may use glew/glad/glLoadGen/etc. whatever already works for you.
#include <GLFW/glfw3.h>
#include <portmidi.h>
#include <thread>
#include <mutex>
typedef std::unique_lock<std::mutex> ScopedLock;
static void glfw_error_callback(int error, const char* description){    fprintf(stderr, "Error %d: %s\n", error, description);}
int *inputids=0;
char inifile[1024];
PortMidiStream *stream=0;
int curport=-1;
int quit=false;
PmDeviceInfo *devices=0;
std::mutex oscmutex;
int keydown[128]={};
char kitname[256]="127.0.0.1";
const char* port="9000";
int topnote=127;
int ccx=12,ccy=13,ccspicy=1;
int xpos[16]={0},ypos[16]={0},spiciness[16]={0};
int highestnoteseen=0;
int lastchan=0;
int lastcontroller[16]={},lastlastcontroller[16]={};
int inmin=1,inmax=16;
int outchan=1;
int sustainmode=0;
float velocitysens=100.f;
float velocity2spicy=0.f;
bool aftertouch2spicy=false;
int lastnotes[16]={};
typedef uint32_t u32;
int fd=-1;
char ip[256]="";
struct addrinfo* addr=0;
#undef min
#undef max
int min(int a,int b) { return (a<b)?a:b; }
int max(int a,int b) { return (a>b)?a:b; }
int clamp(int x, int a, int b) { return min(max(x,a),b); }
struct Message {	
    // "/dreams\0" - 8
    // ",m\0\0" - 4
    char header[12]="/dreams\0,m\0";
	unsigned char deviceidx=0;
	unsigned char msg=0;
	unsigned char data1=0;
	unsigned char data2=0;	
};
Message lastmsg;

void TryToConnect() {
    if (fd==-1) {
        struct addrinfo hints={};
        hints.ai_family=AF_UNSPEC;
        hints.ai_socktype=SOCK_DGRAM;
        hints.ai_protocol=0;
        hints.ai_flags=AI_ADDRCONFIG;
        printf("connecting to [%s] [%s]\n",ip,port);

        if (getaddrinfo(ip,port,&hints,&addr)!=0) printf("failed to resolve remote socket address");
        else fd=socket(addr->ai_family, addr->ai_socktype,addr->ai_protocol);
        if (fd<0) printf("failed to open socket %s\n",strerror(errno));
        if (fd<0) fd=-2; // -2 means failed
    }
}

void SendOSCMsg(const Message &msg) {
    TryToConnect();
    struct addrinfo *a=addr;
    if (fd>=0 && a) {
    	if (sendto(fd,(const char*)&msg,sizeof(msg),0,a->ai_addr,(int)a->ai_addrlen)<0) 
			printf("failed to send udp packet!\n");				

    }
}
void ProcessMidiEvent(u32 dwParam1, bool mouse) {
    ScopedLock lock(oscmutex);
    Message msg;
    int midichan=(dwParam1 & 0xf);
    if (1) {
		// midi channel remap hack
		midichan++;
		if (midichan<inmin || midichan>inmax) return ; // throw it away		
		midichan+=(outchan-inmin);
		if (midichan<1) midichan=1;
		if (midichan>16) midichan=16;
		midichan--;
		dwParam1&=~15;
		dwParam1|=midichan;
        ////////////////////////////////////

        int chanbit=1<<midichan;
        msg.msg=(unsigned char)dwParam1;
        msg.data1=(unsigned char)(dwParam1>>8);
        msg.data2=(unsigned char)(dwParam1>>16);

		// remap zero-velocity note-on events to note-off ones
		if (msg.msg==0x90 && msg.data2 == 0) {
			msg.msg=0x80;
		}

		int original_data1=msg.data1;
		int original_data2=msg.data2;
        int miditype=(msg.msg&0xf0)>>4;
        unsigned char chan=msg.msg&0xf;			
		lastchan=chan;

		// ignore advanced midi messages
		if (miditype < 0x8 || miditype > 0xe) return;

        if (miditype==8 || miditype==9) {
			// handle note-on and note-off events

			// update the highest detected note
            if (!mouse && msg.data1>highestnoteseen) highestnoteseen=msg.data1;

            // handle top note bound
			int shifted_note=msg.data1+127-topnote;
            if (msg.data1==topnote) msg.data1=127;
            if (msg.data1>=topnote-4 && msg.data1<topnote) {
				if (miditype==8) {
                    // note up. release the shifted note if it is down.
                    if (keydown[shifted_note]&chanbit) msg.data1=shifted_note;
                } else {
                    // note down. shift if the top note is down.
                    if (keydown[127]&chanbit) msg.data1=shifted_note;					
                }
            }

			if (miditype == 9) {
				// handle note-on
				msg.data2 = min(127, max(0, int(msg.data2 * velocitysens * 0.01f + 127.f * 0.01f * (100.f - velocitysens))));
				keydown[msg.data1] |= chanbit;
				lastnotes[chan] = msg.data1;
			} else {
				// handle note-off
				keydown[msg.data1] &= ~chanbit;
			}
        }
		else if (miditype==0xb) {
			// handle control-change events
			if (msg.data1==64) { // sustain pedal!
				if (sustainmode==0)
					msg.data2=0; // disable it
				else if (sustainmode==2)
					msg.data2^=127; // flip it
			}
            if (msg.data1==0x79) {
                xpos[chan]=ypos[chan]=spiciness[chan]=0;
			} else if (msg.data1==0x7b) {
                for (int i=0;i<128;++i) keydown[i]&=~chanbit; 
            }
            else {
                if (lastcontroller[chan]!=msg.data1) {
                    lastlastcontroller[chan]=lastcontroller[chan];
                    lastcontroller[chan]=msg.data1;
                }
                if (msg.data1==ccspicy)  { spiciness[chan]=msg.data2; msg.data1=1; }
                else if (msg.data1==ccx) { xpos[chan]=msg.data2; msg.data1=12; }
                else if (msg.data1==ccy) { ypos[chan]=msg.data2; msg.data1=13; }					
            }			
        }

        SendOSCMsg(msg);
        lastmsg=msg;

		// update spiciness
		if ((miditype==0x9 && msg.data2 != 0) || ((miditype==0xd || miditype==0xa) && aftertouch2spicy)) {
			int newspiciness;
			if (miditype==0xa)
				newspiciness=clamp(int(original_data2),0,127); // polyphonic aftertouch
			else if (miditype==0xd)
				newspiciness=clamp(int(original_data1),0,127); // aftertouch
			else					
				newspiciness=clamp(int(original_data2*velocity2spicy*0.01f),0,127); // note-on

			if (newspiciness!=spiciness[chan]) {
				spiciness[chan]=newspiciness;				
				msg.msg&=0xf;
				msg.msg|=0xb0;
				msg.data1=1;
				msg.data2=spiciness[chan];
				SendOSCMsg(msg);
			}
		}
    }
}

void *MidiThread(){
    int openport=-1;
    while (!quit) {
        if (curport!=openport) {
            if (stream) Pm_Close(stream);
            stream=0;
            openport=curport;
            if (openport>=0) {
                PmError e=Pm_OpenInput(&stream, curport, 0, 128, 0, 0);
                if (e) {
                    printf("portmidi OpenInput error %d %s\n", e, Pm_GetErrorText(e));
                    openport=curport=-1;
                    stream=0;
                }
                else 
                    printf("midi port '%s' opened ok\n",devices[openport].name);
            }
        }
        if (stream && Pm_Poll(stream)==TRUE) {
            PmEvent buffer[128];
            int n=Pm_Read(stream, buffer, 128);
            if (n>0) {
                //printf("%d midi events!\n",n);
                for (int i=0;i<n;++i) {
                    ProcessMidiEvent(buffer[i].message, false);
                }
            }
        } // read inputs
        else {
            usleep(500);
        }
    } // while 1
    if (stream) Pm_Close(stream);
    stream=0;
    return 0;
}


const char *GetIniSetting(const char *key, const char *default_value); // simple ini file parser (see the end of this file)
void SaveIniSettings();

bool combofunc(void*data, int idx, const char **out){
    if (idx==0) *out="<no device>"; else *out=devices[inputids[idx-1]].name;
    return true;    
}
void UpdateIP() {
    ScopedLock lock(oscmutex);
    const char *portstr=strchr(kitname,':');
    strcpy(ip,kitname);
    if (portstr) {
        port=portstr+1; 
        ip[portstr-kitname]=0;
    } else port="9000";
    if (!*port) port="9000";
    if (fd>=0) {
        printf("closing old socket\n");
        int oldfd=fd;
        struct addrinfo *oldaddr=addr;
        fd=-1;
        addr=0;
        freeaddrinfo(oldaddr);
        close(oldfd);
    }
//    if (fd==-2) fd=-1; // try again automatically
}
int main(int argc, char**argv){    
    const char *home = 0;
#ifdef __APPLE__
    FSRef fsref;
    unsigned char path[PATH_MAX];
    if (FSFindFolder(kUserDomain, kCurrentUserFolderType, kDontCreateFolder, &fsref) == noErr && FSRefMakePath(&fsref, path, sizeof(path)) == noErr)
        home=(const char*)path;
#endif
    sprintf(inifile,"%s/midi2osc.ini",home?home:".");
    printf("infile: [%s]\n",inifile);
    Pm_Initialize();
    int nmidi = Pm_CountDevices();
    int ninput=0;
    printf("%d midi devices\n",nmidi);
    devices=(PmDeviceInfo*)calloc(sizeof(PmDeviceInfo), nmidi);
    inputids=(int*)calloc(sizeof(int),nmidi);
	strcpy(kitname,GetIniSetting("IP","127.0.0.1"));
	const char *device=GetIniSetting("Device","");
	inmin=strtol(GetIniSetting("MidiChannelInMin","1"),0,0);
	inmax=strtol(GetIniSetting("MidiChannelInMax","16"),0,0);
	outchan=strtol(GetIniSetting("MidiChannelOut","1"),0,0);
	ccspicy=strtol(GetIniSetting("Spicy","1"),0,0);
	ccx=strtol(GetIniSetting("X","12"),0,0);
	ccy=strtol(GetIniSetting("Y","13"),0,0);
	topnote=strtol(GetIniSetting("TopNote","127"),0,0);
	sustainmode=strtol(GetIniSetting("SustainMode","0"),0,0);
    velocitysens=float(strtol(GetIniSetting("VelocitySens","100"),0,0));
	velocity2spicy=float(strtol(GetIniSetting("Velocity2Spicy","0"),0,0));
	aftertouch2spicy=strtol(GetIniSetting("Aftertouch2SpicyEnable","0"),0,0)!=0;
    for (int i=0;i<nmidi;++i) {
        const PmDeviceInfo *info=Pm_GetDeviceInfo(i);
        devices[i]=*info;
        printf("%d: '%s' %d in, %d out\n",i,info->name,info->input, info->output);
        if (info->input) inputids[ninput++]=i;
        if (info->input && (curport<0 || (device[0] && strstr(info->name,device))))
            curport=i;
    }
    if (ninput==0) {
        printf("no input devices");
    } 
    UpdateIP();
    std::thread midithread(&MidiThread);
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    GLFWwindow* window = glfwCreateWindow(640, WINDOW_HEIGHT, "midi2osc", NULL, NULL);
	glfwSetWindowSizeLimits(window, 640, WINDOW_HEIGHT, 640, WINDOW_HEIGHT);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    gl3wInit();
    // Setup ImGui binding
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui::StyleColorsDark();

    ImGui_ImplOpenGL3_Init(glsl_version);

    //ImGui::StyleColorsClassic();
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), 0); // ImGuiCond_FirstUseEver
        ImGui::SetNextWindowSize(ImVec2(640,WINDOW_HEIGHT), 0);
        ImGui::Begin("midi2osc", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
        int port=0;
        for (int i=0;i<ninput;++i) if (inputids[i]==curport) port=i+1;
        if (ImGui::Combo("midi input device",&port, combofunc,0, ninput+1, ninput+1)) {
            curport=port ? inputids[port-1]:-1;
            SaveIniSettings();
        }
        if (ImGui::InputText("target ip address",kitname,255)) {
            UpdateIP();
            SaveIniSettings();
        }
        if (fd==-2) {
            ImGui::SameLine();
            if (ImGui::Button("connect failed. try again?")) {
                fd=-1;
                TryToConnect();
            }
        }
		ImGui::Separator();		
		if (ImGui::DragIntRange2("midi input channel range",&inmin,&inmax,0.2f,1,16))
			SaveIniSettings();
		if (ImGui::SliderInt("base midi output channel",&outchan,1,16))
			SaveIniSettings();
		if (ImGui::RadioButton("disable sustain pedal", &sustainmode, 0))
			SaveIniSettings();
		if (ImGui::RadioButton("enable sustain pedal", &sustainmode, 1))
			SaveIniSettings();
		if (ImGui::RadioButton("inverted sustain pedal", &sustainmode, 2))
			SaveIniSettings();
		
		
        if (ImGui::SliderFloat("velocity sensitivity", &velocitysens, 0.f, 100.f, "%2.0f%%")) {
            SaveIniSettings();
        }
		if (ImGui::SliderFloat("midi velocity->spiciness", &velocity2spicy, 0.f, 100.f, "%2.0f%%")) {
			SaveIniSettings();
		}
		if (ImGui::Checkbox("midi aftertouch->spiciness enable", &aftertouch2spicy)) {
			SaveIniSettings();
		}
        
		ImGui::Separator();
        ImGui::PushItemWidth(100.f);
        if (ImGui::DragInt("highest note", &topnote, 1, 0, 127)) {
            SaveIniSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("learn")) {
            topnote=highestnoteseen;
            SaveIniSettings();
        }
        if (ImGui::DragInt("spicy CC",&ccspicy,1,0,127)) {
            SaveIniSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("learn##cc")) {
            ccspicy=lastcontroller[lastchan];
            SaveIniSettings();
        }
        if (ImGui::DragInt("##X CC",&ccx,1,0,127)) {
            SaveIniSettings();
        }
        ImGui::SameLine();
        if (ImGui::DragInt("XY CCs",&ccy,1,0,127)) {
            SaveIniSettings();
        }
        ImGui::SameLine();
        ImGui::PopItemWidth();
        if (ImGui::Button("learn##xy")) {
            ccx=min(lastcontroller[lastchan],lastlastcontroller[lastchan]);
            ccy=max(lastcontroller[lastchan],lastlastcontroller[lastchan]);
            SaveIniSettings();
        }
		// draw a keyboard
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float pw=ImGui::GetWindowContentRegionWidth();		
		float ph=64.f;
        float kw=pw/75.f;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 q(p.x+pw,p.y+ph);
        draw_list->AddRectFilled(ImVec2(q.x-64.f,p.y-66.f),ImVec2(q.x,p.y-2.f),0xff808080,2.f);
        draw_list->AddRectFilled(ImVec2(q.x-70.f,p.y-66.f),ImVec2(q.x-66.f,p.y-2.f),0xff808080,2.f);
        draw_list->AddRectFilled(ImVec2(q.x-70.f,p.y-2.f-spiciness[0]*0.5f),ImVec2(q.x-66.f,p.y-2.f),0xff0000ff,2.f);
        float xx=q.x-64.f+xpos[0]*0.5f,yy=p.y-2.f-ypos[0]*0.5f;
        draw_list->AddLine(ImVec2(xx,p.y-66.f),ImVec2(xx,p.y-2.f),0x80000000,1.f);
        draw_list->AddLine(ImVec2(q.x-64.f,yy),ImVec2(q.x,yy),0x80000000,1.f);
        draw_list->AddRectFilled(p,q,0xff000000);
        draw_list->AddCircleFilled(ImVec2(xx+0.5f,yy+0.5f),2.f,0xff0000ff,8);
        const ImVec2 m=ImGui::GetIO().MousePos;
        int mb=ImGui::GetIO().MouseDown[0];
        static int mousekey=-1;
        if (!mb && mousekey>=0) {
            ProcessMidiEvent(0x80+(mousekey<<8),true);
            mousekey=-1;
        }
        int keytab[2][7]={
            {0,2,4,5,7,9,11},
            {1,3,-10000,6,8,10,-10000}};
        for (int blackwhite=0;blackwhite<2;++blackwhite)
        for (int i=0;i<75;++i) {
            int midinote=keytab[blackwhite][i%7]+12*(i/7);
            if (midinote<0 || midinote>127) continue;

            ImU32 col=0xffeeeeee;
            if (midinote>topnote) col=0x80eeeeee;
            if (blackwhite) col^=0xdddddd;
            if (midinote==topnote) col=0xffffee00;
            ImVec2 a,b;
            float blacky=p.y+ph*0.6f;
            if (blackwhite)
                a=ImVec2(p.x+kw*i+kw*0.66f,p.y),b=ImVec2(p.x+kw*i+kw*1.33f,blacky);
            else
                a=ImVec2(p.x+kw*i,p.y),b=ImVec2(p.x+kw*i+kw-1.f,q.y);
            if (m.x>=a.x && m.x<=b.x && m.y>=a.y && m.y<=b.y && (m.y<blacky)==blackwhite) {
                if (mb) {
                    if (mousekey>=0 && mousekey!=midinote) {
                        ProcessMidiEvent(0x80+(mousekey<<8),true);
                        mousekey=-1;
                    }
                    if (mousekey!=midinote) {
                        mousekey=midinote;
                        ProcessMidiEvent(0x90+(mousekey<<8)+(127<<16),true);
                    }
                }
                col^=0x4040;
            }
            if (keydown[midinote]) col^=0x8080;
            draw_list->AddRectFilled(a,b, col, 2.f);
        }
        ImGui::Dummy(ImVec2(pw,ph));
        if (ImGui::Button("PANIC all notes up")) {
            for (int chan=0;chan<16;++chan)				
				ProcessMidiEvent(0x7bb0+chan,true);

        }        		
		ImGui::Separator();
        ImGui::Text("channel %d, spicy=%d, xy=%d,%d; highest note seen=%d, last ccs seen=%d,%d", lastchan+1, spiciness[lastchan],xpos[lastchan],ypos[lastchan],highestnoteseen, lastcontroller[lastchan], lastlastcontroller[lastchan]);
        const char *types[16]={
            0,0,0,0,0,0,0,0,
            "note up","note down","aftertouch","cc","program","channel pressure","pitchbend","sysex"
        };
        ImGui::Text("last midi message: chan %01x %02x %02x %s",lastmsg.msg&15,lastmsg.data1,lastmsg.data2,types[lastmsg.msg>>4]);        
        ImGui::End();
        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();

                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        usleep(16000);
    }
    quit=true;
    midithread.join();
    Pm_Terminate();

    // Cleanup
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    return 0;
}

////////////////////////////// simple ini parser
const char *settings=0;
void LoadIniSettings() {	
	settings="";
	FILE *f=fopen(inifile,"rb");
	if (!f) return ;
	fseek(f,0,SEEK_END);
	int l=ftell(f);
	char *dst=(char*)calloc(1,l+4);	
    settings=dst;
	fseek(f,0,SEEK_SET);	
	while (!feof(f)) {
		char *odst=dst;
		char linebuf[256]={};
		if (!fgets(linebuf,255,f)) break;
		const char *src=linebuf;
		while (*src && isspace(*src)) ++src; // skip white space
		if (*src=='#') continue; // comment
		const char *ks=dst;
		while (*src && !isspace(*src) && *src!='=') *dst++=*src++; // copy key
		if (dst==ks) {dst=odst; continue; } // empty key
		*dst++=0; // null terminate key		
		while (*src && isspace(*src)) ++src; // skip white space after key
		if (*src++!='=') {dst=odst; continue; } // no equals sign		
		while (*src && isspace(*src)) ++src; // skip white space after =
		const char *vs=dst;
		while (*src && *src!='\r' && *src!='\n' && *src!='#') *dst++=*src++; // copy value to end of line
		while (dst>vs && isspace(dst[-1])) --dst; // trim trailing spaces		
		*dst++=0; // null terminate value
	}
	fclose(f);
	*dst++=0; *dst++=0; // empty string to terminate settings	
}
const char *GetIniSetting(const char *key, const char *default_value) {
	if (!settings) LoadIniSettings();
	for (const char *s=settings;*s;) {			
		const char *val=s+strlen(s)+1;
		if (strcmp(s,key)==0) return val;
		s=val+strlen(val)+1;
	}
	return default_value;
}
void SaveIniSettings() {
	if (inmin>inmax) inmin=inmax;
    FILE *f=fopen(inifile,"wb");
	if (!f) {
        printf("failed to open midi2osc.ini\n");
        return ;
    }
    fprintf(f,"IP=%s\n",kitname);
    fprintf(f,"Device=%s\n",(curport>=0)?devices[curport].name:"");
    fprintf(f,"MidiChannelInMin=%d\n",inmin);
	fprintf(f,"MidiChannelInMax=%d\n",inmax);
	fprintf(f,"MidiChannelOut=%d\n",outchan);	
	fprintf(f,"SustainMode=%d\n",sustainmode);	
    fprintf(f,"Spicy=%d\n",ccspicy);
    fprintf(f,"X=%d\n",ccx);
    fprintf(f,"Y=%d\n",ccy);
    fprintf(f,"TopNote=%d\n",topnote);
    fprintf(f,"VelocitySens=%d\n",int(velocitysens));
	fprintf(f,"Velocity2Spicy=%d\n",int(velocity2spicy));
	fprintf(f,"Aftertouch2SpicyEnable=%d\n",int(aftertouch2spicy));
    fclose(f);
}
#ifdef _WIN32
int CALLBACK WinMain(
	HINSTANCE   hInstance,
	HINSTANCE   hPrevInstance,
	LPSTR       lpCmdLine,
	int         nCmdShow
) {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	return main(0,0);
}
#endif
