#include <iostream>
#include "json/json.hpp"

using std::cout;
using std::endl;
using json = nlohmann::json;


int main()
{
	cout << "Start." << endl;

	auto j = json::parse(R"({"happy": true, "pi": 3.141})");

	cout << j["happy"] << endl << j["pi"] << endl;

	return 0;
}
