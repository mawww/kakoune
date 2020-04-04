all:
	$(MAKE) -C src $@

%: FORCE
	$(MAKE) -C src $@

.PHONY: all FORCE
