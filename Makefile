all: lkmpg.tex
	pdflatex -shell-escape lkmpg.tex

clean:
	rm -f *.dvi *.aux *.log *.ps *.pdf *.out lkmpg.bbl lkmpg.blg lkmpg.lof lkmpg.toc
	rm -rf _minted-lkmpg
