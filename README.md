# The Linux Kernel Module Programming Guide

This project keeps the Linux Kernel Module Programming Guide up to date, with [working examples](examples/) for recent 5.x and 6.x kernel versions.
The guide has been around since 2001 and most copies of it on the web only describe old 2.6.x kernels.

The book can be freely accessed via https://sysprog21.github.io/lkmpg/ or [latest PDF file](https://github.com/sysprog21/lkmpg/releases).
The original guide may be found at [Linux Documentation Project](http://www.tldp.org/LDP/lkmpg/).
You may check other [freely available programming books](https://ebookfoundation.github.io/free-programming-books-search/) listed by The [Free Ebook Foundation](https://ebookfoundation.org/) or [Linux online books](https://onlinebooks.library.upenn.edu/webbin/book/browse?type=lcsubc&key=Linux) collected by [The Online Books Page](https://onlinebooks.library.upenn.edu/).

## Getting Started

### Summary
1. Get the latest source code from the [GitHub page](https://github.com/sysprog21/lkmpg).
2. Install the prerequisites.
3. Generate PDF and/or HTML documents.

### Step 1: Get the latest source code

Make sure you can run `git` with an Internet connection.

```shell
$ git clone https://github.com/sysprog21/lkmpg.git && cd lkmpg
```

### Step 2: Install the prerequisites

To generate the book from source, [TeXLive](https://www.tug.org/texlive/) ([MacTeX](https://www.tug.org/mactex/)) is required.

For Ubuntu Linux, macOS, and other Unix-like systems, run the following command(s):

```bash
# Debian / Ubuntu
$ sudo apt install make texlive-full

# Arch / Manjaro
$ sudo pacman -S make texlive-binextra texlive-bin

# macOS
$ brew install mactex
$ sudo tlmgr update --self
```

Note that `latexmk` is required to generated PDF, and it probably has been installed on your OS already. If not, please follow the [installation guide](https://mg.readthedocs.io/latexmk.html#installation).

In macOS systems, package `Pygments` may not be pre-installed. If not, please refer to the [installation guide](https://pygments.org/download/) before generate documents.

Alternatively, using [Docker](https://docs.docker.com/) is recommended, as it guarantees the same dependencies with our GitHub Actions workflow.
After install [docker engine](https://docs.docker.com/engine/install/) on your machine, pull the docker image [twtug/lkmpg](https://hub.docker.com/r/twtug/lkmpg) and run in isolated containers.

```shell
# pull docker image and run it as container
$ docker pull twtug/lkmpg
$ docker run --rm -it -v $(pwd):/workdir twtug/lkmpg
```

[nerdctl](https://github.com/containerd/nerdctl) is a Docker-compatible command line tool for [containerd](https://containerd.io/), and you can replace the above `docker` commands with `nerdctl` counterparts.

### Step 3: Generate PDF and/or HTML documents

Now we could build document with following commands:

```bash
$ make all              # Generate PDF document
$ make html             # Convert TeX to HTML
$ make clean            # Delete generated files
```

## License

The Linux Kernel Module Programming Guide is a free book; you may reproduce and/or modify it under the terms of the [Open Software License](https://opensource.org/licenses/OSL-3.0).
Use of this work is governed by a copyleft license that can be found in the `LICENSE` file.

The complementary sample code is licensed under GNU GPL version 2, as same as Linux kernel.
