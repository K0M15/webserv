NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Isrc -g

SRCS = src/main.cpp src/PollHandler.cpp src/ConnectionManager.cpp src/HttpResponse.cpp src/HttpStatusReason.cpp src/Request.cpp src/Webserver.cpp src/ConfigReader.cpp src/WebserverSettings.cpp src/CGIHandler.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

createTestDIR:
	mkdir -p bin

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

testRequest: createTestDIR
	$(CXX) $(CXXFLAGS) src/Request.cpp tests/testRequest.cpp -o bin/testRequest

testURL: createTestDIR
	$(CXX) $(CXXFLAGS) -Isrc tests/testURL.cpp -o bin/testURL

testPollHandler: createTestDIR
	$(CXX) $(CXXFLAGS) -Isrc src/PollHandler.cpp tests/testPollHandler.cpp -o bin/testPollHandler

testConfigReader: createTestDIR
	$(CXX) $(CXXFLAGS) -Isrc src/ConfigReader.cpp src/WebserverSettings.cpp tests/testConfigReader.cpp -o bin/testConfigReader

testHttpResponse: createTestDIR
	$(CXX) $(CXXFLAGS) src/HttpResponse.cpp src/HttpStatusReason.cpp tests/testHttpResponse.cpp -o bin/testHttpResponse

testHttpStatusReason: createTestDIR
	$(CXX) $(CXXFLAGS) src/HttpStatusReason.cpp tests/testHttpStatusReason.cpp -o bin/testHttpStatusReason

testWebserverSettings: createTestDIR
	$(CXX) $(CXXFLAGS) src/WebserverSettings.cpp tests/testWebserverSettings.cpp -o bin/testWebserverSettings

testCGI: createTestDIR
	$(CXX) $(CXXFLAGS) src/CGIHandler.cpp src/Request.cpp src/ConnectionManager.cpp src/PollHandler.cpp src/HttpResponse.cpp src/HttpStatusReason.cpp tests/testCGI.cpp -o bin/testCGI

tests: testRequest testURL testPollHandler testConfigReader testHttpResponse testHttpStatusReason testWebserverSettings testCGI
	./bin/testRequest tests/sample_request.txt
	./bin/testURL
	./bin/testPollHandler
	./bin/testConfigReader
	./bin/testHttpResponse
	./bin/testHttpStatusReason
	./bin/testWebserverSettings
	./bin/testCGI

re: fclean all

.PHONY: all clean fclean re testRequest testURL testPollHandler testConfigReader testHttpResponse testHttpStatusReason testWebserverSettings testCGI tests
