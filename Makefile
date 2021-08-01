all: lkmpg.tex
	rm -rf _minted-main
	pdflatex -shell-escap lkmpg.tex
	bibtex main >/dev/null || echo
	pdflatex -shell-escape $< 2>/dev/null >/dev/null

clean:
	rm -f *.dvi *.aux *.log *.ps *.pdf *.out lkmpg.bbl lkmpg.blg lkmpg.lof lkmpg.toc
	rm -rf _minted-lkmpg
