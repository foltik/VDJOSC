#ifndef VDJ_OSC_H
#define VDJ_OSC_H

// cl /LD VDJOsc.cpp
// copy /y VDJOsc.dll "C:\Users\Jack\My Documents\VirtualDJ\Plugins64\SoundEffect\"

#include "vdjDsp8.h"

#include <cstdio>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <math.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#include "oscpp/client.hpp"

constexpr char *DEST_IP   = "10.10.1.1";
// constexpr char *DEST_IP   = "10.16.8.48";
constexpr short DEST_PORT = 7777;
constexpr int QUANTUM     = 128;

// constexpr int POLL_MS = 50;

constexpr size_t LABEL_BUFSZ = 64;
constexpr size_t OSC_BUFSZ = 8192;

class VDJOsc : public IVdjPluginDsp8 {
public:
	HRESULT VDJ_API OnLoad();
	HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8 *infos);
	ULONG VDJ_API Release();
    HRESULT VDJ_API OnGetUserInterface(TVdjPluginInterface8 *pluginInterface);
    HRESULT VDJ_API OnParameter(int id);
    HRESULT VDJ_API OnGetParameterString(int id, char *outParam, int outParamSize);

	HRESULT VDJ_API OnStart();
	HRESULT VDJ_API OnStop();
	HRESULT VDJ_API OnProcessSamples(float *buffer, int nb) { return S_OK; };

private:
	void Poll();

	void Send(std::function<void(OSCPP::Client::Packet&)> fn);

	double GetValue(const char *key);
	double GetDouble(const char *key);
	float GetFloat(const char *key);
	bool GetBool(const char *key);

	std::condition_variable m_cond;
	std::mutex m_cond_mutex;
	std::atomic_bool m_pause;
	std::atomic_bool m_quit;
	std::thread m_thread;

	SOCKET m_sock = INVALID_SOCKET;
	sockaddr_in m_dest;
	char m_sendbuf[OSC_BUFSZ];
	std::atomic_int m_packets;

    bool m_playing = false;
	float m_bpm = 120.0;
	int m_beats = 0;

    char m_bpm_label[LABEL_BUFSZ];
	char m_beats_label[LABEL_BUFSZ];
    char m_playing_label[LABEL_BUFSZ];
    char m_dest_label[LABEL_BUFSZ];
	char m_packets_label[LABEL_BUFSZ];
      
	enum IDS {
		ID_BPM,
		ID_BEATS,
		ID_PLAYING,
        ID_DEST,
		ID_PACKETS,
	};
};

#endif