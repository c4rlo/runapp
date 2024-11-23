prog := runapp
prefix := /usr/local
install_runner := sudo

deps := libsystemd

CXXFLAGS_base := -MMD -MP -Wall -Wextra -Werror -Wtype-limits -Wpedantic -pedantic-errors \
                 -std=c++23 -D_GNU_SOURCE -march=native -fno-plt -pipe -Isrc $(shell pkg-config --cflags $(deps))
CXXFLAGS_release := -O3 -flto -DNDEBUG
CXXFLAGS_debug := -Og -ggdb3 -fsanitize=address -fsanitize=undefined -fhardened -D_GLIBCXX_DEBUG

LDFLAGS_base := -pipe -Wl,--sort-common,--as-needed -z relro -z now -z pack-relative-relocs
LDFLAGS_release := -flto -s
LDFLAGS_debug := -fsanitize=address -fsanitize=undefined -fhardened
LDLIBS := $(shell pkg-config --libs $(deps)) -lstdc++

MAKEFLAGS := -j $(shell nproc)
.DEFAULT_GOAL := debug

modes := debug release
cppfiles := $(wildcard src/*.cpp)
build_dirs := $(addprefix build_,$(modes))

$(modes): %: build_%/$(prog)
$(build_dirs):
	mkdir -p $@

define template
objects_$(1) := $$(addprefix build_$(1)/,$$(notdir $$(cppfiles:.cpp=.o)))
$$(objects_$(1)): override CXXFLAGS := $$(CXXFLAGS_base) $$(CXXFLAGS_$(1)) $$(CXXFLAGS)
build_$(1)/%.o: src/%.cpp Makefile
	$$(COMPILE.cc) -o $$@ $$<
build_$(1)/%.o: | build_$(1)
-include $$(objects_$(1):.o=.d)
build_$(1)/$$(prog): override LDFLAGS := $$(LDFLAGS_base) $$(LDFLAGS_$(1)) $$(LDFLAGS)
build_$(1)/$$(prog): $$(objects_$(1)) Makefile
	   $$(LINK.o) $$(filter-out Makefile,$$^) $$(LDLIBS) -o $$@
build_$(1)/$$(prog): | build_$(1)
endef

$(foreach mode,$(modes),$(eval $(call template,$(mode))))

all: $(modes)

clean:
	$(RM) -r $(build_dirs) compile_commands.json

compile_commands.json: Makefile $(cppfiles)
	bear -- $(MAKE) -B debug

install: build_release/$(prog)
	$(install_runner) install -D -t $(DESTDIR)$(prefix)/bin $<

uninstall:
	$(install_runner) $(RM) $(DESTDIR)$(prefix)/bin/$(prog) \

.PHONY: all clean install uninstall $(modes)
.DELETE_ON_ERROR:
