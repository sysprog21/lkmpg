all: lkmpg.tex
	pdflatex -shell-escap lkmpg.tex

html:
	pandoc -s lkmpg.tex -o lkmpg.html

clean:
	rm -f *.dvi *.aux *.log *.ps *.pdf *.out lkmpg.bbl lkmpg.blg lkmpg.lof lkmpg.toc lkmpg.html
	rm -rf _minted-lkmpg
