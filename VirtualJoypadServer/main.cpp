//
// Windows basic types 'n' fun
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <vector>
#include <ViGEm/Client.h>
#include "json/json.hpp"
#include "tcp_server/tcp_server.h"

//
// Link against SetupAPI
//
#pragma comment(lib, "setupapi.lib")

using std::cout;
using std::endl;
using std::string;
using json = nlohmann::json;

const int MAX_CONNECTIONS = 4;
PVIGEM_CLIENT client;
vector<PVIGEM_TARGET> pads(MAX_CONNECTIONS, nullptr);
bool lock = false;
void client_callback(int id, char* data, int length);
TcpServer server("30001", client_callback, 4096);


// Define the callback function
VOID CALLBACK notification(
    PVIGEM_CLIENT Client,
    PVIGEM_TARGET Target,
    UCHAR LargeMotor,
    UCHAR SmallMotor,
    UCHAR LedNumber,
    LPVOID UserData
)
{
    static int count = 1;

    cout.width(3);
    cout << count++ << " ";
    cout.width(3);
    cout << (int)LargeMotor << " ";
    cout.width(3);
    cout << (int)SmallMotor << endl;

    // ���������pads����û�и��õķ�����
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        // �ҵ�Ŀ��
        if (pads[i] != Target)
        {
            continue;
        }
        // ����json
        json j;
        j["id"] = "xbox";// TODO: ps4
        j["LargeMotor"] = LargeMotor;
        j["SmallMotor"] = SmallMotor;
        // ����
        string data_to_send = j.dump();
        server.send_data(i, data_to_send.c_str(), data_to_send.length());
        server.send_data(i, "\n", 2);
    }

}


int add_joypad(int id)
{
	if (lock)
	{
		return 1;
	}
	// ��ֹ��ͻ
	lock = true;

	// �����ֱ�
    VIGEM_ERROR retval = VIGEM_ERROR_NONE;

    // Allocate handle to identify new pad
    const PVIGEM_TARGET pad = vigem_target_x360_alloc();

    // Add client to the bus, this equals a plug-in event
    retval = vigem_target_add(client, pad);

    // Error handling
    if (!VIGEM_SUCCESS(retval))
    {
        cerr << "Target plugin failed with error code: 0x" << std::hex << retval << endl;
        return -1;
    }

    // Register notification
    retval = vigem_target_x360_register_notification(client, pad, &notification, nullptr);

    // Error handling
    if (!VIGEM_SUCCESS(retval))
    {
        std::cerr << "Registering for notification failed with error code: 0x" << std::hex << retval << std::endl;
        return -1;
    }

    // ����
    pads[id] = pad;

	lock = false;
	return 0;
}


void client_callback(int id, char* data, int length)
{
    // ���Գ������������������
    if (id >= MAX_CONNECTIONS)
    {
        return;
    }

    // �����λ��û���ֱ�������һ��
	if (pads.at(id) == nullptr)
	{
		add_joypad(id);
		return;
	}

    // �����ж��Ƿ�Ϊjson�ַ�����������������ٶȿ��ʱ������һ�𣬲�����{}���ж�
    // ��������Ĵ�������̫�ã������˽ضϵ���Ϣ
    if ((length < 1) || (data[0] != '{')/* || (data[length - 1] != '}')*/)
    {
        cout << "ERROR DATA: " << data << endl;
        return;
    }
    string str(data);
    size_t begin = str.find('{');
    size_t end = str.find('}', begin + 1);
    string json_str = str.substr(begin, end - begin + 1);
    //cout << json_str << endl;

    // �����ֱ��¼�
    auto j = json::parse(json_str);
    // �ж��Ƿ��б�ʶ
    string data_id = j["id"];
    if (!(string("xbox") == data_id))// TODO: ps4
    {
        cout << "ERROR!!!UNKNOW TYPE: " << json_str << endl;
        return;
    }
    // ����XUSB_REPORT
    XUSB_REPORT rep
    {
        j["wButtons"],
        j["bLeftTrigger"],
        j["bRightTrigger"],
        j["sThumbLX"],
        j["sThumbLY"],
        j["sThumbRX"],
        j["sThumbRY"]
    };
    vigem_target_x360_update(client, pads[id], rep);
}


int main()
{
	cout << "Start." << endl;
	// Test json
	//auto j = json::parse(R"({"happy": true, "pi": 3.141})");
	//cout << j["happy"] << endl << j["pi"] << endl;

    VIGEM_ERROR retval = VIGEM_ERROR_NONE;

    // Initialize the API
    client = vigem_alloc();
    if (client == nullptr)
    {
        cerr << "Uh, not enough memory to do that?!" << endl;
        return -1;
    }

    // Establish connection to the driver
    retval = vigem_connect(client);
    if (!VIGEM_SUCCESS(retval))
    {
        cerr << "ViGEm Bus connection failed with error code: 0x" << std::hex << retval << endl;
        return -1;
    }

    // ����TCP
	//TcpServer server("30001", client_callback);// Ϊ����callback������ʹ�ã��ĳ�ȫ�ֱ���
    server.start_async();

    cout << "Enter [q] to quit." << endl;
    while(cin.get() != 'q') {}

    // �ر�TCP����Ȼ����û��
    server.stop_after_next_connection();

    // �ͷ������ֱ�
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (pads[i] == nullptr)
        {
            continue;
        }
        // We're done with this pad, free resources (this disconnects the virtual device)
        vigem_target_remove(client, pads[i]);
        vigem_target_free(pads[i]);
    }

    // Once ViGEm interaction is no longer required (e.g. the application is about to end) 
    // the acquired resources need to be freed properly
    vigem_disconnect(client);
    vigem_free(client);

    system("pause");

	return 0;
}
