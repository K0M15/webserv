
CC=c++
FILES= main.cpp
NAME=webserv
FLAGS = -Wall -Wextra -Werror -std=c++17
FLAGS += -g
# FLAGS += -fsanitize=address
OBJ_DIR = objects

.PHONY: clean fclean all re $(NAME)

OBJ_FILES = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(FILES))

all: $(OBJ_DIR) $(NAME)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(NAME): $(OBJ_FILES)
	@echo "Building $(NAME)"
	@$(CC) $(FLAGS) -o $(NAME) $(OBJ_FILES)

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@echo "Building $@"
	@$(CC) $(FLAGS) -c -o $@ $< 

clean:
	@rm -rf $(OBJ_DIR)
	@echo "Deleting Objects"

fclean: clean
	@rm -f $(NAME)
	@echo "Deleting Executable"

re: fclean all
