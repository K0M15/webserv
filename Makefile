NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude -g

SRCS =  src/main.cpp \
		src/PollHandler.cpp \
		src/ConnectionManager.cpp \
		src/HttpResponse.cpp \
		src/HttpStatusReason.cpp \
		src/Request.cpp \
		src/Webserver.cpp \
		src/ConfigReader.cpp \
		src/WebserverSettings.cpp \
		src/CGIHandler.cpp
		src/PathUtils.cpp

OBJ_DIR = obj

OBJS =  obj/main.o \
		obj/PollHandler.o \
		obj/ConnectionManager.o \
		obj/HttpResponse.o \
		obj/HttpStatusReason.o \
		obj/Request.o \
		obj/Webserver.o \
		obj/ConfigReader.o \
		obj/WebserverSettings.o \
		obj/CGIHandler.o
		obj/PathUtils.o

GREEN = \033[1;32m
WHITE = \033[0m

all: 
	@echo "Building Webserv..." 
	@$(MAKE) --no-print-directory $(NAME)
	@echo "▗▖ ▗▖▗▄▄▄▖▗▄▄▖  ▗▄▄▖▗▄▄▄▖▗▄▄▖ ▗▖  ▗▖"
	@echo "▐▌ ▐▌▐▌   ▐▌ ▐▌▐▌   ▐▌   ▐▌ ▐▌▐▌  ▐▌"
	@echo "▐▌ ▐▌▐▛▀▀▘▐▛▀▚▖ ▝▀▚▖▐▛▀▀▘▐▛▀▚▖▐▌  ▐▌"
	@echo "▐▙█▟▌▐▙▄▄▖▐▙▄▞▘▗▄▄▞▘▐▙▄▄▖▐▌ ▐▌ ▝▚▞▘ "
	@echo "A 42 project by: afelger, dabierma, and jpflegha"
	@echo "$(GREEN)Build Completed -> Run with ./webserv test.conf$(WHITE)"

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

createTestDIR:
	mkdir -p bin

clean:
	@rm -f $(OBJS)

fclean: clean
	@rm -f $(NAME)

testRequest: createTestDIR
	$(CXX) $(CXXFLAGS) src/Request.cpp tests/testRequest.cpp -o bin/testRequest

testURL: createTestDIR
	$(CXX) $(CXXFLAGS) tests/testURL.cpp -o bin/testURL

testPollHandler: createTestDIR
	$(CXX) $(CXXFLAGS) src/PollHandler.cpp tests/testPollHandler.cpp -o bin/testPollHandler

testConfigReader: createTestDIR
	$(CXX) $(CXXFLAGS) src/ConfigReader.cpp src/WebserverSettings.cpp tests/testConfigReader.cpp -o bin/testConfigReader

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
