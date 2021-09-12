FROM twtug/lkmpg as builder
LABEL MAINTAINER="25077667"

COPY . /workdir

RUN make all && \
	make html && \
	rm -f *.dvi *.aux *.log *.ps *.out lkmpg.bbl lkmpg.blg lkmpg.lof lkmpg.toc

FROM alpine
WORKDIR /workdir

COPY --from=builder /workdir/lkmpg.pdf /workdir
COPY --from=builder /workdir/html /workdir

