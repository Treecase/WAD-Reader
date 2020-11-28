# Copyright (C) 2020 Trevor Last
# See LICENSE file for copyright and license details.

CXXFLAGS=-Wall -Wextra -g
LDFLAGS=-lSDL2 -lGL -lGLU -lGLEW -lm


SRCDIR=src
OBJDIR=$(SRCDIR)/obj
DEPDIR=$(SRCDIR)/dep



SRC=$(wildcard $(SRCDIR)/*.cpp)
OBJ=$(subst $(SRCDIR),$(OBJDIR),$(SRC:.cpp=.o))
DEP=$(subst $(SRCDIR),$(DEPDIR),$(SRC:.cpp=.d))



wad-reader : $(OBJ) |patches/ textures/
	$(CXX) $^ $(CFLAGS) $(LDFLAGS) -o $@

include $(DEP)


$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) -c $< $(CXXFLAGS) $(LDFLAGS) -o $@

$(DEPDIR)/%.d : $(SRCDIR)/%.cpp
	$(CXX) $^ $(CXXFLAGS) $(LDFLAGS) -MM -MT $(subst $(DEPDIR),$(OBJDIR),$(@:.d=.o)) -MF $@

$(OBJ) :|$(OBJDIR)
$(DEP) :|$(DEPDIR)

$(OBJDIR) :
	@mkdir $@
$(DEPDIR) :
	@mkdir $@
patches/ :
	@mkdir $@
textures/ :
	@mkdir $@



.PHONY: clean
clean:
	@rm -f wad-reader $(OBJ) $(DEP)


