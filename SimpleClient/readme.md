ubuntu:
g++ -std=c++17 -Inlohmann_json/inc main_ubuntu.cpp -o Client_Ubuntu -pthread
./Client_Ubuntu 192.168.1.7:8180 ubuntu

windwos：
cl /EHsc /std:c++17 /utf-8 /Inlohmann_json/inc main.cpp /Fe:Client.exe
Client.exe 7866191rp5vs.vicp.fun:27045 windows

3516CV610:
arm-v01c02-linux-musleabi-g++ -std=c++17 -Inlohmann_json/inc main_iot.cpp -o Client -pthread
/usr/data/Client 192.168.1.7:8180 iot