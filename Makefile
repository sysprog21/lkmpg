PROJ = lkmpg
all: $(PROJ).pdf

$(PROJ).pdf: lkmpg.tex
	@if ! hash latexmk; then echo "No Latexmk found. See https://mg.readthedocs.io/latexmk.html for installation."; exit 1; fi
	rm -rf _minted-$(PROJ)
	latexmk -shell-escape lkmpg.tex -pdf

html: lkmpg.tex html.cfg assets/Manrope_variable.ttf
	sed $ 's/\t/    /g' lkmpg.tex > lkmpg-for-ht.tex
	make4ht --shell-escape --utf8 --format html5 --config html.cfg --output-dir html  lkmpg-for-ht.tex "fn-in"
	ln -sf lkmpg-for-ht.html html/index.html
	cp assets/Manrope_variable.ttf html/Manrope_variable.ttf
	rm -f  lkmpg-for-ht.tex lkmpg-for-ht.xref lkmpg-for-ht.tmp lkmpg-for-ht.html lkmpg-for-ht.css lkmpg-for-ht.4ct lkmpg-for-ht.4tc lkmpg-for-ht.dvi lkmpg-for-ht.lg lkmpg-for-ht.idv lkmpg*.svg lkmpg-for-ht.log lkmpg-for-ht.aux
	rm -rf _minted-$(PROJ) _minted-lkmpg-for-ht

indent:
	(cd examples; find . -name '*.[ch]' | xargs clang-format -i)

clean:
	rm -f *.dvi *.aux *.log *.ps *.pdf *.out lkmpg.bbl lkmpg.blg lkmpg.lof lkmpg.toc lkmpg.fdb_latexmk lkmpg.fls
	rm -rf html

.PHONY: html
