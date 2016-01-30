#!/bin/sh
find . -name "*.tex" -exec pdflatex -shell-escape "{}" \;
