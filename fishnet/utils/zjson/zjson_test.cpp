#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "zjson.hpp"
using namespace std;

int main() {
    zjson::Json json;

    json["x"] = 123;
    json["y"] = "hello world";
    json["z"] = false;

    cout << json.dump() << endl;

    string s = json.dump();

    zjson::Json json2 = zjson::Json::parse(s);

    cout << json2.dump() << endl;

    cout << json2["x"].dump() << endl;

    auto x = json["y"].get<string>();

    cout << x << endl;

    return 0;
}