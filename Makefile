NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

SRCS = src/main.cpp
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
	$(CXX) $(CXXFLAGS) src/URL.hpp tests/testURL.cpp -o bin/testURL

tests: testRequest testURL
	./bin/testRequest tests/sample_request.txt
	./bin/testURL

re: fclean all

.PHONY: all clean fclean re testRequest tests
