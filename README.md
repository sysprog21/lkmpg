# The Linux Kernel Module Programming Guide

This project keeps the Linux Kernel Module Programming Guide reasonably up to date, with [working examples](examples/) for recent 5.x kernel versions.
The guide has been around since 2001 and most copies of it on the web only describe old 2.6.x kernels.

The book can be freely accessed via https://sysprog21.github.io/lkmpg/ or [latest PDF file](https://github.com/sysprog21/lkmpg/releases).
The original guide may be found at [Linux Documentation Project](http://www.tldp.org/LDP/lkmpg/).

## Getting Started

### Compile on Local Machine

To prepare for build this book on your local machine, we're going to have make, TeXLive (MacTeX), dvipng and latexmk installed. On various Unix/Linux operating systems, this can be done simply by:

```bash
# Debian / Ubuntu
$ sudo apt install make texlive-full latexmk dvipng

# Arch / Manjaro
$ sudo pacman -S make texlive-most texlive-bin dvipng

# macOS
$ brew install --cask mactex
$ sudo tlmgr update --self && sudo tlmgr install dvipng latexmk
```

Now we could build document with following commands:

```bash
# download project
$ git clone https://github.com/sysprog21/lkmpg.git && cd lkmpg

# run commands
$ make all              # compile pdf document
$ make html             # convert to HTML
$ make clean            # delete generated files
```

### Compile with Docker

The compile process could be run completely using Docker. **Using Docker is recommended, as it guarantees the same dependencies with our GitHub Actions wokrflow.

After [install docker engine](https://docs.docker.com/engine/install/) on your machine, pulling the docker image [twtug/lkmpg](https://hub.docker.com/r/twtug/lkmpg) and compile with it.

Execute followings

```bash
# download project
$ git clone https://github.com/sysprog21/lkmpg.git && cd lkmpg

# pull docker image and run it as container
$ docker pull twtug/lkmpg
$ docker run --rm -it -v $(pwd):/workdir twtug/lkmpg

# run commands
$ make all              # compile pdf document
$ make html             # convert to HTML
$ make clean            # delete generated files
```

## License

The Linux Kernel Module Programming Guide is a free book; you may reproduce and/or modify it under the terms of the [Open Software License](https://opensource.org/licenses/OSL-3.0).
Use of this work is governed by a copyleft license that can be found in the `LICENSE` file.

The complementary sample code is licensed under GNU GPL version 2, as same as Linux kernel.
