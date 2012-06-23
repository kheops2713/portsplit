SUBDIRS = src

CLEANDIRS = $(SUBDIRS:%=clean-%)

.PHONY: $(SUBDIRS) clean

$(SUBDIRS):
	$(MAKE) -C $@

all: $(SUBDIRS)

clean: $(CLEANDIRS)

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
