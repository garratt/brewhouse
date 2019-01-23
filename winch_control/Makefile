all: brewhub sheets_test

brewhub: gpio.h grainfather.h logger.h relay.h scale.h valves.h winch.h brewhub.cc brewmanager.h
	g++ brewhub.cc -g -std=c++11 -Wall -I../third_party/rapidjson/include -lpthread -lftdi -lcurl -o brewhub

sheets_test: logger.h sheets_test.cc brewmanager.h
	g++ sheets_test.cc -g -std=c++11 -Wall -I../third_party/rapidjson/include -lpthread -lcurl -o sheets_test
