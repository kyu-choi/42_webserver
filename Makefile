NAME := webserv

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
CPPFLAGS := -Iinclude -MMD -MP

SRC_DIR := src
OBJ_DIR := build

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/config/Config.cpp \
	$(SRC_DIR)/core/Client.cpp \
	$(SRC_DIR)/core/EventLoop.cpp \
	$(SRC_DIR)/core/Server.cpp \
	$(SRC_DIR)/core/StateMachine.cpp \
	$(SRC_DIR)/http/Http.cpp \
	$(SRC_DIR)/http/HttpRequest.cpp \
	$(SRC_DIR)/http/RequestParser.cpp \
	$(SRC_DIR)/utils/PathPolicy.cpp

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re

-include $(DEPS)
