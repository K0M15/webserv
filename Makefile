NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Isrc -g

SRCS = src/main.cpp src/PollHandler.cpp src/ConnectionManager.cpp src/HttpStatusReason.cpp src/Request.cpp src/Webserver.cpp src/ConfigReader.cpp src/WebserverSettings.cpp
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

tests: testRequest testURL testPollHandler
	./bin/testRequest tests/sample_request.txt
	./bin/testURL
	./bin/testPollHandler

re: fclean all

.PHONY: all clean fclean re testRequest testURL testPollHandler tests
