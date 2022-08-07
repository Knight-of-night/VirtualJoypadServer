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
}


int add_joypad(int id)
{
	if (lock)
	{
		return 1;
	}
	// 防止冲突
	lock = true;

	// 插入手柄
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

    // 保存
    pads[id] = pad;

	lock = false;
	return 0;
}


void client_callback(int id, char* data, int length)
{
    // 忽略超过最大连接数的连接
    if (id >= MAX_CONNECTIONS)
    {
        return;
    }

    // 如果此位置没有手柄，增加一个
	if (pads.at(id) == nullptr)
	{
		add_joypad(id);
		return;
	}

    // 处理手柄事件
    auto j = json::parse(data);
    // 判断是否有标识
    string data_id = j["id"];
    if (!(string("xbox") == data_id))// TODO: ps4
    {
        cout << "ERROR!!!UNKNOW DATA!!!" << endl;
        return;
    }
    // 构造XUSB_REPORT
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

    // 启动TCP
	TcpServer server("30001", client_callback);
    server.start_async();

    cout << "Enter [q] to quit." << endl;
    while(cin.get() != 'q') {}

    // 关闭TCP，虽然可能没用
    server.stop_after_next_connection();

    // 释放所有手柄
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
