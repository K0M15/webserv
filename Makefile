NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude -g -MMD -MP

SRCS =  src/main.cpp \
		src/PollHandler.cpp \
		src/ConnectionManager.cpp \
		src/RequestHandler.cpp \
		src/SessionManager.cpp \
		src/Response.cpp \
		src/HttpStatusReason.cpp \
		src/Request.cpp \
		src/Webserver.cpp \
		src/ConfigReader.cpp \
		src/WebserverSettings.cpp \
		src/CGIHandler.cpp \
		src/PathUtils.cpp \
		src/Chunked.cpp \
		src/URL.cpp

OBJ_DIR = obj
DEPS = $(OBJS:.o=.d)

OBJS =  obj/main.o \
		obj/PollHandler.o \
		obj/ConnectionManager.o \
		obj/RequestHandler.o \
		obj/SessionManager.o \
		obj/Response.o \
		obj/HttpStatusReason.o \
		obj/Request.o \
		obj/Webserver.o \
		obj/ConfigReader.o \
		obj/WebserverSettings.o \
		obj/CGIHandler.o \
		obj/PathUtils.o \
		obj/Chunked.o \
		obj/URL.o

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

-include $(DEPS)

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
	@rm -f $(OBJS) $(DEPS)

fclean: clean
	@rm -f $(NAME)

testRequest: createTestDIR
	$(CXX) $(CXXFLAGS) src/Request.cpp src/Chunked.cpp src/URL.cpp src/PathUtils.cpp src/PollHandler.cpp tests/testRequest.cpp -o bin/testRequest

testURL: createTestDIR
	$(CXX) $(CXXFLAGS) src/URL.cpp src/PathUtils.cpp tests/testURL.cpp -o bin/testURL

testChunked: createTestDIR
	$(CXX) $(CXXFLAGS) src/Request.cpp src/Chunked.cpp src/URL.cpp src/PathUtils.cpp src/PollHandler.cpp tests/testChunked.cpp -o bin/testChunked

testPollHandler: createTestDIR
	$(CXX) $(CXXFLAGS) src/PollHandler.cpp tests/testPollHandler.cpp -o bin/testPollHandler

testConfigReader: createTestDIR
	$(CXX) $(CXXFLAGS) src/ConfigReader.cpp src/WebserverSettings.cpp tests/testConfigReader.cpp -o bin/testConfigReader

testHttpResponse: createTestDIR
	$(CXX) $(CXXFLAGS) src/Response.cpp src/HttpStatusReason.cpp src/PathUtils.cpp tests/testHttpResponse.cpp -o bin/testResponse

testHttpStatusReason: createTestDIR
	$(CXX) $(CXXFLAGS) src/HttpStatusReason.cpp tests/testHttpStatusReason.cpp -o bin/testHttpStatusReason

testWebserverSettings: createTestDIR
	$(CXX) $(CXXFLAGS) src/WebserverSettings.cpp tests/testWebserverSettings.cpp -o bin/testWebserverSettings

testExpect: createTestDIR
	$(CXX) $(CXXFLAGS) src/Request.cpp src/Chunked.cpp src/URL.cpp src/PathUtils.cpp src/PollHandler.cpp src/WebserverSettings.cpp src/CGIHandler.cpp src/Response.cpp src/HttpStatusReason.cpp tests/testExpect.cpp -o bin/testExpect

testCGI: createTestDIR
	$(CXX) $(CXXFLAGS) \
		src/ConnectionManager.cpp \
		src/RequestHandler.cpp \
		src/SessionManager.cpp \
		src/CGIHandler.cpp \
		src/Request.cpp \
		src/Chunked.cpp \
		src/URL.cpp \
		src/PathUtils.cpp \
		src/PollHandler.cpp \
		src/Response.cpp \
		src/HttpStatusReason.cpp \
		src/WebserverSettings.cpp \
		tests/testCGI.cpp \
		-o bin/testCGI

testHead: createTestDIR
	$(CXX) $(CXXFLAGS) src/CGIHandler.cpp src/Request.cpp src/Chunked.cpp src/ConnectionManager.cpp src/RequestHandler.cpp src/SessionManager.cpp src/PathUtils.cpp src/PollHandler.cpp src/Response.cpp src/HttpStatusReason.cpp src/URL.cpp src/WebserverSettings.cpp tests/testHead.cpp -o bin/testHead

tests: testRequest testURL testChunked testPollHandler testConfigReader testHttpResponse testHttpStatusReason testWebserverSettings testCGI testHead testExpect
	./bin/testRequest tests/sample_request.txt
	./bin/testURL
	./bin/testChunked
	./bin/testPollHandler
	./bin/testConfigReader
	./bin/testResponse
	./bin/testHttpStatusReason
	./bin/testWebserverSettings
	./bin/testCGI
	./bin/testHead
	./bin/testExpect

re: fclean all

debug:
	@$(MAKE) --no-print-directory re CXXFLAGS="$(CXXFLAGS) -DDEBUG"

.PHONY: all clean fclean re testRequest testURL testChunked testPollHandler testConfigReader testResponse testHttpStatusReason testWebserverSettings testCGI testExpect tests debug testHead
