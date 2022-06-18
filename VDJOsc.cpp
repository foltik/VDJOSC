#include "VDJOsc.h"

HRESULT VDJ_API DllGetClassObject(const GUID &rclsid, const GUID &riid, void **ppObject) {
	if (memcmp(&rclsid, &CLSID_VdjPlugin8, sizeof(GUID)) == 0
        && memcmp(&riid, &IID_IVdjPluginBasic8, sizeof(GUID)) == 0)
        return CLASS_E_CLASSNOTAVAILABLE;

	*ppObject = new VDJOsc();
	return NO_ERROR;
}

HRESULT VDJ_API VDJOsc::OnLoad() {
    WSAStartup(MAKEWORD(2, 2), nullptr);
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in src;
    src.sin_family = AF_INET;
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    src.sin_port = htons(0);

    m_dest.sin_family = AF_INET;
    m_dest.sin_addr.s_addr = inet_addr(DEST_IP);
    m_dest.sin_port = htons(DEST_PORT);

    bind(m_sock, (SOCKADDR*)&src, sizeof(src));

    m_pause.store(true);
    m_quit.store(false);

    m_thread = std::thread(&VDJOsc::Poll, this);

    snprintf(m_bpm_label, LABEL_BUFSZ, "120.00");
    snprintf(m_beats_label, LABEL_BUFSZ, "0");
    snprintf(m_playing_label, LABEL_BUFSZ, "False");
    snprintf(m_dest_label, LABEL_BUFSZ, "%s:%d", DEST_IP, DEST_PORT);
    snprintf(m_packets_label, LABEL_BUFSZ, "0");

    DeclareParameterString(m_bpm_label, ID_BPM, "BPM", "BPM", LABEL_BUFSZ);
    DeclareParameterString(m_beats_label, ID_BEATS, "Beats", "B", LABEL_BUFSZ);
    DeclareParameterString(m_playing_label, ID_PLAYING, "Playing", "P", LABEL_BUFSZ);
    DeclareParameterString(m_dest_label, ID_DEST, "Destination", "D", LABEL_BUFSZ);
    DeclareParameterString(m_packets_label, ID_PACKETS, "Packets", "N", LABEL_BUFSZ);

    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnGetPluginInfo(TVdjPluginInfo8 *infos) {
    infos->PluginName = "VDJOsc";
    infos->Author = "Jack Foltz";
    infos->Description = "VDJ Osc Bridge";
    infos->Version = "1.0";
    infos->Flags = 0x00;
    infos->Bitmap = NULL;

    return S_OK;
}

ULONG VDJ_API VDJOsc::Release() {
    m_quit.store(true);
    m_thread.join();

    closesocket(m_sock);

    delete this;
    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnGetUserInterface(TVdjPluginInterface8 *pluginInterface) {
    pluginInterface->Type = VDJINTERFACE_DEFAULT;
    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnParameter(int id) {
    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnGetParameterString(int id, char *outParam, int outParamSize) {
    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnStart() {
    m_pause.store(false);
    m_cond.notify_all();
    return S_OK;
}

HRESULT VDJ_API VDJOsc::OnStop() {
    m_pause.store(true);
    return S_OK;
}

void VDJOsc::Poll() {
    while (!m_quit.load()) {
        if (m_pause.load()) {
            std::unique_lock<std::mutex> lock(m_cond_mutex);
            m_cond.wait(lock, [this]{ return !m_pause.load(); });
        }

        float bpm = GetFloat("get bpm");
        if (bpm != m_bpm) {
            Send([=](auto &p) {
                p.openMessage("/bpm", 1)
                    .float32(bpm)
                .closeMessage();
            });
            m_bpm = bpm;
            snprintf(m_bpm_label, LABEL_BUFSZ, "%.2f", bpm);
        }

        bool playing = GetBool("play");
        if (playing != m_playing) {
            Send([=](auto &p) { 
                p.openMessage("/playing", 1)
                    .int32(playing)
                .closeMessage();
            });
            m_playing = playing;
            snprintf(m_playing_label, LABEL_BUFSZ, "%s", playing ? "True" : "False");
        }
        
        // beatpos is 1/4 div
        int beats = floorl(GetDouble("get beatpos") * (QUANTUM / 4));
        if (beats != m_beats) {
            Send([=](auto &p) {
                p.openMessage("/beats", 1)
                    .int32(m_beats)
                .closeMessage();
            });
            m_beats = beats;
            snprintf(m_beats_label, LABEL_BUFSZ, "%d / %d / %d", beats / QUANTUM, beats / (QUANTUM / 4), beats);
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void VDJOsc::Send(std::function<void(OSCPP::Client::Packet&)> fn) {
    OSCPP::Client::Packet packet(m_sendbuf, OSC_BUFSZ);
    fn(packet);

    sendto(m_sock, (const char*)m_sendbuf, packet.size(), 0, (SOCKADDR*)&m_dest, sizeof(m_dest));

    int packets = m_packets.fetch_add(1) + 1;
    snprintf(m_packets_label, LABEL_BUFSZ, "%d", packets);
}

double VDJOsc::GetValue(const char *cmd) {
    constexpr size_t size = 64;
    char command[size];
    snprintf(command, size, "%s", cmd);

    double ret;
    GetInfo(command, &ret);

    return ret;
}

double VDJOsc::GetDouble(const char *cmd) { return GetValue(cmd); }
float VDJOsc::GetFloat(const char *cmd) { return GetDouble(cmd); }
bool VDJOsc::GetBool(const char *cmd) { return GetValue(cmd) == 1.0 ? true : false; }