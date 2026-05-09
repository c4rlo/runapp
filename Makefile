## These are designed to be overridden by packaging scripts:
prefix := /usr/local
install_runner := sudo
native_flag := -march=native
##

prog := runapp
deps := libsystemd

CXXFLAGS_base := -MMD -MP -Wall -Wextra -Werror -Wtype-limits -Wpedantic -pedantic-errors \
                 -Wlogical-op -Wnull-dereference -Wuseless-cast -Wcast-align=strict \
                 -std=c++23 -D_GNU_SOURCE -fno-plt $(native_flag) -pipe -Isrc \
                 $(shell pkg-config --cflags $(deps))
CXXFLAGS_release := -O3 -flto -DNDEBUG
CXXFLAGS_debug := -Og -ggdb3 -fsanitize=address -fsanitize=undefined -fhardened -D_GLIBCXX_DEBUG

LDFLAGS_base := -pipe -Wl,--sort-common,--as-needed -z relro -z now -z pack-relative-relocs
LDFLAGS_release := -flto -s
LDFLAGS_debug := -fsanitize=address -fsanitize=undefined -fhardened
LDLIBS := $(shell pkg-config --libs $(deps))

MAKEFLAGS := -j $(shell nproc)
.DEFAULT_GOAL := debug

modes := debug release
cppfiles := $(wildcard src/*.cpp)
build_dirs := $(addprefix build_,$(modes))

$(modes): %: build_%/$(prog)
$(build_dirs):
	mkdir -p $@

define mode_template
objects_$(1) := $$(addprefix build_$(1)/,$$(notdir $$(cppfiles:.cpp=.o)))
$$(objects_$(1)): override CXXFLAGS := $$(CXXFLAGS_base) $$(CXXFLAGS_$(1)) $$(CXXFLAGS)
build_$(1)/%.o: src/%.cpp Makefile | build_$(1)
	$$(COMPILE.cc) -o $$@ $$<
-include $$(objects_$(1):.o=.d)
build_$(1)/$$(prog): override LDFLAGS := $$(LDFLAGS_base) $$(LDFLAGS_$(1)) $$(LDFLAGS)
build_$(1)/$$(prog): $$(objects_$(1)) Makefile
	   $$(LINK.cc) $$(filter-out Makefile,$$^) $$(LDLIBS) -o $$@
build_$(1)/$$(prog): | build_$(1)
endef

$(foreach mode,$(modes),$(eval $(call mode_template,$(mode))))

all: $(modes)

clean:
	$(RM) -r $(build_dirs) compile_commands.json

lint: $(cppfiles) compile_commands.json
	clang-tidy --quiet $(cppfiles)

compile_commands.json: Makefile $(cppfiles)
	bear -- $(MAKE) -B $(objects_debug)

install: build_release/$(prog)
	$(install_runner) install -D -t $(DESTDIR)$(prefix)/bin $<
	$(install_runner) install -D -m644 -t $(DESTDIR)$(prefix)/share/man/man1 runapp.1

uninstall:
	$(install_runner) $(RM) \
		$(DESTDIR)$(prefix)/bin/$(prog) \
		$(DESTDIR)$(prefix)/share/man/man1/runapp.1

.PHONY: all clean install lint uninstall $(modes)
.DELETE_ON_ERROR:
