# The .html -> .gz rule
%.html.gz: %.html
	gzip -c9 $< > $@

# The .gz -> .h rule
%.h: %.html.gz
	xxd -i $< > $@

# Build rule
all: index_ov2640.h

clean:
	rm -f *.html.gz *.h
